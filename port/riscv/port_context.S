/*
 * port_context.s - RISC-V RV32I/IMC/IMAC/IMAFC Context Switch Trap Handler
 * Part of SertOS Kernel.
 *
 * Integer context frame layout (rv32i/imc/imac, no FPU):
 *   offsets 0..108:   ra, t0..t2, s0..s1, a0..a7, s2..s11, t3..t6
 *   offset  112:      mepc
 *   offset  116:      mstatus
 *   offsets 120..124: pad (total 128 bytes, 16-byte aligned)
 *
 * FPU-extended context frame layout (rv32imafc, __riscv_flen == 32):
 *   offsets 0..116:   same as above
 *   offsets 120..244: f0..f31  (32 x 4 bytes)
 *   offset  248:      fcsr
 *   offset  252:      pad (total 256 bytes, 16-byte aligned)
 */

    .section .text
    .align 4
    .global sertos_riscv_trap_handler
    .type sertos_riscv_trap_handler, @function
    .global sertos_port_start_first_task
    .type sertos_port_start_first_task, @function

/* --------------------------------------------------------------------------
 * Frame sizing and FPU capability selection
 * -------------------------------------------------------------------------- */
#if defined(__riscv_flen) && (__riscv_flen > 0)
    #define FRAME_SIZE  256
    #define HAS_FPU     1
#else
    #define FRAME_SIZE  128
    #define HAS_FPU     0
#endif

/* --------------------------------------------------------------------------
 * sertos_port_start_first_task
 * -------------------------------------------------------------------------- */
sertos_port_start_first_task:
    la      t0, sertos_riscv_trap_handler
    csrw    mtvec, t0

    la      t0, sertos_current_tcb
    lw      a0, 0(t0)
    lw      sp, 0(a0)

    lw      t0, 112(sp)
    csrw    mepc, t0
    lw      t1, 116(sp)
    csrw    mstatus, t1

#if HAS_FPU
    /* Ensure FPU is active in hardware before issuing FP instructions */
    li      t2, (3 << 13)
    csrs    mstatus, t2

    lw      t2, 248(sp)
    fscsr   t2
    flw     f0,  120(sp)
    flw     f1,  124(sp)
    flw     f2,  128(sp)
    flw     f3,  132(sp)
    flw     f4,  136(sp)
    flw     f5,  140(sp)
    flw     f6,  144(sp)
    flw     f7,  148(sp)
    flw     f8,  152(sp)
    flw     f9,  156(sp)
    flw     f10, 160(sp)
    flw     f11, 164(sp)
    flw     f12, 168(sp)
    flw     f13, 172(sp)
    flw     f14, 176(sp)
    flw     f15, 180(sp)
    flw     f16, 184(sp)
    flw     f17, 188(sp)
    flw     f18, 192(sp)
    flw     f19, 196(sp)
    flw     f20, 200(sp)
    flw     f21, 204(sp)
    flw     f22, 208(sp)
    flw     f23, 212(sp)
    flw     f24, 216(sp)
    flw     f25, 220(sp)
    flw     f26, 224(sp)
    flw     f27, 228(sp)
    flw     f28, 232(sp)
    flw     f29, 236(sp)
    flw     f30, 240(sp)
    flw     f31, 244(sp)
#endif

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

    addi    sp, sp, FRAME_SIZE
    mret
    .size sertos_port_start_first_task, .-sertos_port_start_first_task

/* --------------------------------------------------------------------------
 * sertos_riscv_trap_handler
 * -------------------------------------------------------------------------- */
sertos_riscv_trap_handler:
    addi    sp, sp, -FRAME_SIZE

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
    csrr    t1, mstatus
    csrr    t2, mcause
    bgez    t2, handle_exception

    /* Interrupt path: save mepc / mstatus */
    sw      t0, 112(sp)
    sw      t1, 116(sp)

#if HAS_FPU
    /* Ensure FPU is accessible before read */
    li      t3, (3 << 13)
    csrs    mstatus, t3

    fsw     f0,  120(sp)
    fsw     f1,  124(sp)
    fsw     f2,  128(sp)
    fsw     f3,  132(sp)
    fsw     f4,  136(sp)
    fsw     f5,  140(sp)
    fsw     f6,  144(sp)
    fsw     f7,  148(sp)
    fsw     f8,  152(sp)
    fsw     f9,  156(sp)
    fsw     f10, 160(sp)
    fsw     f11, 164(sp)
    fsw     f12, 168(sp)
    fsw     f13, 172(sp)
    fsw     f14, 176(sp)
    fsw     f15, 180(sp)
    fsw     f16, 184(sp)
    fsw     f17, 188(sp)
    fsw     f18, 192(sp)
    fsw     f19, 196(sp)
    fsw     f20, 200(sp)
    fsw     f21, 204(sp)
    fsw     f22, 208(sp)
    fsw     f23, 212(sp)
    fsw     f24, 216(sp)
    fsw     f25, 220(sp)
    fsw     f26, 224(sp)
    fsw     f27, 228(sp)
    fsw     f28, 232(sp)
    fsw     f29, 236(sp)
    fsw     f30, 240(sp)
    fsw     f31, 244(sp)
    frcsr   t3
    sw      t3,  248(sp)
