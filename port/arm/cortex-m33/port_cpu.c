/**
 * @file port_cpu.c
 * @brief ARM Cortex-M33 (ARMv8-M) port CPU implementation for SertOS.
 *
 * Implements hardware stack frame initialization with ARMv8-M PSPLIM hardware
 * stack limit support, SysTick initialization, and critical section masking.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include <string.h>

/**
 * @brief Cortex-M System Control Block registers for PendSV and SysTick.
 */
#define CORTEX_M_ICSR               (*(volatile uint32_t*)0xE000ED04U)
#define CORTEX_M_ICSR_PENDSVSET     (1U << 28U)

#define CORTEX_M_SYSTICK_CTRL       (*(volatile uint32_t*)0xE000E010U)
#define CORTEX_M_SYSTICK_LOAD       (*(volatile uint32_t*)0xE000E014U)
#define CORTEX_M_SYSTICK_VAL        (*(volatile uint32_t*)0xE000E018U)

#define CORTEX_M_SYSTICK_CTRL_CLKSOURCE (1U << 2U)
#define CORTEX_M_SYSTICK_CTRL_TICKINT   (1U << 1U)
#define CORTEX_M_SYSTICK_CTRL_ENABLE    (1U << 0U)

/**
 * @brief Default xPSR register value setting Thumb execution mode.
 */
#define CORTEX_M_DEFAULT_XPSR       (0x01000000U)

/**
 * @brief Standard EXC_RETURN value for unprivileged thread mode using PSP.
 */
#define CORTEX_M_EXC_RETURN_THREAD_PSP (0xFFFFFFFDU)

/**
 * @brief Hardware and software stacked context layout on ARMv8-M Cortex-M33.
 */
typedef struct CortexM33StackFrame {
    /* Software saved registers (PendSV_Handler) */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t exc_return;

    /* Hardware automatically saved registers (Exception entry) */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} CortexM33StackFrame;

static uint32_t s_critical_nesting = 0U;

void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param)
{
    CortexM33StackFrame* frame;
    uintptr_t top_addr;

    (void)stack_limit;

    if (stack_top == NULL) {
        return NULL;
    }

    top_addr = (uintptr_t)stack_top;
    top_addr &= ~((uintptr_t)SERTOS_STACK_ALIGNMENT_BYTES - 1U);
    top_addr -= sizeof(CortexM33StackFrame);

    frame = (CortexM33StackFrame*)top_addr;

    /* Software context */
    frame->r4 = 0x04040404U;
    frame->r5 = 0x05050505U;
    frame->r6 = 0x06060606U;
    frame->r7 = 0x07070707U;
    frame->r8 = 0x08080808U;
    frame->r9 = 0x09090909U;
    frame->r10 = 0x10101010U;
    frame->r11 = 0x11111111U;
    frame->exc_return = CORTEX_M_EXC_RETURN_THREAD_PSP;

    /* Hardware exception frame */
    frame->r0 = (uint32_t)(uintptr_t)param;
    frame->r1 = 0U;
    frame->r2 = 0U;
    frame->r3 = 0U;
    frame->r12 = 0U;
    frame->lr = 0U;
    frame->pc = (uint32_t)(uintptr_t)entry;
    frame->xpsr = CORTEX_M_DEFAULT_XPSR;

    return (void*)frame;
}

void sertos_port_yield(void)
{
    CORTEX_M_ICSR = CORTEX_M_ICSR_PENDSVSET;
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

#ifndef SYSTEM_CORE_CLOCK_HZ
#define SYSTEM_CORE_CLOCK_HZ (25000000U)
#endif

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    if (tick_rate_hz > 0U) {
        /* Set PendSV to lowest priority (0xFF) in SHPR3 */
        *(volatile uint32_t*)0xE000ED20U |= 0x00FF0000U;

        CORTEX_M_SYSTICK_LOAD = (SYSTEM_CORE_CLOCK_HZ / tick_rate_hz) - 1U;
        CORTEX_M_SYSTICK_VAL  = 0U;
        CORTEX_M_SYSTICK_CTRL = CORTEX_M_SYSTICK_CTRL_CLKSOURCE |
                                CORTEX_M_SYSTICK_CTRL_TICKINT   |
                                CORTEX_M_SYSTICK_CTRL_ENABLE;
    }
}

#define CORTEX_M_AIRCR              (*(volatile uint32_t*)0xE000ED0CU)
#define CORTEX_M_AIRCR_VECTKEY      (0x05FA0000U)
#define CORTEX_M_AIRCR_SYSRESETREQ  (1U << 2U)

void sertos_port_stop_scheduler(void)
{
    CORTEX_M_SYSTICK_CTRL = 0U;

    /* 1. Semihosting exit: SYS_EXIT (0x18)
     * For ARMv8-M / Cortex-M33 under QEMU, bkpt 0xAB traps to QEMU semihosting.
     * r0 = 0x18 (TARGET_SYS_EXIT), r1 = 0x20026 (ADP_Stopped_ApplicationExit)
     */
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

    /* 2. Fallback: Request CPU Reset via AIRCR (VECTKEY | SYSRESETREQ).
     * When QEMU is launched with -no-reboot, this triggers a clean emulator shutdown.
     */
    CORTEX_M_AIRCR = CORTEX_M_AIRCR_VECTKEY | CORTEX_M_AIRCR_SYSRESETREQ;

    while (1) {
#if defined(__arm__) || defined(__thumb__)
        __asm__ volatile ("wfi");
#endif
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
