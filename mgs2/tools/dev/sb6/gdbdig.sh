#!/bin/sh
# gdbdig.sh <tag> <lane mask>: dig_x86.sh's run (indoor route, translator from vb 30,800 to 30,900, digest file, rain and splash 1/2) under
# gdb in /home/claude/sb6/xd, to find where it stops; the backtrace into gdb_<tag>.txt
cd /home/claude/sb6/xd || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"
env MGS2_DISC=/home/claude/mgs2/disc MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 MGS2_PKT_HASH= \
  MGS2_XLIVE=30800:30900:/home/claude/sb6/xd/dig_$1.txt MGS2_XLIVE_FAST=1 MGS2_XLIVE_ONE=1 MGS2_XLIVE_SKIP=1 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 \
  MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_SB_LANE=$2 \
  gdb -batch -ex run -ex bt -ex 'info registers rip' -ex 'x/6i $pc' -ex 'frame 1' -ex 'info locals' --args /home/claude/sb6/x1/boot /home/claude/mgs2/SLES_503.83 30900 > out_$1.log 2> gdb_$1.txt
echo "rc $?" >> gdb_$1.txt
