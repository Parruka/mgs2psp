#!/bin/sh
# fa33.sh: the fully associative LRU figure (build_gl20/fa, FF8's fa.c: 256 lines of 64 bytes, each window from an empty cache) of gl33's traces
# lay/t0m.it (deck route) and lay/t1m.it (inside route); printed in thousands of misses a window, as gl20's notes.
R=/home/claude/main/build_gl35; : > $R/lay/fa_nosegs.txt
for t in t0m t1m; do for w in 0 1 2 3 4 5 6 7; do echo "$t window $w: $(/home/claude/main/build_gl20/fa $R/lay/$t.it $R/lay/fa_nosegs.txt $w 256 2>&1 | tr '\n' ' ')"; done; done
