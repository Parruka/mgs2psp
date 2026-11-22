#!/bin/sh
# skipall.sh <tag> <vb list>: gsref of /home/claude/c4cap/<tag>.cap with each undrawn entry left out in turn, the displays of <vb list>
# (the frames of full steps: shown one vblank later) compared with the whole PS2 frame (out_<tag>)
t=$1; shift; C=/home/claude/c4cap; D=$(cd "$(dirname "$0")" && pwd)/imdiff.py
cd $C; mkdir -p out_sk_$t
for e in 19bb50 198b60 19be50 19af68 202008 198e58 1a8148 199478 ALL; do
  if [ $e = ALL ]; then S=19bb50,198b60,19be50,19af68,202008,198e58,1a8148,199478; else S=$e; fi
  GSREF_SKIPENT=$S timeout 600 ./gsref $t.cap out_sk_$t > /dev/null 2>&1
  for v in "$@"; do printf "%s %s vb %s: " $t $e $v; python3 -I $D out_$t/vb${v}_disp1.ppm out_sk_$t/vb${v}_disp1.ppm 448; done; done
