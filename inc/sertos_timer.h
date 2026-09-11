/**
 * @file sertos_timer.h
 * @brief Monotonic software timers primitive for SertOS.
 *
 * Provides one-shot and periodic software timers driven by system scheduler ticks
 * with zero dynamic memory allocation requirements.
 */

#ifndef SERTOS_TIMER_H
#define SERTOS_TIMER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "linked_list.h"

/**
 * @brief Magic word assigned to timer canary field ('TIMR').
 */
#define SERTOS_TIMER_MAGIC_WORD  (0x54494D52U)

struct SertosTimer;

/**
 * @brief Opaque software timer handle.
 */
typedef struct SertosTimer* SertosTimerHandle;

/**
 * @brief Timer expiration callback function pointer type.
 *
 * @param handle Handle of the expired timer.
 * @param param  User-supplied parameter passed during timer creation.
 */
typedef void (*SertosTimerCallback)(SertosTimerHandle handle, void* param);

/**
 * @brief Software timer configuration parameters.
 */
typedef struct SertosTimerConfig {
    const char* name;               /**< Descriptive timer name string. */
    SertosTick period;              /**< Duration in ticks (must be > 0). */
    bool is_periodic;               /**< True for periodic auto-reload, false for one-shot. */
    SertosTimerCallback callback;   /**< Expiration callback function pointer. */
    void* param;                    /**< User argument passed to callback. */
} SertosTimerConfig;

/**
 * @brief Software timer control structure.
 */
typedef struct SertosTimer {
    const char* name;               /**< Descriptive name for debugging. */
    SertosTick period_ticks;        /**< Timer period in scheduler ticks. */
    SertosTick remaining_ticks;     /**< Ticks remaining until next expiration. */
    bool is_periodic;               /**< True if recurring periodic timer; false if one-shot. */
    bool is_active;                 /**< True if currently armed and counting down. */
    SertosTimerCallback callback;   /**< Callback invoked upon expiration. */
    void* param;                    /**< User parameter passed to callback. */
    LinkedListNode node;            /**< Intrusive list node in active timers queue. */
    bool is_statically_allocated;   /**< True if caller-provided static storage; false if from pool. */
    uint32_t magic;                 /**< Integrity canary word. */
} SertosTimer;

/**
 * @brief Statically initializes a software timer using caller-allocated storage.
 *
 * @param[in]     config     Pointer to timer configuration structure.
 * @param[in,out] timer      Pointer to caller-allocated SertosTimer structure.
 * @param[out]    out_handle Pointer to store the initialized timer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_create_static(const SertosTimerConfig* config,
                                        SertosTimer* timer,
                                        SertosTimerHandle* out_handle);

/**
 * @brief Creates a software timer allocating storage from the kernel memory_pool.
 *
 * @param[in]  config     Pointer to timer configuration structure.
 * @param[out] out_handle Pointer to store the initialized timer handle.
 * @return SERTOS_STATUS_OK on success, or SERTOS_STATUS_ERROR_NO_MEMORY.
 */
SertosStatus sertos_timer_create(const SertosTimerConfig* config,
                                 SertosTimerHandle* out_handle);

/**
 * @brief Deletes a software timer and frees dynamic storage if applicable.
 *
 * @param[in] handle Timer handle to delete.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_delete(SertosTimerHandle handle);

/**
 * @brief Starts (arms) a software timer.
 *
 * @param[in] handle Timer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_start(SertosTimerHandle handle);

/**
 * @brief Stops (disarms) a running software timer.
 *
 * @param[in] handle Timer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_stop(SertosTimerHandle handle);

/**
 * @brief Resets and restarts a software timer with its configured period.
 *
 * @param[in] handle Timer handle.
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_reset(SertosTimerHandle handle);

/**
 * @brief Modifies the period of a software timer.
 *
 * @param[in] handle     Timer handle.
 * @param[in] new_period New duration in ticks (must be > 0).
 * @return SERTOS_STATUS_OK on success, or error status code.
 */
SertosStatus sertos_timer_change_period(SertosTimerHandle handle, SertosTick new_period);

/**
 * @brief Queries whether a software timer is currently active and counting.
 *
 * @param[in] handle Timer handle.
 * @return true if active, false otherwise.
 */
bool sertos_timer_is_active(SertosTimerHandle handle);

/**
 * @brief Software timer subsystem tick driver.
 *
 * Invoked monotonically by the kernel scheduler tick handler.
 */
void sertos_timer_tick(void);

/**
 * @brief Initializes the software timer subsystem list container.
 */
void sertos_timer_init(void);

#endif /* SERTOS_TIMER_H */
