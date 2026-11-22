#!/bin/sh
# runs8.sh: me9 (task #130) what differs inside when the picture is one step late without the copy (id1): frames where the GE list
# differs with the same sizes (15759, 26188), with other sizes (23285), and content only (31663), saved from a normal run (jn) and a
# no-copy run (jd) for gerdiff.py; after runs7.sh
cd /home/claude/me9; L=/home/claude/me9/runs8.log
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
n=0; while ! grep -q 'done' runs7.log 2>/dev/null; do n=$((n+1)); [ $n -gt 2880 ] && { st "gave up waiting"; exit 1; }; sleep 10; done
st "start"
V=15759,23285,26188,31663
( sh chkd.sh i jng MGS2_XLIVE_GER=/home/claude/me9/ger/jn:$V; st "jng rc $?" ) &
sh chkd.sh i jdg MGS2_XLIVE_DEFER=1 MGS2_XLIVE_GER=/home/claude/me9/ger/jd:$V; st "jdg rc $?"
wait; st "done"
