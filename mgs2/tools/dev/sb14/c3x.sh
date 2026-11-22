#!/bin/sh
# c3x.sh <tag> [env...]: task #144 (the census of the rooms' meshes), the three routes on sb13's twin x1 with everything on as gl15's
# default (C1 on, C2 off), plus the given env; the census only reads, so the hash lines must equal sb12/x2's runs (out_*t15.log)
t=$1; shift; L=/home/claude/sb13/c3x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st "start $t $*"
( sh /home/claude/sb13/chk13.sh d d$t "$@"; st "d$t rc $(cat /home/claude/sb13/x1/rc_d$t.txt)" ) &
( sh /home/claude/sb13/chk13.sh i i$t "$@"; st "i$t rc $(cat /home/claude/sb13/x1/rc_i$t.txt)" ) &
sh /home/claude/sb13/chk13.sh c c$t "$@"; st "c$t rc $(cat /home/claude/sb13/x1/rc_c$t.txt)"
wait; st "done $t"
