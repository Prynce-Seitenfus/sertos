/**
 * @file sertos_types.h
 * @brief Fundamental kernel types, error codes, and handles for SertOS.
 *
 * Conforms to ISO C99 and MISRA C:2012. Defines architecture-agnostic primitives
 * and return status enumerations.
 */

#ifndef SERTOS_TYPES_H
#define SERTOS_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_config.h"

/**
 * @brief Kernel operation return status codes.
 */
typedef enum SertosStatus {
    SERTOS_STATUS_OK                     = 0,  /**< Operation completed successfully. */
    SERTOS_STATUS_ERROR_NULL_PTR         = 1,  /**< A null pointer argument was provided. */
    SERTOS_STATUS_ERROR_INVALID_PARAM    = 2,  /**< An invalid parameter value was provided. */
    SERTOS_STATUS_ERROR_NO_MEMORY        = 3,  /**< Memory pool exhausted or stack buffer insufficient. */
    SERTOS_STATUS_ERROR_TIMEOUT          = 4,  /**< Operation timed out before completion. */
    SERTOS_STATUS_ERROR_RESOURCE_BUSY    = 5,  /**< Resource unavailable or locked. */
    SERTOS_STATUS_ERROR_NOT_INITIALIZED  = 6,  /**< Kernel subsystem not initialized. */
    SERTOS_STATUS_ERROR_ISR_CONTEXT      = 7,  /**< Operation illegal from interrupt service routine. */
    SERTOS_STATUS_ERROR_STACK_OVERFLOW   = 8   /**< Stack canary corrupted or boundary violated. */
} SertosStatus;

/**
 * @brief Task priority level.
 *
 * Lower numerical values denote lower priority; 0 is reserved for the Idle Task.
 */
typedef uint8_t SertosPriority;

/**
 * @brief System tick count representation.
 */
typedef uint32_t SertosTick;

/**
 * @brief Special timeout constants.
 */
#define SERTOS_NO_WAIT       ((SertosTick)0U)
#define SERTOS_WAIT_FOREVER  ((SertosTick)0xFFFFFFFFU)

/**
 * @brief Stack memory alignment boundary in bytes.
 */
#define SERTOS_STACK_ALIGNMENT_BYTES (SERTOS_CONFIG_STACK_ALIGNMENT_BYTES)

/**
 * @brief Task entry function pointer type.
 *
 * @param param User-supplied context pointer passed during task creation.
 */
typedef void (*SertosTaskFunction)(void* param);

/* Forward declaration of the internal Task Control Block structure. */
struct SertosTaskControlBlock;

/**
 * @brief Opaque task handle exposed to application and API layers.
 */
typedef struct SertosTaskControlBlock* SertosTaskHandle;

#endif /* SERTOS_TYPES_H */
