#!/bin/sh
# t1runs.sh: task #133 step 1 T1 sizing: the three routes on sb10/x1 built with -DT1_SIM (ge/gelive.c: a second texture memo's would-be
# hits), lanes 1-4, rain and splash 1/2, everything else as chk10.sh
L=/home/claude/sb10/t1runs.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st start
( sh /home/claude/sb10/chk10.sh d t1d; st "d rc $(cat /home/claude/sb10/x1/rc_t1d.txt)" ) &
( sh /home/claude/sb10/chk10.sh i t1i; st "i rc $(cat /home/claude/sb10/x1/rc_t1i.txt)" ) &
sh /home/claude/sb10/chk10.sh c t1c; st "c rc $(cat /home/claude/sb10/x1/rc_t1c.txt)"
wait; st done
