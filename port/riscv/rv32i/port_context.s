/*
 * port_context.s - RISC-V RV32I Context Switch Trap Handler
 * Part of SertOS Kernel.
 */

    .section .text
    .align 4
    .global sertos_riscv_trap_handler
    .type sertos_riscv_trap_handler, @function
    .global sertos_port_start_first_task
    .type sertos_port_start_first_task, @function

sertos_port_start_first_task:
    la      t0, sertos_current_tcb
    lw      a0, 0(t0)
    lw      sp, 0(a0)

    lw      t0, 112(sp)
    csrw    mepc, t0
    lw      t1, 116(sp)
    csrw    mstatus, t1

    lw      ra, 0(sp)
    lw      t0, 4(sp)
    lw      t1, 8(sp)
    lw      t2, 12(sp)
    lw      s0, 16(sp)
    lw      s1, 20(sp)
    lw      a0, 24(sp)
    lw      a1, 28(sp)
    lw      a2, 32(sp)
    lw      a3, 36(sp)
    lw      a4, 40(sp)
    lw      a5, 44(sp)
    lw      a6, 48(sp)
    lw      a7, 52(sp)
    lw      s2, 56(sp)
    lw      s3, 60(sp)
    lw      s4, 64(sp)
    lw      s5, 68(sp)
    lw      s6, 72(sp)
    lw      s7, 76(sp)
    lw      s8, 80(sp)
    lw      s9, 84(sp)
    lw      s10, 88(sp)
    lw      s11, 92(sp)
    lw      t3, 96(sp)
    lw      t4, 100(sp)
    lw      t5, 104(sp)
    lw      t6, 108(sp)

    addi    sp, sp, 128
    mret
    .size sertos_port_start_first_task, .-sertos_port_start_first_task

sertos_riscv_trap_handler:
    /* Allocate 32 words on task stack */
    addi    sp, sp, -128

    /* Save caller-saved and callee-saved registers */
    sw      ra, 0(sp)
    sw      t0, 4(sp)
    sw      t1, 8(sp)
    sw      t2, 12(sp)
    sw      s0, 16(sp)
    sw      s1, 20(sp)
    sw      a0, 24(sp)
    sw      a1, 28(sp)
    sw      a2, 32(sp)
    sw      a3, 36(sp)
    sw      a4, 40(sp)
    sw      a5, 44(sp)
    sw      a6, 48(sp)
    sw      a7, 52(sp)
    sw      s2, 56(sp)
    sw      s3, 60(sp)
    sw      s4, 64(sp)
    sw      s5, 68(sp)
    sw      s6, 72(sp)
    sw      s7, 76(sp)
    sw      s8, 80(sp)
    sw      s9, 84(sp)
    sw      s10, 88(sp)
    sw      s11, 92(sp)
    sw      t3, 96(sp)
    sw      t4, 100(sp)
    sw      t5, 104(sp)
    sw      t6, 108(sp)

    csrr    t0, mepc
    sw      t0, 112(sp)
    csrr    t1, mstatus
    sw      t1, 116(sp)

    /* Store sp in tcb->stack_ptr */
    la      t0, sertos_current_tcb
    lw      t1, 0(t0)
    beqz    t1, restore_riscv_task
    sw      sp, 0(t1)

restore_riscv_task:
    call    sertos_scheduler_perform_switch

    /* Load new sp from tcb->stack_ptr (a0 contains sertos_current_tcb) */
    lw      sp, 0(a0)

    lw      t0, 112(sp)
    csrw    mepc, t0
    lw      t1, 116(sp)
    csrw    mstatus, t1

    lw      ra, 0(sp)
    lw      t0, 4(sp)
    lw      t1, 8(sp)
    lw      t2, 12(sp)
    lw      s0, 16(sp)
    lw      s1, 20(sp)
    lw      a0, 24(sp)
    lw      a1, 28(sp)
    lw      a2, 32(sp)
    lw      a3, 36(sp)
    lw      a4, 40(sp)
    lw      a5, 44(sp)
    lw      a6, 48(sp)
    lw      a7, 52(sp)
    lw      s2, 56(sp)
    lw      s3, 60(sp)
    lw      s4, 64(sp)
    lw      s5, 68(sp)
    lw      s6, 72(sp)
    lw      s7, 76(sp)
    lw      s8, 80(sp)
    lw      s9, 84(sp)
    lw      s10, 88(sp)
    lw      s11, 92(sp)
    lw      t3, 96(sp)
    lw      t4, 100(sp)
    lw      t5, 104(sp)
    lw      t6, 108(sp)

    addi    sp, sp, 128
    mret

    .size sertos_riscv_trap_handler, .-sertos_riscv_trap_handler
    .end
