/*
 * port_context.s - ARM Cortex-M4 SVC / PendSV Context Switch Assembly Routine
 * Part of SertOS Kernel. Strict GNU Assembler Syntax.
 */

    .syntax unified
    .thumb
    .text
    .align 2
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .global SVC_Handler
    .type SVC_Handler, %function
    .global sertos_port_start_first_task
    .type sertos_port_start_first_task, %function

SVC_Handler:
    ldr     r0, =sertos_current_tcb
    ldr     r1, [r0]
    ldr     r0, [r1]               /* r0 = tcb->stack_ptr */

    ldmia   r0!, {r4-r11, lr}      /* Pop software registers and EXC_RETURN */
    msr     psp, r0                /* Set PSP to hardware exception frame */
    isb

    movs    r1, #2                 /* Privileged Thread mode using PSP */
    msr     control, r1
    isb

    cpsie   i
    bx      lr
    .size SVC_Handler, .-SVC_Handler

sertos_port_start_first_task:
    cpsie   i
    svc     0
1:  b       1b
    .size sertos_port_start_first_task, .-sertos_port_start_first_task

PendSV_Handler:
    cpsid   i

    mrs     r0, psp
    isb

    /* Push software-managed registers R4-R11 and EXC_RETURN */
    stmdb   r0!, {r4-r11, lr}

    ldr     r1, =sertos_current_tcb
    ldr     r2, [r1]
    cmp     r2, #0
    beq     restore_m4_task

    /* Store updated top of stack to tcb->stack_ptr */
    str     r0, [r2]

restore_m4_task:
    /* Perform context switch step: updates states and sertos_current_tcb, returns next TCB in R0 */
    push    {lr}
    bl      sertos_scheduler_perform_switch
    pop     {lr}

    ldr     r2, [r0]               /* Load incoming tcb->stack_ptr */

    /* Pop software registers R4-R11 and EXC_RETURN */
    ldmia   r2!, {r4-r11, lr}

    msr     psp, r2
    isb

    cpsie   i
    bx      lr

    .size PendSV_Handler, .-PendSV_Handler
    .end
