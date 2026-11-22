#!/bin/sh
# task #167: who writes the stack bytes 0x70003ec8-0x70003ecf that P3bebf0's 16-byte load takes as VF3.zw (vb 30800-30814), H3R off / on;
# ASLR off so the host addresses map with nm (PIE base 0x555555554000)
cd /home/claude/sbg0/xt5 || exit 1
MG=/mnt/project-files/mgs2; PAD="$(cat $MG/play/pad_w01af.txt)"; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/h3r
for m in off:fffef9df,fffeffdf on:fffff9df,ffffffdf; do t=${m%%:*}; c=${m#*:}
  { env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
    MGS2_PKT_HASH= MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
    MGS2_WWATCH=30700:70003ec8,70003ecc nice -n 10 setarch x86_64 -R ./boot /home/claude/mgs2/SLES_503.83 30817; echo "rc $?"; } 2>&1 | grep -E '^wwatch|^rc' > $S/ww_$t.txt &
done; wait
echo "ww done $(date +%H:%M:%S)"; wc -l $S/ww_off.txt $S/ww_on.txt
