#!/bin/sh
# tberuns.sh: me13 e (task #147) the take-back's x86 check in the xe twin, two at a time: the warm take-back (MGS2_XLIVE_TBK=3: the
# translator's state and the GS map kept) at tbruns.sh's five points (deck 6,000, 15,800, 24,000; indoors 33,000; codec route 36,000);
# then where the cold start's differences come from at 6,000 and 15,800 (1: only the translator's state kept, 2: only the GS map); then
# the cold start again (0) at the five points, with the copy set's marks on the new map.  Tags e<r><X/100>k<K>; each compared with sb14's
# x1 run from X (cmp13.py); log tbe.log
L=/home/claude/me13/tbe.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
run() { sh /home/claude/me13/tbe.sh $1 $2 $3 $4 $5; st "$2 $(cat /home/claude/me13/xe/rc_$2.txt)"
  PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_$1t0.log /home/claude/me13/xe/out_$2.log $3 2>&1 | head -1 >> $L; }
st "start (me13 e, the five points)"
run d ed60k3 6000 25200 3 & run d ed158k3 15800 25200 3 & wait
run d ed240k3 24000 25200 3 & run i ei330k3 33000 37800 3 & wait
run c ec360k3 36000 40000 3 & run d ed60k1 6000 25200 1 & wait
run d ed60k2 6000 25200 2 & run d ed158k1 15800 25200 1 & wait
run d ed158k2 15800 25200 2 & run d ed240k0 24000 25200 0 & wait
run i ei330k0 33000 37800 0 & run c ec360k0 36000 40000 0 & wait
run d ed60k0 6000 25200 0 & run d ed158k0 15800 25200 0 & wait
st "done"
