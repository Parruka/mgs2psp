#!/bin/sh
# fit17c.sh <A> <B> [functions]: fitcmp of two models17c runs (sb17m2/model/c_<A>, c_<B>), 80 steps, build sb17m2/d (task #158)
M=/home/claude/sb17m2/model; A=$1; B=$2; shift 2
python3 -I /mnt/project-files/mgs2/tools/dev/sb6/fitcmp.py $M/c_$A $M/c_$B 80 /home/claude/sb17m2/d "$@"
