#!/bin/sh
# quick.sh <build dir> [vb] (dev): the deck route (runh.sh d) from boot to vb (default 4400) in <build dir>/Q; hash lines against
# prof/hashes_x86_deck_v27.txt up to vb: "equal" or the first differing line
B=$1; V=${2:-4400}; D=$B/Q; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D
env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)" MGS2_SIF_LOG=0 ./boot /home/claude/mgs2/SLES_503.83 $V > out.log 2>&1
grep -a '^hash: vb' out.log | awk '$3 % 100 == 0 {print $3, $5, $7, $9}' > q.ref
n=$(wc -l < q.ref); head -$n /mnt/project-files/mgs2/prof/hashes_x86_deck_v27.txt > q.want
d=$(diff q.ref q.want | grep '^<' | head -1)
echo "$(basename $B): $n lines; ${d:-equal}"
