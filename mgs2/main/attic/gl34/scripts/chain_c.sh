#!/bin/sh
# chain_c.sh: post_b34.sh as soon as pgo_b34.sh has ended
R=/home/claude/main/build_gl34
until grep -q 'pgo_b34.sh: done\|stage a did not end well' $R/pgo_b34.out || ! pgrep -f pgo_b34.sh > /dev/null; do sleep 10; done
grep -q 'relink rc 0' $R/pgo_b34.out || { echo "chain_c.sh: no relink"; exit 1; }
sh $R/post_b34.sh
