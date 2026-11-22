/* xme_rest.S - gl28 (2026-11-19; MGS2_ME_REST, power_design.md section 5 "R1" with power_critique.md section B): the picture loop's
   Media Engine rests in HALT through the main CPU's turns.  This file is v30me5's psp/me_rest.S (2026-10-26, mgs2/psp/me_rest.S; Ffm
   chose "Build it" on the decision card of 2026-10-26 20:43; 76,548 rests and 0 lost wakes on Ffm's PSP in v30me5 run 2) with its
   symbols renamed me_rest* -> xme_rest* and nothing else changed: the same instructions in the same order (tools/xme_rest_chk.py
   compares them with v30me5's file by mnemonic and operand at every build).  The pattern is mcidclan's (psp-media-engine-custom-core
   me-core-custom.c and me-soft-interrupt, MIT, credited in third_party/ps2rc/NOTICE.md and psp_harness.md).
   Register writes, the three of that card and no other: the ME's interrupt enable 0xBC300008 = 0x80000000 and flags 0xBC300000 =
   0xFFFFFFFF once (xme_rest_init), the acknowledgement 0x80000000 to 0xBC300000 at each wake (xme_rest_vec), all on the ME; the main
   CPU's interrupt to the ME 0xBC100044 = 1 is psp/me/hle_me_mgs2.c's hle_me_kick (kernel mode).  CP0 writes on the ME only: $25 (the
   exception vector), $12 (Status: IE + IM2 only inside xme_rest's window), $14 (EPC in the handler).  HALT is .word 0x70000000.
   Called only from psp/xme_me.c (xme_me_main: xme_rest_init when the main CPU asked for the rest at the start; xme_me_wait: xme_rest
   in a main-CPU turn, after the handshake).  Words in the ME's boot memory after psp/me/hle_me_mgs2.c's progress words (0xBFC00400,
   [0..11]; k_start clears [0..15] at every start), as v30me5:
     [12] 0x430  the ME's interrupt flags 0xBC300000 as the handler found them at the last wake
     [13] 0x434  the handler's wakes (counts on)
     [14] 0x438  every flag bit the handler has found
     [15] 0x43c  the EPC of the last wake (without its kseg0 bit)
     [16] 0x440  the handler's save of $at
     [17] 0x444  the exception vector before xme_rest_init (hle_me_mgs2.c's hle_me_fault, kseg0): every other exception goes there,
                 so it is recorded and the ME halts for good, as before (the main CPU's take-back finds it: hle_me_exception) */
    .set push
    .set noreorder
    .set noat
    .text
    .align 2

/* void xme_rest_init(void): once, from xme_me_main (psp/xme_me.c; v30me5: from me_mix_work), before the first rest */
    .globl xme_rest_init
    .ent xme_rest_init
xme_rest_init:
    lui   $t2, 0xBFC0
    mfc0  $t0, $25
    sw    $t0, 0x444($t2)         /* [17] the vector the reset stub set */
    sync
    la    $t0, xme_rest_vec
    lui   $t1, 0x8000
    or    $t0, $t0, $t1           /* its kseg0 alias, as the stub gives hle_me_fault's */
    mtc0  $t0, $25
    sync
    lui   $t1, 0xBC30
    lui   $t0, 0x8000
    sw    $t0, 8($t1)             /* 0xBC300008 = 0x80000000: the main CPU's interrupt enabled at the ME's interrupt controller */
    sync
    li    $t0, -1
    sw    $t0, 0($t1)             /* 0xBC300000 = 0xFFFFFFFF: every flag cleared */
    sync
    jr    $ra
    nop
    .end xme_rest_init

/* void xme_rest(void): Status + IE + IM2, HALT until the main CPU's interrupt, Status back as it was (IE off).  An interrupt taken
   anywhere from xme_rest_begin to xme_rest_win_last (one already pending when IE goes on, or the one that ends the HALT) comes back to
   xme_rest_end through xme_rest_vec, which keeps $t0 (it uses $k0, $k1 and $at, saved).  The handler's address is only 4-byte aligned,
   as mcidclan's are (aligned(4) in me-core-custom.c and me-soft-interrupt): a 16-byte alignment here would move v30's code. */
    .globl xme_rest
    .globl xme_rest_begin
    .globl xme_rest_halt
    .globl xme_rest_end
    .globl xme_rest_win_end
    .globl xme_rest_win_last
    .ent xme_rest
