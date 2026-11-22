#!/bin/sh
# runs7.sh: me9 (task #130, and open item #84) the deck route with the PSP texture path checked against the map at every indexed use
# (MGS2_XLIVE_TK=1) and the map against what the uploads sent (MGS2_XLIVE_TEX=1): normally (dtk) and one step late with the copy (d1tk);
# for the late deck's one texture whose frames differ between the two while the texture check saw no difference; after runs6.sh
cd /home/claude/me9; L=/home/claude/me9/runs7.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q 'd2n rc' runs6.log 2>/dev/null; do n=$((n+1)); [ $n -gt 2160 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"
( sh chkd.sh d dtk MGS2_XLIVE_TK=1 MGS2_XLIVE_TEX=1; st "dtk rc $?" ) &
sh chkd.sh d d1tk MGS2_XLIVE_DEFER=1 MGS2_XLIVE_SNAP=2 MGS2_XLIVE_TK=1 MGS2_XLIVE_TEX=1; st "d1tk rc $?"
wait; st "done"
