/**
 * @file port_windows.c
 * @brief Preemptive Windows host simulator port implementation for SertOS.
 *
 * Implements preemptive multitasking using native Win32 threads and synchronization
 * events, supported by a 1 kHz high-resolution multimedia tick thread.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include "sertos_config.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdbool.h>
#include <stdint.h>
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
 * @brief Windows-specific task context descriptor.
 */
typedef struct PortWindowsContext {
    HANDLE thread_handle;               /**< Win32 native thread handle. */
    HANDLE event_handle;                /**< Synchronization auto-reset event. */
    DWORD thread_id;                    /**< Win32 thread identifier. */
    SertosTaskControlBlock* tcb;        /**< Associated SertOS TCB pointer. */
    bool is_suspended_by_tick;          /**< True if preemptively suspended by tick thread. */
    bool in_use;                        /**< Allocation flag within static pool. */
} PortWindowsContext;

/**
 * @brief Static context descriptor pool (zero dynamic allocation).
 */
static PortWindowsContext s_task_contexts[SERTOS_CONFIG_MAX_TASKS];

/**
 * @brief Critical section object for scheduler reentrancy guard.
 */
static CRITICAL_SECTION s_port_cs;
static bool s_cs_initialized = false;

/**
 * @brief Signal event used to unblock sertos_port_start_first_task upon termination.
 */
static HANDLE s_exit_event = NULL;

/**
 * @brief High-resolution periodic tick thread handle and identifier.
 */
static HANDLE s_tick_thread = NULL;
static DWORD s_tick_thread_id = 0U;

/**
 * @brief Global scheduler running state flag.
 */
static volatile bool s_scheduler_running = false;

/**
 * @brief Pointer to context descriptor of the currently executing task.
 */
static PortWindowsContext* s_running_ctx = NULL;

/**
 * @brief Native Win32 thread wrapper invoking SertOS task entry.
 *
 * @param param Pointer to associated SertosTaskControlBlock.
 * @return Thread exit code.
 */
static DWORD WINAPI port_task_wrapper(LPVOID param)
{
    SertosTaskControlBlock* tcb = (SertosTaskControlBlock*)param;
    PortWindowsContext* ctx;

    if (tcb == NULL) {
        return 1U;
    }

    ctx = (PortWindowsContext*)tcb->port_context;
    if (ctx == NULL) {
        return 1U;
    }

    /* Wait for scheduler dispatch signal */
    (void)WaitForSingleObject(ctx->event_handle, INFINITE);

    if (s_scheduler_running && (tcb->entry_func != NULL)) {
        tcb->entry_func(tcb->param);
    }

    (void)sertos_task_delete(tcb);
    return 0U;
}

/**
 * @brief Periodic 1 kHz tick driver thread function.
 *
 * @param param Unused thread argument.
 * @return Thread exit code.
 */
static DWORD WINAPI port_tick_thread_func(LPVOID param)
{
    (void)param;
    (void)timeBeginPeriod(1U);

    while (s_scheduler_running) {
        Sleep(1U);
        if (s_scheduler_running) {
            sertos_scheduler_tick();
        }
    }

    (void)timeEndPeriod(1U);
    return 0U;
}

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
    size_t i;

    if (!s_cs_initialized) {
        InitializeCriticalSection(&s_port_cs);
        s_cs_initialized = true;
    }

    s_exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_scheduler_running = true;

    s_tick_thread = CreateThread(NULL, 0U, port_tick_thread_func, NULL, 0U, &s_tick_thread_id);

    for (i = 0U; i < SERTOS_CONFIG_MAX_TASKS; i++) {
        if (s_task_contexts[i].in_use && (s_task_contexts[i].thread_handle != NULL)) {
            (void)ResumeThread(s_task_contexts[i].thread_handle);
        }
    }

    current = sertos_scheduler_get_current_tcb();
    if (current != NULL) {
        current->state = SERTOS_TASK_STATE_RUNNING;
        s_running_ctx = (PortWindowsContext*)current->port_context;
        if (s_running_ctx != NULL) {
            (void)SetEvent(s_running_ctx->event_handle);
        }
    }

    (void)WaitForSingleObject(s_exit_event, INFINITE);

    s_scheduler_running = false;
    if (s_tick_thread != NULL) {
        (void)WaitForSingleObject(s_tick_thread, 100U);
        (void)CloseHandle(s_tick_thread);
        s_tick_thread = NULL;
    }
    if (s_exit_event != NULL) {
        (void)CloseHandle(s_exit_event);
        s_exit_event = NULL;
    }
    if (s_cs_initialized) {
        DeleteCriticalSection(&s_port_cs);
        s_cs_initialized = false;
    }
}

