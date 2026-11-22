#!/bin/sh
# rl34.sh (gl34's final build, as gl33's rl33.sh): the use build's objects (build_gl34/u: every object compiled from /home/claude/t34 through the link farm
# build_gl34/m with gl34's own profiles build_gl34/gcda) linked again with the code order of gl34's search (build_gl34/ord/final:
# layout_elf.ld, layout_w00a.ld, layout_w01a.ld) into build_gl34/psp.  No compile: psp.sh's RELINK branch (its gates run: xme_rest_chk,
# xme_me_chk's kept output, mesec_chk, me_call_chk).  gen.o is u's, already behind the wrapper; gen_compiled.md5 copied for pgochk.sh.
R=/home/claude/main/build_gl34; D=${1:-psp}; cd $R/m || exit 1
[ -e $R/$D ] && { echo "rl34.sh: $R/$D exists: look at it first"; exit 1; }
RELINK=$R/u PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$R/$D TITLE="MGS2 harness gl34" sh scripts/psp.sh > $R/$D.out 2>&1
r=$?; echo "psp.sh rc $r"; cp $R/u/gen_compiled.md5 $R/$D/; tail -14 $R/$D.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $R/$D.out | head; exit $r
