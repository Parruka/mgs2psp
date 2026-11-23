#!/bin/sh
# rl39.sh [folder, default psp] (gl39's final build, as build_gl38/rl38b.sh): the use build's objects (build_gl39/u) linked again with
# the code order of gl39's search (build_gl39/ord/final) into build_gl39/<folder>.  No compile: psp.sh's RELINK branch (its gates run).
R=/home/claude/main/build_gl39; D=${1:-psp}; cd $R/m || exit 1
[ -e $R/$D ] && { echo "rl39.sh: $R/$D exists: look at it first"; exit 1; }
RELINK=${U:-$R/u} PGO=use GCDA=$R/gcda ORDER=${ORD:-$R/ord/final} PSPD=$R/$D TITLE="MGS2 harness gl39" sh scripts/psp.sh > $R/$D.out 2>&1
r=$?; echo "psp.sh rc $r"; cp ${U:-$R/u}/gen_compiled.md5 $R/$D/; tail -14 $R/$D.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $R/$D.out | head; exit $r
