#!/bin/sh
# runs_l.sh <dir> <tag>: a plain ME-safe twin (mk.sh's output) on the three routes in parallel, each compared with sb14's x1 run
# (cmp13.py: hash lines and frames), then the picture heap's lines (task #149: the local copy's check before its buffer)
X=$1; t=$2; export X; s=$(date +%s)
nice -n 10 sh /home/claude/sb14/chk14.sh d d$t & nice -n 10 sh /home/claude/sb14/chk14.sh i i$t & nice -n 10 sh /home/claude/sb14/chk14.sh c c$t & wait
echo "runs $t done in $(( $(date +%s) - s )) s"
for r in d i c; do
  echo "$r: $(PYTHONDONTWRITEBYTECODE=1 python3 -I /home/claude/me13/tools/cmp13.py /home/claude/sb14/x1/out_${r}t0.log $X/out_$r$t.log 2>&1 | head -1)"
  grep -a "picture heap" $X/out_$r$t.log | cut -c1-330
  grep -a "texture sources:" $X/out_$r$t.log | cut -c1-200
done
