/**
 * @file sertos_task.h
 * @brief Task Control Block (TCB) structure and task management API.
 *
 * Provides static and dynamic task instantiation, state transitions,
 * priority configuration, and stack high-water mark detection.
 */

#ifndef SERTOS_TASK_H
#define SERTOS_TASK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_config.h"
#include "linked_list.h"

/**
 * @brief Maximum characters in a task diagnostic name, including null terminator.
 */
#define SERTOS_TASK_NAME_MAX_LEN     (SERTOS_CONFIG_MAX_TASK_NAME_LEN)

/**
 * @brief Magic word assigned to TCB canary field to verify struct integrity ('TCB!').
 */
#define SERTOS_TASK_MAGIC_WORD       (0x54434221U)

/**
 * @brief Byte pattern used for stack painting to calculate high-water mark.
 */
#define SERTOS_TASK_STACK_FILL_BYTE  (0xA5U)

/**
 * @brief Task lifecycle states.
 */
typedef enum SertosTaskState {
    SERTOS_TASK_STATE_READY = 0,     /**< Task is in ready queue, awaiting execution. */
    SERTOS_TASK_STATE_RUNNING,       /**< Task is actively executing on the CPU. */
    SERTOS_TASK_STATE_BLOCKED,       /**< Task is waiting for a delay, event, or IPC primitive. */
    SERTOS_TASK_STATE_SUSPENDED,     /**< Task is explicitly suspended and ineligible for execution. */
    SERTOS_TASK_STATE_TERMINATED     /**< Task has completed execution and is inactive. */
} SertosTaskState;

/**
 * @brief Task Control Block (TCB).
 *
 * Encapsulates all state, stack limits, and priority metadata for a task.
 * Note: stack_ptr MUST remain the first field in the structure to enable
 * single-instruction dereferencing in architecture context-switch assembly routines.
 */
typedef struct SertosTaskControlBlock {
    void* stack_ptr;                        /**< Top of stack saved during context switch. MUST be first member. */
    void* stack_base;                       /**< Base address of stack buffer (lowest address). */
    size_t stack_size;                      /**< Total stack size in bytes. */
    void* stack_limit;                      /**< Lowest valid stack limit address (for PSPLIM/boundary checks). */
    SertosTaskState state;                  /**< Current lifecycle state. */
    SertosPriority priority;                /**< Active scheduling priority. */
    SertosPriority base_priority;           /**< Original base priority (used in priority inheritance). */
    SertosTick delay_ticks;                 /**< Ticks remaining if blocked on a time delay. */
    SertosTaskFunction entry_func;          /**< Entry point function pointer. */
    void* param;                            /**< Argument passed to entry function. */
    char name[SERTOS_TASK_NAME_MAX_LEN];    /**< Descriptive name for debugging. */
    LinkedListNode state_node;              /**< Intrusive list node for ready/delay/suspend queues. */
    LinkedListNode event_node;              /**< Intrusive list node for IPC blocking wait-lists. */
    struct LinkedList* wait_list;           /**< Pointer to IPC wait list if blocked, or NULL. */
    bool is_statically_allocated;           /**< True if buffers were caller-provided; false if from memory_pool. */
    uint32_t magic;                         /**< Canary word (SERTOS_TASK_MAGIC_WORD) for integrity verification. */
    void* port_context;                     /**< Target architecture or simulator port context descriptor. */
} SertosTaskControlBlock;

/**
 * @brief Task creation configuration structure.
 */
typedef struct SertosTaskConfig {
    const char* name;                  /**< Null-terminated task name string. */
    SertosTaskFunction entry_func;     /**< Pointer to task entry function. */
    void* param;                       /**< Pointer passed as argument to entry function. */
    SertosPriority priority;           /**< Desired task priority. */
    void* stack_buffer;                /**< Caller-allocated stack buffer (NULL if using memory_pool). */
    size_t stack_size;                 /**< Size of stack buffer in bytes. */
} SertosTaskConfig;

