#!/bin/sh
# rl42.sh [folder, default psp] (gl42: build_gl41/rl41.sh with build_gl42; gl41: rl40.sh; ORD= the order folder, default build_gl42/ord/s1) (gl40, as build_gl39/rl39p.sh): the use build's objects (build_gl40/u, U= another) linked again with
# the code order of gl40's search (build_gl40/ord/final, ORD= another) into build_gl40/<folder>.  No compile: psp.sh's RELINK branch
# (its gates run).  Data pads: padld/me_sec_pad2.ld (ME_PAD, XL_PAD) and padld/ro_pad2.ld (RO_PAD) as gl39's final link, plus
# padld/end_pad.ld (END_PAD, NOBITS after .me_bss) when END_PAD is set; padld/me_sec_pad3.ld (ME_PRE before .me_bss) when ME_PRE is set.  Defaults: gl39's 0x15c0 / 0x210 / 0x1c40, no END_PAD.
R=/home/claude/main/build_gl42; D=${1:-psp}; cd $R/m || exit 1
[ -e $R/$D ] && { echo "rl42.sh: $R/$D exists: look at it first"; exit 1; }
P=$R/padld; X="-Wl,--defsym,ME_PAD=${ME_PAD:-0x15c0} -Wl,--defsym,XL_PAD=${XL_PAD:-0x210} -Wl,-T,$P/ro_pad2.ld -Wl,--defsym,RO_PAD=${RO_PAD:-0x1c40}"
[ -n "$END_PAD" ] && X="$X -Wl,-T,$P/end_pad.ld -Wl,--defsym,END_PAD=$END_PAD"
MS=$P/me_sec_pad2.ld; [ -n "$ME_PRE" ] && { MS=$P/me_sec_pad3.ld; X="$X -Wl,--defsym,ME_PRE=$ME_PRE"; }
echo "rl42.sh: $D MESEC=$MS XLD=$X"
MESEC=$MS XLD="$X" RELINK=${U:-$R/u} PGO=use GCDA=$R/gcda ORDER=${ORD:-$R/ord/s1} PSPD=$R/$D TITLE="MGS2 harness gl42" nice -n 19 sh scripts/psp.sh > $R/$D.out 2>&1
r=$?; echo "psp.sh rc $r"; cp ${U:-$R/u}/gen_compiled.md5 $R/$D/; tail -14 $R/$D.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $R/$D.out | head; exit $r
