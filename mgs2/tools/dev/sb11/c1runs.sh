#!/bin/sh
# c1runs.sh: task #134, the C1 census and GE model (MGS2_C1_MODEL=1) on the three routes with everything on (as sb10's t2fchk.sh);
# the frames, contents and hashes must equal sb10's final runs (t1cmp.py against /home/claude/sb10/x1/out_t2f{d,i,c}on.log)
L=/home/claude/sb11/c1runs.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st start
( sh /home/claude/sb11/chk11.sh d c1d MGS2_C1_MODEL=1; st "d rc $(cat /home/claude/sb11/x1/rc_c1d.txt)" ) &
( sh /home/claude/sb11/chk11.sh i c1i MGS2_C1_MODEL=1; st "i rc $(cat /home/claude/sb11/x1/rc_c1i.txt)" ) &
sh /home/claude/sb11/chk11.sh c c1c MGS2_C1_MODEL=1; st "c rc $(cat /home/claude/sb11/x1/rc_c1c.txt)"
wait; st done
