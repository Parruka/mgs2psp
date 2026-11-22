#!/bin/sh
# x86t15.sh: gl15's x86 turns check (task #140) with its nine-window cycle on sb12's twin x2 (built with gl15's default, C1 on): the
# three routes with everything on (C2 off), GE overlap off, GE overlap off with C2 on, everything on, C2 on, everything on, C2 on,
# everything on, C2 on; 100 vblanks each from vb 1000; out_{i,d,c}t15.log in x2; then t14cmp.py (from 1000, 100, 9 windows, C2 on in 2,
# 4, 6 and 8) against x1's C1-on runs with C2 off and on (out_*t1.log, out_*t3.log).
L=/home/claude/sb12/c2x.log; st() { echo "$(date +%H:%M:%S) $*" >> $L; }; export X=/home/claude/sb12/x2
E="MGS2_OPT_CYCLE=1000:100:ffffffff,ffffefff,ffffefff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff,ffffffff MGS2_OPT2_CYCLE=ffffffdf,ffffffdf,ffffffff,ffffffdf,ffffffff,ffffffdf,ffffffff,ffffffdf,ffffffff"
st "start x2 t15 (gl15 nine-window turns)"
( sh /home/claude/sb12/chk12x.sh d dt15 $E; st "x2 dt15 rc $(cat $X/rc_dt15.txt)" ) &
( sh /home/claude/sb12/chk12x.sh i it15 $E; st "x2 it15 rc $(cat $X/rc_it15.txt)" ) &
sh /home/claude/sb12/chk12x.sh c ct15 $E; st "x2 ct15 rc $(cat $X/rc_ct15.txt)"
wait; st "done x2 t15"
