/**
 * @file port_cpu.c
 * @brief RISC-V RV32I port CPU implementation for SertOS.
 *
 * Implements hardware stack frame initialization for 32-bit RISC-V integer cores,
 * CLINT machine timer, and critical section masking.
 */

#include "sertos_port.h"
#include "sertos_task.h"
#include "sertos_scheduler.h"
#include <string.h>

#if defined(__riscv_flen) && (__riscv_flen > 0)
/**
 * @brief RISC-V RV32IMAFC FPU-extended context frame (64 words = 256 bytes).
 */
typedef struct RiscV32StackFrame {
    uint32_t ra;        /**< Return address (x1, offset 0). */
    uint32_t t0;        /**< Temporary register (x5, offset 4). */
    uint32_t t1;        /**< Temporary register (x6, offset 8). */
    uint32_t t2;        /**< Temporary register (x7, offset 12). */
    uint32_t s0;        /**< Saved register / frame pointer (x8, offset 16). */
    uint32_t s1;        /**< Saved register (x9, offset 20). */
    uint32_t a0;        /**< Function argument 0 / return value (param) (x10, offset 24). */
    uint32_t a1;        /**< Function argument 1 (x11, offset 28). */
    uint32_t a2;        /**< Function argument 2 (x12, offset 32). */
    uint32_t a3;        /**< Function argument 3 (x13, offset 36). */
    uint32_t a4;        /**< Function argument 4 (x14, offset 40). */
    uint32_t a5;        /**< Function argument 5 (x15, offset 44). */
    uint32_t a6;        /**< Function argument 6 (x16, offset 48). */
    uint32_t a7;        /**< Function argument 7 (x17, offset 52). */
    uint32_t s2;        /**< Saved register (x18, offset 56). */
    uint32_t s3;        /**< Saved register (x19, offset 60). */
    uint32_t s4;        /**< Saved register (x20, offset 64). */
    uint32_t s5;        /**< Saved register (x21, offset 68). */
    uint32_t s6;        /**< Saved register (x22, offset 72). */
    uint32_t s7;        /**< Saved register (x23, offset 76). */
    uint32_t s8;        /**< Saved register (x24, offset 80). */
    uint32_t s9;        /**< Saved register (x25, offset 84). */
    uint32_t s10;       /**< Saved register (x26, offset 88). */
    uint32_t s11;       /**< Saved register (x27, offset 92). */
    uint32_t t3;        /**< Temporary register (x28, offset 96). */
    uint32_t t4;        /**< Temporary register (x29, offset 100). */
    uint32_t t5;        /**< Temporary register (x30, offset 104). */
    uint32_t t6;        /**< Temporary register (x31, offset 108). */
    uint32_t mepc;      /**< Machine exception program counter (offset 112). */
    uint32_t mstatus;   /**< Machine status register (offset 116). */
    uint32_t f[32];     /**< Floating point registers f0..f31 (offsets 120..244). */
    uint32_t fcsr;      /**< Floating point control and status register (offset 248). */
    uint32_t pad;       /**< Align to 256 bytes (offset 252). */
} RiscV32StackFrame;
#else
/**
 * @brief RISC-V RV32I integer-only standard context frame (32 words = 128 bytes).
 */
typedef struct RiscV32StackFrame {
    uint32_t ra;        /**< Return address (x1, offset 0). */
    uint32_t t0;        /**< Temporary register (x5, offset 4). */
    uint32_t t1;        /**< Temporary register (x6, offset 8). */
    uint32_t t2;        /**< Temporary register (x7, offset 12). */
    uint32_t s0;        /**< Saved register / frame pointer (x8, offset 16). */
    uint32_t s1;        /**< Saved register (x9, offset 20). */
    uint32_t a0;        /**< Function argument 0 / return value (param) (x10, offset 24). */
    uint32_t a1;        /**< Function argument 1 (x11, offset 28). */
    uint32_t a2;        /**< Function argument 2 (x12, offset 32). */
    uint32_t a3;        /**< Function argument 3 (x13, offset 36). */
    uint32_t a4;        /**< Function argument 4 (x14, offset 40). */
    uint32_t a5;        /**< Function argument 5 (x15, offset 44). */
    uint32_t a6;        /**< Function argument 6 (x16, offset 48). */
    uint32_t a7;        /**< Function argument 7 (x17, offset 52). */
    uint32_t s2;        /**< Saved register (x18, offset 56). */
    uint32_t s3;        /**< Saved register (x19, offset 60). */
    uint32_t s4;        /**< Saved register (x20, offset 64). */
    uint32_t s5;        /**< Saved register (x21, offset 68). */
    uint32_t s6;        /**< Saved register (x22, offset 72). */
    uint32_t s7;        /**< Saved register (x23, offset 76). */
    uint32_t s8;        /**< Saved register (x24, offset 80). */
    uint32_t s9;        /**< Saved register (x25, offset 84). */
    uint32_t s10;       /**< Saved register (x26, offset 88). */
    uint32_t s11;       /**< Saved register (x27, offset 92). */
    uint32_t t3;        /**< Temporary register (x28, offset 96). */
    uint32_t t4;        /**< Temporary register (x29, offset 100). */
    uint32_t t5;        /**< Temporary register (x30, offset 104). */
    uint32_t t6;        /**< Temporary register (x31, offset 108). */
    uint32_t mepc;      /**< Machine exception program counter (offset 112). */
    uint32_t mstatus;   /**< Machine status register (offset 116). */
    uint32_t pad[2];    /**< Align to 128 bytes (offsets 120, 124). */
} RiscV32StackFrame;
#endif

