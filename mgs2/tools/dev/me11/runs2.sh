#!/bin/sh
# runs2.sh: me11 b (task #142), two steps late (D2): the design's copy set laid over RAM: every kind but the upload granules, plus the
# packets' own upload granules and those in the 13 regions of 64 KB whose texture sources the game rewrites (census, all three routes);
# one route at a time (nice 10); hashes and pictures against sb9's normal runs
cd /home/claude/me11 || exit 1
L=/home/claude/me11/runs2.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
st "start"
D="MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0 MGS2_XLIVE_SNAPP=1 MGS2_XLIVE_SNAPR=0c0,12c,137,13a,13b,13c,13d,13e,13f,140,141,142,1ed"
for p in "d drg" "i irg" "c crg"; do
  set -- $p
  CEND=40000 nice -n 10 sh chkd.sh $1 $2 $D
  a=$(grep "^hash" xd/out_$2.log | md5sum | cut -c1-12); b=$(grep "^hash" /home/claude/sb9/x1/out_$1"2on".log | md5sum | cut -c1-12)
  st "$2 $(cat xd/rc_$2.txt 2>/dev/null) hashes $a ref $b $( [ "$a" = "$b" ] && echo SAME || echo DIFFER)"
  PYTHONDONTWRITEBYTECODE=1 python3 -I tools/picdiff3.py /home/claude/sb9/x1/out_$1"2on".log xd/out_$2.log >> $L 2>&1
done
st "done"
