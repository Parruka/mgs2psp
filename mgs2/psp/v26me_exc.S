/* v26me_exc.S - v26me3 (2026-10-24): the main CPU's default exception handler of the crash catcher (psp/v26me_exc.c registers it in
   kernel mode through ARK-4's KUBridge, as tt_port's psp_bench did).  After PSPSDK's pspexception_asm.S (BSD licence, Copyright (c) 2005
   James Forshaw; tt_port/src/psp_bench/exc.S is that file renamed): the kernel's exception manager has kept the thread's v0 / v1 in
   COP0 control registers 4 / 5; the handler stores the registers in mgs2_exc_regs and returns (eret) into mgs2_exc_trap, in the
   faulting thread and mode, on the catcher's own 16 KB stack (the thread's stack may be what failed).  Only the first exception is
   taken (mgs2_exc_taken): a later one parks its thread in mgs2_exc_park (sleeps for good, on a small stack of its own) and keeps the
   first record.  Layout of mgs2_exc_regs (psp/v26me_exc.c ExcRegs): r0-r31, Status, LO, HI, BadVAddr, Cause, EPC. */
	.set noreorder
	.set noat

	.text
	.global mgs2_exc_handler
	.ent    mgs2_exc_handler
mgs2_exc_handler:
	nop
	nop
	la      $v0, mgs2_exc_taken
	lw      $v1, 0($v0)
	bne     $v1, $0, 2f
	li      $v1, 1
	sw      $v1, 0($v0)
	la      $v0, mgs2_exc_regs
	sw      $0, 0($v0)
	sw      $1, 4($v0)
	cfc0    $1, $4                  # the thread's v0
	sw      $1, 8($v0)
	cfc0    $1, $5                  # the thread's v1
	sw      $1, 12($v0)
	sw      $4, 16($v0)
	sw      $5, 20($v0)
	sw      $6, 24($v0)
	sw      $7, 28($v0)
	sw      $8, 32($v0)
	sw      $9, 36($v0)
	sw      $10, 40($v0)
	sw      $11, 44($v0)
	sw      $12, 48($v0)
	sw      $13, 52($v0)
	sw      $14, 56($v0)
	sw      $15, 60($v0)
	sw      $16, 64($v0)
	sw      $17, 68($v0)
	sw      $18, 72($v0)
	sw      $19, 76($v0)
	sw      $20, 80($v0)
	sw      $21, 84($v0)
	sw      $22, 88($v0)
	sw      $23, 92($v0)
	sw      $24, 96($v0)
	sw      $25, 100($v0)
	sw      $26, 104($v0)
	sw      $27, 108($v0)
	sw      $28, 112($v0)
	sw      $29, 116($v0)
	sw      $30, 120($v0)
	sw      $31, 124($v0)
	mfc0    $v1, $12                # Status
	sw      $v1, 128($v0)
	mflo    $v1
	sw      $v1, 132($v0)
	mfhi    $v1
	sw      $v1, 136($v0)
	mfc0    $v1, $8                 # BadVAddr
	sw      $v1, 140($v0)
	mfc0    $v1, $13                # Cause
	sw      $v1, 144($v0)
	mfc0    $v1, $14                # EPC
	sw      $v1, 148($v0)
	la      $v0, mgs2_exc_trap
	mtc0    $v0, $14
	la      $sp, mgs2_exc_stack + 16384 - 32
	nop
	nop
	eret
	nop
	nop
2:	la      $v0, mgs2_exc_park      # a later exception: the first record stays
	mtc0    $v0, $14
	la      $sp, mgs2_exc_pstack + 1024 - 32
	nop
	nop
	eret
	nop
	nop
	.end    mgs2_exc_handler

	.global mgs2_exc_park
	.ent    mgs2_exc_park
mgs2_exc_park:
1:	jal     sceKernelSleepThread
	nop
	b       1b
	nop
	.end    mgs2_exc_park

	.bss
	.align  4
	.global mgs2_exc_stack
mgs2_exc_stack:
	.space  16384
	.global mgs2_exc_pstack
mgs2_exc_pstack:
	.space  1024
