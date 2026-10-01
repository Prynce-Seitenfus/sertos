/**
 * @file sertos_stream_buffer.h
 * @brief Single-Producer Single-Consumer (SPSC) byte stream buffer primitive for SertOS.
 *
 * Implements lightweight, byte-oriented stream buffers built upon lock-free ring_buffer
 * with priority-ordered sender and receiver blocking wait lists and configurable
 * receive trigger levels. Conforms to ISO C99 and MISRA C:2012.
 */

#ifndef SERTOS_STREAM_BUFFER_H
#define SERTOS_STREAM_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"
#include "ring_buffer.h"
#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Magic word assigned to stream buffer canary field ('STRB').
 */
#define SERTOS_STREAM_BUFFER_MAGIC_WORD  (0x53545242U)

/**
 * @brief Stream buffer control structure.
 */
typedef struct SertosStreamBuffer {
    RingBuffer ring_buf;            /**< Underlying lock-free circular ring buffer. */
    size_t trigger_level_bytes;     /**< Minimum bytes required to unblock waiting reader. */
    LinkedList wait_send;           /**< Priority list of tasks blocked waiting for buffer space. */
    LinkedList wait_recv;           /**< Priority list of tasks blocked waiting for data. */
    void* storage_buffer;           /**< Pointer to raw byte storage buffer. */
    bool is_statically_allocated;   /**< True if caller-provided static storage; false if from pool. */
    uint32_t magic;                 /**< Integrity canary word. */
} SertosStreamBuffer;

/**
 * @brief Opaque stream buffer handle exposed to application code.
 */
typedef struct SertosStreamBuffer* SertosStreamBufferHandle;

/**
 * @brief Statically initializes a stream buffer using caller-allocated storage.
 *
 * @param[in,out] stream_buf          Pointer to caller-allocated SertosStreamBuffer structure.
 * @param[in]     storage_buffer      Pointer to caller-allocated byte buffer (must be power-of-2 size).
 * @param[in]     storage_size        Total size of storage buffer in bytes (must be power of 2 >= 2).
 * @param[in]     trigger_level_bytes Minimum bytes required to unblock a waiting receiver.
 * @param[out]    out_handle          Pointer to store the initialized stream buffer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_stream_buffer_create_static(SertosStreamBuffer* stream_buf,
                                                void* storage_buffer,
                                                size_t storage_size,
                                                size_t trigger_level_bytes,
                                                SertosStreamBufferHandle* out_handle);

/**
 * @brief Creates a stream buffer allocating control block and storage from memory_pool.
 *
 * @param[in]  buffer_size         Usable capacity in bytes.
 * @param[in]  trigger_level_bytes Minimum bytes required to unblock a waiting receiver.
 * @param[out] out_handle          Pointer to store the initialized stream buffer handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_stream_buffer_create(size_t buffer_size,
                                         size_t trigger_level_bytes,
                                         SertosStreamBufferHandle* out_handle);

/**
 * @brief Deletes a stream buffer, unblocking waiting tasks and freeing dynamic memory if applicable.
 *
 * @param[in] handle Stream buffer handle to delete.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_stream_buffer_delete(SertosStreamBufferHandle handle);

/**
 * @brief Writes a stream of bytes to the buffer, blocking if the buffer is full.
 *
 * @param[in] handle  Stream buffer handle.
 * @param[in] data    Pointer to data bytes to write.
 * @param[in] length  Number of bytes to write.
 * @param[in] timeout Maximum ticks to wait if buffer is full (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return Number of bytes actually written into the stream buffer.
 */
size_t sertos_stream_buffer_send(SertosStreamBufferHandle handle,
                                 const void* data,
                                 size_t length,
                                 SertosTick timeout);

/**
 * @brief Reads a stream of bytes from the buffer, blocking until trigger level is met.
 *
 * @param[in]  handle     Stream buffer handle.
 * @param[out] buffer     Destination memory buffer where bytes are copied.
 * @param[in]  max_length Maximum number of bytes to read.
 * @param[in]  timeout    Maximum ticks to wait (SERTOS_NO_WAIT, SERTOS_WAIT_FOREVER, or tick count).
 * @return Number of bytes actually read from the stream buffer.
 */
size_t sertos_stream_buffer_receive(SertosStreamBufferHandle handle,
                                    void* buffer,
                                    size_t max_length,
                                    SertosTick timeout);

/**
 * @brief Writes bytes to the stream buffer from an ISR context without blocking.
 *
 * @param[in]  handle                Stream buffer handle.
 * @param[in]  data                  Pointer to data bytes to write.
 * @param[in]  length                Number of bytes to write.
 * @param[out] out_higher_prio_woken Set to true if a higher priority task was unblocked.
 * @return Number of bytes actually written into the stream buffer.
 */
size_t sertos_stream_buffer_send_from_isr(SertosStreamBufferHandle handle,
                                          const void* data,
                                          size_t length,
                                          bool* out_higher_prio_woken);

/**
 * @brief Reads bytes from the stream buffer from an ISR context without blocking.
 *
 * @param[in]  handle                Stream buffer handle.
 * @param[out] buffer                Destination memory buffer where bytes are copied.
 * @param[in]  max_length            Maximum number of bytes to read.
 * @param[out] out_higher_prio_woken Set to true if a higher priority task was unblocked.
 * @return Number of bytes actually read from the stream buffer.
 */
size_t sertos_stream_buffer_receive_from_isr(SertosStreamBufferHandle handle,
                                             void* buffer,
                                             size_t max_length,
                                             bool* out_higher_prio_woken);

/**
 * @brief Updates the receive trigger level threshold.
 *
 * @param[in] handle              Stream buffer handle.
 * @param[in] trigger_level_bytes New trigger level in bytes.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_stream_buffer_set_trigger_level(SertosStreamBufferHandle handle,
                                                    size_t trigger_level_bytes);

/**
 * @brief Resets the stream buffer to an empty state and unblocks any blocked senders.
 *
 * @param[in] handle Stream buffer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_stream_buffer_reset(SertosStreamBufferHandle handle);

/**
 * @brief Returns the number of bytes currently stored in the stream buffer.
 *
 * @param[in] handle Stream buffer handle.
 * @return Number of bytes available to read, or 0 if handle is invalid.
 */
size_t sertos_stream_buffer_bytes_available(SertosStreamBufferHandle handle);

/**
 * @brief Returns the number of free byte spaces available for writing.
 *
 * @param[in] handle Stream buffer handle.
 * @return Number of free bytes available, or 0 if handle is invalid.
 */
size_t sertos_stream_buffer_spaces_available(SertosStreamBufferHandle handle);

/**
 * @brief Queries whether the stream buffer is empty.
 *
 * @param[in] handle Stream buffer handle.
 * @return true if empty or handle is invalid, false if data is present.
 */
bool sertos_stream_buffer_is_empty(SertosStreamBufferHandle handle);

/**
 * @brief Queries whether the stream buffer is full.
 *
 * @param[in] handle Stream buffer handle.
 * @return true if full, false otherwise.
 */
bool sertos_stream_buffer_is_full(SertosStreamBufferHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* SERTOS_STREAM_BUFFER_H */
