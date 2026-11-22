#!/bin/sh
# runs5.sh: me9 (task #130) the indoor route with the picture one step late and no copy (id1): where a step's transfers come without the
# copy's own time (is2's copy runs inside the step), and the frames that differ without the copy; starts when runs4.sh is done
cd /home/claude/me9; L=/home/claude/me9/runs5.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q ' done$' runs4.log 2>/dev/null; do n=$((n+1)); [ $n -gt 1080 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"; sh chkd.sh i id1 MGS2_XLIVE_DEFER=1; st "id1 rc $?"
