#!/bin/sh
# fa36n.sh (gl36 retrain): fa36.sh on the merged traces of rt_order.sh: lay/t0n.it (10 windows: 16,001 16,101 26,501 27,501 27,801
# 29,601 29,801 30,101 31,101 | 17,501) and lay/t1n.it (8), thousands of misses a window, fully associative LRU (256 lines of 64 B).
R=/home/claude/main/build_gl36; : > $R/lay/fa_nosegs.txt
for w in 0 1 2 3 4 5 6 7 8 9; do echo "t0n window $w: $(/home/claude/main/build_gl20/fa $R/lay/t0n.it $R/lay/fa_nosegs.txt $w 256 2>&1 | tr '\n' ' ')"; done
for w in 0 1 2 3 4 5 6 7; do echo "t1n window $w: $(/home/claude/main/build_gl20/fa $R/lay/t1n.it $R/lay/fa_nosegs.txt $w 256 2>&1 | tr '\n' ' ')"; done
