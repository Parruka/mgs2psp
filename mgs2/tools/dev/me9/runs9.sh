#!/bin/sh
# runs9.sh: me9 (task #130, open item #84) the deck route with the PSP texture path compared with the map at every indexed use
# (MGS2_XLIVE_TK=2688: the PSP's budget, so eviction runs; MGS2_XLIVE_GX cannot run with it, so the frames are GB's) and the map with
# what the uploads sent (MGS2_XLIVE_TEX=1): normally (tkn) and one step late with the copy (tkd); after runs8.sh
cd /home/claude/me9/xd; L=/home/claude/me9/runs9.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q 'done' /home/claude/me9/runs8.log 2>/dev/null; do n=$((n+1)); [ $n -gt 2880 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"
PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"; END=25200
run() { t=$1; shift; env MGS2_DISC=/home/claude/psp5/mgs2disc.pak MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= MGS2_XLIVE=0:$END MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GB=1 \
  MGS2_XLIVE_TK=2688 MGS2_XLIVE_TEX=1 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=15 "$@" ./boot /home/claude/mgs2/SLES_503.83 $END > out_$t.log 2>&1
  echo "rc $?" > rc_$t.txt; st "$t rc $(cat rc_$t.txt)"; }
( run tkn ) &
run tkd MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2
wait; st "done"
