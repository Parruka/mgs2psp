#!/bin/sh
# gl14_go.sh: after both order searches (order_s1.txt, order_s2.txt written): pickseed14.sh (the lower held-out test, then the lower
# sum), then gl14_mk.sh <seed> (final link, folder, checks).  Log: /home/claude/sbg14/drv.log
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl14; X=/home/claude/sbg14; L=$X/drv.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
{ [ -f $X/lay/order_s1.txt ] && [ -f $X/lay/order_s2.txt ]; } || { st "go: an order search failed"; exit 1; }
sh $S/pickseed14.sh > $X/pickseed.out 2>&1; cat $X/pickseed.out >> $L
sd=$(sed -n 's/^pick seed //p' $X/pickseed.out); [ -n "$sd" ] || { st "go: no seed picked"; exit 1; }
sh $S/gl14_mk.sh $sd
