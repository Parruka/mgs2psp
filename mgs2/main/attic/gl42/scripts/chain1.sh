#!/bin/sh
# chain1.sh: one heavy job at a time: the twin arms off41 noff nchk lw7 pgo (runs42a.sh), then the no-PGO PSP build n42 + its package (pkn42.sh)
cd /home/claude/main/build_gl42 || exit 1
bash runs42a.sh off41 noff nchk lw7 pgo > runs42a_b.out 2>&1
sh pkn42.sh > pkn42.out 2>&1
echo "chain1.sh: done $(date -u +%H:%M:%S)"
