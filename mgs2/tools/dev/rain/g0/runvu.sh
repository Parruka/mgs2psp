#!/bin/sh
# sbg0 (task #165): runvu.sh = runtl.sh on xt2 (boot_vu.c: MGS2_VU_LOG=<from>:<to> prints VU0 CLIP, STAT, MAC and a VF+ACC hash before and after each draw-pipeline call); runtl.sh <tag> <OPT2 cycle words> <end vb> [env...]: runt.sh to that vblank (for MGS2_READ_LOG); runt.sh: the indoor route to vb 37,800 on the read-hash build xt (mkt.sh), gl17's game
# settings, every vblank's "rh:" line (the loads game code makes outside the draw pipeline, the renderer's frame code left out
# (rl_skip.py's default list), line_sph's act's loads of the hide bits G0H set as the translation leaves them (MGS2_READ_G0)), the hash lines
# with the masked game state and the virtual time.  out_<tag>.log in xt.
t=$1; c=$2; E=$3; shift 3; X=/home/claude/sbg0/xt2; MG=/mnt/project-files/mgs2
cd $X || exit 1
SK=$(python3 -I $MG/tools/dev/rl_skip.py ./boot 2>/dev/null); G0=$(python3 -I $MG/tools/dev/rl_skip.py ./boot S_w00a_P333380 2>/dev/null)
echo "skip $SK g0 $G0" > rs_$t.txt
PAD="$(cat $MG/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
  MGS2_PKT_HASH= MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_READ_HASH=1 MGS2_READ_SKIP=$SK MGS2_READ_G0=$G0 "$@" nice -n 10 ./boot /home/claude/mgs2/SLES_503.83 $E > out_$t.log 2>&1
echo "rc $?" > rc_$t.txt
echo "$t rc $(cat rc_$t.txt) $(date +%H:%M:%S)"