void sertos_port_yield(void)
{
    PortWindowsContext* next_ctx;
    PortWindowsContext* prev_ctx;
    SertosTaskControlBlock* current;
    DWORD current_tid;

    if (!s_scheduler_running) {
        return;
    }

    if (!s_cs_initialized) {
        InitializeCriticalSection(&s_port_cs);
        s_cs_initialized = true;
    }

    EnterCriticalSection(&s_port_cs);

    current = sertos_scheduler_get_current_tcb();
    if (current == NULL) {
        LeaveCriticalSection(&s_port_cs);
        return;
    }

    next_ctx = (PortWindowsContext*)current->port_context;
    current_tid = GetCurrentThreadId();

    if (current_tid == s_tick_thread_id) {
        if ((next_ctx != NULL) && (next_ctx != s_running_ctx)) {
            prev_ctx = s_running_ctx;
            s_running_ctx = next_ctx;
            if ((prev_ctx != NULL) && (prev_ctx->thread_handle != NULL)) {
                (void)SuspendThread(prev_ctx->thread_handle);
                prev_ctx->is_suspended_by_tick = true;
            }
            if (next_ctx->is_suspended_by_tick) {
                next_ctx->is_suspended_by_tick = false;
                (void)ResumeThread(next_ctx->thread_handle);
            } else {
                (void)SetEvent(next_ctx->event_handle);
            }
        }
        LeaveCriticalSection(&s_port_cs);
        return;
    }

    if (next_ctx == s_running_ctx) {
        LeaveCriticalSection(&s_port_cs);
        Sleep(1U);
        return;
    }

    prev_ctx = s_running_ctx;
    s_running_ctx = next_ctx;

    if (next_ctx != NULL) {
        if (next_ctx->is_suspended_by_tick) {
            next_ctx->is_suspended_by_tick = false;
            (void)ResumeThread(next_ctx->thread_handle);
        } else {
            (void)SetEvent(next_ctx->event_handle);
        }
    }

    LeaveCriticalSection(&s_port_cs);

    if (prev_ctx != NULL) {
        (void)WaitForSingleObject(prev_ctx->event_handle, INFINITE);
    }
}

uint32_t sertos_port_enter_critical(void)
{
    if (!s_cs_initialized) {
        InitializeCriticalSection(&s_port_cs);
        s_cs_initialized = true;
    }

    EnterCriticalSection(&s_port_cs);
    return 0U;
}

void sertos_port_exit_critical(uint32_t status)
{
    (void)status;
    if (s_cs_initialized) {
        LeaveCriticalSection(&s_port_cs);
    }
}

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    (void)tick_rate_hz;
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    size_t i;
    PortWindowsContext* ctx = NULL;

    if (tcb == NULL) {
        return;
    }

    for (i = 0U; i < SERTOS_CONFIG_MAX_TASKS; i++) {
        if (!s_task_contexts[i].in_use) {
            ctx = &s_task_contexts[i];
            ctx->in_use = true;
            break;
        }
    }

    if (ctx == NULL) {
        return;
    }

    ctx->tcb = tcb;
    ctx->is_suspended_by_tick = false;
    ctx->event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    ctx->thread_handle = CreateThread(NULL, 0U, port_task_wrapper, tcb, CREATE_SUSPENDED, &ctx->thread_id);

    tcb->port_context = (void*)ctx;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    PortWindowsContext* ctx;

    if (tcb == NULL) {
        return;
    }

    ctx = (PortWindowsContext*)tcb->port_context;
    if (ctx != NULL) {
        if (ctx->thread_handle != NULL) {
            (void)TerminateThread(ctx->thread_handle, 0U);
            (void)CloseHandle(ctx->thread_handle);
            ctx->thread_handle = NULL;
        }
        if (ctx->event_handle != NULL) {
            (void)CloseHandle(ctx->event_handle);
            ctx->event_handle = NULL;
        }
        ctx->in_use = false;
        ctx->tcb = NULL;
        tcb->port_context = NULL;
    }
}

void sertos_port_stop_scheduler(void)
{
    s_scheduler_running = false;
    if (s_exit_event != NULL) {
        (void)SetEvent(s_exit_event);
    }
}
