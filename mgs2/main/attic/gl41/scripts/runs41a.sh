#!/bin/bash
# runs41a.sh (gl41, THE TWIN'S ARMS on build_gl41/tw; against cfg/hashref_gl38.txt (c9ddb593), 685 lines each).  Every arm has gl40's
# native keys (N, X), the digests' cfg (D) and the reuse off, as build_gl40/runs40a.sh.  (run it with bash: wait -n)
#  off40  gl40's schedule keys (build_gl40/tools/sched40.py) with every gl41 switch off (MGS2_NGE unset; P1 and P3 off: bits 19, 20
#         clear in every word): frames = gl40's sch arm (b8a2ae4fb016 x 16,903): gl41's code with its switches off draws gl40's picture
#  off    gl41's schedule keys (tools/sched41.py) with every gl41 switch off (MGS2_NGE_CYCLE all 0, words ffe7f9df)
#  p1     off + P1 on everywhere (ffeff9df), P3 off       p3  off + P3 on everywhere (fff7f9df), P1 off
#  sch    gl41's schedule keys as the delivery cfg has them (the emulator's digests are checked against it)
#  noff   sch with MGS2_NGE_CYCLE all 0 and MGS2_GX_SEM=2 (drawn2): the content and drawn2 reference for nchk and lw7
#  nchk   sch without MGS2_NGE_CYCLE, MGS2_NGE=1 (on in the whole run), MGS2_NGE_CHK=1, MGS2_GX_SEM=2: every referenced group checked
#  lw7    nchk + MGS2_NAT_LW=2 (the PSP's late walk through gl40's copy plan) + MGS2_XP_CHECK=1 (every plan checked)
# gl41 (23:45): TW=<twin folder> (default tw), G2=<prefix of the arms' outputs> (default g2)
cd /home/claude/main/build_gl41; TW=${TW:-tw}; G2=${G2:-g2}; R=/home/claude/main/build_gl41/cfg/hashref_gl38.txt; N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K40=$(python3 -I ../build_gl40/tools/sched40.py keys | sed -e 's/fffbf9df/ffe3f9df/g' -e 's/fffff9df/ffe7f9df/g' | tr '\n' ' ')
K=$(python3 -I tools/sched41.py keys | tr '\n' ' ')
KOFF=$(python3 -I tools/sched41.py keys | sed -e 's/fff7f9df\|ffeff9df\|fffff9df/ffe7f9df/g' -e '/^MGS2_NGE_CYCLE=/s/1/0/g' | tr '\n' ' ')
KP1=$(echo "$KOFF" | sed 's/ffe7f9df/ffeff9df/g'); KP3=$(echo "$KOFF" | sed 's/ffe7f9df/fff7f9df/g')
K0=$(python3 -I tools/sched41.py keys | sed '/^MGS2_NGE_CYCLE=/s/1/0/g' | tr '\n' ' ')
KN=$(python3 -I tools/sched41.py keys | grep -v '^MGS2_NGE_CYCLE=' | tr '\n' ' ')
run() { t=$1; shift; REF=$R nice -n ${NI:-10} sh tw_run41.sh $TW $t "$@" > ${G2}_$t.out 2>&1; gzip -f $TW/x86/out_$t.log; }
A=${*:-off40 off p1 p3 sch noff nchk lw7}
for a in $A; do case $a in
  off40) run off40 $X "$D" $K40 MGS2_OPT2=ffe7f9df & ;;
  off) run off $X "$D" $KOFF MGS2_OPT2=ffe7f9df & ;;
  p1) run p1 $X "$D" $KP1 MGS2_OPT2=ffeff9df & ;;
  p3) run p3 $X "$D" $KP3 MGS2_OPT2=fff7f9df & ;;
  sch) run sch $X "$D" $K & ;;
  noff) run noff $X "$D" $K0 MGS2_GX_SEM=2 & ;;
  nchk) run nchk $X "$D" $KN MGS2_NGE=1 MGS2_NGE_CHK=1 MGS2_GX_SEM=2 & ;;
  lw7) run lw7 $X "$D" $KN MGS2_NGE=1 MGS2_NGE_CHK=1 MGS2_GX_SEM=2 MGS2_NAT_LW=2 MGS2_XP_CHECK=1 & ;;
esac; [ $(jobs -p | wc -l) -ge ${JN:-3} ] && wait -n 2>/dev/null || true; done; wait
for t in $A; do echo "$t: $(grep -a 'reference lines\|equals\|differ' ${G2}_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat $TW/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  content $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* content ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* content ')  drawn2 $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* drawn2 ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* drawn2 ')"
  echo "  $(zcat $TW/x86/out_$t.log.gz | grep -a 'gl40 XP totals\|NAT PROBLEMS\|^nat: PROBLEMS\|^nge chk\|nge: check\|^nge: native step 1 (' | cut -c1-400 | tr '\n' '|')"; done
echo "runs41a.sh: done $(date -u +%H:%M:%S)"
