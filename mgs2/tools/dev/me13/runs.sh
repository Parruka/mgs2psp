#!/bin/sh
# runs.sh [prefix [env...]]: me13 (task #147) the three routes side by side with the deadline copy, the filtered guard log (MGS2_XLIVE_DLF=1)
# and the hand-over at vb 60 (me13 b: the late walk takes the lanes, MGS2_XLIVE_LW default 1); out_<prefix><r>.log, each against sb14's x1
# run with the same settings but today's walk (out_<r>t0.log, every line equal to the trims-on runs); prefix f: the me13 a runs
P=${1:-g}; [ $# -gt 0 ] && shift
L=/home/claude/me13/runs.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
st "start $P $*"
( sh /home/claude/me13/run1.sh d ${P}d 25200 MGS2_XLIVE_DLF=1 "$@"; st "${P}d $(cat /home/claude/me13/xd/rc_${P}d.txt)" ) &
( sh /home/claude/me13/run1.sh i ${P}i 37800 MGS2_XLIVE_DLF=1 "$@"; st "${P}i $(cat /home/claude/me13/xd/rc_${P}i.txt)" ) &
( sh /home/claude/me13/run1.sh c ${P}c 40000 MGS2_XLIVE_DLF=1 "$@"; st "${P}c $(cat /home/claude/me13/xd/rc_${P}c.txt)" ) &
wait
for r in d i c; do PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_${r}t0.log /home/claude/me13/xd/out_$P$r.log >> $L 2>&1; done
st "done $P"