#define MSTATUS_MPP_MACHINE (3U << 11U)
#define MSTATUS_MPIE_ENABLE (1U << 7U)
#define MSTATUS_FS_INITIAL  (1U << 13U)

static volatile uint32_t s_critical_nesting = 0U;
static volatile uint32_t s_in_isr = 0U;
static uint32_t s_tick_cycles = 10000U;

extern void sertos_riscv_trap_handler(void);

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
#if defined(__riscv_flen) && (__riscv_flen > 0)
    frame->mstatus = MSTATUS_MPP_MACHINE | MSTATUS_MPIE_ENABLE | MSTATUS_FS_INITIAL;
#else
    frame->mstatus = MSTATUS_MPP_MACHINE | MSTATUS_MPIE_ENABLE;
#endif
    frame->a0 = (uint32_t)(uintptr_t)param;

    return (void*)frame;
}

void sertos_port_yield(void)
{
#if defined(__riscv)
    if (s_in_isr == 0U) {
        __asm__ volatile ("ecall" ::: "memory");
    }
#endif
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

void sertos_port_riscv_timer_handler(void)
{
#if defined(__riscv)
    volatile uint32_t* const mtimecmp_l = (volatile uint32_t*)0x02004000U;
    volatile uint32_t* const mtimecmp_h = (volatile uint32_t*)0x02004004U;
    volatile uint32_t* const mtime_l    = (volatile uint32_t*)0x0200BFF8U;
    volatile uint32_t* const mtime_h    = (volatile uint32_t*)0x0200BFFCU;

    uint32_t now_h;
    uint32_t now_l;
    uint64_t next;

    do {
        now_h = *mtime_h;
        now_l = *mtime_l;
    } while (now_h != *mtime_h);

    next = (((uint64_t)now_h << 32U) | (uint64_t)now_l) + (uint64_t)s_tick_cycles;

    *mtimecmp_h = 0xFFFFFFFFU;
    *mtimecmp_l = (uint32_t)(next & 0xFFFFFFFFU);
    *mtimecmp_h = (uint32_t)(next >> 32U);

    s_in_isr = 1U;
    sertos_scheduler_tick();
    s_in_isr = 0U;
#endif
}

void sertos_port_tick_init(uint32_t tick_rate_hz)
{
#if defined(__riscv)
    uintptr_t trap_addr = (uintptr_t)sertos_riscv_trap_handler;
    __asm__ volatile ("csrw mtvec, %0" :: "r" (trap_addr) : "memory");

#if defined(__riscv_flen) && (__riscv_flen > 0)
    /* Enable FPU access in M-mode (mstatus.FS = Initial = 1) */
    __asm__ volatile ("csrs mstatus, %0" :: "r" (MSTATUS_FS_INITIAL) : "memory");
#endif

    if (tick_rate_hz > 0U) {
        volatile uint32_t* const mtimecmp_l = (volatile uint32_t*)0x02004000U;
        volatile uint32_t* const mtimecmp_h = (volatile uint32_t*)0x02004004U;
        volatile uint32_t* const mtime_l    = (volatile uint32_t*)0x0200BFF8U;
        volatile uint32_t* const mtime_h    = (volatile uint32_t*)0x0200BFFCU;

        uint32_t now_h;
        uint32_t now_l;
        uint64_t next;

        /* QEMU virt CLINT clock is 10 MHz */
        s_tick_cycles = 10000000U / tick_rate_hz;

        do {
            now_h = *mtime_h;
            now_l = *mtime_l;
        } while (now_h != *mtime_h);

        next = (((uint64_t)now_h << 32U) | (uint64_t)now_l) + (uint64_t)s_tick_cycles;

        *mtimecmp_h = 0xFFFFFFFFU;
        *mtimecmp_l = (uint32_t)(next & 0xFFFFFFFFU);
        *mtimecmp_h = (uint32_t)(next >> 32U);

        /* Enable Machine Timer Interrupt (MTIE = bit 7 in mie) */
        __asm__ volatile ("csrs mie, %0" :: "r" (1U << 7U) : "memory");
    }
#else
    (void)tick_rate_hz;
#endif
}

void sertos_port_task_create_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_task_delete_hook(struct SertosTaskControlBlock* tcb)
{
    (void)tcb;
}

void sertos_port_stop_scheduler(void)
{
#if defined(__riscv)
    volatile uint32_t* test_device;

    /* Disable interrupts globally */
    __asm__ volatile ("csrci mstatus, 8" ::: "memory");

    /* QEMU virt test finisher device exit (FINISHER_PASS = 0x5555) */
    test_device = (volatile uint32_t*)0x100000U;
    *test_device = 0x5555U;

    while (1) {
        __asm__ volatile ("wfi");
    }
#endif
}
