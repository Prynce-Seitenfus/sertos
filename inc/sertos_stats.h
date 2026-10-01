/**
 * @file sertos_stats.h
 * @brief Runtime kernel statistics (telemetry) API for SertOS.
 *
 * Provides deterministic, zero-allocation runtime accounting of per-task CPU
 * time, context-switch counts, idle/CPU load, and tick telemetry. Accounting is
 * always compiled in and enabled at runtime through
 * SertosConfig.enable_runtime_stats. Conforms to ISO C99 and MISRA C:2012.
 */

#ifndef SERTOS_STATS_H
#define SERTOS_STATS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sertos_types.h"
#include "sertos_task.h"

/**
 * @brief Per-task runtime statistics snapshot record.
 */
typedef struct SertosTaskStats {
    const char* name;                /**< Task diagnostic name pointer. */
    SertosPriority priority;         /**< Active scheduling priority. */
    SertosTaskState state;           /**< Current lifecycle state at snapshot time. */
    SertosRunCount run_time;         /**< Accumulated run-time counter units in the RUNNING state. */
    uint32_t cpu_percent_x100;       /**< CPU share as fixed-point hundredths of a percent (0..10000). */
    uint32_t switch_in_count;        /**< Number of times the task was scheduled in. */
    size_t stack_high_water;         /**< Unused stack bytes (high-water mark) at snapshot time. */
} SertosTaskStats;

/**
 * @brief System-wide runtime statistics snapshot record.
 */
typedef struct SertosSystemStats {
    SertosRunCount total_run_time;   /**< Sum of run-time across all tasks (including idle). */
    SertosRunCount idle_run_time;    /**< Run-time accumulated by the Idle Task. */
    uint32_t idle_percent_x100;      /**< Idle share as fixed-point hundredths of a percent (0..10000). */
    uint32_t total_switches;         /**< Total number of context switches since reset. */
    SertosTick total_ticks;          /**< Monotonic system tick count at snapshot time. */
    uint32_t task_count;             /**< Number of tasks currently tracked by the scheduler. */
} SertosSystemStats;

/**
 * @brief Enables or disables runtime statistics accounting.
 *
 * Normally invoked internally by sertos_scheduler_init_with_config() from the
 * SertosConfig.enable_runtime_stats field. When disabled, the per-switch
 * accounting hot path is reduced to a single predictable branch.
 *
 * @param[in] enabled True to enable accounting, false to disable.
 */
void sertos_stats_set_enabled(bool enabled);

/**
 * @brief Queries whether runtime statistics accounting is currently enabled.
 *
 * @return true if accounting is enabled, false otherwise.
 */
bool sertos_stats_is_enabled(void);

/**
 * @brief Context-switch accounting hook invoked by the scheduler.
 *
 * Accumulates the outgoing task's run-time and records switch-in metadata for
 * the incoming task. Executes fully only when accounting is enabled; otherwise
 * returns immediately. Intended for internal scheduler use on the switch path.
 *
 * @param[in,out] prev Outgoing task control block, or NULL if none.
 * @param[in,out] next Incoming task control block.
 */
void sertos_stats_on_switch(SertosTaskControlBlock* prev, SertosTaskControlBlock* next);

/**
 * @brief Resets all runtime statistics counters to zero.
 *
 * Zeroes per-task run-time and switch counts, the global switch counter, and
 * reseeds each task's run-time baseline to the current counter value.
 */
void sertos_stats_reset(void);

/**
 * @brief Retrieves a consistent system-wide statistics snapshot.
 *
 * @param[out] out Pointer to caller-allocated system statistics structure.
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_NULL_PTR if out is
 *         NULL, or SERTOS_STATUS_ERROR_NOT_INITIALIZED if stats are disabled.
 */
SertosStatus sertos_stats_get_system(SertosSystemStats* out);

/**
 * @brief Fills a caller-provided array with per-task statistics snapshots.
 *
 * Walks all scheduler task lists under a critical section and writes up to
 * @p capacity records, computing each task's CPU share against the total.
 *
 * @param[out] out_array Caller-allocated array of task statistics records.
 * @param[in]  capacity  Maximum number of records that out_array can hold.
 * @return Number of records written (0 if disabled or arguments invalid).
 */
size_t sertos_stats_get_tasks(SertosTaskStats* out_array, size_t capacity);

/**
 * @brief Retrieves the runtime statistics of a single task.
 *
 * @param[in]  handle Task handle to query.
 * @param[out] out    Pointer to caller-allocated task statistics structure.
 * @return SERTOS_STATUS_OK on success, SERTOS_STATUS_ERROR_NULL_PTR on a NULL
 *         argument, or SERTOS_STATUS_ERROR_NOT_INITIALIZED if stats disabled.
 */
SertosStatus sertos_stats_get_task(SertosTaskHandle handle, SertosTaskStats* out);

/**
 * @brief Returns overall CPU load as fixed-point hundredths of a percent.
 *
 * Computed as 10000 minus the idle share, i.e. the fraction of run-time spent
 * in non-idle tasks.
 *
 * @return CPU load in hundredths of a percent (0..10000), or 0 if disabled.
 */
uint32_t sertos_stats_get_cpu_load_x100(void);

#endif /* SERTOS_STATS_H */
