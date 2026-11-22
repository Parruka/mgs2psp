#!/bin/sh
# runs4.sh: me9 (task #130) the indoor frames where the copy run is4 differs from the normal run (vb 26,007 and 36,627-37,799): the same
# frames saved from a normal run (ing) and a copy run (isg) for gerdiff.py; starts when the codec copy run and the deck D2 run are done
cd /home/claude/me9; L=/home/claude/me9/runs4.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while [ ! -f xd/rc_cs2.txt ] || [ ! -f xd/rc_d2s2t.txt ]; do n=$((n+1)); [ $n -gt 720 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"
V=26007,36627,36705,37443,37799
( sh chkd.sh i ing MGS2_XLIVE_GER=/home/claude/me9/ger/in:$V; st "ing rc $?" ) &
sh chkd.sh i isg MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_GER=/home/claude/me9/ger/is:$V; st "isg rc $?"
wait; st "done"
