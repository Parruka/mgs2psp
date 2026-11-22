#!/bin/sh
# relink.sh TREE NAME: build_<NAME>/stage = gl23's stage objects + ge_xme.o, ge_xme_me.o, xl_psp.o recompiled from TREE (gl20 gcda3
# profiles, the u2 / gl22 compile lines), then psp.sh relinks into build_<NAME>/psp.  (gl24's recipe, 2026-11-19)
set -e
T=$1; N=$2; M=/home/claude/main
cd $M; mkdir -p build_$N/stage
cp build_gl23/stage/*.o build_gl23/stage/genf.txt build_gl23/stage/ge_xme_me.dis build_gl23/stage/xme_me_chk.out build_$N/stage/
cd build_$N/stage; rm -f -- ge_xme.o ge_xme_me.o xl_psp.o
cp ../../build_gl20/gcda3/ge_xme.gcda ../../build_gl20/gcda3/xl_psp.gcda .
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
for o in ge_xme ge_xme_me; do
  c=$(head -1 ../../build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //; s#/home/claude/main/ge/#$T/ge/#; s#/home/claude/main/psp/xme_me.c#$T/psp/xme_me.c#")
  echo "$o <- $(echo "$c" | grep -o "$T/[^ ]*")"
  sh -c "$c" > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
c=$(head -1 ../../build_gl22/stage/cc_xl_psp.o.log | sed "s/^cmd: //; s#/home/claude/main/psp/xl_psp.c#$T/psp/xl_psp.c#")
echo "xl_psp <- $(echo "$c" | grep -o "$T/[^ ]*")"
sh -c "$c" > cc_xl_psp.o.log 2>&1 || { echo FAIL xl_psp; head -20 cc_xl_psp.o.log; exit 1; }
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xme.o
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xme.o
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xme_me.o
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xme_me.o
echo "xme_me undef: $(psp-nm -u ge_xme_me.o | awk '{ print $2 }' | tr '\n' ' ')"
psp-objdump -d ge_xme_me.o > ge_xme_me.dis
python3 -I $M/tools/xme_me_chk.py ge_xme_me.dis > xme_me_chk.out 2>&1 || true; tail -1 xme_me_chk.out
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o
rm -f -- ge_xme.gcda xl_psp.gcda
cat cc_ge_xme.o.log cc_ge_xme_me.o.log cc_xl_psp.o.log | head -20
cd $M
RELINK=$M/build_$N/stage PGO=use GCDA=$M/build_gl20/gcda3 ORDER=$M/build_gl20/psp PSPD=$M/build_$N/psp TITLE="MGS2 harness mainline $N" sh scripts/psp.sh > build_$N/psp.out 2>&1
echo "psp.sh rc $?"; tail -12 build_$N/psp.out; grep -i 'mesec\|FAIL\|warn' build_$N/psp.out | head
