/* exc3.S - crash catcher 3 (2026-10-27, task #71; psp/exc3.c): psp/v26me4_exc.S with two additions.  It also stores the kernel's
   interrupt-context word (control register 13, as uOFW's sceKernelIsIntrContext reads it) and the Allegrex's interrupt enable (mfic, as
   uOFW's sceKernelCpuSuspendIntr reads it), and after mgs2_exc_screen it turns interrupts back on when mgs2_exc_screen says so
   (mgs2_exc_ints: the cfg's MGS2_EXC=3, a user-mode thread that had interrupts on and was not in an interrupt handler).  Why: Ffm's PSP
   switched itself off a while after catcher 2's record of gl2d's crash (Ffm 2026-10-27 21:10), so a crash nobody watched leaves no
   record; with interrupts on, the other threads run again and psp/exc3.c's writer thread puts the record in crash.txt.  The CPU stays in
   kernel mode at this code's kernel address (the handler is registered at its kernel alias, and branches and jal keep that segment):
   an interrupt taken in kernel mode at a user address is the interrupt manager's break 0x3FF (v30me6b, mgs2_me_audio).  The interrupt
   manager saves this context in the thread's own block (control register 14), not on this stack, and the thread scheduler can then
   run the other threads; this thread spins on at its priority until the writer thread suspends it.  A second exception spins at once
   with the exception level set (interrupts off): the first record stays, as before.  Layout of mgs2_exc_regs (psp/exc3.c ExcRegs): as
   v26me4's, then control register 13 and the interrupt enable. */
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
	cfc0    $v1, $13                # the interrupt-context word: 0 outside interrupt handlers
	sw      $v1, 172($v0)
	mfic    $v1, $0                 # the interrupt enable: 0 when the kernel had interrupts suspended
	sw      $v1, 176($v0)
	mfc0    $t0, $12                # from here on: kernel mode, not at exception level, interrupts off (PSPLink clears the same
	li      $t1, 0xFFFFFFE4         # bits before its C code); with interrupts off no thread runs and nothing draws over the record
	and     $t0, $t0, $t1           # while it is drawn; an exception in the screen code is then an ordinary one, which comes back
	mtc0    $t0, $12                # here and spins (mgs2_exc_taken)
	nop
	nop
	la      $sp, mgs2_exc_stack + 16384 - 32
	jal     mgs2_exc_screen
	nop
	la      $v0, mgs2_exc_ints      # mgs2_exc_screen's answer: 1 = interrupts back on
	lw      $v1, 0($v0)
	beq     $v1, $0, 9f
	nop
	mfc0    $t0, $12
	ori     $t0, $t0, 1             # Status.IE: interrupts on; still kernel mode, still this kernel address
	mtc0    $t0, $12
	nop
	nop
9:	b       9b                      # the end: the record stays on the screen (and, with interrupts on, the writer thread saves it)
	nop
	.end    mgs2_exc_handler

	.bss
	.align  4
	.global mgs2_exc_stack
mgs2_exc_stack:
	.space  16384
