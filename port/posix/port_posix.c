/**
 * @file port_posix.c
 * @brief POSIX simulator port implementation for SertOS.
 *
 * Provides architecture stack frame initialization, critical section masking,
 * and context switching hooks for Linux, macOS, and POSIX-compliant environments.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include <string.h>

/**
 * @brief Simulated architectural exception stack frame (AAPCS-compatible 16-word layout).
 */
typedef struct PortStackFrame {
    void* r4;
    void* r5;
    void* r6;
    void* r7;
    void* r8;
    void* r9;
    void* r10;
    void* r11;
    void* r0;   /**< Parameter passed to task entry (R0). */
    void* r1;
    void* r2;
    void* r3;
    void* r12;
    void* lr;   /**< Link register / return address. */
    void* pc;   /**< Program counter / entry point. */
    void* psr;  /**< Program status register. */
} PortStackFrame;

/**
 * @brief Simulated critical section nesting counter.
 */
static uint32_t s_critical_nesting = 0U;

/**
 * @brief Simulated yield execution counter for verification.
 */
static uint32_t s_yield_count = 0U;

/**
 * @brief Configured system tick rate in Hz.
 */
static uint32_t s_tick_rate_hz = 0U;

void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param)
{
    PortStackFrame* frame;
    uintptr_t top_addr;

    (void)stack_limit;

    if (stack_top == NULL) {
        return NULL;
    }

    top_addr = (uintptr_t)stack_top;
    top_addr &= ~((uintptr_t)SERTOS_STACK_ALIGNMENT_BYTES - 1U);
    top_addr -= sizeof(PortStackFrame);

    frame = (PortStackFrame*)top_addr;

    frame->r4  = (void*)(uintptr_t)0x04040404U;
    frame->r5  = (void*)(uintptr_t)0x05050505U;
    frame->r6  = (void*)(uintptr_t)0x06060606U;
    frame->r7  = (void*)(uintptr_t)0x07070707U;
    frame->r8  = (void*)(uintptr_t)0x08080808U;
    frame->r9  = (void*)(uintptr_t)0x09090909U;
    frame->r10 = (void*)(uintptr_t)0x10101010U;
    frame->r11 = (void*)(uintptr_t)0x11111111U;

    frame->r0  = param;
    frame->r1  = (void*)0U;
    frame->r2  = (void*)0U;
    frame->r3  = (void*)0U;
    frame->r12 = (void*)0U;
    frame->lr  = (void*)0U;
    frame->pc  = (void*)(uintptr_t)entry;
    frame->psr = (void*)(uintptr_t)0x01000000U;

    return (void*)frame;
}

void sertos_port_start_first_task(void)
{
    SertosTaskControlBlock* current;

    current = sertos_scheduler_get_current_tcb();
    if (current != NULL) {
        current->state = SERTOS_TASK_STATE_RUNNING;
    }
}

void sertos_port_yield(void)
{
    s_yield_count++;
}

uint32_t sertos_port_enter_critical(void)
{
    uint32_t prev_nesting = s_critical_nesting;
    s_critical_nesting++;
    return prev_nesting;
}

void sertos_port_exit_critical(uint32_t status)
{
    (void)status;
    if (s_critical_nesting > 0U) {
        s_critical_nesting--;
    }
}

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    s_tick_rate_hz = tick_rate_hz;
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_stop_scheduler(void)
{
}
