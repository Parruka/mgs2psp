#!/bin/sh
# t2chk.sh: task #133 step 1 T2 (P07 calls held to the transfer's end, OPT2_C7) on the x86 twin sb10/x1: the three routes with T2 on (the
# default), and with its checks (MGS2_C7_CHK=1: each held call's placed memory against its own; MGS2_SB_CHK=1: the streams' results against
# the VU memory's; MGS2_SB_POISON=1: the scratch memory poisoned before each placement and rebuild); the codec route also with every held
# call through the rebuilt vertices (MGS2_SB_FORCE=1).  Compare with T1's x1 out_t1{d,i,c}on.log afterwards (scratchpad t1cmp.py)
L=/home/claude/sb10/t2chk.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st start
( sh /home/claude/sb10/chk10.sh d t2don; st "d on rc $(cat /home/claude/sb10/x1/rc_t2don.txt)"
  sh /home/claude/sb10/chk10.sh d t2dck MGS2_C7_CHK=1 MGS2_SB_CHK=1 MGS2_SB_POISON=1; st "d check rc $(cat /home/claude/sb10/x1/rc_t2dck.txt)" ) &
( sh /home/claude/sb10/chk10.sh i t2ion; st "i on rc $(cat /home/claude/sb10/x1/rc_t2ion.txt)"
  sh /home/claude/sb10/chk10.sh i t2ick MGS2_C7_CHK=1 MGS2_SB_CHK=1 MGS2_SB_POISON=1; st "i check rc $(cat /home/claude/sb10/x1/rc_t2ick.txt)" ) &
sh /home/claude/sb10/chk10.sh c t2con; st "c on rc $(cat /home/claude/sb10/x1/rc_t2con.txt)"
sh /home/claude/sb10/chk10.sh c t2cck MGS2_C7_CHK=1 MGS2_SB_CHK=1 MGS2_SB_POISON=1; st "c check rc $(cat /home/claude/sb10/x1/rc_t2cck.txt)"
sh /home/claude/sb10/chk10.sh c t2cfp MGS2_C7_CHK=1 MGS2_SB_FORCE=1 MGS2_SB_POISON=1; st "c force rc $(cat /home/claude/sb10/x1/rc_t2cfp.txt)"
wait; st done
