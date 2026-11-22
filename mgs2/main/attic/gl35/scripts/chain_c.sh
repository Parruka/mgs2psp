#!/bin/sh
# chain_c.sh: post_b35.sh as soon as pgo_b35.sh has ended
R=/home/claude/main/build_gl35
until grep -q 'pgo_b35.sh: done\|stage a did not end well' $R/pgo_b35.out || ! pgrep -f pgo_b35.sh > /dev/null; do sleep 10; done
grep -q 'relink rc 0' $R/pgo_b35.out || { echo "chain_c.sh: no relink"; exit 1; }
sh $R/post_b35.sh
