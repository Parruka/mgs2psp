#!/bin/sh
# runs39c.sh (gl39, THE GATE'S TWIN ARMS, on the twin rebuilt from the final sources: build_gl39/tw2, tw39.sh tw2).  Against
# cfg/hashref_gl38.txt (c9ddb593), 685 lines each:
#  p0     gl38's own keys and schedule (build_gl38/tools/sched38.py), no gl39 key: the new code with the old keys
#  m5off  p0 with M5 off in every turn (MGS2_OPT2 / _CYCLE fffbf9df): M5 alone (p0 has it on)
#  p1     no kinds, F1 on in every turn, the re-use off: the plain picture of gl39's defaults
#  sch    gl39's schedule (tools/sched39.py keys): the cfg's arm
#  sck    sch + MGS2_NAT_F1CHK=1 (the re-use off: the check has no notes to compare with; counted for the record)
#  ckr    sch + MGS2_NAT_F1CHK=1 + MGS2_NAT_REUSE_CYCLE=1,1,0,0: F1's check mode with the re-use's notes in half the turns
#  ckp    no kinds + MGS2_NAT_F1CHK=1 + MGS2_NAT_REUSE_CYCLE=1,1,1,1: F1's check mode against the notes in every turn
cd /home/claude/main/build_gl39; R=/home/claude/main/build_gl39/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K8=$(python3 -I ../build_gl38/tools/sched38.py keys | tr '\n' ' '); K=$(python3 -I tools/sched39.py keys | tr '\n' ' ')
F=fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df,fffbf9df
run() { t=$1; shift; REF=$R nice -n 10 sh tw_run39.sh tw2 $t "$@" > g2_$t.out 2>&1; gzip -f tw2/x86/out_$t.log; }
run p0 $X MGS2_NAT_REUSE_CYCLE=1,1,0,0 $K8 MGS2_GL_RAIN_NEAR=1 "$D" & run m5off $X MGS2_NAT_REUSE_CYCLE=1,1,0,0 $K8 MGS2_GL_RAIN_NEAR=1 "$D" MGS2_OPT2=fffbf9df MGS2_OPT2_CYCLE=$F & wait
run p1 $X "$D" MGS2_NAT_F1=1 & run sch $X "$D" $K & wait
run sck $X "$D" $K MGS2_NAT_F1CHK=1 & run ckr $X "$D" $K MGS2_NAT_F1CHK=1 MGS2_NAT_REUSE_CYCLE=1,1,0,0 & wait
run ckp $X "$D" MGS2_NAT_F1CHK=1 MGS2_NAT_REUSE_CYCLE=1,1,1,1
for t in p0 m5off p1 sch sck ckr ckp; do echo "$t: $(grep -a 'reference lines\|equals\|differ' g2_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat tw2/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat tw2/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  $(zcat tw2/x86/out_$t.log.gz | grep -a '^natf1: totals' | cut -c1-80) ... $(zcat tw2/x86/out_$t.log.gz | grep -a '^natf1: totals' | grep -o 'check mode.*' | cut -c1-200)"
  echo "  $(zcat tw2/x86/out_$t.log.gz | grep -a 'NAT PROBLEMS\|^nat: PROBLEMS\|reuse totals' | cut -c1-260 | tr '\n' '|')"; done
echo "runs39c.sh: done $(date -u +%H:%M:%S)"
