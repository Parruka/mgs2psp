#!/bin/sh
# pc41.sh <ov folder> <out folder> [extra flags] (gl41s): gl39's boot_rt.o compile (build_gl39/u/cc_boot_rt.o.log: psp.sh's line, PGO=use
# with build_gl39/gcda/boot_rt.gcda) with -I<ov> first and MGS2_GEN_VU = <ov>/gen_vu.h: the overlay's headers (l41_overlay.py: their
# shared lines keep gl39's file name and line numbers, so the functions they do not change keep gl39's profile).
. /home/claude/main/build_gl39/m/scripts/env.sh >/dev/null 2>&1
M=/home/claude/main/build_gl39/m; OV=$1; P=$2; shift 2
mkdir -p $P && cd $P || exit 1; cp /home/claude/main/build_gl39/gcda/boot_rt.gcda .
s=$(date +%s)
nice -n 10 psp-gcc -O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv '-DK_HSTACK=(128*1024)' '-DIOP_HSTACK=(64*1024)' -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I$OV -I$M/boot -I$M/rt -I$M/runtime -I$M/psp -I$M/runtime -I$M/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -fprofile-use -fprofile-partial-training -Wno-missing-profile -Wno-error=coverage-mismatch -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning -ffunction-sections "-DMGS2_GEN_VU=\"$OV/gen_vu.h\"" -DVP_FAST -Wno-error=coverage-mismatch -DVP_VFPU -DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576 -Dmain=mgs2_main -I/mnt/project-files/mgs2/third_party/ps2rc/ps2rc/iop/rt -I/mnt/project-files/mgs2/third_party/ps2rc/audio -DGUEST_SPLIT_RT -DME_GUARD_RT "$@" -c $M/boot/boot.c -o boot_rt.o > cc.log 2>&1
echo "rc $? $(( $(date +%s) - s )) s $(md5sum boot_rt.o 2>/dev/null)" | tee -a cc.log
