#!/bin/sh
# cc_w00a.sh <dir with gen_s_w00a.gcda> <header path>: psp/Makefile's gen_s_w00a.o command of h2/d (PGO=use LAYOUT=1, genf.txt), with
# STAGE_GEN = <header path> (a copy of the deck header behind '#line 1 "/home/claude/v30/WP/gen_s_w00a.h"', the name the profile was
# made with: GCC's profile id of a static function includes its file name)
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
cd $1 || exit 1; s=$(date +%s)
psp-gcc -O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv '-DK_HSTACK=(128*1024)' '-DIOP_HSTACK=(64*1024)' -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime -I/mnt/project-files/mgs2/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning  -ffunction-sections "-DSTAGE_GEN=\"$2\"" -c /mnt/project-files/mgs2/boot/gen_stage.c -o gen_s_w00a.o > cc_w00a.log 2>&1
echo "rc $? $(( $(date +%s) - s )) s" >> cc_w00a.log
