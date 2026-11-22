/* v26me4_exc.S - v26me4 (2026-10-24): the crash catcher's default exception handler, second form (psp/v26me4_exc.c registers it in
   kernel mode through ARK-4's KUBridge, as psp/v26me_exc.c did). v26me3's handler (psp/v26me_exc.S, after PSPSDK's
   pspexception_asm.S) returned (eret) into a trap that made system calls; its self-test switched Ffm's PSP off (psp_harness.md "Build
   v26me3", run 1). This one never returns: in the exception itself it stores the registers, sets kernel mode with interrupts off and
   the exception level cleared (the bits PSPLink clears before its C code), calls mgs2_exc_screen (psp/v26me4_exc.c: the record on the
   screen with plain VRAM writes, no system call) on a stack of its own, then spins; the PSP stays on with the record until the power
   switch. As PSPLink's handler (psplinkusb psplink/exception_asm.S, BSD licence, James F), which reads the thread's v0 / v1 from COP0
   control registers 4 / 5 and its EPC, Status and Cause from control registers 0, 2 and 3: both those and the COP0 registers' own
   EPC, Status and Cause are recorded (which pair holds the thread's values on this firmware is one of the things the self-test
   shows). A second exception that reaches the handler (one in the screen code) spins at once and keeps the first record. Layout of
   mgs2_exc_regs (psp/v26me4_exc.c ExcRegs): r0-r31, Status, LO, HI, BadVAddr, Cause, EPC (COP0's), then control registers 0 (EPC), 2
   (Status), 3 (Cause), 15 and 16. */
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
	bne     $v1, $0, 9f             # a second exception: the first record stays
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
	cfc0    $v1, $0                 # the kernel's copies: EPC
	sw      $v1, 152($v0)
	cfc0    $v1, $2                 # Status
	sw      $v1, 156($v0)
	cfc0    $v1, $3                 # Cause
	sw      $v1, 160($v0)
	cfc0    $v1, $15
	sw      $v1, 164($v0)
	cfc0    $v1, $16
	sw      $v1, 168($v0)
	mfc0    $t0, $12                # from here on: kernel mode, not at exception level, interrupts off (PSPLink clears the same
	li      $t1, 0xFFFFFFE4         # bits before its C code); with interrupts off no thread runs again and nothing draws over
	and     $t0, $t0, $t1           # the record; an exception in the screen code is then an ordinary one, which comes back here
	mtc0    $t0, $12                # and spins (mgs2_exc_taken)
	nop
	nop
	la      $sp, mgs2_exc_stack + 16384 - 32
	jal     mgs2_exc_screen
	nop
9:	b       9b                      # the end: the record stays on the screen
	nop
	.end    mgs2_exc_handler

	.bss
	.align  4
	.global mgs2_exc_stack
mgs2_exc_stack:
	.space  16384
