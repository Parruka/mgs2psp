#!/bin/sh
# t1chk.sh: task #133 step 1 T1 (the texture memo's front) on the x86 twin sb10/x1: the three routes with T1 on (the default) and in its
# check mode (MGS2_TX_FRONT_CHK=1), lanes 1-4, rain and splash 1/2, as chk10.sh; compare with sb9's x1 out_*2on.log afterwards
L=/home/claude/sb10/t1chk.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }
st start
( sh /home/claude/sb10/chk10.sh d t1don; st "d on rc $(cat /home/claude/sb10/x1/rc_t1don.txt)"
  sh /home/claude/sb10/chk10.sh d t1dck MGS2_TX_FRONT_CHK=1; st "d check rc $(cat /home/claude/sb10/x1/rc_t1dck.txt)" ) &
( sh /home/claude/sb10/chk10.sh i t1ion; st "i on rc $(cat /home/claude/sb10/x1/rc_t1ion.txt)"
  sh /home/claude/sb10/chk10.sh i t1ick MGS2_TX_FRONT_CHK=1; st "i check rc $(cat /home/claude/sb10/x1/rc_t1ick.txt)" ) &
sh /home/claude/sb10/chk10.sh c t1con; st "c on rc $(cat /home/claude/sb10/x1/rc_t1con.txt)"
sh /home/claude/sb10/chk10.sh c t1cck MGS2_TX_FRONT_CHK=1; st "c check rc $(cat /home/claude/sb10/x1/rc_t1cck.txt)"
wait; st done
