/**
 * @file port_posix.c
 * @brief Preemptive POSIX host simulator port implementation for SertOS.
 *
 * Implements deterministic multitasking using standard POSIX threads (pthreads),
 * condition variables, thread-local storage context mapping, and a 1 kHz tick thread.
 */

#if !defined(_WIN32)

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include "sertos_config.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>
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
 * @brief POSIX-specific task context descriptor.
 */
typedef struct PortPosixContext {
    pthread_t thread;                   /**< Native pthread handle. */
    pthread_cond_t cond;                /**< Synchronization condition variable. */
    bool signaled;                      /**< Signal state flag for condition. */
    SertosTaskControlBlock* tcb;        /**< Associated SertOS TCB pointer. */
    bool in_use;                        /**< Allocation flag within static pool. */
} PortPosixContext;

/**
 * @brief Static context descriptor pool (zero dynamic allocation).
 */
static PortPosixContext s_task_contexts[SERTOS_CONFIG_MAX_TASKS];

/**
 * @brief Thread-local storage pointer to the current thread's context descriptor.
 */
static __thread PortPosixContext* s_tls_my_ctx = NULL;

/**
 * @brief Dedicated recursive mutex for critical section masking.
 */
static pthread_mutex_t s_critical_mutex;
static bool s_critical_mutex_initialized = false;

/**
 * @brief Synchronization mutex for task condition variables and yielding.
 */
static pthread_mutex_t s_yield_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Synchronization primitives for scheduler termination.
 */
static pthread_mutex_t s_exit_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_exit_cond = PTHREAD_COND_INITIALIZER;
static bool s_exit_signaled = false;

/**
 * @brief High-resolution periodic tick thread.
 */
static pthread_t s_tick_thread;
static bool s_tick_thread_created = false;

/**
 * @brief Global scheduler running state flag.
 */
static volatile bool s_scheduler_running = false;

/**
 * @brief Pointer to context descriptor of the currently executing task.
 */
static PortPosixContext* s_running_ctx = NULL;

/**
 * @brief Initializes the recursive critical section mutex.
 */
