#!/bin/sh
# pickseed16.sh: gl16's two order searches (gl15's pickseed15.sh on /home/claude/sbg16): the "optimized order, whole traces" line of
# each (weighted sum = both traces' I-misses, test = the held-out windows); the seed with the lower test (then sum) is printed last.
L=/home/claude/sbg16/lay
for s in 1 2; do printf 'seed %s: ' $s; grep 'optimized order, whole traces' $L/ls_s$s.out | sed -E 's/.*weighted sum ([0-9]+), test ([0-9]+).*/sum \1 test \2/'; done
for s in 1 2; do grep 'optimized order, whole traces' $L/ls_s$s.out | sed -E "s/.*weighted sum ([0-9]+), test ([0-9]+).*/\2 \1 $s/"; done | sort -n | head -1 | awk '{print "pick seed " $3}'
