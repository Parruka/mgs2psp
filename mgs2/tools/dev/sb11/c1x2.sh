#!/bin/sh
# c1x2.sh <tag> [env...]: task #135 c1opt2 (the exact trims after c1opt: bones unrolled, the constants' key in place, the list's 12-word
# compares, the slot scan unrolled, the depth range by a bound where it cannot move), the three routes on the x86 twin in x3 as c1x.sh;
# none: C1 off (the default), MGS2_OPT2=ffffffff: C1 on.  Must equal x1's c1opt runs (out_{d,i,c}off.log, out_{d,i,c}on3.log)
t=$1; shift; L=/home/claude/sb11/c1x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }; export X=/home/claude/sb11/x3
st "start x3 $t $*"
( sh /home/claude/sb11/chk11x.sh d d$t "$@"; st "x3 d$t rc $(cat $X/rc_d$t.txt)" ) &
( sh /home/claude/sb11/chk11x.sh i i$t "$@"; st "x3 i$t rc $(cat $X/rc_i$t.txt)" ) &
sh /home/claude/sb11/chk11x.sh c c$t "$@"; st "x3 c$t rc $(cat $X/rc_c$t.txt)"
wait; st "done x3 $t"
