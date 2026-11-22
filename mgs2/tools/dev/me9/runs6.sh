#!/bin/sh
# runs6.sh: me9 (task #130) the deck route with the picture two steps late and no copy (d2n): what reading the game's RAM two steps
# late does without a copy; starts when runs5.sh's indoor run is done
cd /home/claude/me9; L=/home/claude/me9/runs6.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q 'id1 rc' runs5.log 2>/dev/null; do n=$((n+1)); [ $n -gt 1440 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"; sh chkd.sh d d2n MGS2_XLIVE_DEFER=2; st "d2n rc $?"