xme_rest:
    mfc0  $t0, $12
    ori   $t1, $t0, 0x401
xme_rest_begin:
    mtc0  $t1, $12                /* interrupts on: IE + IM2 */
    sync
    nop
    nop
xme_rest_halt:
    .word 0x70000000              /* HALT */
    nop
    nop
    nop
xme_rest_end:
    mtc0  $t0, $12                /* interrupts off */
    sync
    nop
    nop
xme_rest_win_end:
    jr    $ra
xme_rest_win_last:
    nop                           /* the jump's delay slot, the window's last word: on the Allegrex an exception there may give
                                     this address as EPC (mcidclan's handler steps an EPC with Cause.BD back by 4) */
    .end xme_rest

/* the ME's exception vector from xme_rest_init on (CP0 $25, at its kseg0 alias) */
    .align 2
    .globl xme_rest_vec
    .ent xme_rest_vec
xme_rest_vec:
    mfc0  $k0, $13                /* Cause */
    andi  $k1, $k0, 0x7c          /* ExcCode: 0 = an interrupt */
    bne   $k1, $zero, 9f
    andi  $k1, $k0, 0x400         /* (delay slot) IP2: the interrupt from the main CPU */
    beq   $k1, $zero, 9f
    nop
    mfc0  $k0, $14                /* EPC without its kseg0 bit: the link address */
    sll   $k0, $k0, 1
    srl   $k0, $k0, 1
    la    $k1, xme_rest_begin
    sltu  $k1, $k0, $k1
    bne   $k1, $zero, 9f          /* before xme_rest's window */
    nop
    la    $k1, xme_rest_win_last
    sltu  $k1, $k1, $k0
    bne   $k1, $zero, 9f          /* after it */
    nop
    lui   $k1, 0xBFC0
    sw    $k0, 0x43c($k1)         /* [15] EPC of this wake */
    sw    $at, 0x440($k1)         /* [16] $at saved */
    lui   $k0, 0xBC30
    lw    $at, 0($k0)             /* the flags as found */
    sw    $at, 0x430($k1)         /* [12] */
    lw    $k0, 0x438($k1)
    or    $k0, $k0, $at
    sw    $k0, 0x438($k1)         /* [14] every bit found */
    lw    $k0, 0x434($k1)
    addiu $k0, $k0, 1
    sw    $k0, 0x434($k1)         /* [13] wakes */
    sync
    lui   $k0, 0xBC30
    lui   $k1, 0x8000
    sw    $k1, 0($k0)             /* acknowledge: bit 31 of the flags written (mcidclan) */
    sync
    li    $at, 4096               /* then read until bit 31 is clear (mcidclan waits for the whole word: here bit 31, at most 4,096 reads) */
1:  lw    $k1, 0($k0)
    srl   $k1, $k1, 31
    beq   $k1, $zero, 2f
    addiu $at, $at, -1
    bne   $at, $zero, 1b
    nop
2:  lui   $k1, 0xBFC0
    lw    $at, 0x440($k1)         /* $at back */
    mfc0  $k0, $12
    ori   $k0, $k0, 1
    xori  $k0, $k0, 1             /* Status: IE off (eret clears EXL) */
    mtc0  $k0, $12
    la    $k0, xme_rest_end
    lui   $k1, 0x8000
    or    $k0, $k0, $k1
    mtc0  $k0, $14                /* back to xme_rest_end (kseg0) */
    sync
    nop
    nop
    eret
    nop
    nop
    nop
    nop
    nop
    nop
    nop
9:  lui   $k0, 0xBFC0
    lw    $k0, 0x444($k0)         /* [17]: hle_me_fault records Cause / EPC / BadVAddr and halts */
    jr    $k0
    nop
    .end xme_rest_vec
    .set pop
