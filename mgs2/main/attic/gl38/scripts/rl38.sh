#!/bin/sh
# rl38.sh (gl35's final build, as gl33's rl33.sh): the use build's objects (build_gl38/u: every object compiled from /home/claude/t35 through the link farm
# build_gl38/m with gl35's own profiles build_gl38/gcda) linked again with the code order of gl35's search (build_gl38/ord/final:
# layout_elf.ld, layout_w00a.ld, layout_w01a.ld) into build_gl38/psp.  No compile: psp.sh's RELINK branch (its gates run: xme_rest_chk,
# xme_me_chk's kept output, mesec_chk, me_call_chk).  gen.o is u's, already behind the wrapper; gen_compiled.md5 copied for pgochk.sh.
R=/home/claude/main/build_gl38; D=${1:-psp}; cd $R/m || exit 1
[ -e $R/$D ] && { echo "rl38.sh: $R/$D exists: look at it first"; exit 1; }
RELINK=$R/u PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$R/$D TITLE="MGS2 harness gl38" sh scripts/psp.sh > $R/$D.out 2>&1
r=$?; echo "psp.sh rc $r"; cp $R/u/gen_compiled.md5 $R/$D/; tail -14 $R/$D.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $R/$D.out | head; exit $r
