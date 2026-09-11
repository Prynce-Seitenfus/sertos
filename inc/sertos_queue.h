/**
 * @file sertos_queue.h
 * @brief Thread-safe message queue IPC primitive for SertOS.
 *
 * Implements multi-task safe FIFO message queues wrapping ring_buffer with
 * priority-ordered sender and receiver blocking wait lists.
 */

#ifndef SERTOS_QUEUE_H
#define SERTOS_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"
#include "ring_buffer.h"
#include "linked_list.h"

/**
 * @brief Magic word assigned to queue canary field ('QUEU').
 */
#define SERTOS_QUEUE_MAGIC_WORD  (0x51554555U)

/**
 * @brief Message queue control structure.
 */
typedef struct SertosQueue {
    RingBuffer ring_buf;            /**< Underlying lock-free circular ring buffer. */
    size_t item_size;               /**< Size of an individual queue item in bytes. */
    size_t max_items;               /**< Maximum capacity in items. */
    LinkedList wait_send;           /**< Priority list of tasks blocked waiting to send (queue full). */
    LinkedList wait_recv;           /**< Priority list of tasks blocked waiting to receive (queue empty). */
    void* storage_buffer;           /**< Pointer to raw byte storage buffer. */
    bool is_statically_allocated;   /**< True if caller-provided static storage; false if from pool. */
    uint32_t magic;                 /**< Integrity canary word. */
} SertosQueue;

/**
 * @brief Opaque queue handle exposed to application code.
 */
typedef struct SertosQueue* SertosQueueHandle;

/**
 * @brief Statically initializes a message queue using caller-allocated storage.
 *
 * @param[in,out] queue          Pointer to caller-allocated SertosQueue structure.
 * @param[in]     storage_buffer Pointer to caller-allocated byte buffer (must be power-of-2 size).
 * @param[in]     storage_size   Total size of storage buffer in bytes (must be power of 2 >= 2).
 * @param[in]     item_size      Size in bytes of an individual message item (must be > 0).
 * @param[out]    out_handle     Pointer to store the initialized queue handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_queue_create_static(SertosQueue* queue,
                                        void* storage_buffer,
                                        size_t storage_size,
                                        size_t item_size,
                                        SertosQueueHandle* out_handle);

/**
 * @brief Creates a message queue allocating queue structure and buffer from memory_pool.
 *
 * @param[in]  item_size  Size in bytes of an individual message item.
 * @param[in]  max_items  Maximum number of items the queue can hold.
 * @param[out] out_handle Pointer to store the initialized queue handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_queue_create(size_t item_size,
                                 size_t max_items,
                                 SertosQueueHandle* out_handle);

/**
 * @brief Deletes a message queue, unblocking all waiting tasks and freeing dynamic memory.
 *
 * @param[in] handle Queue handle to delete.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_queue_delete(SertosQueueHandle handle);

/**
 * @brief Posts an item to the queue, blocking if the queue is full.
 *
 * @param[in] handle  Queue handle.
 * @param[in] item    Pointer to the item to copy into the queue.
 * @param[in] timeout Maximum ticks to wait (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_TIMEOUT, or error status code.
 */
SertosStatus sertos_queue_send(SertosQueueHandle handle, const void* item, SertosTick timeout);

/**
 * @brief Receives (copies out) an item from the queue, blocking if the queue is empty.
 *
 * @param[in]  handle  Queue handle.
 * @param[out] item    Pointer to memory where the received item is copied.
 * @param[in]  timeout Maximum ticks to wait (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_TIMEOUT, or error status code.
 */
SertosStatus sertos_queue_receive(SertosQueueHandle handle, void* item, SertosTick timeout);

/**
 * @brief Inspects the front item without removing it from the queue.
 *
 * @param[in]  handle  Queue handle.
 * @param[out] item    Pointer to memory where the inspected item is copied.
 * @param[in]  timeout Maximum ticks to wait.
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_TIMEOUT, or error status code.
 */
SertosStatus sertos_queue_peek(SertosQueueHandle handle, void* item, SertosTick timeout);

/**
 * @brief Sends an item to the queue from an ISR context.
 *
 * @param[in]  handle                 Queue handle.
 * @param[in]  item                   Pointer to the item to send.
 * @param[out] out_higher_prio_woken  Set to true if a higher priority task was unblocked.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_queue_send_from_isr(SertosQueueHandle handle,
                                        const void* item,
                                        bool* out_higher_prio_woken);

/**
 * @brief Receives an item from the queue from an ISR context.
 *
 * @param[in]  handle                 Queue handle.
 * @param[out] item                   Pointer to store the received item.
 * @param[out] out_higher_prio_woken  Set to true if a higher priority task was unblocked.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_queue_receive_from_isr(SertosQueueHandle handle,
                                           void* item,
                                           bool* out_higher_prio_woken);

/**
 * @brief Returns the number of items currently stored in the queue.
 *
 * @param[in] handle Queue handle.
 * @return Number of items in queue, or 0 if handle is invalid.
 */
size_t sertos_queue_get_count(SertosQueueHandle handle);

/**
 * @brief Returns the available remaining item slots in the queue.
 *
 * @param[in] handle Queue handle.
 * @return Number of free item slots, or 0 if handle is invalid.
 */
size_t sertos_queue_get_spaces_available(SertosQueueHandle handle);

/**
 * @brief Resets the queue to an empty state.
 *
 * @param[in] handle Queue handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_queue_reset(SertosQueueHandle handle);

#endif /* SERTOS_QUEUE_H */
