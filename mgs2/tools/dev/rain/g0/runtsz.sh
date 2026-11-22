#!/bin/sh
# sbg0 (task #165): runtsz.sh <tag> <OPT2 cycle words> <end vb> <extra skip ranges> [env...]: runtlz.sh (log gzipped on the fly) with more
# MGS2_READ_SKIP ranges after rl_skip.py's default list (hex host offsets from rt_dgt_rd, [lo, hi)), e.g. the two clip-flag reloads that
# keep only their own tests' bits (rain_gas_pers 0x3318f0 = rf523bf, w01a P313e60 0x313f4c = r12fffcd in this build).
t=$1; c=$2; E=$3; XS=$4; shift 4; X=${X:-/home/claude/sbg0/xt}; MG=/mnt/project-files/mgs2   # X=/home/claude/sbg0/xt3: the H3 build
cd $X || exit 1
SK=$(python3 -I $MG/tools/dev/rl_skip.py ./boot 2>/dev/null),$XS; G0=$(python3 -I $MG/tools/dev/rl_skip.py ./boot S_w00a_P333380 2>/dev/null)
echo "skip $SK g0 $G0" > rs_$t.txt
PAD="$(cat $MG/play/pad_w01af.txt)"
{ env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
  MGS2_PKT_HASH= MGS2_OPT_CYCLE=1000:100:ffffffff,ffffffff MGS2_OPT2_CYCLE=$c MGS2_GS_MASK=$MG/prof/gs_mask_v1.txt MGS2_HASH_TIME=1 \
  MGS2_READ_HASH=1 MGS2_READ_SKIP=$SK MGS2_READ_G0=$G0 "$@" nice -n 10 ./boot /home/claude/mgs2/SLES_503.83 $E; echo "rc $?" > rc_$t.txt; } 2>&1 | gzip -1 > out_$t.log.gz
echo "$t rc $(cat rc_$t.txt) $(date +%H:%M:%S)"
