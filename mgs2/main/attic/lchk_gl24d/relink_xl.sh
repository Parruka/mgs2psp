#!/bin/sh
# relink_xl.sh TREE NAME BASE: build_BASE/stage's objects, only xl_psp.o compiled again from TREE/psp/xl_psp.c, relinked as build_NAME
set -e
T=$1; N=$2; BASE=$3; M=/home/claude/main
cd $M; mkdir -p build_$N/stage
cp build_$BASE/stage/*.o build_$BASE/stage/genf.txt build_$N/stage/
[ -f build_$BASE/stage/ge_xme_me.dis ] && cp build_$BASE/stage/ge_xme_me.dis build_$BASE/stage/xme_me_chk.out build_$N/stage/
cd build_$N/stage; rm -f -- xl_psp.o
cp ../../build_gl20/gcda3/xl_psp.gcda .
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
c=$(head -1 ../../build_gl22/stage/cc_xl_psp.o.log | sed "s/^cmd: //; s#/home/claude/main/psp/xl_psp.c#$T/psp/xl_psp.c#"); sh -c "$c" > cc_xl_psp.o.log 2>&1 || exit 1
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o; rm -f -- xl_psp.gcda
cd $M; RELINK=$M/build_$N/stage PGO=use GCDA=$M/build_gl20/gcda3 ORDER=$M/build_gl20/psp PSPD=$M/build_$N/psp TITLE="MGS2 harness mainline $N" sh scripts/psp.sh > build_$N/psp.out 2>&1
