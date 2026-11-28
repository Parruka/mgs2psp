#!/bin/sh
# rt43a_t3.sh (gl43, 17:05): rt43a.sh's t3 and model mu again with gl38's guard cfg made B-off (cfg_emu/mgs2_m37_rc0_b43off.cfg): the
# file's OPT2 words fffff9df have bit 21, which gl43 reads as OPT2_B43, so the first t3 / mu ran B on (9 of 55 hashes differed from
# hashref_gl38 from vb 23,500; logs in old/guard_b_on).  nice 19, one heavy job.  Expected: hashref_gl38 in both.
R=/home/claude/main/build_gl43; cd $R/m || exit 1
[ -e $R/lay/t3.it ] && { echo "rt43a_t3.sh: $R/lay/t3.it exists"; exit 1; }
G=$R/cfg_emu/mgs2_m37_rc0_b43off.cfg; H38=/home/claude/main/build_gl41/cfg/hashref_gl38.txt
gd() { [ -d /home/claude/main/build_gl43/ms/$1/.ppsspp ] && rm -rf "/home/claude/main/build_gl43/ms/$1"; }
TCFG=$G TREF=$H38 nice -n 19 sh scripts/pgo.sh trace $R/u t3 16121 16161 21121 21161 23121 23161 27521 27561 > $R/trace_t3.out 2>&1; echo "t3 rc $? $(date -u +%H:%M:%S)"; gd t3
PSPPROF_DHIST=64 nice -n 19 sh scripts/pgo.sh model $R/u mu $G $H38 16110:16190 21110:21190 23110:23190 27510:27590 > $R/model_mu.out 2>&1; echo "mu rc $? $(date -u +%H:%M:%S)"; gd mu
tail -n 2 $R/trace_t3.out | cut -c1-200; tail -n 3 $R/model_mu.out | cut -c1-240; ls -la $R/lay/t3.it; echo "rt43a_t3.sh: done $(date -u +%H:%M:%S)"