static void init_critical_mutex(void)
{
    pthread_mutexattr_t attr;

    if (!s_critical_mutex_initialized) {
        (void)pthread_mutexattr_init(&attr);
        (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        (void)pthread_mutex_init(&s_critical_mutex, &attr);
        (void)pthread_mutexattr_destroy(&attr);
        s_critical_mutex_initialized = true;
    }
}

/**
 * @brief Native pthread wrapper invoking SertOS task entry.
 *
 * @param param Pointer to associated SertosTaskControlBlock.
 * @return NULL on thread termination.
 */
static void* port_task_wrapper(void* param)
{
    SertosTaskControlBlock* tcb = (SertosTaskControlBlock*)param;
    PortPosixContext* ctx;

    if (tcb == NULL) {
        return NULL;
    }

    ctx = (PortPosixContext*)tcb->port_context;
    if (ctx == NULL) {
        return NULL;
    }

    /* Assign thread-local context pointer */
    s_tls_my_ctx = ctx;

    /* Wait unconditionally for initial scheduler start signal */
    (void)pthread_mutex_lock(&s_yield_mutex);
    while (!ctx->signaled) {
        (void)pthread_cond_wait(&ctx->cond, &s_yield_mutex);
    }
    (void)pthread_mutex_unlock(&s_yield_mutex);

    if (s_scheduler_running && (tcb->entry_func != NULL)) {
        tcb->entry_func(tcb->param);
    }

    (void)sertos_task_delete(tcb);
    return NULL;
}

/**
 * @brief Periodic 1 kHz tick driver thread function.
 *
 * @param param Unused thread argument.
 * @return NULL on thread exit.
 */
static void* port_tick_thread_func(void* param)
{
    struct timespec req;

    (void)param;
    req.tv_sec = 0;
    req.tv_nsec = 1000000L; /* 1 ms */

    while (s_scheduler_running) {
        (void)nanosleep(&req, NULL);
        if (s_scheduler_running) {
            sertos_scheduler_tick();
        }
    }

    return NULL;
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

    init_critical_mutex();
    s_exit_signaled = false;
    s_scheduler_running = true;

    /* Create and start periodic tick thread */
    if (pthread_create(&s_tick_thread, NULL, port_tick_thread_func, NULL) == 0) {
        s_tick_thread_created = true;
    }

    /* Signal the first task to run */
    (void)pthread_mutex_lock(&s_yield_mutex);
    current = sertos_scheduler_get_current_tcb();
    if (current != NULL) {
        current->state = SERTOS_TASK_STATE_RUNNING;
        s_running_ctx = (PortPosixContext*)current->port_context;
        if (s_running_ctx != NULL) {
            s_running_ctx->signaled = true;
            (void)pthread_cond_signal(&s_running_ctx->cond);
        }
    }
    (void)pthread_mutex_unlock(&s_yield_mutex);

    /* Block main thread until scheduler stop is signaled */
    (void)pthread_mutex_lock(&s_exit_mutex);
    while (!s_exit_signaled) {
        (void)pthread_cond_wait(&s_exit_cond, &s_exit_mutex);
    }
    (void)pthread_mutex_unlock(&s_exit_mutex);

    s_scheduler_running = false;

    /* Stop tick thread */
    if (s_tick_thread_created) {
        (void)pthread_join(s_tick_thread, NULL);
        s_tick_thread_created = false;
    }
}

void sertos_port_yield(void)
{
    PortPosixContext* my_ctx;
    PortPosixContext* next_ctx;
    SertosTaskControlBlock* current;
    pthread_t self;

    if (!s_scheduler_running) {
        return;
    }

    self = pthread_self();

    (void)pthread_mutex_lock(&s_yield_mutex);

    current = sertos_scheduler_get_current_tcb();
    if (current == NULL) {
        (void)pthread_mutex_unlock(&s_yield_mutex);
        return;
    }

    next_ctx = (PortPosixContext*)current->port_context;

    /* If invoked from the tick thread */
    if (s_tick_thread_created && pthread_equal(self, s_tick_thread)) {
        if ((next_ctx != NULL) && (next_ctx != s_running_ctx)) {
            PortPosixContext* old_ctx = s_running_ctx;
            s_running_ctx = next_ctx;
            if (old_ctx != NULL) {
                old_ctx->signaled = false;
            }
            next_ctx->signaled = true;
            (void)pthread_cond_signal(&next_ctx->cond);
        }
        (void)pthread_mutex_unlock(&s_yield_mutex);
        return;
    }

    /* Invoked from a task thread */
    my_ctx = s_tls_my_ctx;

    if ((my_ctx != NULL) && (next_ctx == my_ctx)) {
        (void)pthread_mutex_unlock(&s_yield_mutex);
        usleep(1000U);
        return;
    }

    s_running_ctx = next_ctx;
    if (next_ctx != NULL) {
        next_ctx->signaled = true;
        (void)pthread_cond_signal(&next_ctx->cond);
    }

    if (my_ctx != NULL) {
        my_ctx->signaled = false;
        while (!my_ctx->signaled && s_scheduler_running) {
            (void)pthread_cond_wait(&my_ctx->cond, &s_yield_mutex);
        }
    }

    (void)pthread_mutex_unlock(&s_yield_mutex);
}

uint32_t sertos_port_enter_critical(void)
{
    init_critical_mutex();
    (void)pthread_mutex_lock(&s_critical_mutex);
    return 0U;
}

void sertos_port_exit_critical(uint32_t status)
{
    (void)status;
    if (s_critical_mutex_initialized) {
        (void)pthread_mutex_unlock(&s_critical_mutex);
    }
}

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    (void)tick_rate_hz;
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    size_t i;
    PortPosixContext* ctx = NULL;

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
    ctx->signaled = false;
    (void)pthread_cond_init(&ctx->cond, NULL);
    tcb->port_context = (void*)ctx;

    if (pthread_create(&ctx->thread, NULL, port_task_wrapper, tcb) != 0) {
        tcb->port_context = NULL;
        ctx->in_use = false;
        return;
    }
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    PortPosixContext* ctx;

    if (tcb == NULL) {
        return;
    }

    ctx = (PortPosixContext*)tcb->port_context;
    if (ctx != NULL) {
        (void)pthread_cancel(ctx->thread);
        (void)pthread_join(ctx->thread, NULL);
        (void)pthread_cond_destroy(&ctx->cond);
        ctx->in_use = false;
        ctx->tcb = NULL;
        tcb->port_context = NULL;
    }
}

void sertos_port_stop_scheduler(void)
{
    size_t i;

    s_scheduler_running = false;

    (void)pthread_mutex_lock(&s_yield_mutex);
    for (i = 0U; i < SERTOS_CONFIG_MAX_TASKS; i++) {
        if (s_task_contexts[i].in_use) {
            s_task_contexts[i].signaled = true;
            (void)pthread_cond_broadcast(&s_task_contexts[i].cond);
        }
    }
    (void)pthread_mutex_unlock(&s_yield_mutex);

    (void)pthread_mutex_lock(&s_exit_mutex);
    s_exit_signaled = true;
    (void)pthread_cond_broadcast(&s_exit_cond);
    (void)pthread_mutex_unlock(&s_exit_mutex);
}

#else

/* Provide dummy definitions when compiling on Win32 without POSIX */
#include "sertos_port.h"
#include <stddef.h>

void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param)
{
    (void)stack_top;
    (void)stack_limit;
    (void)entry;
    (void)param;
    return stack_top;
}

void sertos_port_start_first_task(void) {}
void sertos_port_yield(void) {}
uint32_t sertos_port_enter_critical(void) { return 0U; }
void sertos_port_exit_critical(uint32_t status) { (void)status; }
void sertos_port_tick_init(uint32_t tick_rate_hz) { (void)tick_rate_hz; }
void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb) { (void)tcb; }
void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb) { (void)tcb; }
void sertos_port_stop_scheduler(void) {}

#endif /* !defined(_WIN32) */
