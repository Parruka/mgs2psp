#!/bin/sh
# c2x.sh <tag> [env...]: task #140 (C2 census and GE model), the three routes with everything on as sb11's c1x.sh, plus the given env;
# with MGS2_C2_MODEL=1 the runs must equal sb11's C1-off runs (x4 out_doff5 / ioff5 / coff5: picture and hash lines)
t=$1; shift; L=/home/claude/sb12/c2x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st "start $t $*"
( sh /home/claude/sb12/chk12.sh d d$t "$@"; st "d$t rc $(cat /home/claude/sb12/x1/rc_d$t.txt)" ) &
( sh /home/claude/sb12/chk12.sh i i$t "$@"; st "i$t rc $(cat /home/claude/sb12/x1/rc_i$t.txt)" ) &
sh /home/claude/sb12/chk12.sh c c$t "$@"; st "c$t rc $(cat /home/claude/sb12/x1/rc_c$t.txt)"
wait; st "done $t"
