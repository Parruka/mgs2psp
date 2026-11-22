#!/bin/sh
# mk_pgo.sh <build dir> <header dir> <extra make args...>: PSP build with the genf.txt flags and the vp9 compiled VU0; appends "make rc N"
B=$1; W=$2; shift 2
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
MG=/mnt/project-files/mgs2
cd $B || exit 1
GENF="$(cat genf.txt)"
make -f $MG/psp/Makefile -j${J:-1} W=$W "$@" GENF="$GENF" RTX="-DMGS2_GEN_VU='\"${VP:-/home/claude/psp/vp9}/gen_vu.h\"' -DVP_FAST $RTXX" > make.log 2>&1
echo "make rc $?" >> make.log
