#!/bin/sh
# rl41p.sh <boot_rt.o> <out folder> <order folder> (gl41s): rl41o.sh with RO_PAD (default gl39's 0x1c40) and MESEC (default gl39's
# padld/me_sec_pad2.ld) taken from the environment, XLDX = more link flags, for the layout check (gl41 levers measured in a layout close to gl39's).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s; R=/home/claude/main/build_gl39; O=$1; D=$2; OR=$3
[ -e $D ] && { echo "rl41p.sh: $D exists"; exit 1; }
U=$S/u_$(basename $D); mkdir -p $U || exit 1
for f in $R/u/*.o $R/u/genf.txt $R/u/ge_xme_me.dis $R/u/xme_me_chk.out; do ln -sf $f $U/; done; rm -f $U/boot_rt.o; cp $O $U/boot_rt.o || exit 1
cd $R/m && MESEC=${MESEC:-$R/padld/me_sec_pad2.ld} XLD="-Wl,--defsym,ME_PAD=0x15c0 -Wl,--defsym,XL_PAD=0x210 -Wl,-T,$R/padld/ro_pad2.ld -Wl,--defsym,RO_PAD=${RO_PAD:-0x1c40}${XLDX:+ $XLDX}" \
  RELINK=$U PGO=use GCDA=$R/gcda ORDER=$OR PSPD=$D TITLE="MGS2 harness gl39" nice -n 10 sh scripts/psp.sh > $D.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -n 12 $D.out | cut -c1-200; exit $r
