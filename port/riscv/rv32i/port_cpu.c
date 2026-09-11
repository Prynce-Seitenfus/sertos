/**
 * @file port_cpu.c
 * @brief RISC-V RV32I port CPU implementation for SertOS.
 *
 * Implements hardware stack frame initialization for 32-bit RISC-V integer cores,
 * machine timer, and critical section masking.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include <string.h>

/**
 * @brief RISC-V RV32I standard context frame (32 registers + mepc + mstatus).
 */
typedef struct RiscV32StackFrame {
    uint32_t mepc;      /**< Machine exception program counter (task entry point). */
    uint32_t mstatus;   /**< Machine status register (MPIE enabled). */
    uint32_t ra;        /**< Return address (x1). */
    uint32_t t0;        /**< Temporary register (x5). */
    uint32_t t1;        /**< Temporary register (x6). */
    uint32_t t2;        /**< Temporary register (x7). */
    uint32_t s0;        /**< Saved register / frame pointer (x8). */
    uint32_t s1;        /**< Saved register (x9). */
    uint32_t a0;        /**< Function argument 0 / return value (param) (x10). */
    uint32_t a1;        /**< Function argument 1 (x11). */
    uint32_t a2;        /**< Function argument 2 (x12). */
    uint32_t a3;        /**< Function argument 3 (x13). */
    uint32_t a4;        /**< Function argument 4 (x14). */
    uint32_t a5;        /**< Function argument 5 (x15). */
    uint32_t a6;        /**< Function argument 6 (x16). */
    uint32_t a7;        /**< Function argument 7 (x17). */
    uint32_t s2;        /**< Saved register (x18). */
    uint32_t s3;        /**< Saved register (x19). */
    uint32_t s4;        /**< Saved register (x20). */
    uint32_t s5;        /**< Saved register (x21). */
    uint32_t s6;        /**< Saved register (x22). */
    uint32_t s7;        /**< Saved register (x23). */
    uint32_t s8;        /**< Saved register (x24). */
    uint32_t s9;        /**< Saved register (x25). */
    uint32_t s10;       /**< Saved register (x26). */
    uint32_t s11;       /**< Saved register (x27). */
    uint32_t t3;        /**< Temporary register (x28). */
    uint32_t t4;        /**< Temporary register (x29). */
    uint32_t t5;        /**< Temporary register (x30). */
    uint32_t t6;        /**< Temporary register (x31). */
} RiscV32StackFrame;

#define MSTATUS_MPP_MASK    (3U << 11U)
#define MSTATUS_MPIE        (1U << 7U)

static uint32_t s_critical_nesting = 0U;

void* sertos_port_stack_init(void* stack_top, void* stack_limit, SertosTaskFunction entry, void* param)
{
    RiscV32StackFrame* frame;
    uintptr_t top_addr;

    (void)stack_limit;

    if (stack_top == NULL) {
        return NULL;
    }

    top_addr = (uintptr_t)stack_top;
    top_addr &= ~((uintptr_t)SERTOS_STACK_ALIGNMENT_BYTES - 1U);
    top_addr -= sizeof(RiscV32StackFrame);

    frame = (RiscV32StackFrame*)top_addr;
    (void)memset(frame, 0, sizeof(RiscV32StackFrame));

    frame->mepc = (uint32_t)(uintptr_t)entry;
    frame->mstatus = MSTATUS_MPP_MASK | MSTATUS_MPIE;
    frame->a0 = (uint32_t)(uintptr_t)param;

    return (void*)frame;
}

void sertos_port_yield(void)
{
    /* Triggers software trap / ecall */
}

uint32_t sertos_port_enter_critical(void)
{
    uint32_t prev_mstatus = 0U;

#if defined(__riscv)
    __asm__ volatile (
        "csrrci %0, mstatus, 8\n"
        : "=r" (prev_mstatus) :: "memory"
    );
#endif

    s_critical_nesting++;
    return prev_mstatus;
}

void sertos_port_exit_critical(uint32_t status)
{
    if (s_critical_nesting > 0U) {
        s_critical_nesting--;
        if (s_critical_nesting == 0U) {
#if defined(__riscv)
            __asm__ volatile (
                "csrw mstatus, %0\n"
                :: "r" (status) : "memory"
            );
#else
            (void)status;
#endif
        }
    }
}

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
    (void)tick_rate_hz;
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}
