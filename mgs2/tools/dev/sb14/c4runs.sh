#!/bin/sh
# c4runs.sh: task #144, sb14's checks: t0 both trims off, t1 on with MGS2_C3_CHK=1 (every call compared), t2 on (the plain path),
# t3 on with MGS2_SB_CHK=1 (the lanes' input against VU memory)
L=/home/claude/sb14/c4x.log; echo "$(date +%H:%M:%S) c4runs: step 3 trims checks" >> $L
sh /home/claude/sb14/c4x.sh t0 MGS2_OPT2=ffffff1f
sh /home/claude/sb14/c4x.sh t1 MGS2_C3_CHK=1
sh /home/claude/sb14/c4x.sh t2
sh /home/claude/sb14/c4x.sh t3 MGS2_SB_CHK=1
echo "$(date +%H:%M:%S) c4runs done" >> $L
