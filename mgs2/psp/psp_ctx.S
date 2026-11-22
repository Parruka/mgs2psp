# Coroutine switch for the PSP build (ucontext.h): saves / restores the callee-saved state of the Allegrex o32/eabi convention
# (s0..s7, fp, gp, sp, ra, f20..f31, FCR31).  s7 is the pinned guest memory base: the same value in every context.
	.set noreorder
	.set noat
	.text
	.globl psp_getcontext
	.ent psp_getcontext
psp_getcontext:
	sw $16, 0($4)
	sw $17, 4($4)
	sw $18, 8($4)
	sw $19, 12($4)
	sw $20, 16($4)
	sw $21, 20($4)
	sw $22, 24($4)
	sw $23, 28($4)
	sw $30, 32($4)
	sw $28, 36($4)
	sw $29, 40($4)
	sw $31, 44($4)
	swc1 $f20, 48($4)
	swc1 $f21, 52($4)
	swc1 $f22, 56($4)
	swc1 $f23, 60($4)
	swc1 $f24, 64($4)
	swc1 $f25, 68($4)
	swc1 $f26, 72($4)
	swc1 $f27, 76($4)
	swc1 $f28, 80($4)
	swc1 $f29, 84($4)
	swc1 $f30, 88($4)
	swc1 $f31, 92($4)
	cfc1 $8, $31
	sw $8, 96($4)
	jr $31
	move $2, $0
	.end psp_getcontext

	.globl psp_setcontext
	.ent psp_setcontext
psp_setcontext:
	lw $16, 0($4)
	lw $17, 4($4)
	lw $18, 8($4)
	lw $19, 12($4)
	lw $20, 16($4)
	lw $21, 20($4)
	lw $22, 24($4)
	lw $23, 28($4)
	lw $30, 32($4)
	lw $28, 36($4)
	lw $29, 40($4)
	lw $31, 44($4)
	lwc1 $f20, 48($4)
	lwc1 $f21, 52($4)
	lwc1 $f22, 56($4)
	lwc1 $f23, 60($4)
	lwc1 $f24, 64($4)
	lwc1 $f25, 68($4)
	lwc1 $f26, 72($4)
	lwc1 $f27, 76($4)
	lwc1 $f28, 80($4)
	lwc1 $f29, 84($4)
	lwc1 $f30, 88($4)
	lwc1 $f31, 92($4)
	lw $8, 96($4)
	ctc1 $8, $31
	jr $31
	move $2, $0
	.end psp_setcontext

	.globl psp_swapcontext
	.ent psp_swapcontext
psp_swapcontext:
	sw $16, 0($4)
	sw $17, 4($4)
	sw $18, 8($4)
	sw $19, 12($4)
	sw $20, 16($4)
	sw $21, 20($4)
	sw $22, 24($4)
	sw $23, 28($4)
	sw $30, 32($4)
	sw $28, 36($4)
	sw $29, 40($4)
	sw $31, 44($4)
	swc1 $f20, 48($4)
	swc1 $f21, 52($4)
	swc1 $f22, 56($4)
	swc1 $f23, 60($4)
	swc1 $f24, 64($4)
	swc1 $f25, 68($4)
	swc1 $f26, 72($4)
	swc1 $f27, 76($4)
	swc1 $f28, 80($4)
	swc1 $f29, 84($4)
	swc1 $f30, 88($4)
	swc1 $f31, 92($4)
	cfc1 $8, $31
	sw $8, 96($4)
	b psp_setcontext
	move $4, $5
	.end psp_swapcontext

# a context made by psp_makecontext starts here: s0 = its function, s1 = its context; when the function returns, uc_link takes over
	.globl psp_uc_tramp
	.ent psp_uc_tramp
psp_uc_tramp:
	jalr $16
	nop
	lw $4, 108($17)
	beq $4, $0, 1f
	nop
	j psp_setcontext
	nop
1:	j psp_uc_end
	nop
	.end psp_uc_tramp
