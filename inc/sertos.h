/**
 * @file sertos.h
 * @brief Primary umbrella header for the SertOS real-time operating system.
 *
 * Exposes all kernel APIs, task management functions, synchronization primitives,
 * IPC queues, software timers, configuration options, and hardware port contracts.
 */

#ifndef SERTOS_H
#define SERTOS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Core types and error status codes */
#include "sertos_types.h"

/* User and application configuration settings */
#include "sertos_config.h"

/* Task management and TCB structures */
#include "sertos_task.h"

/* Scheduler control and tick functions */
#include "sertos_scheduler.h"

/* Runtime kernel statistics and telemetry */
#include "sertos_stats.h"

/* Mutual exclusion synchronization primitive (PIP-supported) */
#include "sertos_mutex.h"

/* Binary and counting semaphores */
#include "sertos_sem.h"

/* Thread-safe message queue IPC primitive */
#include "sertos_queue.h"

/* Single-producer single-consumer byte stream buffer */
#include "sertos_stream_buffer.h"

/* Monotonic software timers */
#include "sertos_timer.h"

/* Architecture port contract */
#include "sertos_port.h"

#ifdef __cplusplus
}
#endif

#endif /* SERTOS_H */
