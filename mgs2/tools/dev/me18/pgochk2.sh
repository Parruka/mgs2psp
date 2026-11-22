#!/bin/sh
# pgochk2.sh: gl18's base w00a (sbg18/cc_w00a18.sh's line) compiled again with the warnings kept, to list the functions whose profile
# GCC drops in the base itself (control flow mismatch), against the guarded w00a's list (/dev/shm/pgochk/warn_gen_s_w00a.o.txt).
# Output in /dev/shm/pgochk/r2 (removed by hand after).
O=/dev/shm/pgochk/r2; mkdir -p $O; cp /home/claude/sbg18/base/gen_s_w00a.gcda $O/
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
cd /home/claude/sbg18/base || exit 1
PUSE="-fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning"
GENFW=$(sed 's/ -w / /' genf.txt)
s=$(date +%s)
eval "psp-gcc $GENFW -Wno-error=coverage-mismatch $PUSE -ffunction-sections -DSTAGE_GEN='\"/home/claude/sbg0/wpl/gen_s_w00a.h\"' -c /mnt/project-files/mgs2/boot/gen_stage.c -o $O/gen_s_w00a.o" 2>&1 | grep -i "control flow\|number of counters" > /dev/shm/pgochk/warn_base_w00a.txt
echo "base w00a done $(( $(date +%s) - s )) s, $(grep -c '' /dev/shm/pgochk/warn_base_w00a.txt) lines" > /dev/shm/pgochk/status2.txt
