#!/bin/bash
# runs40a.sh (gl40 step 7.2, THE TWIN'S ARMS on build_gl40/tw; against cfg/hashref_gl38.txt (c9ddb593), 685 lines each).  Every arm has
# gl39's native keys (N, X), the digests' cfg (D) and the reuse off.  MGS2_NAT_LW=2: every lane transfer through the PSP's copy plan
# (ge/xlme.c xm_plan, the arena, the late walk), so XP runs in the twin.
#  (run it with bash: wait -n)
#  sch   gl40's schedule keys (tools/sched40.py), no LW: the delivery cfg's arm (the emulator's digests are checked against it)
#  p0    gl39's schedule keys (build_gl39/tools/sched39.py), no LW: the new code with gl39's keys (digests vs build_gl39/tw2 sch)
#  m5off sch with M5 off in every turn (MGS2_OPT2 / _CYCLE fffbf9df)
#  lw0   sch + LW 2 + MGS2_XP_ALL=0 (gl39's plan in every plan)
#  lw3   sch + LW 2 + MGS2_XP_ALL=3 (XP's both parts in every plan): frames against lw0
#  lw7   sch + LW 2 + MGS2_XP_CHECK=1 (both parts + the check in every plan of the route): 0 differing
#  lws   sch + LW 2 (gl40's XP cycle as the cfg has it)
cd /home/claude/main/build_gl40; R=/home/claude/main/build_gl40/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K9=$(python3 -I ../build_gl39/tools/sched39.py keys | tr '\n' ' '); K=$(python3 -I tools/sched40.py keys | tr '\n' ' ')
F=fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run40.sh tw $t "$@" > g2_$t.out 2>&1; gzip -f tw/x86/out_$t.log; }
A=${*:-sch p0 m5off lw0 lw3 lw7 lws}
for a in $A; do case $a in
  sch) run sch $X "$D" $K & ;;
  p0) run p0 $X "$D" $K9 & ;;
  m5off) run m5off $X "$D" $K MGS2_OPT2=fffbf9df MGS2_OPT2_CYCLE=$F & ;;
  lw0) run lw0 $X "$D" $K MGS2_NAT_LW=2 MGS2_XP_ALL=0 & ;;
  lw3) run lw3 $X "$D" $K MGS2_NAT_LW=2 MGS2_XP_ALL=3 & ;;
  lw7) run lw7 $X "$D" $K MGS2_NAT_LW=2 MGS2_XP_CHECK=1 & ;;
  lws) run lws $X "$D" $K MGS2_NAT_LW=2 & ;;
esac; [ $(jobs -p | wc -l) -ge ${JN:-3} ] && wait -n 2>/dev/null || true; done; wait
for t in $A; do echo "$t: $(grep -a 'reference lines\|equals\|differ' g2_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat tw/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat tw/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  $(zcat tw/x86/out_$t.log.gz | grep -a 'gl40 XP totals\|NAT PROBLEMS\|^nat: PROBLEMS' | cut -c1-400 | tr '\n' '|')"; done
echo "runs40a.sh: done $(date -u +%H:%M:%S)"
