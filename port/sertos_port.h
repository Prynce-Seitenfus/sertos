/**
 * @file sertos_port.h
 * @brief Architecture-independent hardware abstraction port contract for SertOS.
 *
 * All hardware-specific and platform-dependent mechanisms (stack initialization,
 * context switching, interrupt masking, and tick timer) must implement this contract.
 */

#ifndef SERTOS_PORT_H
#define SERTOS_PORT_H

#include <stdint.h>
#include <stddef.h>
#include "sertos_types.h"

/**
 * @brief Initializes a task stack frame to match the target architecture's layout.
 *
 * Synthesizes the initial register frame such that when the scheduler performs
 * a context restore, execution begins at the entry function with param in R0/first arg.
 *
 * @param stack_top   Pointer to the high address of the task stack buffer.
 * @param stack_limit Lowest valid address of the stack buffer (for PSPLIM / limit checks).
 * @param entry       Task entry function pointer.
 * @param param       User-supplied argument passed to the entry function.
 * @return Updated stack pointer pointing to the top of the simulated stack frame.
 */
void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param);

/**
 * @brief Transitions to unprivileged thread mode and starts the first ready task.
 *
 * Restores the context of the task pointed to by sertos_scheduler_get_current_tcb().
 */
void sertos_port_start_first_task(void);

/**
 * @brief Triggers a software interrupt or context switch exception (e.g. PendSV on ARM).
 */
void sertos_port_yield(void);

/**
 * @brief Enters a critical section by disabling interrupts.
 *
 * @return Previous interrupt status/mask register value.
 */
uint32_t sertos_port_enter_critical(void);

/**
 * @brief Exits a critical section by restoring the previous interrupt mask.
 *
 * @param status Previous interrupt status returned by sertos_port_enter_critical().
 */
void sertos_port_exit_critical(uint32_t status);

/**
 * @brief Configures the hardware periodic tick timer (e.g. SysTick) for the specified rate.
 *
 * @param tick_rate_hz Desired tick rate in Hertz (e.g. 1000 for 1 ms tick).
 */
void sertos_port_tick_init(uint32_t tick_rate_hz);

/**
 * @brief Optional port hook executed upon task creation.
 *
 * @param tcb Pointer to initialized task control block.
 */
void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb);

/**
 * @brief Optional port hook executed upon task deletion.
 *
 * @param tcb Pointer to task control block being deleted.
 */
void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb);

/**
 * @brief Signals the simulator port to terminate the scheduler and exit.
 */
void sertos_port_stop_scheduler(void);

#endif /* SERTOS_PORT_H */
