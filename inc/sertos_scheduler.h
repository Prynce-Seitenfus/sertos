/**
 * @file sertos_scheduler.h
 * @brief Preemptive priority scheduler API for SertOS.
 *
 * Implements deterministic O(1) scheduling using hardware-accelerated bitmap
 * priority queries and intrusive circular doubly-linked lists.
 */

#ifndef SERTOS_SCHEDULER_H
#define SERTOS_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"

/**
 * @brief Initializes the core scheduler data structures.
 *
 * Sets up ready lists, the blocked delay list, suspended list, initializes tick
 * counters, and creates the system Idle Task. Must be called prior to
 * any task creation or scheduler invocation.
 *
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_scheduler_init(void);

/**
 * @brief Starts the preemptive multitasking scheduler.
 *
 * Selects the highest-priority ready task, configures the architecture timer,
 * and invokes the port layer to restore context and begin execution.
 *
 * @warning On bare-metal targets, this function never returns.
 */
void sertos_scheduler_start(void);

/**
 * @brief Stops the preemptive multitasking scheduler.
 *
 * Terminates multitasking on host simulators (Windows / POSIX) and returns
 * control from sertos_scheduler_start().
 */
void sertos_scheduler_stop(void);

/**
 * @brief Periodic system tick handler.
 *
 * Typically invoked by the hardware timer ISR (e.g. SysTick on ARM Cortex-M)
 * or by host test drivers. Increments system tick, updates blocked task delays,
 * performs time-slicing among equal-priority tasks, and initiates context
 * switching if a higher-priority task transitions to ready.
 */
void sertos_scheduler_tick(void);

/**
 * @brief Voluntarily yields the remainder of the current task's execution slice.
 *
 * Triggers a context switch to allow the next ready task at the same or higher
 * priority to run.
 */
void sertos_scheduler_yield(void);

/**
 * @brief Disables preemptive task switching (scheduler lock).
 *
 * Interrupts remain enabled, but context switching is deferred until
 * sertos_scheduler_unlock() is invoked. Calls may be nested.
 */
void sertos_scheduler_lock(void);

/**
 * @brief Re-enables preemptive task switching.
 *
 * Decrements the lock counter. If the counter reaches zero and a context switch
 * was requested while locked, the pending switch is triggered immediately.
 */
void sertos_scheduler_unlock(void);

/**
 * @brief Queries whether the scheduler preemption lock is currently held.
 *
 * @return true if scheduler is locked, false otherwise.
 */
bool sertos_scheduler_is_locked(void);

/**
 * @brief Queries whether the scheduler is actively running.
 *
 * @return true if multitasking is active, false otherwise.
 */
bool sertos_scheduler_is_running(void);

/**
 * @brief Returns the total monotonic tick count elapsed since scheduler start.
 *
 * @return Monotonic tick count.
 */
SertosTick sertos_scheduler_get_tick_count(void);

/**
 * @brief Blocks the currently running task for a specified duration in ticks.
 *
 * @param[in] ticks Number of ticks to remain blocked. If ticks is 0, yields execution slice.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_scheduler_delay(SertosTick ticks);

/* --- Internal Kernel Scheduler Primitives --- */

/**
 * @brief Inserts a task into the corresponding priority ready list.
 *
 * @param[in] tcb Pointer to the task control block.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_scheduler_add_ready(SertosTaskControlBlock* tcb);

/**
 * @brief Removes a task from the corresponding priority ready list.
 *
 * @param[in] tcb Pointer to the task control block.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_scheduler_remove_ready(SertosTaskControlBlock* tcb);

/**
 * @brief Evaluates whether a higher-priority task is ready and requests context switch.
 */
void sertos_scheduler_reschedule(void);

/**
 * @brief Returns the TCB of the currently executing task.
 *
 * @return Pointer to current TCB, or NULL if scheduler not running.
 */
SertosTaskControlBlock* sertos_scheduler_get_current_tcb(void);

/**
 * @brief Sets the currently executing task TCB pointer.
 *
 * @param[in] tcb Pointer to current TCB.
 */
void sertos_scheduler_set_current_tcb(SertosTaskControlBlock* tcb);

/**
 * @brief Selects the highest-priority ready task from the ready bitmap and lists.
 *
 * @return Pointer to selected TCB, or NULL if no task is ready.
 */
SertosTaskControlBlock* sertos_scheduler_select_next_task(void);

/**
 * @brief Currently executing task control block pointer.
 *
 * Exported with external linkage for direct single-cycle dereference
 * by target architecture context-switch assembly routines.
 */
extern SertosTaskControlBlock* volatile sertos_current_tcb;

/**
 * @brief Context switch transition helper invoked by architecture exception handlers.
 *
 * Marks current task as READY (if previously RUNNING), selects highest-priority ready
 * task, transitions it to RUNNING, updates sertos_current_tcb, and returns it.
 *
 * @return Pointer to the newly activated SertosTaskControlBlock.
 */
SertosTaskControlBlock* sertos_scheduler_perform_switch(void);

/**
 * @brief Switches execution context to the next selected task.
 *
 * Invoked by yield, tick, or reschedule handlers.
 */
void sertos_scheduler_switch_context(void);

/**
 * @brief Blocks the currently executing task onto an IPC wait list.
 *
 * @param[in,out] wait_list Pointer to the IPC object's wait list.
 * @param[in]     timeout   Maximum ticks to wait (or SERTOS_WAIT_FOREVER).
 * @return SERTOS_STATUS_OK if unblocked by event, or SERTOS_STATUS_ERROR_TIMEOUT.
 */
SertosStatus sertos_scheduler_wait_list_block(LinkedList* wait_list, SertosTick timeout);

/**
 * @brief Unblocks the highest-priority task waiting on an IPC wait list.
 *
 * @param[in,out] wait_list Pointer to the IPC object's wait list.
 * @return Pointer to unblocked task TCB, or NULL if wait list was empty.
 */
SertosTaskControlBlock* sertos_scheduler_wait_list_unblock_highest(LinkedList* wait_list);

/**
 * @brief Removes a task from an IPC wait list.
 *
 * @param[in,out] wait_list Pointer to the wait list.
 * @param[in,out] tcb       Pointer to the task control block.
 */
void sertos_scheduler_wait_list_remove(LinkedList* wait_list, SertosTaskControlBlock* tcb);

#endif /* SERTOS_SCHEDULER_H */
