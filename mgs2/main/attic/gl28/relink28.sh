#!/bin/sh
# relink28.sh TREE NAME [ROOT] (gl28, 2026-11-19; relink25.sh + the ME's rest object and the battery unit): ROOT (default
# /home/claude/main/build_NAME)/stage = gl23's stage objects + ge_xme.o, ge_xme_me.o, ge_xlme.o, xl_psp.o recompiled from TREE (gl20
# gcda3 profiles, the u2 / gl22 compile lines, as relink25.sh) + v26me_bat.o compiled from TREE with psp.sh's line + xme_rest.o
# assembled from TREE/psp/xme_rest.S as psp.sh does (only when the tree has it); then psp.sh relinks into ROOT/psp (it links xme_rest.o
# when the stage has it and runs its gate: ge_xme_me.o's names, tools/xme_rest_chk.py).  With TREE=/home/claude/t27 (no xme_rest.S) the
# objects and the EBOOT must be gl27's (the recipe's check).
set -e
T=$1; N=$2; M=/home/claude/main; R=${3:-$M/build_$N}
[ -n "$T" ] && [ -n "$N" ] || { echo "relink28.sh TREE NAME [ROOT]"; exit 1; }
[ -e $R ] && { echo "relink28.sh: $R exists: look at it first"; exit 1; }
mkdir -p $R/stage
cp $M/build_gl23/stage/*.o $M/build_gl23/stage/genf.txt $M/build_gl23/stage/ge_xme_me.dis $M/build_gl23/stage/xme_me_chk.out $R/stage/
cd $R/stage; rm -f -- ge_xme.o ge_xme_me.o ge_xlme.o xl_psp.o v26me_bat.o
cp $M/build_gl20/gcda3/ge_xlme.gcda $M/build_gl20/gcda3/ge_xme.gcda $M/build_gl20/gcda3/xl_psp.gcda .
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
SDK=/home/claude/pspsdk/pspdev/psp/sdk
for o in ge_xme ge_xme_me ge_xlme; do
  c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //; s#/home/claude/main/ge/#$T/ge/#; s#/home/claude/main/psp/xme_me.c#$T/psp/xme_me.c#")
  echo "$o <- $(echo "$c" | grep -o "$T/[^ ]*")"
  sh -c "$c" > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
c=$(head -1 $M/build_gl22/stage/cc_xl_psp.o.log | sed "s/^cmd: //; s#/home/claude/main/psp/xl_psp.c#$T/psp/xl_psp.c#")
echo "xl_psp <- $(echo "$c" | grep -o "$T/[^ ]*")"
sh -c "$c" > cc_xl_psp.o.log 2>&1 || { echo FAIL xl_psp; head -20 cc_xl_psp.o.log; exit 1; }
# gl28: the battery unit with psp.sh's own line (scripts/psp.sh: BASEF + the catcher's defines), from the tree
BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
c="psp-gcc $BASEF -fno-merge-constants -DME_DIAG3 -DME_DIAG4 -DME_DIAG5 -DME_EXC -DME_EXC_HOLD -DSYS_DIAG -I$SDK/include -c $T/psp/v26me_bat.c -o v26me_bat.o"
echo "v26me_bat <- $T/psp/v26me_bat.c"
{ echo "cmd: $c"; sh -c "$c"; } > cc_v26me_bat.o.log 2>&1 || { echo FAIL v26me_bat; head -20 cc_v26me_bat.o.log; exit 1; }
# gl28: the ME's rest, as psp.sh assembles it (v30me5.sh's me_rest.o line), only when the tree has it
if [ -f $T/psp/xme_rest.S ]; then
  c="psp-gcc -c $T/psp/xme_rest.S -o xme_rest.o"; echo "xme_rest <- $T/psp/xme_rest.S"
  { echo "cmd: $c"; sh -c "$c"; } > cc_xme_rest.o.log 2>&1 || { echo FAIL xme_rest; head -20 cc_xme_rest.o.log; exit 1; }
  python3 -I $M/tools/xme_rest_chk.py xme_rest.o /mnt/project-files/mgs2/psp/me_rest.S > xme_rest_chk.out 2>&1 || { cat xme_rest_chk.out; exit 1; }
  tail -1 xme_rest_chk.out
fi
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xme.o
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xme.o
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xme_me.o
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xme_me.o
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xlme.o
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xlme.o
echo "xme_me undef: $(psp-nm -u ge_xme_me.o | awk '{ print $2 }' | tr '\n' ' ')"
psp-objdump -d ge_xme_me.o > ge_xme_me.dis
python3 -I $M/tools/xme_me_chk.py ge_xme_me.dis > xme_me_chk.out 2>&1 || { cat xme_me_chk.out; exit 1; }
tail -1 xme_me_chk.out
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o
rm -f -- ge_xme.gcda ge_xlme.gcda xl_psp.gcda
cat cc_ge_xme.o.log cc_ge_xme_me.o.log cc_xl_psp.o.log cc_v26me_bat.o.log | head -20
cd $M
set +e
RELINK=$R/stage PGO=use GCDA=$M/build_gl20/gcda3 ORDER=$M/build_gl20/psp PSPD=$R/psp TITLE="MGS2 harness mainline $N" sh scripts/psp.sh > $R/psp.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -16 $R/psp.out; grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more' $R/psp.out | head
exit $r
