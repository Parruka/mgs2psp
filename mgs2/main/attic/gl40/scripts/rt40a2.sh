#!/bin/sh
# rt40a2.sh (gl40): rt40a.sh's steps 3 and 4 again (t3 = the guard cfg's eight 5-vblank windows; the model of u on the four 80-vblank
# guard windows): the first try was cut at 16:55 by the session's pause (t3 had 6 of 8 windows, mu 3 of 4: set aside in x_int/).
R=/home/claude/main/build_gl40; cd $R/m || exit 1; G=/home/claude/main/build_gl38/cfg_emu/mgs2_m37_rc0.cfg
[ -e $R/lay/t3.it ] && { echo "rt40a2.sh: lay/t3.it exists"; exit 1; }
TCFG=$G nice -n 10 sh scripts/pgo.sh trace $R/u t3 16121 16161 21121 21161 23121 23161 27521 27561 > $R/trace_t3.out 2>&1 &
PSPPROF_DHIST=64 nice -n 10 sh scripts/pgo.sh model $R/u mu $G $R/cfg/hashref_gl38.txt 16110:16190 21110:21190 23110:23190 27510:27590 > $R/model_mu.out 2>&1 &
wait
[ -d $R/ms/t3/.ppsspp ] && rm -rf "/home/claude/main/build_gl40/ms/t3"; [ -d $R/ms/mu/.ppsspp ] && rm -rf "/home/claude/main/build_gl40/ms/mu"
tail -n 3 $R/trace_t3.out | cut -c1-200; cat $R/model_mu.out | cut -c1-240; ls -la $R/lay/t3.it; echo "rt40a2.sh: done $(date -u +%H:%M:%S)"
