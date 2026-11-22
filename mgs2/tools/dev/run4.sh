#!/bin/sh
# run3.sh <dir> <tag> [env...]: run2.sh for the probe binary in <dir>
D=$1; T=$2; shift 2
cd $D || exit 1
env "$@" MGS2_FPU_RZ=1 MGS2_HASH_EVERY=100 MGS2_SKIP_MOVIES=1 MGS2_DISC=/home/claude/psppak/mgs2disc.pak MGS2_PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)" MGS2_SIF_LOG=0 ./boot /home/claude/mgs2/SLES_503.83 ${MAXVB:-25200} 2>&1 | head -c 20000000 > run_$T.log
grep "^hash" run_$T.log | awk '{print $3,$5,$7}' > hashes_$T.txt
echo "$T: $(wc -l < hashes_$T.txt) hashes, $(diff hashes_$T.txt /mnt/project-files/mgs2/boot/hashes_x86_rz.txt | grep -c '^[<>]') differing lines" > run_$T.status
