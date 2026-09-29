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
#include <stddef.h>
#include <stdbool.h>

/* Allow application-level custom header override if present */
#if defined(SERTOS_USE_CUSTOM_CONFIG) || (defined(__has_include) && __has_include("sertos_app_config.h"))
#include "sertos_app_config.h"
#endif

/**
 * @brief Maximum number of priority levels supported by the scheduler.
 *
 * Supported range: 1 to 32. Priority 0 is reserved for the system Idle Task.
 * Priority (SERTOS_CONFIG_MAX_PRIORITIES - 1) is the highest priority.
 */
#ifndef SERTOS_CONFIG_MAX_PRIORITIES
#define SERTOS_CONFIG_MAX_PRIORITIES        (32U)
#endif

/**
 * @brief Maximum concurrent tasks supported by the host simulator port.
 */
#ifndef SERTOS_CONFIG_MAX_TASKS
#define SERTOS_CONFIG_MAX_TASKS             (32U)
#endif

/**
 * @brief Default system tick timer frequency in Hertz (Hz).
 *
 * 1000U corresponds to a 1-millisecond resolution tick interval.
 * Can be overridden at runtime via SertosConfig without rebuilding the library.
 */
#ifndef SERTOS_CONFIG_TICK_RATE_HZ
#define SERTOS_CONFIG_TICK_RATE_HZ          (1000U)
#endif

/**
 * @brief Architecture-enforced stack alignment boundary in bytes.
 *
 * 8-byte alignment required by ARM AAPCS and 64-bit platforms.
 */
#ifndef SERTOS_CONFIG_STACK_ALIGNMENT_BYTES
#define SERTOS_CONFIG_STACK_ALIGNMENT_BYTES (8U)
#endif

/**
 * @brief Architecture-specific minimum stack frame size in bytes.
 */
#ifndef SERTOS_CONFIG_MINIMAL_STACK_SIZE
#define SERTOS_CONFIG_MINIMAL_STACK_SIZE    (256U)
#endif

/**
 * @brief Default stack size for application tasks in bytes.
 */
#ifndef SERTOS_CONFIG_DEFAULT_STACK_SIZE
#define SERTOS_CONFIG_DEFAULT_STACK_SIZE    (1024U)
#endif

/**
 * @brief Dedicated stack size in bytes allocated for the default system Idle Task.
 */
#ifndef SERTOS_CONFIG_IDLE_TASK_STACK_SIZE
#define SERTOS_CONFIG_IDLE_TASK_STACK_SIZE  (512U)
#endif

/**
 * @brief Maximum string length of a task diagnostic name if stored as fixed buffer.
 */
#ifndef SERTOS_CONFIG_MAX_TASK_NAME_LEN
#define SERTOS_CONFIG_MAX_TASK_NAME_LEN     (32U)
#endif

/**
 * @brief Default round-robin time-slicing among tasks of equal priority.
 *
 * 1U to enable time slicing, 0U for cooperative time slicing at equal priority.
 * Can be overridden at runtime via SertosConfig without rebuilding the library.
 */
#ifndef SERTOS_CONFIG_TIME_SLICING
#define SERTOS_CONFIG_TIME_SLICING          (1U)
#endif

/**
 * @brief Enable or disable runtime assertion and canary checking.
 */
#ifndef SERTOS_CONFIG_ASSERT_ENABLED
#define SERTOS_CONFIG_ASSERT_ENABLED        (1U)
#endif

#endif /* SERTOS_CONFIG_H */
