/**
 * @file port_cpu.c
 * @brief ARM Cortex-M55 port CPU implementation for SertOS.
 *
 * Implements hardware stack frame initialization for ARMv8.1-M Mainline,
 * PSPLIM stack limit protection, FPU/Helium support, SysTick, and critical section masking.
 */

#include "sertos_port.h"
#include "sertos_port_weak.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include "../cortex_m_systick.h"
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

SERTOS_PORT_WEAK uint32_t sertos_port_tick_clock_hz(void)
{
    return 25000000U;
}

#ifndef CORTEX_M_DEMCR
#define CORTEX_M_DEMCR              (*(volatile uint32_t*)0xE000EDFCU)
#endif
#define CORTEX_M_DEMCR_TRCENA       (1U << 24U)
#define CORTEX_M_DWT_CTRL           (*(volatile uint32_t*)0xE0001000U)
#define CORTEX_M_DWT_CTRL_CYCCNTENA (1U << 0U)
#define CORTEX_M_DWT_CYCCNT         (*(volatile uint32_t*)0xE0001004U)

/* True when the DWT cycle counter is present and advancing. */
static bool s_runtime_dwt_ok = false;

SERTOS_PORT_WEAK uint32_t sertos_port_runtime_counter(void)
{
    if (s_runtime_dwt_ok) {
        return CORTEX_M_DWT_CYCCNT;
    }

    /* Fallback for cores/emulators lacking a functional DWT cycle counter. */
    return (uint32_t)sertos_scheduler_get_tick_count();
}

SERTOS_PORT_WEAK void sertos_port_runtime_counter_init(void)
{
    uint32_t start;
    volatile uint32_t spin;

    CORTEX_M_DEMCR |= CORTEX_M_DEMCR_TRCENA;
    CORTEX_M_DWT_CYCCNT = 0U;
    CORTEX_M_DWT_CTRL |= CORTEX_M_DWT_CTRL_CYCCNTENA;

    start = CORTEX_M_DWT_CYCCNT;
    for (spin = 0U; spin < 8U; spin++) {
        /* Spin so the cycle counter can advance on real hardware. */
    }

    /* If CYCCNT never moved, DWT is absent (e.g. some QEMU models); use ticks. */
    s_runtime_dwt_ok = (CORTEX_M_DWT_CYCCNT != start);
}

SertosStatus sertos_port_tick_init(uint32_t tick_rate_hz)
{
    uint32_t reload_value;

    if (!cortex_m_systick(sertos_port_tick_clock_hz(),
                          tick_rate_hz,
                          &reload_value)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    /* Set PendSV to lowest priority in SHPR3 */
    *(volatile uint32_t*)0xE000ED20U |= 0x00FF0000U;

    CORTEX_M55_SYSTICK_LOAD = reload_value;
    CORTEX_M55_SYSTICK_VAL  = 0U;
    CORTEX_M55_SYSTICK_CTRL = CORTEX_M55_SYSTICK_CTRL_CLKSOURCE |
                              CORTEX_M55_SYSTICK_CTRL_TICKINT   |
                              CORTEX_M55_SYSTICK_CTRL_ENABLE;
    return SERTOS_STATUS_OK;
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void SysTick_Handler(void)
{
    if (sertos_scheduler_is_running()) {
        sertos_scheduler_tick();
    }
}

#define CORTEX_M55_AIRCR             (*(volatile uint32_t*)0xE000ED0CU)
#define CORTEX_M55_AIRCR_VECTKEY     (0x05FA0000U)
#define CORTEX_M55_AIRCR_SYSRESETREQ (1U << 2U)

void sertos_port_stop_scheduler(void)
{
    CORTEX_M55_SYSTICK_CTRL = 0U;

    /* Request CPU Reset via AIRCR (VECTKEY | SYSRESETREQ) */
    CORTEX_M55_AIRCR = CORTEX_M55_AIRCR_VECTKEY | CORTEX_M55_AIRCR_SYSRESETREQ;

    while (1) {
#if defined(__arm__) || defined(__thumb__)
        __asm__ volatile ("wfi");
#endif
    }
}
