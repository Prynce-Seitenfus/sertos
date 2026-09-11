/*
 * port_context.s - ARM Cortex-M33 PendSV Context Switch Assembly Routine
 * Part of SertOS Kernel. Strict GNU Assembler Syntax.
 */

    .syntax unified
    .thumb
    .text
    .align 2
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .global sertos_port_start_first_task
    .type sertos_port_start_first_task, %function

sertos_port_start_first_task:
    /* Load address of currently active TCB */
    ldr     r0, =sertos_current_tcb
    ldr     r1, [r0]
    ldr     r0, [r1]               /* r0 = tcb->stack_ptr */

    /* Set ARMv8-M Process Stack Pointer Limit (PSPLIM) from tcb->stack_limit (offset 12) */
    ldr     r2, [r1, #12]
    msr     psplim, r2

    /* Pop software-managed registers R4-R11 and EXC_RETURN */
    ldmia   r0!, {r4-r11, lr}

    /* Set Process Stack Pointer to start of hardware exception frame */
    msr     psp, r0
    isb

    /* Switch to unprivileged thread mode using PSP */
    movs    r0, #3
    msr     control, r0
    isb

    /* Enable interrupts and return to task entry */
    cpsie   i
    bx      lr
    .size sertos_port_start_first_task, .-sertos_port_start_first_task

PendSV_Handler:
    /* Disable interrupts during context save/restore */
    cpsid   i

    /* Read Process Stack Pointer */
    mrs     r0, psp
    isb

    /* Push software-managed registers R4-R11 and EXC_RETURN to thread stack */
    stmdb   r0!, {r4-r11, lr}

    /* Load current TCB address */
    ldr     r1, =sertos_current_tcb
    ldr     r2, [r1]
    cmp     r2, #0
    beq     restore_next_task

    /* Save updated top of stack into tcb->stack_ptr (first member of TCB) */
    str     r0, [r2]

restore_next_task:
    /* Perform context switch step: updates states and sertos_current_tcb, returns next TCB in R0 */
    push    {lr}
    bl      sertos_scheduler_perform_switch
    pop     {lr}

    /* Load new task top of stack from tcb->stack_ptr */
    ldr     r2, [r0]

    /* Set ARMv8-M Process Stack Pointer Limit (PSPLIM) from tcb->stack_limit (offset 12) */
    ldr     r3, [r0, #12]
    msr     psplim, r3

    /* Pop software-managed registers R4-R11 and EXC_RETURN */
    ldmia   r2!, {r4-r11, lr}

    /* Update Process Stack Pointer */
    msr     psp, r2
    isb

    /* Re-enable interrupts and return from exception */
    cpsie   i
    bx      lr

    .size PendSV_Handler, .-PendSV_Handler
    .end
