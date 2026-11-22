#!/bin/sh
# runs1.sh: me11, two steps late (D2): the copy laid over RAM with everything (all), without the upload granules (noup: texture sources
# read late), tags and the data after them only (tin: REF data read late too); on the deck, the indoor route and the codec route; two at a
# time, after me10's census
cd /home/claude/me11 || exit 1
L=/home/claude/me11/runs1.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q done /home/claude/me10/runs1.log 2>/dev/null; do n=$((n+1)); [ $n -gt 720 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"
D="MGS2_XLIVE_DEFER=2 MGS2_XLIVE_SNAP=2"
( sh chkd.sh d dall $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=1; st "dall $(cat xd/rc_dall.txt)" ) &
sh chkd.sh d dnoup $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0; st "dnoup $(cat xd/rc_dnoup.txt)"
wait
( sh chkd.sh d dtin $D MGS2_XLIVE_SNAPK=3 MGS2_XLIVE_SNAPU=0; st "dtin $(cat xd/rc_dtin.txt)" ) &
sh chkd.sh i iall $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=1; st "iall $(cat xd/rc_iall.txt)"
wait
( sh chkd.sh i inoup $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0; st "inoup $(cat xd/rc_inoup.txt)" ) &
sh chkd.sh i itin $D MGS2_XLIVE_SNAPK=3 MGS2_XLIVE_SNAPU=0; st "itin $(cat xd/rc_itin.txt)"
wait
( CEND=40000 sh chkd.sh c call $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=1; st "call $(cat xd/rc_call.txt)" ) &
CEND=40000 sh chkd.sh c cnoup $D MGS2_XLIVE_SNAPK=15 MGS2_XLIVE_SNAPU=0; st "cnoup $(cat xd/rc_cnoup.txt)"
wait
CEND=40000 sh chkd.sh c ctin $D MGS2_XLIVE_SNAPK=3 MGS2_XLIVE_SNAPU=0; st "ctin $(cat xd/rc_ctin.txt)"
st "done"
