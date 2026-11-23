#!/bin/sh
# rl_np.sh (st1g, emulator only): st1g/u's objects linked again as pspb.sh does but WITHOUT gl40's two NOBITS pads (ME_PRE 0x1d00 before
# .me_bss, END_PAD 0x1a00 after it; RO_PAD, ME_PAD and XL_PAD kept) into st1g/psp_np: the image ends 13.75 KB lower, for the test of
# newlib's heap at vb 10,850 (the ME picture heap's 64 -> 128 KB resize).  Pads change cache placement only.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1g; R=/home/claude/main/build_gl40; MM=$S/m; P=$S/psp_np
. /home/claude/main/scripts/env.sh
[ -e $P ] && { echo "rl_np.sh: $P exists"; exit 1; }
cd $MM || exit 1
PD=$R/padld; X="-Wl,--defsym,ME_PAD=0x15c0 -Wl,--defsym,XL_PAD=0x210 -Wl,-T,$PD/ro_pad2.ld -Wl,--defsym,RO_PAD=0x260"
MESEC=$PD/me_sec_pad2.ld XLD="$X" RELINK=$S/u PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$P MGS=$MM/build_mg TITLE="MGS2 harness gl40 nge1 np (emulator only)" nice -n 19 sh $MM/scripts/psp.sh > $S/psp_np.out 2>&1
r=$?; echo "psp.sh rc $r"; grep -i 'mesec\|FAIL\|xme_rest_chk\|me_call' $S/psp_np.out | head; exit $r
