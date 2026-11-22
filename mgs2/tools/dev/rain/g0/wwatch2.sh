#!/bin/sh
# task #167: who writes the main-RAM words outside every mask that differ between H3R off and on in the EE dumps at vb 23,300
# (0x1b8bb0-0x1b8c03, 0x1b8fbc-0x1b900f, 0x1e5080-0x1e50a3: the first and last differing word of each), on the G0R trace build xt6 with
# G0R off; ASLR off so the host addresses map with nm (PIE base 0x555555554000)
cd /home/claude/sbg0/xt6 || exit 1
MG=/mnt/project-files/mgs2; PAD="$(cat $MG/play/pad_w01af.txt)"; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/h3r
for m in off:fffcf9df,fffcffdf on:fffdf9df,fffdffdf; do t=${m%%:*}; c=${m#*:}
  { env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
    MGS2_PKT_HASH= MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
    MGS2_WWATCH=23296:1b8bb0,1b8c00,1b8fbc,1b900c,1e5080,1e50a0 nice -n 10 setarch x86_64 -R ./boot /home/claude/mgs2/SLES_503.83 23300; echo "rc $?"; } 2>&1 | grep -E '^wwatch|^rc' > $S/ww2_$t.txt &
done; wait
echo "ww2 done $(date +%H:%M:%S)"; wc -l $S/ww2_off.txt $S/ww2_on.txt
python3 -I $S/wwmap.py /home/claude/sbg0/xt6/boot $S/ww2_off.txt 23296 23300 > $S/ww2m_off.txt
python3 -I $S/wwmap.py /home/claude/sbg0/xt6/boot $S/ww2_on.txt 23296 23300 > $S/ww2m_on.txt
awk '{print $2, $3, $4}' $S/ww2m_off.txt | sort | uniq -c | sort -rn | head -20; echo ---; awk '{print $2, $3, $4}' $S/ww2m_on.txt | sort | uniq -c | sort -rn | head -20
