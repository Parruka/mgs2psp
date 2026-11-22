#!/bin/sh
# runs2.sh: me9 (task #130) after the deck texture check: D1 + copy on the indoor and codec routes, D2 + copy on the deck; one at a time.
cd /home/claude/me9 || exit 1
L=/home/claude/me9/runs2.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
n=0; while [ ! -f xd/rc_dntex.txt ] && [ $n -lt 720 ]; do sleep 5; n=$((n + 1)); done
st "dntex done: $(cat xd/rc_dntex.txt 2>/dev/null)"
st "i D1 copy start"; sh chkd.sh i is2 MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2; st "i rc $(cat xd/rc_is2.txt)"
st "c D1 copy start"; CEND=40000 sh chkd.sh c cs2 MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2; st "c rc $(cat xd/rc_cs2.txt 2>/dev/null)"
st "d D2 copy start"; sh chkd.sh d d2s2 MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2; st "d2 rc $(cat xd/rc_d2s2.txt)"
st "done"
