#!/bin/sh
# c2runs.sh: task #140, the C2 x86 build's checks on the three routes (x1), one setting after another (c2x.sh's three routes in parallel):
# t2 C2 on (C1 off), t0 both off (the default: must equal sb11 x4 out_*off5), t1 C1 on (must equal sb11 x4 out_*on5), t3 both on
L=/home/claude/sb12/c2x.log; echo "$(date +%H:%M:%S) c2runs: C2 build checks" >> $L
sh /home/claude/sb12/c2x.sh t2 MGS2_OPT2=ffffffef
sh /home/claude/sb12/c2x.sh t0
sh /home/claude/sb12/c2x.sh t1 MGS2_OPT2=ffffffdf
sh /home/claude/sb12/c2x.sh t3 MGS2_OPT2=ffffffff
echo "$(date +%H:%M:%S) c2runs done" >> $L
