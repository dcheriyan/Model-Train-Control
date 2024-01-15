//Part of this file was provided. The sections we added are noted with NEW.

// https://www.rpi4os.com/part1-bootstrapping/

// ***************************************
// SCTLR_EL1, System Control Register (EL1)
// Architecture Reference Manual Section D17.2.118
// ***************************************
#define SCTLR_RESERVED (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
#define USER_MASK_ACCESS (1 << 9)
#define SCTLR_WFE_WFI_ENABLED (1 << 18) | (1 << 16)
#define SCTLR_VALUE_MMU_DISABLED (SCTLR_RESERVED | USER_MASK_ACCESS | SCTLR_WFE_WFI_ENABLED)

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2)
// Architecture Reference Manual Section D17.2.48
// ***************************************
#define HCR_RW (1 << 31)

// ***************************************
// SPSR_EL2, Saved Program Status Register (EL2)
// Architecture Reference Manual Section C5.2.19
// ***************************************
#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL1      (5 << 0)
#define SPSR_VALUE (SPSR_MASK_ALL | SPSR_EL1)


// ensure the linker puts this at the start of the kernel image
.section ".text.boot"
.global _start
_start:
    // check processor ID is zero (executing on main core), else loop
//    mrs  x0, mpidr_el1
//    and  x0, x0, #3
//    cbnz x0, exit

    // are we already in EL1?
    mrs  x1, CurrentEL
    and  x1, x1, #8
    cbz  x1, el1_entry

    // otherwise, switch to EL1 by fake exception to return from
    ldr x2, =HCR_RW
    msr hcr_el2, x2

    ldr x3, =SPSR_VALUE
    msr spsr_el2, x3

    adr x4, el1_entry
    msr elr_el2, x4

    eret // -> el1_entry

el1_entry:
    // configure processor and mmu
    ldr x2, =SCTLR_VALUE_MMU_DISABLED
    msr sctlr_el1, x2


    // Setup exception vector table. NEW
    adr x0, vector_table
    msr VBAR_EL1, x0

    // mask-out exceptions at EL1
    msr DAIFSet, #0b1111
    // initialize SP
    msr SPSel, #1
    ldr     x0, =kernel_stackend
    mov     sp, x0
    // Jump to our main() routine in C/C++
    bl      kmain

    // use watchdog to reboot
    mov x2, 36
    mov x0, 28
    movk  x2, 0xfe10, lsl 16
    movk  x0, 0xfe10, lsl 16
    mov w3, 1
    mov w1, 32
    movk  w3, 0x5a00, lsl 16
    str w3, [x2]
    movk  w1, 0x5a00, lsl 16
    str w1, [x0]

exit:
    wfi
    b    exit

