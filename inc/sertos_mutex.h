/**
 * @file sertos_mutex.h
 * @brief Mutual exclusion synchronization primitive with Priority Inheritance Protocol (PIP).
 *
 * Implements recursive mutexes supporting bounded priority inversion avoidance via PIP,
 * priority-ordered task blocking, and zero-allocation static instantiation.
 */

#ifndef SERTOS_MUTEX_H
#define SERTOS_MUTEX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"
#include "linked_list.h"

/**
 * @brief Magic word assigned to mutex canary field ('MUTX').
 */
#define SERTOS_MUTEX_MAGIC_WORD  (0x4D555458U)

/**
 * @brief Mutex control structure.
 */
typedef struct SertosMutex {
    SertosTaskControlBlock* owner;              /**< Task control block currently holding the lock, or NULL. */
    uint32_t lock_count;                        /**< Recursive reentrancy nesting counter. */
    SertosPriority original_owner_priority;     /**< Original base priority of the owner prior to inheritance. */
    LinkedList wait_list;                       /**< Priority-ordered list of tasks blocked waiting on the mutex. */
    bool is_statically_allocated;               /**< True if caller-allocated static storage; false if from pool. */
    uint32_t magic;                             /**< Integrity canary word. */
} SertosMutex;

/**
 * @brief Opaque mutex handle exposed to application code.
 */
typedef struct SertosMutex* SertosMutexHandle;

/**
 * @brief Statically initializes a mutex using caller-allocated storage.
 *
 * @param[in,out] mutex      Pointer to caller-allocated SertosMutex structure.
 * @param[out]    out_handle Pointer to store the initialized mutex handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_mutex_create_static(SertosMutex* mutex, SertosMutexHandle* out_handle);

/**
 * @brief Creates a mutex allocating storage from the kernel memory_pool.
 *
 * @param[out] out_handle Pointer to store the initialized mutex handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_mutex_create(SertosMutexHandle* out_handle);

/**
 * @brief Deletes a mutex, restoring owner priority and freeing dynamic memory.
 *
 * @param[in] handle Mutex handle to delete.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_mutex_delete(SertosMutexHandle handle);

/**
 * @brief Locks (acquires) the mutex with Priority Inheritance Protocol.
 *
 * If the lock is held by a lower-priority task, that owner's active priority is boosted
 * to the caller's priority until the lock is released. Supports recursive reentrant locking.
 *
 * @param[in] handle  Mutex handle.
 * @param[in] timeout Maximum ticks to wait (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_TIMEOUT, or error status code.
 */
SertosStatus sertos_mutex_lock(SertosMutexHandle handle, SertosTick timeout);

/**
 * @brief Unlocks (releases) the mutex, restoring owner priority and unblocking the next waiting task.
 *
 * @param[in] handle Mutex handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_mutex_unlock(SertosMutexHandle handle);

/**
 * @brief Retrieves the handle of the task currently owning the mutex.
 *
 * @param[in] handle Mutex handle.
 * @return Task handle of owner, or NULL if unlocked or invalid.
 */
SertosTaskHandle sertos_mutex_get_owner(SertosMutexHandle handle);

#endif /* SERTOS_MUTEX_H */
