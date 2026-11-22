#!/bin/sh
# c4runs3.sh: task #144, 3w's checks on x3: t4 all on with MGS2_C3_CHK=1, t5 all on (the plain path); both against x1's t0
L=/home/claude/sb14/c4x.log; echo "$(date +%H:%M:%S) c4runs3: 3w checks" >> $L
sh /home/claude/sb14/c4x3.sh t4 MGS2_C3_CHK=1
sh /home/claude/sb14/c4x3.sh t5
echo "$(date +%H:%M:%S) c4runs3 done" >> $L
