/*
 * port_context.s - ARM Cortex-M23 PendSV Context Switch Assembly Routine
 * Part of SertOS Kernel. Strict GNU Assembler Syntax for ARMv8-M Baseline.
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

    /* Set ARMv8-M Process Stack Pointer Limit (PSPLIM) from tcb->stack_limit (offset 12) */
    ldr     r2, [r1, #12]
    msr     psplim, r2

    /* Restore high registers R8-R11 */
    ldmia   r0!, {r4-r7}
    mov     r8, r4
    mov     r9, r5
    mov     r10, r6
    mov     r11, r7

    /* Restore low registers R4-R7 and EXC_RETURN */
    ldmia   r0!, {r4-r7}
    ldr     r3, [r0]
    adds    r0, r0, #4
    mov     lr, r3

    /* Set PSP to hardware frame */
    msr     psp, r0

    /* Switch to Privileged Thread mode using PSP */
    movs    r1, #2
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

    /* Save EXC_RETURN and low registers R4-R7 */
    mov     r3, lr
    subs    r0, r0, #4
    str     r3, [r0]
    subs    r0, r0, #16
    stmia   r0!, {r4-r7}
    subs    r0, r0, #16

    /* Save high registers R8-R11 */
    mov     r4, r8
    mov     r5, r9
    mov     r6, r10
    mov     r7, r11
    subs    r0, r0, #16
    stmia   r0!, {r4-r7}
    subs    r0, r0, #16

    ldr     r1, =sertos_current_tcb
    ldr     r2, [r1]
    cmp     r2, #0
    beq     restore_m23_task

    str     r0, [r2]

restore_m23_task:
    /* Perform context switch step: updates states and sertos_current_tcb, returns next TCB in R0 */
    push    {r1, lr}
    bl      sertos_scheduler_perform_switch
    pop     {r1, r2}
    mov     lr, r2

    /* Set ARMv8-M Process Stack Pointer Limit (PSPLIM) from tcb->stack_limit (offset 12) */
    ldr     r3, [r0, #12]
    msr     psplim, r3

    ldr     r0, [r0]               /* r0 = tcb->stack_ptr */

    /* Restore high registers R8-R11 */
    ldmia   r0!, {r4-r7}
    mov     r8, r4
    mov     r9, r5
    mov     r10, r6
    mov     r11, r7

    /* Restore low registers R4-R7 and EXC_RETURN */
    ldmia   r0!, {r4-r7}
    ldr     r3, [r0]
    adds    r0, r0, #4
    mov     lr, r3

    msr     psp, r0

    cpsie   i
    bx      lr

    .size PendSV_Handler, .-PendSV_Handler
    .end
