#!/bin/sh
# sumrun.sh <tag> <reference log>: me9 (task #130) one late-picture run against the normal run: state hashes, frames, the copy's totals
X=/home/claude/me9/xd; a=$X/out_$1.log; b=$2
echo "== $1 ($(cat $X/rc_$1.txt 2>/dev/null))"
ha=$(grep -c '^hash: vb' $a); hd=$(diff <(grep '^hash: vb' $a) <(grep '^hash: vb' $b) | grep -c '^[<>]'); echo "state hash lines $ha, differing lines $hd"
python3 -I /home/claude/me9/picdiff.py $a $b | head -14
grep -E '^me9 (totals|snap totals|transfers queued)' $a | cut -c1-900
grep -c '^me9 snap: vb' $a | sed 's/^/snap change lines printed: /'
grep '^me9 snap: vb' $a | awk "{print \$8}" | sort | uniq -c | sort -rn | head -8
