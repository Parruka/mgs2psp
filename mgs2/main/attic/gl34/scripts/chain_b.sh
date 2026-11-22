#!/bin/sh
# chain_b.sh: pgo_b34.sh as soon as pgo_a34.sh has ended well (its 'done' line and 20 profiles in gcda)
R=/home/claude/main/build_gl34
until grep -q 'pgo_a34.sh: done\|rc [1-9]' $R/pgo_a34.out; do sleep 10; done
grep -q 'pgo_a34.sh: done' $R/pgo_a34.out && [ "$(ls $R/gcda/*.gcda | wc -l)" = 20 ] || { echo "chain_b.sh: stage a did not end well"; exit 1; }
grep -c 'compared .* 0 differ' $R/pgo_a34.out
sh $R/pgo_b34.sh
