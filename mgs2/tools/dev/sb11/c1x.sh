#!/bin/sh
# c1x.sh <tag> [env...]: task #135 (C1 direct), the three routes with everything on as c1runs.sh, plus the given env (none: C1 off, the
# default; MGS2_OPT2=ffffffff: C1 on).  The C1-off runs must equal sb10's final runs (t1cmp.py against /home/claude/sb10/x1/out_t2f{d,i,c}on.log)
t=$1; shift; L=/home/claude/sb11/c1x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st "start $t $*"
( sh /home/claude/sb11/chk11.sh d d$t "$@"; st "d$t rc $(cat /home/claude/sb11/x1/rc_d$t.txt)" ) &
( sh /home/claude/sb11/chk11.sh i i$t "$@"; st "i$t rc $(cat /home/claude/sb11/x1/rc_i$t.txt)" ) &
sh /home/claude/sb11/chk11.sh c c$t "$@"; st "c$t rc $(cat /home/claude/sb11/x1/rc_c$t.txt)"
wait; st "done $t"
