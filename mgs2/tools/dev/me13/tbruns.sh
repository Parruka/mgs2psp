#!/bin/sh
# tbruns.sh: me13 d (task #147) the take-back's x86 check (tb.sh), two at a time beside gl16's checks: X = 6,000, 15,800 and 24,000 on the
# deck route, 33,000 indoors, 36,000 on the codec route; frames from X on against sb14's x1 run (cmp13.py <ref> <log> X); log tbruns.log
L=/home/claude/me13/tbruns.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
st "start (me13 d)"
( sh /home/claude/me13/tb.sh d tbd6 6000 25200; st "tbd6 $(cat /home/claude/me13/xd/rc_tbd6.txt)" ) &
( sh /home/claude/me13/tb.sh d tbd15 15800 25200; st "tbd15 $(cat /home/claude/me13/xd/rc_tbd15.txt)" ) &
wait
( sh /home/claude/me13/tb.sh d tbd24 24000 25200; st "tbd24 $(cat /home/claude/me13/xd/rc_tbd24.txt)" ) &
( sh /home/claude/me13/tb.sh i tbi33 33000 37800; st "tbi33 $(cat /home/claude/me13/xd/rc_tbi33.txt)" ) &
wait
sh /home/claude/me13/tb.sh c tbc36 36000 40000; st "tbc36 $(cat /home/claude/me13/xd/rc_tbc36.txt)"
for x in d:6:6000 d:15:15800 d:24:24000 i:33:33000 c:36:36000; do r=${x%%:*}; k=${x#*:}; k=${k%%:*}; v=${x##*:}
  PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_${r}t0.log /home/claude/me13/xd/out_tb$r$k.log $v >> $L 2>&1; done
st "done"
