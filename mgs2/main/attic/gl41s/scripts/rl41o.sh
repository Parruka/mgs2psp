#!/bin/sh
# rl41o.sh <boot_rt.o> <out folder> <order folder> (gl41s; rl41.sh with another ORDER): gl39's final link (build_gl39/rl39p.sh: RELINK of build_gl39/u, ORDER ord/final, MESEC and
# XLD = padld's me_sec_pad2.ld / ro_pad2.ld with ME_PAD 0x15c0, XL_PAD 0x210, RO_PAD 0x1c40, as build_gl39/psp/make.log) of build_gl39/u's
# objects with this boot_rt.o in place of u's.  psp.sh's RELINK branch writes in <out folder> only; its gates run (xme, mesec_chk39, me_call_chk).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s; R=/home/claude/main/build_gl39; O=$1; D=$2; OR=$3
[ -e $D ] && { echo "rl41.sh: $D exists"; exit 1; }
U=$S/u_$(basename $D); mkdir -p $U || exit 1
for f in $R/u/*.o $R/u/genf.txt $R/u/ge_xme_me.dis $R/u/xme_me_chk.out; do ln -sf $f $U/; done; rm -f $U/boot_rt.o; cp $O $U/boot_rt.o || exit 1
cd $R/m && MESEC=$R/padld/me_sec_pad2.ld XLD="-Wl,--defsym,ME_PAD=0x15c0 -Wl,--defsym,XL_PAD=0x210 -Wl,-T,$R/padld/ro_pad2.ld -Wl,--defsym,RO_PAD=0x1c40" \
  RELINK=$U PGO=use GCDA=$R/gcda ORDER=$OR PSPD=$D TITLE="MGS2 harness gl39" nice -n 10 sh scripts/psp.sh > $D.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -n 12 $D.out | cut -c1-200; exit $r
