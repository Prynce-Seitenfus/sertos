/**
 * @file sertos_sem.h
 * @brief Binary and counting semaphore synchronization primitives for SertOS.
 *
 * Implements deterministic counting and binary semaphores supporting priority-ordered
 * task blocking, configurable timeouts, and ISR signaling conforming to MISRA C:2012.
 */

#ifndef SERTOS_SEM_H
#define SERTOS_SEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"
#include "linked_list.h"

/**
 * @brief Magic word assigned to semaphore canary field ('SEMA').
 */
#define SERTOS_SEM_MAGIC_WORD  (0x53454D41U)

/**
 * @brief Semaphore control structure.
 */
typedef struct SertosSemaphore {
    size_t count;                   /**< Current available resource count or token state. */
    size_t max_count;               /**< Maximum permissible count (1 for binary semaphore). */
    LinkedList wait_list;           /**< Priority-ordered list of tasks blocked waiting on semaphore. */
    bool is_statically_allocated;   /**< True if caller-provided static storage; false if from memory_pool. */
    uint32_t magic;                 /**< Integrity canary word. */
} SertosSemaphore;

/**
 * @brief Opaque semaphore handle exposed to application code.
 */
typedef struct SertosSemaphore* SertosSemHandle;

/**
 * @brief Statically initializes a counting semaphore using caller-allocated storage.
 *
 * @param[in,out] sem           Pointer to caller-allocated SertosSemaphore structure.
 * @param[in]     initial_count Starting resource count.
 * @param[in]     max_count     Maximum capacity (must be >= initial_count and > 0).
 * @param[out]    out_handle    Pointer to store the initialized semaphore handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_sem_create_counting_static(SertosSemaphore* sem,
                                               size_t initial_count,
                                               size_t max_count,
                                               SertosSemHandle* out_handle);

/**
 * @brief Statically initializes a binary semaphore (0 or 1).
 *
 * @param[in,out] sem           Pointer to caller-allocated SertosSemaphore structure.
 * @param[in]     initial_state True if initially available (token present), false if empty.
 * @param[out]    out_handle    Pointer to store the initialized semaphore handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_sem_create_binary_static(SertosSemaphore* sem,
                                             bool initial_state,
                                             SertosSemHandle* out_handle);

/**
 * @brief Creates a counting semaphore allocating storage from the kernel memory_pool.
 *
 * @param[in]  initial_count Starting resource count.
 * @param[in]  max_count     Maximum capacity (must be >= initial_count and > 0).
 * @param[out] out_handle    Pointer to store the initialized semaphore handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_sem_create_counting(size_t initial_count,
                                        size_t max_count,
                                        SertosSemHandle* out_handle);

/**
 * @brief Creates a binary semaphore allocating storage from the kernel memory_pool.
 *
 * @param[in]  initial_state True if initially available, false if empty.
 * @param[out] out_handle    Pointer to store the initialized semaphore handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_sem_create_binary(bool initial_state, SertosSemHandle* out_handle);

/**
 * @brief Deletes a semaphore, unblocking all waiting tasks and freeing dynamic memory.
 *
 * @param[in] handle Semaphore handle to delete.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_sem_delete(SertosSemHandle handle);

/**
 * @brief Acquires (decrements) a semaphore token, blocking if unavailable.
 *
 * @param[in] handle  Semaphore handle.
 * @param[in] timeout Maximum ticks to wait (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_TIMEOUT, or error status code.
 */
SertosStatus sertos_sem_take(SertosSemHandle handle, SertosTick timeout);

/**
 * @brief Releases (increments) a semaphore token, unblocking the highest-priority waiting task.
 *
 * @param[in] handle Semaphore handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_RESOURCE_BUSY if max count reached.
 */
SertosStatus sertos_sem_give(SertosSemHandle handle);

/**
 * @brief Releases a semaphore from an Interrupt Service Routine (ISR) context.
 *
 * @param[in]  handle                 Semaphore handle.
 * @param[out] out_higher_prio_woken  Set to true if an unblocked task has higher priority.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_sem_give_from_isr(SertosSemHandle handle, bool* out_higher_prio_woken);

/**
 * @brief Returns the current token count of the semaphore.
 *
 * @param[in] handle Semaphore handle.
 * @return Current count, or 0 if handle is invalid.
 */
size_t sertos_sem_get_count(SertosSemHandle handle);

#endif /* SERTOS_SEM_H */
