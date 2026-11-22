#!/bin/sh
# tbgruns.sh: me13 g (task #147) the take-back with the whole picture state kept (MGS2_XLIVE_TBK=15: the translator's state, the GS map,
# the live frame's depth window and the texture cache) at the other five points (deck 1,500, 6,000, 15,800, 24,000; indoors 33,000),
# two at a time in the xg twin; tags g<r><X/100>k15; each compared with sb14's x1 run from X (cmp13.py); log tbg.log
L=/home/claude/me13/tbg.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
run() { sh /home/claude/me13/tbg.sh $1 $2 $3 $4 $5; st "$2 $(cat /home/claude/me13/xg/rc_$2.txt)"
  PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_$1t0.log /home/claude/me13/xg/out_$2.log $3 2>&1 | head -1 >> $L; }
st "start (me13 g, TBK 15 at the other five points)"
run d gd15k15 1500 3000 15 & run d gd240k15 24000 25200 15 & wait
run i gi330k15 33000 37800 15 & run d gd60k15 6000 25200 15 & wait
run d gd158k15 15800 25200 15
st "done"
