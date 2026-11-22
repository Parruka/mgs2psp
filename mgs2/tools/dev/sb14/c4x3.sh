#!/bin/sh
# c4x3.sh <tag> [env...]: as c4x.sh on sb14's twin x3 (3v, 3z and 3w, task #144): the three routes in parallel; every picture and xlive
# line must equal x1's trims-off run (out_*t0.log), the hash lines sb12/x2's
t=$1; shift; X=/home/claude/sb14/x3; export X; L=/home/claude/sb14/c4x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st "start x3 $t $*"
( sh /home/claude/sb14/chk14.sh d d$t "$@"; st "x3 d$t rc $(cat $X/rc_d$t.txt)" ) &
( sh /home/claude/sb14/chk14.sh i i$t "$@"; st "x3 i$t rc $(cat $X/rc_i$t.txt)" ) &
sh /home/claude/sb14/chk14.sh c c$t "$@"; st "x3 c$t rc $(cat $X/rc_c$t.txt)"
wait; st "done x3 $t"
