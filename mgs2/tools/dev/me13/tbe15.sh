#!/bin/sh
# tbe15.sh: me13 e (task #147): the take-back at vb 1,500 on the deck route (the opening) with each keep mode, to vb 3,000; log tbe.log
L=/home/claude/me13/tbe.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
st "start (me13 e, X 1500)"
for k in 0 1 2 3; do ( sh /home/claude/me13/tbe.sh d tbe15k$k 1500 3000 $k; st "tbe15k$k $(cat /home/claude/me13/xe/rc_tbe15k$k.txt)" ) & done
wait
for k in 0 1 2 3; do PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_dt0.log /home/claude/me13/xe/out_tbe15k$k.log 1500 | head -1 >> $L 2>&1; done
st "done"
