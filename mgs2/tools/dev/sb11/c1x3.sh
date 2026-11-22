#!/bin/sh
# c1x3.sh <tag> [env...]: task #135 c1opt3 (c1opt2 with the depth bound xc_c1_zin by the box's centre and half extents, no branch
# per axis), the three routes on the x86 twin in x4 as c1x.sh;
# none: C1 off (the default), MGS2_OPT2=ffffffff: C1 on.  Must equal x1's c1opt runs (out_{d,i,c}off.log, out_{d,i,c}on3.log)
t=$1; shift; L=/home/claude/sb11/c1x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }; export X=/home/claude/sb11/x4
st "start x4 $t $*"
( sh /home/claude/sb11/chk11x.sh d d$t "$@"; st "x4 d$t rc $(cat $X/rc_d$t.txt)" ) &
( sh /home/claude/sb11/chk11x.sh i i$t "$@"; st "x4 i$t rc $(cat $X/rc_i$t.txt)" ) &
sh /home/claude/sb11/chk11x.sh c c$t "$@"; st "x4 c$t rc $(cat $X/rc_c$t.txt)"
wait; st "done x4 $t"