#endif

    li      t3, 0x80000007          /* Machine timer interrupt */
    bne     t2, t3, do_switch
    call    sertos_port_riscv_timer_handler
    j       do_switch

handle_exception:
    /* Check for Machine ecall (11) or User ecall (8) */
    li      t3, 11
    beq     t2, t3, handle_ecall
    li      t3, 8
    beq     t2, t3, handle_ecall

trap_fault_loop:
    wfi
    j       trap_fault_loop

handle_ecall:
    /* Advance mepc past ecall instruction (+4), save state */
    addi    t0, t0, 4
    sw      t0, 112(sp)
    sw      t1, 116(sp)

#if HAS_FPU
    /* Ensure FPU is accessible before read */
    li      t3, (3 << 13)
    csrs    mstatus, t3

    fsw     f0,  120(sp)
    fsw     f1,  124(sp)
    fsw     f2,  128(sp)
    fsw     f3,  132(sp)
    fsw     f4,  136(sp)
    fsw     f5,  140(sp)
    fsw     f6,  144(sp)
    fsw     f7,  148(sp)
    fsw     f8,  152(sp)
    fsw     f9,  156(sp)
    fsw     f10, 160(sp)
    fsw     f11, 164(sp)
    fsw     f12, 168(sp)
    fsw     f13, 172(sp)
    fsw     f14, 176(sp)
    fsw     f15, 180(sp)
    fsw     f16, 184(sp)
    fsw     f17, 188(sp)
    fsw     f18, 192(sp)
    fsw     f19, 196(sp)
    fsw     f20, 200(sp)
    fsw     f21, 204(sp)
    fsw     f22, 208(sp)
    fsw     f23, 212(sp)
    fsw     f24, 216(sp)
    fsw     f25, 220(sp)
    fsw     f26, 224(sp)
    fsw     f27, 228(sp)
    fsw     f28, 232(sp)
    fsw     f29, 236(sp)
    fsw     f30, 240(sp)
    fsw     f31, 244(sp)
    frcsr   t3
    sw      t3,  248(sp)
#endif

do_switch:
    /* Store sp in current tcb->stack_ptr */
    la      t0, sertos_current_tcb
    lw      t1, 0(t0)
    beqz    t1, restore_riscv_task
    sw      sp, 0(t1)

restore_riscv_task:
    call    sertos_scheduler_perform_switch

    /* Load new sp from next tcb->stack_ptr (a0 = next TCB) */
    lw      sp, 0(a0)

    lw      t0, 112(sp)
    csrw    mepc, t0
    lw      t1, 116(sp)
    csrw    mstatus, t1

#if HAS_FPU
    /* Ensure FPU is active in hardware before issuing FP instructions */
    li      t2, (3 << 13)
    csrs    mstatus, t2

    lw      t2, 248(sp)
    fscsr   t2
    flw     f0,  120(sp)
    flw     f1,  124(sp)
    flw     f2,  128(sp)
    flw     f3,  132(sp)
    flw     f4,  136(sp)
    flw     f5,  140(sp)
    flw     f6,  144(sp)
    flw     f7,  148(sp)
    flw     f8,  152(sp)
    flw     f9,  156(sp)
    flw     f10, 160(sp)
    flw     f11, 164(sp)
    flw     f12, 168(sp)
    flw     f13, 172(sp)
    flw     f14, 176(sp)
    flw     f15, 180(sp)
    flw     f16, 184(sp)
    flw     f17, 188(sp)
    flw     f18, 192(sp)
    flw     f19, 196(sp)
    flw     f20, 200(sp)
    flw     f21, 204(sp)
    flw     f22, 208(sp)
    flw     f23, 212(sp)
    flw     f24, 216(sp)
    flw     f25, 220(sp)
    flw     f26, 224(sp)
    flw     f27, 228(sp)
    flw     f28, 232(sp)
    flw     f29, 236(sp)
    flw     f30, 240(sp)
    flw     f31, 244(sp)
#endif

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

    addi    sp, sp, FRAME_SIZE
    mret

    .size sertos_riscv_trap_handler, .-sertos_riscv_trap_handler
    .end
	