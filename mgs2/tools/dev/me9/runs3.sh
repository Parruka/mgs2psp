#!/bin/sh
# runs3.sh: me9 (task #130), runs2.sh's three runs with two at a time (runs2.sh was stopped while it still waited): after the deck
# texture check, D2 + copy on the deck and D1 + copy indoors together, then D1 + copy on the codec route.
cd /home/claude/me9 || exit 1
L=/home/claude/me9/runs3.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
n=0; while [ ! -f xd/rc_dntex.txt ] && [ $n -lt 720 ]; do sleep 5; n=$((n + 1)); done
st "dntex done: $(cat xd/rc_dntex.txt 2>/dev/null)"
( st "d D2 copy start"; sh chkd.sh d d2s2 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2; st "d2 rc $(cat xd/rc_d2s2.txt)" ) &
p1=$!
st "i D1 copy start"; sh chkd.sh i is2 MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2; st "i rc $(cat xd/rc_is2.txt)"
st "c D1 copy start"; CEND=40000 sh chkd.sh c cs2 MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2; st "c rc $(cat xd/rc_cs2.txt 2>/dev/null)"
wait $p1
st "done"
