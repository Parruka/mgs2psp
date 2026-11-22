#!/bin/sh
# wruns.sh: me12 (task #142) the writer tracker on the three routes, one at a time (nice 10, so gl15's work comes first); each run's hash
# lines checked against sb9/x1's normal runs (out_<r>2on.log): the tracker must not change the game's state.
cd /home/claude/me12 || exit 1
L=/home/claude/me12/wruns.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
st "start"
for p in "d wd" "i wi" "c wc"; do
  set -- $p
  nice -n 10 sh chkw.sh $1 $2
  a=$(grep "^hash" xd/out_$2.log | md5sum | cut -c1-12); na=$(grep -c "^hash" xd/out_$2.log)
  b=$(grep "^hash" /home/claude/sb9/x1/out_$1"2on".log | md5sum | cut -c1-12); nb=$(grep -c "^hash" /home/claude/sb9/x1/out_$1"2on".log)
  st "$2 $(cat xd/rc_$2.txt 2>/dev/null) hashes $na $a, reference $nb $b $( [ "$a" = "$b" ] && echo SAME || echo DIFFER)"
done
st "done"