//EVERYTHING AFTER THIS IS NEW
.global restoreContext
restoreContext:
    sub sp, sp, #8
    str x8, [sp]
    sub sp, sp, #8
    str x9, [sp]
    sub sp, sp, #8
    str x10, [sp]
    sub sp, sp, #8
    str x11, [sp]
    sub sp, sp, #8
    str x12, [sp]
    sub sp, sp, #8
    str x13, [sp]
    sub sp, sp, #8
    str x14, [sp]
    sub sp, sp, #8
    str x15, [sp]
    sub sp, sp, #8
    str x16, [sp]
    sub sp, sp, #8
    str x17, [sp]
    sub sp, sp, #8
    str x18, [sp]
    sub sp, sp, #8
    str x19, [sp]
    sub sp, sp, #8
    str x20, [sp]
    sub sp, sp, #8
    str x21, [sp]
    sub sp, sp, #8
    str x22, [sp]
    sub sp, sp, #8
    str x23, [sp]
    sub sp, sp, #8
    str x24, [sp]
    sub sp, sp, #8
    str x25, [sp]
    sub sp, sp, #8
    str x26, [sp]
    sub sp, sp, #8
    str x27, [sp]
    sub sp, sp, #8
    str x28, [sp]
    sub sp, sp, #8
    str x29, [sp]
    sub sp, sp, #8
    str x30, [sp]
    
    msr sp_el0, x1
    msr elr_el1, x2
    msr spsr_el1, x3
    ldr x1, [x0, #8]
    ldr x2, [x0, #16]
    ldr x3, [x0, #24]
    ldr x4, [x0, #32]
    ldr x5, [x0, #40]
    ldr x6, [x0, #48]
    ldr x7, [x0, #56]
    ldr x8, [x0, #64]
    ldr x9, [x0, #72]
    ldr x10, [x0, #80]
    ldr x11, [x0, #88]
    ldr x12, [x0, #96]
    ldr x13, [x0, #104]
    ldr x14, [x0, #112]
    ldr x15, [x0, #120]
    ldr x16, [x0, #128]
    ldr x17, [x0, #136]
    ldr x18, [x0, #144]
    ldr x19, [x0, #152]
    ldr x20, [x0, #160]
    ldr x21, [x0, #168]
    ldr x22, [x0, #176]
    ldr x23, [x0, #184]
    ldr x24, [x0, #192]
    ldr x25, [x0, #200]
    ldr x26, [x0, #208]
    ldr x27, [x0, #216]
    ldr x28, [x0, #224]
    ldr x29, [x0, #232]
    ldr x30, [x0, #240]
    ldr x0, [x0]
    eret

// Synchronous
my_svc_context_switch:
    //switch stack pointers
    msr SPSel, #0
    //Make sure we don't corrupt our stack
    sub sp, sp, #256
    //store registers
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    stp x30, x0, [ sp, #240]
    //Revert our changes to the stack pointer
    add sp, sp, #256
    //Change the spsel back to el 1
    msr SPSel, #1

    ldr x30, [sp]
    add sp, sp, #8
    ldr x29, [sp]
    add sp, sp, #8
    ldr x28, [sp]
    add sp, sp, #8
    ldr x27, [sp]
    add sp, sp, #8
    ldr x26, [sp]
    add sp, sp, #8
    ldr x25, [sp]
    add sp, sp, #8
    ldr x24, [sp]
    add sp, sp, #8
    ldr x23, [sp]
    add sp, sp, #8
    ldr x22, [sp]
    add sp, sp, #8
    ldr x21, [sp]
    add sp, sp, #8
    ldr x20, [sp]
    add sp, sp, #8
    ldr x19, [sp]
    add sp, sp, #8
    ldr x18, [sp]
    add sp, sp, #8
    ldr x17, [sp]
    add sp, sp, #8
    ldr x16, [sp]
    add sp, sp, #8
    ldr x15, [sp]
    add sp, sp, #8
    ldr x14, [sp]
    add sp, sp, #8
    ldr x13, [sp]
    add sp, sp, #8
    ldr x12, [sp]
    add sp, sp, #8
    ldr x11, [sp]
    add sp, sp, #8
    ldr x10, [sp]
    add sp, sp, #8
    ldr x9, [sp]
    add sp, sp, #8
    ldr x8, [sp]
    add sp, sp, #8

    ret
    //21 < 32 instructions to store registers on top of the stack\

.text
.global vector_table
.balign 0x800
vector_table:
    .rept 0x400
    .byte 0
    .endr
.balign 0x400

b my_svc_context_switch
.balign 0x80

// same thing as svc
irq_switch:

sub sp, sp, #8
str x0, [sp]
sub sp, sp, #8
str x1, [sp]
ldr x0, =setInterrupt
mov x1, #1
str x1, [x0]
ldr x1, [sp]
add sp, sp, #8
ldr x0, [sp]
add sp, sp, #8

b my_svc_context_switch
.balign 0x80

.balign 0x80
vector_table_cont:
    .rept 0x300
    .byte 0
    .endr

.section ".bss"
.balign 16
kernel_stack:
    .rept 0x800000
    .byte 0
    .endr
kernel_stackend:
.balign 0x1000000
stack_1:
    .rept 0x100000
    .byte 0
    .endr
stack_1end:
stack_2:
    .rept 0x100000
    .byte 0
    .endr
stack_2end:
stack_3:
    .rept 0x100000
    .byte 0
    .endr
stack_3end:
stack_4:
    .rept 0x100000
    .byte 0
    .endr
stack_4end:
stack_5:
    .rept 0x100000
    .byte 0
    .endr
stack_5end:
stack_6:
    .rept 0x100000
    .byte 0
    .endr
stack_6end:
stack_7:
    .rept 0x100000
    .byte 0
    .endr
stack_7end:
stack_8:
    .rept 0x100000
    .byte 0
    .endr
stack_8end:
stack_9:
    .rept 0x100000
    .byte 0
    .endr
stack_9end:
stack_10:
    .rept 0x100000
    .byte 0
    .endr
stack_10end:
stack_11:
    .rept 0x100000
    .byte 0
    .endr
stack_11end:
stack_12:
    .rept 0x100000
    .byte 0
    .endr
stack_12end:
stack_13:
    .rept 0x100000
    .byte 0
    .endr
stack_13end:
stack_14:
    .rept 0x100000
    .byte 0
    .endr
stack_14end:
stack_15:
    .rept 0x100000
    .byte 0
    .endr
stack_15end:
stack_16:
    .rept 0x100000
    .byte 0
    .endr
stack_16end:
