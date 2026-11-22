#!/bin/sh
# tbfruns.sh: me13 f (task #147) the take-back's x86 check in the xf twin, two at a time: MGS2_XLIVE_TBK=7 (the translator's state, the
# GS map and the live frame's depth window kept) at the six points (deck 1,500, 6,000, 15,800, 24,000; indoors 33,000; codec route
# 36,000), 24,000 and 33,000 first (me13 e's TBK=3 left their first frame different).  Tags f<r><X/100>k7; each compared with sb14's x1
# run from X (cmp13.py); log tbf.log
L=/home/claude/me13/tbf.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
run() { sh /home/claude/me13/tbf.sh $1 $2 $3 $4 $5; st "$2 $(cat /home/claude/me13/xf/rc_$2.txt)"
  PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_$1t0.log /home/claude/me13/xf/out_$2.log $3 2>&1 | head -1 >> $L; }
st "start (me13 f, TBK 7 at the six points)"
run d fd240k7 24000 25200 7 & run i fi330k7 33000 37800 7 & wait
run c fc360k7 36000 40000 7 & run d fd15k7 1500 3000 7 & wait
run d fd60k7 6000 25200 7 & run d fd158k7 15800 25200 7 & wait
st "done"
