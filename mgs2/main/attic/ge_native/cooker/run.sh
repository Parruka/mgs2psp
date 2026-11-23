#!/bin/sh
# run.sh <tag> [VAR=value ...]: one twin run of st0/bld/boot to vb 37,800 with h1/tr.sh's settings (gl39 defaults: main/scripts/twin.sh,
# tw_run39.sh's cycle words, runs39c.sh's p1 keys) and MGS2_NGE=1, against build_gl39/cfg/hashref_gl38.txt (c9ddb593).
# st0/bld: out_<tag>.log.gz, hashref_<tag>.txt, cmp_<tag>.txt, nge_<tag>.txt (the harness's lines).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st0; X=$S/bld; t=$1; shift
MG=/mnt/project-files/mgs2; M=/home/claude/main; SL=/home/claude/mgs2/SLES_503.83; PAK="/mnt/user-data/uploads/mg2 port/Builds/MGS2T_gl18/mgs2disc.pak"
REF=${REF:-$M/build_gl39/cfg/hashref_gl38.txt}; END=${END:-37800}
C8=ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff
D8=fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df,fffff9df
cd $X || exit 1; s=$(date +%s)
env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $MG/play/pad_w01af.txt)" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=0:37800 MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GX=0:37800 \
  MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_SB_LANE=15 \
  MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 \
  MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_OPT2=fffff9df MGS2_HASH_DENSE=15700:16010 MGS2_HASH_REF=$REF MGS2_HASH_REF_DUMP=$X/split_$t \
  MGS2_OPT_CYCLE=24000:100:$C8 MGS2_OPT2_CYCLE=$D8 MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b \
  MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1 MGS2_NAT_CULL=1 \
  MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X MGS2_NAT_F1=1 MGS2_NGE=1 "$@" \
  nice -n 10 ./boot "$SL" $END 2>&1 | gzip -1 > out_$t.log.gz
zcat out_$t.log.gz | grep "^nge:" > nge_$t.txt
zcat out_$t.log.gz > $S/.ref_$t.tmp
python3 -I $M/tools/refcmp.py $REF $S/.ref_$t.tmp hashref_$t.txt > cmp_$t.txt 2>&1; r=$?
rm -f $S/.ref_$t.tmp
echo "run.sh $t: refcmp rc $r, $(( $(date +%s) - s )) s; $(sed -n 2p cmp_$t.txt)" | tee rc_$t.txt
