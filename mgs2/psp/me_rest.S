/* me_rest.S - v30me5 (2026-10-26; psp/v30me5.sh, psp_harness.md "Build v30me5"): the Media Engine rests when it has nothing to mix.
   Ffm chose "Build it" (decision card 2026-10-26 20:43) after the polling ME's PSP switched itself off 2 of 2 times on the cheap
   battery (v30 runs 1 and 2) while the ME-off runs finished; the new register writes are the three of that card: the ME's interrupt
   enable 0xBC300008 and flags 0xBC300000 (here, on the ME) and the main CPU's interrupt to the ME 0xBC100044 (psp/me/hle_me_mgs2.c).
   The pattern is mcidclan's (psp-media-engine-custom-core me-core-custom.c and me-soft-interrupt, MIT, credited in
   third_party/ps2rc/NOTICE.md and psp_harness.md): the handler's address in CP0 $25, bit 31 of 0xBC300008 enabled, every flag of
   0xBC300000 cleared, IE + IM2 (0x401) in Status; the handler acknowledges by writing bit 31 to 0xBC300000 and reading the flags until
   it is clear.  HALT (.word 0x70000000, as psp/me/hle_me_mgs2.c's stops) waits for an interrupt (upspd wiki); this is its first use
   with a wake-up.  Runs on the Media Engine only (hle_me.h's rules); Status keeps IE off everywhere but in me_rest's window.  Words in
   the ME's boot memory after psp/me/hle_me_mgs2.c's progress words (0xBFC00400, [0..11]; k_start clears [0..15] at every start):
     [12] 0x430  the ME's interrupt flags 0xBC300000 as the handler found them at the last wake
     [13] 0x434  the handler's wakes (counts on)
     [14] 0x438  every flag bit the handler has found
     [15] 0x43c  the EPC of the last wake (without its kseg0 bit)
     [16] 0x440  the handler's save of $at
     [17] 0x444  the exception vector before me_rest_init (hle_me_mgs2.c's hle_me_fault, kseg0): every other exception goes there, so
                 it is recorded and the ME halts for good, as before */
    .set push
    .set noreorder
    .set noat
    .text
    .align 2

/* void me_rest_init(void): once, from me_mix_work, before the first rest */
    .globl me_rest_init
    .ent me_rest_init
me_rest_init:
    lui   $t2, 0xBFC0
    mfc0  $t0, $25
    sw    $t0, 0x444($t2)         /* [17] the vector the reset stub set */
    sync
    la    $t0, me_rest_vec
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
    .end me_rest_init

/* void me_rest(void): Status + IE + IM2, HALT until the main CPU's interrupt, Status back as it was (IE off).  An interrupt taken
   anywhere from me_rest_begin to me_rest_win_last (one already pending when IE goes on, or the one that ends the HALT) comes back to
   me_rest_end through me_rest_vec, which keeps $t0 (it uses $k0, $k1 and $at, saved).  The handler's address is only 4-byte aligned,
   as mcidclan's are (aligned(4) in me-core-custom.c and me-soft-interrupt): a 16-byte alignment here would move v30's code. */
    .globl me_rest
    .globl me_rest_begin
    .globl me_rest_halt
    .globl me_rest_end
    .globl me_rest_win_end
    .globl me_rest_win_last
    .ent me_rest
me_rest:
    mfc0  $t0, $12
    ori   $t1, $t0, 0x401
me_rest_begin:
    mtc0  $t1, $12                /* interrupts on: IE + IM2 */
    sync
    nop
    nop
me_rest_halt:
    .word 0x70000000              /* HALT */
    nop
    nop
    nop
me_rest_end:
    mtc0  $t0, $12                /* interrupts off */
    sync
    nop
    nop
me_rest_win_end:
    jr    $ra
me_rest_win_last:
    nop                           /* the jump's delay slot, the window's last word: on the Allegrex an exception there may give
                                     this address as EPC (mcidclan's handler steps an EPC with Cause.BD back by 4) */
    .end me_rest

/* the ME's exception vector from me_rest_init on (CP0 $25, at its kseg0 alias) */
    .align 2
    .globl me_rest_vec
    .ent me_rest_vec
me_rest_vec:
    mfc0  $k0, $13                /* Cause */
    andi  $k1, $k0, 0x7c          /* ExcCode: 0 = an interrupt */
    bne   $k1, $zero, 9f
    andi  $k1, $k0, 0x400         /* (delay slot) IP2: the interrupt from the main CPU */
    beq   $k1, $zero, 9f
    nop
    mfc0  $k0, $14                /* EPC without its kseg0 bit: the link address */
    sll   $k0, $k0, 1
    srl   $k0, $k0, 1
    la    $k1, me_rest_begin
    sltu  $k1, $k0, $k1
    bne   $k1, $zero, 9f          /* before me_rest's window */
    nop
    la    $k1, me_rest_win_last
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
    la    $k0, me_rest_end
    lui   $k1, 0x8000
    or    $k0, $k0, $k1
    mtc0  $k0, $14                /* back to me_rest_end (kseg0) */
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
    .end me_rest_vec
    .set pop
