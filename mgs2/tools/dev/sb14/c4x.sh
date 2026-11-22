#!/bin/sh
# c4x.sh <tag> [env...]: task #144, Stage C step 3's exact trims (3v two passes, 3z depth range by the box) on sb14's twin x1: the
# three routes with everything on as gl15's default (C1 on, C2 off; 3v and 3z on by default), plus the given env (MGS2_OPT2=ffffff1f:
# both trims off).  The hash lines must equal sb12/x2's runs (out_*t15.log); every picture and xlive line must equal the trims-off run's
t=$1; shift; L=/home/claude/sb14/c4x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st "start $t $*"
( sh /home/claude/sb14/chk14.sh d d$t "$@"; st "d$t rc $(cat /home/claude/sb14/x1/rc_d$t.txt)" ) &
( sh /home/claude/sb14/chk14.sh i i$t "$@"; st "i$t rc $(cat /home/claude/sb14/x1/rc_i$t.txt)" ) &
sh /home/claude/sb14/chk14.sh c c$t "$@"; st "c$t rc $(cat /home/claude/sb14/x1/rc_c$t.txt)"
wait; st "done $t"
