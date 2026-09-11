/**
 * @file sertos_config.h
 * @brief User-configurable kernel configuration parameters for SertOS.
 *
 * Provides compile-time configuration knobs for memory limits, priority levels,
 * tick frequency, and feature enablement conforming to MISRA C:2012.
 */

#ifndef SERTOS_CONFIG_H
#define SERTOS_CONFIG_H

#include <stdint.h>

/**
 * @brief Maximum number of priority levels supported by the scheduler.
 *
 * Supported range: 1 to 32. Priority 0 is reserved for the system Idle Task.
 * Priority (SERTOS_CONFIG_MAX_PRIORITIES - 1) is the highest priority.
 */
#define SERTOS_CONFIG_MAX_PRIORITIES        (32U)

/**
 * @brief System tick timer frequency in Hertz (Hz).
 *
 * 1000U corresponds to a 1-millisecond resolution tick interval.
 */
#define SERTOS_CONFIG_TICK_RATE_HZ          (1000U)

/**
 * @brief Architecture-enforced stack alignment boundary in bytes.
 *
 * 8-byte alignment required by ARM AAPCS and 64-bit platforms.
 */
#define SERTOS_CONFIG_STACK_ALIGNMENT_BYTES (8U)

/**
 * @brief Architecture-specific minimum stack frame size in bytes.
 */
#define SERTOS_CONFIG_MINIMAL_STACK_SIZE    (256U)

/**
 * @brief Default stack size for application tasks in bytes.
 */
#define SERTOS_CONFIG_DEFAULT_STACK_SIZE    (1024U)

/**
 * @brief Dedicated stack size in bytes allocated for the system Idle Task.
 */
#define SERTOS_CONFIG_IDLE_TASK_STACK_SIZE  (512U)

/**
 * @brief Maximum string length of a task diagnostic name including null terminator.
 */
#define SERTOS_CONFIG_MAX_TASK_NAME_LEN     (16U)

/**
 * @brief Enable or disable round-robin time-slicing among tasks of equal priority.
 *
 * 1U to enable time slicing, 0U for cooperative time slicing at equal priority.
 */
#define SERTOS_CONFIG_TIME_SLICING          (1U)

/**
 * @brief Enable or disable runtime assertion and canary checking.
 */
#define SERTOS_CONFIG_ASSERT_ENABLED        (1U)

#endif /* SERTOS_CONFIG_H */