/**
 * @brief Statically creates a new task using caller-supplied TCB and stack buffers.
 *
 * Provides deterministic task initialization with zero dynamic memory allocation.
 * The stack buffer is painted with SERTOS_TASK_STACK_FILL_BYTE to allow high-water mark detection.
 *
 * @param[in]  config     Pointer to task configuration parameters.
 * @param[out] tcb        Pointer to caller-allocated SertosTaskControlBlock structure.
 * @param[out] out_handle Pointer to variable where resulting task handle is stored.
 * @return SERTOS_STATUS_OK on success, or an error status code on failure.
 */
SertosStatus sertos_task_create_static(const SertosTaskConfig* config,
                                       SertosTaskControlBlock* tcb,
                                       SertosTaskHandle* out_handle);

/**
 * @brief Creates a new task allocating the TCB and stack from the kernel memory_pool.
 *
 * @param[in]  config     Pointer to task configuration parameters.
 * @param[out] out_handle Pointer to variable where resulting task handle is stored.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY if pool is exhausted.
 */
SertosStatus sertos_task_create(const SertosTaskConfig* config, SertosTaskHandle* out_handle);

/**
 * @brief Deletes and terminates a task, releasing dynamic memory if dynamically allocated.
 *
 * @param[in] handle Handle of task to delete (pass NULL to delete the calling task).
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_task_delete(SertosTaskHandle handle);

/**
 * @brief Suspends execution of a task.
 *
 * Ineligible for scheduling until explicitly resumed via sertos_task_resume().
 *
 * @param[in] handle Handle of task to suspend (pass NULL to suspend the current task).
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_task_suspend(SertosTaskHandle handle);

/**
 * @brief Resumes a previously suspended task.
 *
 * Transitions the task back to SERTOS_TASK_STATE_READY.
 *
 * @param[in] handle Handle of task to resume.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_task_resume(SertosTaskHandle handle);

/**
 * @brief Retrieves the current state of a task.
 *
 * @param[in] handle Handle of the task to query.
 * @return Current SertosTaskState, or SERTOS_TASK_STATE_TERMINATED if handle is invalid.
 */
SertosTaskState sertos_task_get_state(SertosTaskHandle handle);

/**
 * @brief Returns the handle of the currently executing task.
 *
 * @return Handle of the active task, or NULL if scheduler is not running.
 */
SertosTaskHandle sertos_task_get_current(void);

/**
 * @brief Retrieves the human-readable name of the task.
 *
 * @param[in] handle Handle of the task to query.
 * @return Pointer to task name string, or "Unknown" if handle is invalid.
 */
const char* sertos_task_get_name(SertosTaskHandle handle);

/**
 * @brief Retrieves the assigned priority of a task.
 *
 * @param[in] handle Handle of the task to query.
 * @return Current priority level, or 0 if handle is invalid.
 */
SertosPriority sertos_task_get_priority(SertosTaskHandle handle);

/**
 * @brief Updates the priority of a task.
 *
 * If the modified task has a higher priority than the currently running task,
 * an immediate preemption rescheduling is triggered.
 *
 * @param[in] handle       Handle of the task.
 * @param[in] new_priority New priority level.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_task_set_priority(SertosTaskHandle handle, SertosPriority new_priority);

/**
 * @brief Calculates the unused stack space (in bytes) since task creation.
 *
 * Scans the stack buffer from the base upwards for the paint byte pattern.
 *
 * @param[in] handle Handle of the task.
 * @return Number of unused bytes remaining, or 0 if stack overflow occurred or handle is invalid.
 */
size_t sertos_task_get_stack_high_water_mark(SertosTaskHandle handle);

/**
 * @brief Computes CRC-32 integrity checksum over the TCB metadata fields.
 *
 * @param[in] tcb Pointer to task control block.
 * @return Computed CRC-32 checksum, or 0 if tcb is NULL.
 */
uint32_t sertos_task_compute_crc(const SertosTaskControlBlock* tcb);

/**
 * @brief Validates whether a state transition is legal according to task lifecycle FSM.
 *
 * @param[in] from Source task state.
 * @param[in] to   Target task state.
 * @return true if transition is permissible, false otherwise.
 */
bool sertos_task_is_valid_transition(SertosTaskState from, SertosTaskState to);

#endif /* SERTOS_TASK_H */
