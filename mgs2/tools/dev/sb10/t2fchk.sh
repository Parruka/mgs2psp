#!/bin/sh
# t2fchk.sh: T2's final source (up_last's division-free forms) on the x86 twin sb10/x1: the three routes with everything on, against T1's
# runs (x1 out_t1{d,i,c}on.log; t1cmp.py)
L=/home/claude/sb10/t2fchk.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st start
( sh /home/claude/sb10/chk10.sh d t2fdon; st "d on rc $(cat /home/claude/sb10/x1/rc_t2fdon.txt)" ) &
( sh /home/claude/sb10/chk10.sh i t2fion; st "i on rc $(cat /home/claude/sb10/x1/rc_t2fion.txt)" ) &
sh /home/claude/sb10/chk10.sh c t2fcon; st "c on rc $(cat /home/claude/sb10/x1/rc_t2fcon.txt)"
wait; st done
