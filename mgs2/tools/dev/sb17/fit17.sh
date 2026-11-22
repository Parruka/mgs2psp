#!/bin/sh
# fit17.sh <A> <B> [functions]: fitcmp of two model17 runs (sb17m/model/m_<A>, m_<B>), 80 steps, build sb17m/d (task #158)
M=/home/claude/sb17m/model; A=$1; B=$2; shift 2
python3 -I /mnt/project-files/mgs2/tools/dev/sb6/fitcmp.py $M/m_$A $M/m_$B 80 /home/claude/sb17m/d "$@"
