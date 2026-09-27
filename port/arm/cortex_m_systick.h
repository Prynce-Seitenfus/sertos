#ifndef CORTEX_M_SYSTICK_H
#define CORTEX_M_SYSTICK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORTEX_M_SYSTICK_MAX_RELOAD_COUNTS (0x01000000U)

/**
 * @brief Converts timer clock and tick rate to a valid 24-bit SysTick reload.
 *
 * @param timer_clock_hz SysTick input clock frequency in Hertz.
 * @param tick_rate_hz Desired periodic tick frequency in Hertz.
 * @param reload_value Destination for the SysTick LOAD value.
 * @return true if the reload value fits SysTick, otherwise false.
 */
static inline bool cortex_m_systick(uint32_t timer_clock_hz,
                                    uint32_t tick_rate_hz,
                                    uint32_t* reload_value)
{
    uint32_t counts_per_tick;

    if ((timer_clock_hz == 0U) || (tick_rate_hz == 0U) || (reload_value == NULL)) {
        return false;
    }

    counts_per_tick = timer_clock_hz / tick_rate_hz;
    if ((counts_per_tick == 0U) ||
        (counts_per_tick > CORTEX_M_SYSTICK_MAX_RELOAD_COUNTS)) {
        return false;
    }

    *reload_value = counts_per_tick - 1U;
    return true;
}

#endif /* CORTEX_M_SYSTICK_H */
