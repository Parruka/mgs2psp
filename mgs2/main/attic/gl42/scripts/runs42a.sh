#!/bin/bash
# runs42a.sh [arms] (gl42, THE TWIN'S ARMS on build_gl42/tw, one at a time, nice 19).  Every arm has gl41's native keys (N, X) and the
# digests' cfg (D), as build_gl41/runs41a.sh.  REF: hashref_gl38 (c9ddb593) for the arms whose state must be gl38's; gl42's own for the rest.
#  sch    gl42's schedule keys as the delivery cfg has them (tools/sched42.py; C2 on before and in every turn): ITS hashref is gl42's
#         (the people native change the state hash from vb ~24,200 on); the emulator's digests are checked against its lines
#  off41  gl41's schedule keys (build_gl41/tools/sched41.py, gl41's OPT2 words: C2 off) and MGS2_OPT2=fffff9df: gl42's code with step 2
#         off must give gl41's twin (hashref_gl38; digest lines a9d11ad8eed6 and content 073dd0e5adc7 x 14,303, gl41's tw2 sch arm)
#  noff   sch with MGS2_NGE_CYCLE all 0 and MGS2_GX_SEM=2 (drawn2): hashref_gl38; the content and drawn2 reference with C2 on
#  nchk   sch without MGS2_NGE_CYCLE, MGS2_NGE=2 (ship and people native the whole run), MGS2_NGE_CHK=1, MGS2_GX_SEM=2: every referenced
#         group checked (ship and people), content and drawn2 = noff's
#  lw7    nchk + MGS2_NAT_LW=2 (the PSP's late walk through gl40's copy plan, with gl42's copy: gl41's ship copy then the people's) +
#         MGS2_XP_CHECK=1 (every plan checked)
#  pgo    the PGO training's keys (tools/sched42.py pgokeys: the cycles to 128 turns, every kind past t73): the trainings' hashref
cd /home/claude/main/build_gl42; TW=${TW:-tw}; G2=${G2:-g1}; R38=/home/claude/main/build_gl41/cfg/hashref_gl38.txt; R42=${R42:-cfg/hashref_gl42.txt}
N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K41=$(python3 -I ../build_gl41/tools/sched41.py keys | tr '\n' ' ')
K=$(python3 -I tools/sched42.py keys | tr '\n' ' ')
KP=$(python3 -I tools/sched42.py pgokeys | tr '\n' ' ')
K0=$(python3 -I tools/sched42.py keys | sed '/^MGS2_NGE_CYCLE=/s/[12]/0/g' | tr '\n' ' ')
KN=$(python3 -I tools/sched42.py keys | grep -v '^MGS2_NGE_CYCLE=' | tr '\n' ' ')
C2="MGS2_OPT2=fffff9ff"
run() { t=$1; r=$2; shift 2; REF=$r sh tw_run42.sh $TW $t "$@" > ${G2}_$t.out 2>&1; gzip -f $TW/x86/out_$t.log; }
A=${*:-sch off41 noff nchk lw7 pgo}
for a in $A; do case $a in
  sch) run sch $R38 $X "$D" $K $C2 ;;
  off41) run off41 $R38 $X "$D" $K41 MGS2_OPT2=fffff9df ;;
  noff) run noff $R38 $X "$D" $K0 $C2 MGS2_GX_SEM=2 ;;
  nchk) run nchk $R42 $X "$D" $KN $C2 MGS2_NGE=2 MGS2_NGE_CHK=1 MGS2_GX_SEM=2 ;;
  lw7) run lw7 $R42 $X "$D" $KN $C2 MGS2_NGE=2 MGS2_NGE_CHK=1 MGS2_GX_SEM=2 MGS2_NAT_LW=2 MGS2_XP_CHECK=1 ;;
  pgo) run pgo $R42 $X "$D" $KP $C2 ;;
esac; done
for t in $A; do echo "$t: $(grep -a 'reference lines\|equals\|differ' ${G2}_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat $TW/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  content $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* content ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* content ')  drawn2 $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* drawn2 ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* drawn2 ')"
  echo "  $(zcat $TW/x86/out_$t.log.gz | grep -a 'gl40 XP totals\|NAT PROBLEMS\|^nat: PROBLEMS\|^nge chk\|nge: check\|^nge: native step 1 (\|^nge people totals\|^np6 \|split: cannot' | cut -c1-300 | tr '\n' '|')"; done
echo "runs42a.sh: done $(date -u +%H:%M:%S)"
