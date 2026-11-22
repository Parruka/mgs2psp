/* exc3b.S - crash catcher 3b (2026-10-28, task #73; psp/exc3b.c): psp/exc3.S (gl4's catcher 3) with one change, the way the stopped
   thread is given back to the system before interrupts come back on.  gl4's self-test on Ffm's PSP (2026-10-28 13:40, photo 13:47)
   drew the record, set Status.IE (ic 1) and then nothing ran: no writer line, no battery.txt line, the PSP off about 10 s later.  Catcher
   3 had left the thread in kernel mode on this file's stack in user memory, with no system-call context, spinning; catcher 1 (v26me3),
   which went on in user mode on the same kind of stack, also ended with the PSP off.  Now, as PSPLink does it (psplinkusb
   psplink/exception_asm.S and exception.c psplinkTrap, BSD licence, Copyright (c) 2005 James F): after the record (drawn as before, with
   interrupts off, on this file's stack) the thread moves to its own kernel stack (control register 15), builds the 32-byte system-call
   context the kernel's own system-call entry builds there (Status, EPC, sp, ra, k1, FCSR, v0, link; uOFW interruptman start.S
   syscallExcepHandler) and puts it in the thread's slot (control register 16), sets k1 = 0 (a kernel caller), turns interrupts on and
   calls the system's sceKernelSleepThread (psp/exc3b.c finds it at the start: mgs2_exc_sleep[0]).  To the system the thread is then a
   thread asleep inside a system call, as PSPLink leaves a crashed thread; the writer thread saves the record.  If the sleep ever
   returns, its count and result go to mgs2_exc_sleep[1] and [2] and the thread sleeps again.  No system call is made with the
   syscall instruction (the kernel breaks on one from kernel mode: syscallExcepHandler's loc_00000E98).
   Unchanged from exc3.S: the registers, the record and the second exception's spin.  exc3.S's own notes follow. */
/* exc3.S's own notes: crash catcher 3 (2026-10-27, task #71; psp/exc3.c): psp/v26me4_exc.S with two additions.  It also stores the kernel's
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
	la      $v0, mgs2_exc_ints      # mgs2_exc_screen's answer: 1 = the thread goes back to the system, asleep
	lw      $v1, 0($v0)
	beq     $v1, $0, 9f
	nop
	la      $s0, mgs2_exc_sleep     # {the system's sceKernelSleepThread, returns from it, its last result}
	lw      $t9, 0($s0)
	beq     $t9, $0, 9f             # (not found at the start: mgs2_exc_screen then answers 0)
	nop
	la      $v0, mgs2_exc_regs
	lw      $t0, 156($v0)           # Status at the exception (the kernel's copy, control register 2)
	andi    $t2, $t0, 0x18
	beq     $t2, $0, 9f             # from user mode only (mgs2_exc_screen checks it too)
	cfc0    $t3, $15                # the thread's kernel stack: its top, unused while the thread ran in user mode
	beq     $t3, $0, 9f
	addiu   $t3, $t3, -32           # the system-call context, laid out as the kernel's system-call entry lays it out:
	sw      $t0, 0($t3)             #   Status
	lw      $t0, 152($v0)
	sw      $t0, 4($t3)             #   EPC (the kernel's copy, control register 0)
	lw      $t0, 116($v0)
	sw      $t0, 8($t3)             #   the thread's sp
	lw      $t0, 124($v0)
	sw      $t0, 12($t3)            #   its ra
	lw      $t0, 108($v0)
	sw      $t0, 16($t3)            #   its k1
	sw      $0, 20($t3)             #   FCSR (PSPLink: 0)
	sw      $0, 24($t3)             #   v0 (PSPLink: 0)
	sw      $0, 28($t3)             #   PSPLink's link to an earlier context
	cfc0    $t0, $16                # the thread's slot for its system-call context
	beq     $t0, $0, 1f
	move    $sp, $t3                # (delay slot) from here on: the thread's kernel stack, as in a system call
	lw      $t1, 0($t0)             # (none from user mode; PSPLink keeps it)
	sw      $t1, 28($sp)
	sw      $sp, 0($t0)
1:	move    $k1, $0                 # a kernel caller (PSPLink: "placate the kernel")
	mfc0    $t0, $12
	ori     $t0, $t0, 1             # Status.IE: interrupts on (the interrupt enable, mfic, was 1: mgs2_exc_screen checks it)
	mtc0    $t0, $12
	nop
	nop
2:	lw      $t9, 0($s0)             # the system's sceKernelSleepThread: this thread waits there; the others run
	jalr    $t9
	nop
	lw      $t0, 4($s0)             # it returned (a wakeup): count it, keep the result, sleep again
	addiu   $t0, $t0, 1
	sw      $t0, 4($s0)
	b       2b
	sw      $v0, 8($s0)
9:	b       9b                      # catcher 2's end: interrupts off, the record stays on the screen
	nop
	.end    mgs2_exc_handler

	.bss
	.align  4
	.global mgs2_exc_stack
mgs2_exc_stack:
	.space  16384
