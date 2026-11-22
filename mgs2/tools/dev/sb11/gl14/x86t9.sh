#!/bin/sh
# x86t9.sh: gl14's x86 turns check with the nine-window cycle (task #136; x86t.sh had the first six-window one): sb11's twin x4 (C1's
# final form) on the three routes: everything on, GE overlap off, GE overlap off with C1 on, everything on, T1 off, everything on, T2
# off, everything on, C1 on; 100 vblanks each from vb 1000; out_{i,d,c}t9.log in x4; then t14cmp.py (from 1000, 100, 9 windows, C1 on in
# 2 and 8) against x4's C1 off and on runs (out_*off5.log, out_*on5.log).
L=/home/claude/sb11/c1x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }; export X=/home/claude/sb11/x4
E="MGS2_OPT_CYCLE=1000:100:ffffffff,ffffefff,ffffefff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff MGS2_OPT2_CYCLE=ffffffef,ffffffef,ffffffff,ffffffef,ffffffeb,ffffffef,ffffffe7,ffffffef,ffffffff"
st "start x4 t9 (gl14 nine-window turns)"
( sh /home/claude/sb11/chk11x.sh d dt9 $E; st "x4 dt9 rc $(cat $X/rc_dt9.txt)" ) &
( sh /home/claude/sb11/chk11x.sh i it9 $E; st "x4 it9 rc $(cat $X/rc_it9.txt)" ) &
sh /home/claude/sb11/chk11x.sh c ct9 $E; st "x4 ct9 rc $(cat $X/rc_ct9.txt)"
wait; st "done x4 t9"
