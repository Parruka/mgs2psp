#!/bin/sh
# rl2.sh: V3a's read log in w01a+ (st2/rl.sh's binary and settings, MGS2_READ_LOG on 5 sample vblanks whose read hash differs), base then
# V3a (stubs 1214a0 + 12e8b8 every step), gzip'd logs st2/rh/out_rl2b.log.gz, out_rl2v.log.gz.  One after the other, nice 19.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; X=$S/st0/xt; R=$S/st2/rh; MG=/mnt/project-files/mgs2
PAK="/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak"; PADF=$MG/play/pad_w01af.txt; SL=/home/claude/mgs2/SLES_503.83
cd $X || exit 1
SK=$(python3 -I $MG/tools/dev/rl_skip.py ./boot 2>/dev/null); G0=$(python3 -I $MG/tools/dev/rl_skip.py ./boot S_w00a_P333380 2>/dev/null)
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
RL=31660:31661,33701:33701,36501:36501,37201:37201
for t in rl2b rl2v; do s=$(date +%s)
  if [ $t = rl2v ]; then V="MGS2_LITE_ALL=0 MGS2_LITE_FN=1214a0,12e8b8"; else V=""; fi
  env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
    MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
    MGS2_OPT2=fffff9df MGS2_READ_HASH=1 MGS2_READ_SKIP=$SK MGS2_READ_G0=$G0 MGS2_READ_LOG=$RL $V nice -n 19 ./boot "$SL" 37202 2>&1 | gzip -1 > $R/out_$t.log.gz
  echo "$t rc $? $(( $(date +%s) - s )) s" > $R/rc_$t.txt; done
