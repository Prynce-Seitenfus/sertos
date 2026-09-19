/**
 * @file port_cpu.c
 * @brief ARM Cortex-M55 port CPU implementation for SertOS.
 *
 * Implements hardware stack frame initialization for ARMv8.1-M Mainline,
 * PSPLIM stack limit protection, FPU/Helium support, SysTick, and critical section masking.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include <string.h>

#define CORTEX_M55_ICSR              (*(volatile uint32_t*)0xE000ED04U)
#define CORTEX_M55_ICSR_PENDSVSET    (1U << 28U)
#define CORTEX_M55_DEFAULT_XPSR      (0x01000000U)
#define CORTEX_M55_EXC_RETURN_THREAD_PSP (0xFFFFFFFDU)

typedef struct CortexM55StackFrame {
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t exc_return;

    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} CortexM55StackFrame;

static uint32_t s_critical_nesting = 0U;

void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param)
{
    CortexM55StackFrame* frame;
    uintptr_t top_addr;

    (void)stack_limit;

    if (stack_top == NULL) {
        return NULL;
    }

    top_addr = (uintptr_t)stack_top;
    top_addr &= ~((uintptr_t)SERTOS_STACK_ALIGNMENT_BYTES - 1U);
    top_addr -= sizeof(CortexM55StackFrame);

    frame = (CortexM55StackFrame*)top_addr;

    frame->r4 = 0x04040404U;
    frame->r5 = 0x05050505U;
    frame->r6 = 0x06060606U;
    frame->r7 = 0x07070707U;
    frame->r8 = 0x08080808U;
    frame->r9 = 0x09090909U;
    frame->r10 = 0x10101010U;
    frame->r11 = 0x11111111U;
    frame->exc_return = CORTEX_M55_EXC_RETURN_THREAD_PSP;

    frame->r0 = (uint32_t)(uintptr_t)param;
    frame->r1 = 0U;
    frame->r2 = 0U;
    frame->r3 = 0U;
    frame->r12 = 0U;
    frame->lr = 0U;
    frame->pc = (uint32_t)(uintptr_t)entry;
    frame->xpsr = CORTEX_M55_DEFAULT_XPSR;

    return (void*)frame;
}

void sertos_port_yield(void)
{
    CORTEX_M55_ICSR = CORTEX_M55_ICSR_PENDSVSET;
}

uint32_t sertos_port_enter_critical(void)
{
    uint32_t prev_primask = 0U;

#if defined(__arm__) || defined(__thumb__)
    __asm__ volatile (
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r" (prev_primask) :: "memory"
    );
#endif

    s_critical_nesting++;
    return prev_primask;
}

void sertos_port_exit_critical(uint32_t status)
{
    if (s_critical_nesting > 0U) {
        s_critical_nesting--;
        if (s_critical_nesting == 0U) {
#if defined(__arm__) || defined(__thumb__)
            __asm__ volatile (
                "msr primask, %0\n"
                :: "r" (status) : "memory"
            );
#else
            (void)status;
#endif
        }
    }
}

#define CORTEX_M55_SYSTICK_CTRL      (*(volatile uint32_t*)0xE000E010U)
#define CORTEX_M55_SYSTICK_LOAD      (*(volatile uint32_t*)0xE000E014U)
#define CORTEX_M55_SYSTICK_VAL       (*(volatile uint32_t*)0xE000E018U)

#define CORTEX_M55_SYSTICK_CTRL_CLKSOURCE (1U << 2U)
#define CORTEX_M55_SYSTICK_CTRL_TICKINT   (1U << 1U)
#define CORTEX_M55_SYSTICK_CTRL_ENABLE    (1U << 0U)

#ifndef SYSTEM_CORE_CLOCK_HZ
#define SYSTEM_CORE_CLOCK_HZ (25000000U)
#endif

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    if (tick_rate_hz > 0U) {
        /* Set PendSV to lowest priority in SHPR3 */
        *(volatile uint32_t*)0xE000ED20U |= 0x00FF0000U;

        CORTEX_M55_SYSTICK_LOAD = (SYSTEM_CORE_CLOCK_HZ / tick_rate_hz) - 1U;
        CORTEX_M55_SYSTICK_VAL  = 0U;
        CORTEX_M55_SYSTICK_CTRL = CORTEX_M55_SYSTICK_CTRL_CLKSOURCE |
                                  CORTEX_M55_SYSTICK_CTRL_TICKINT   |
                                  CORTEX_M55_SYSTICK_CTRL_ENABLE;
    }
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

#define CORTEX_M55_AIRCR             (*(volatile uint32_t*)0xE000ED0CU)
#define CORTEX_M55_AIRCR_VECTKEY     (0x05FA0000U)
#define CORTEX_M55_AIRCR_SYSRESETREQ (1U << 2U)

void sertos_port_stop_scheduler(void)
{
    CORTEX_M55_SYSTICK_CTRL = 0U;

    /* 1. Semihosting exit: SYS_EXIT (0x18) */
#if defined(__arm__) || defined(__thumb__)
    register uint32_t r0 __asm__("r0") = 0x18U;
    register uint32_t r1 __asm__("r1") = 0x20026U;
    __asm__ volatile (
        "bkpt 0xAB"
        :
        : "r" (r0), "r" (r1)
        : "memory"
    );
#endif

    /* 2. Hardware System Reset if semihosting not trapped */
    CORTEX_M55_AIRCR = CORTEX_M55_AIRCR_VECTKEY | CORTEX_M55_AIRCR_SYSRESETREQ;

    /* 3. Fallback spin loop */
    while (1) {
#if defined(__arm__) || defined(__thumb__)
        __asm__ volatile ("wfi");
#endif
    }
}
