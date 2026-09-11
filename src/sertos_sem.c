/**
 * @file sertos_sem.c
 * @brief Binary and counting semaphore synchronization implementation for SertOS.
 *
 * Conforms to ISO C99 and MISRA C:2012.
 */

#include "sertos_sem.h"
#include "sertos_scheduler.h"
#include "sertos_port.h"
#include "memory_pool.h"

SertosStatus sertos_sem_create_counting_static(SertosSemaphore* sem,
                                               size_t initial_count,
                                               size_t max_count,
                                               SertosSemHandle* out_handle)
{
    if ((sem == NULL) || (out_handle == NULL)) {
        return SERTOS_STATUS_ERROR_NULL_PTR;
    }
    if ((max_count == 0U) || (initial_count > max_count)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    sem->count = initial_count;
    sem->max_count = max_count;
    (void)linked_list_init(&sem->wait_list);
    sem->is_statically_allocated = true;
    sem->magic = SERTOS_SEM_MAGIC_WORD;

    *out_handle = sem;
    return SERTOS_STATUS_OK;
}

SertosStatus sertos_sem_create_binary_static(SertosSemaphore* sem,
                                             bool initial_state,
                                             SertosSemHandle* out_handle)
{
    size_t initial_count = initial_state ? 1U : 0U;
    return sertos_sem_create_counting_static(sem, initial_count, 1U, out_handle);
}

SertosStatus sertos_sem_create_counting(size_t initial_count,
                                        size_t max_count,
                                        SertosSemHandle* out_handle)
{
    SertosSemaphore* sem;
    SertosStatus status;

    if (out_handle == NULL) {
        return SERTOS_STATUS_ERROR_NULL_PTR;
    }

    sem = (SertosSemaphore*)memory_pool_malloc(sizeof(SertosSemaphore));
    if (sem == NULL) {
        return SERTOS_STATUS_ERROR_NO_MEMORY;
    }

    status = sertos_sem_create_counting_static(sem, initial_count, max_count, out_handle);
    if (status != SERTOS_STATUS_OK) {
        memory_pool_free(sem);
        return status;
    }

    sem->is_statically_allocated = false;
    return SERTOS_STATUS_OK;
}

SertosStatus sertos_sem_create_binary(bool initial_state, SertosSemHandle* out_handle)
{
    size_t initial_count = initial_state ? 1U : 0U;
    return sertos_sem_create_counting(initial_count, 1U, out_handle);
}

SertosStatus sertos_sem_delete(SertosSemHandle handle)
{
    uint32_t crit_status;
    SertosTaskControlBlock* unblocked;

    if ((handle == NULL) || (handle->magic != SERTOS_SEM_MAGIC_WORD)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    crit_status = sertos_port_enter_critical();
    handle->magic = 0U;

    /* Wake all tasks waiting on the deleted semaphore */
    do {
        unblocked = sertos_scheduler_wait_list_unblock_highest(&handle->wait_list);
    } while (unblocked != NULL);

    if (!handle->is_statically_allocated) {
        memory_pool_free(handle);
    }
    sertos_port_exit_critical(crit_status);

    if (sertos_scheduler_is_running()) {
        sertos_scheduler_reschedule();
    }

    return SERTOS_STATUS_OK;
}

SertosStatus sertos_sem_take(SertosSemHandle handle, SertosTick timeout)
{
    uint32_t crit_status;

    if ((handle == NULL) || (handle->magic != SERTOS_SEM_MAGIC_WORD)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    crit_status = sertos_port_enter_critical();
    if (handle->count > 0U) {
        handle->count--;
        sertos_port_exit_critical(crit_status);
        return SERTOS_STATUS_OK;
    }

    if (timeout == SERTOS_NO_WAIT) {
        sertos_port_exit_critical(crit_status);
        return SERTOS_STATUS_ERROR_TIMEOUT;
    }

    sertos_port_exit_critical(crit_status);
    return sertos_scheduler_wait_list_block(&handle->wait_list, timeout);
}

SertosStatus sertos_sem_give(SertosSemHandle handle)
{
    uint32_t crit_status;
    SertosTaskControlBlock* unblocked;

    if ((handle == NULL) || (handle->magic != SERTOS_SEM_MAGIC_WORD)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    crit_status = sertos_port_enter_critical();
    unblocked = sertos_scheduler_wait_list_unblock_highest(&handle->wait_list);
    if (unblocked != NULL) {
        sertos_port_exit_critical(crit_status);
        if (sertos_scheduler_is_running()) {
            sertos_scheduler_reschedule();
        }
        return SERTOS_STATUS_OK;
    }

    if (handle->count < handle->max_count) {
        handle->count++;
        sertos_port_exit_critical(crit_status);
        return SERTOS_STATUS_OK;
    }

    sertos_port_exit_critical(crit_status);
    return SERTOS_STATUS_ERROR_RESOURCE_BUSY;
}

SertosStatus sertos_sem_give_from_isr(SertosSemHandle handle, bool* out_higher_prio_woken)
{
    uint32_t crit_status;
    SertosTaskControlBlock* unblocked;
    SertosTaskControlBlock* current;

    if ((handle == NULL) || (handle->magic != SERTOS_SEM_MAGIC_WORD)) {
        return SERTOS_STATUS_ERROR_INVALID_PARAM;
    }

    if (out_higher_prio_woken != NULL) {
        *out_higher_prio_woken = false;
    }

    crit_status = sertos_port_enter_critical();
    unblocked = sertos_scheduler_wait_list_unblock_highest(&handle->wait_list);
    if (unblocked != NULL) {
        current = sertos_scheduler_get_current_tcb();
        if ((out_higher_prio_woken != NULL) && (current != NULL)) {
            if (unblocked->priority > current->priority) {
                *out_higher_prio_woken = true;
            }
        }
        sertos_port_exit_critical(crit_status);
        return SERTOS_STATUS_OK;
    }

    if (handle->count < handle->max_count) {
        handle->count++;
        sertos_port_exit_critical(crit_status);
        return SERTOS_STATUS_OK;
    }

    sertos_port_exit_critical(crit_status);
    return SERTOS_STATUS_ERROR_RESOURCE_BUSY;
}

size_t sertos_sem_get_count(SertosSemHandle handle)
{
    if ((handle == NULL) || (handle->magic != SERTOS_SEM_MAGIC_WORD)) {
        return 0U;
    }
    return handle->count;
}
