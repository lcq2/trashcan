.equ UART_BASE, 0xC2000000

.option norvc
.section .text.init,"ax",@progbits
.globl reset_vector

reset_vector:
    j do_reset

trap_vector:

    mret

do_reset:
    li x1, 0
    li x2, 0
    li x3, 0
    li x4, 0
    li x5, 0
    li x6, 0
    li x7, 0
    li x8, 0
    li x9, 0
    li x10, 0
    li x11, 0
    li x12, 0
    li x13, 0
    li x14, 0
    li x15, 0
    li x16, 0
    li x17, 0
    li x18, 0
    li x19, 0
    li x20, 0
    li x21, 0
    li x22, 0
    li x23, 0
    li x24, 0
    li x25, 0
    li x26, 0
    li x27, 0
    li x28, 0
    li x29, 0
    li x30, 0
    li x31, 0

    # setup exception handler
    la t0, trap_vector
    csrw mtvec, t0

	li a0, UART_BASE
	li t0, 0
	sw t0, 0x10(a0)
	li t0, 0x70001
	sw t0, 0x08(a0)
	sw t0, 0x0C(a0)
	la a0, hello_msg
	jal puts
	jal echo
1:	j 1b

puts:
	li a2, UART_BASE
	li t0, 1
	slli t0, t0, 31
1:	lbu a1, (a0)
	beqz a1, 3f
2:	lw a3, 0(a2)
	and a3, a3, t0
	bnez a3, 2b
	sw a1, (a2)
	addi a0, a0, 1
	j 1b
3:  ret

echo:
	li a2, UART_BASE
	li t0, 1
	slli t0, t0, 31
1:
	lw a0, 4(a2)
	and a1, a0, t0
	bnez a1, 1b
	sw a0, (a2)
	j 1b

hello_msg:
	.string "Hello from bootloader\n"

.bss
.align 12

.data
.align 6


trap_table:
    .word 0x1111
    .word 0x2222

