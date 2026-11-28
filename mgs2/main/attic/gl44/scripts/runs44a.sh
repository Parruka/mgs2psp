#!/bin/bash
# runs44a.sh [arms] (gl44, THE TWIN'S ARMS on build_gl44/tw, nice 19, up to four at once): as build_gl43/runs43a.sh (gl42's native keys N, X and
# the digests' cfg D in every arm), each with the seed log (MGS2_B43_SEEDLOG) and the act log (MGS2_ACTLOG) from vb 23,000 and MGS2_HALF=10
# (hex 0x10: a bit of no half table entry, so every act from the start comes through rt_half_skip and both logs see w00a's first visit).
#  off43  gl43's schedule keys (build_gl43/tools/sched43.py) with bit 22 CLEARED in every word (ffdff9ff -> ff9ff9ff, fffff9ff -> ffbff9ff):
#         gl44's code with V44 never on must be gl43's twin (hashref_gl43 byte for byte, its digests, content and seed log)
#  s44n   gl44's schedule keys (tools/sched44.py) with V44 cleared in every word (fffff9ff -> ffbff9ff): the V-off arm of gl44's table
#  s44    gl44's schedule keys as the delivery cfg has them: ITS hashref is gl44's; seed log, act log, iop, vfb, pictures = s44n's
#  von    s44 with V44 on in EVERY word, MGS2_OPT2 too (ff9ff9ff -> ffdff9ff, ffbff9ff -> fffff9ff): the most V (w00a's first visit included)
#  pgo    the PGO training's keys (sched44.py pgokeys: 128 turns): the trainings' hashref (hashref_gl44pgo)
#  pgo0   pgo with MGS2_NGE_CYCLE all 0: the reference of the trainings that run with MGS2_ME_PIC=0
cd /home/claude/main/build_gl44; TW=${TW:-tw}; G2=${G2:-g1}; R43=/home/claude/main/build_gl43/cfg/hashref_gl43.txt
N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K43=$(python3 -I ../build_gl43/tools/sched43.py keys | sed '/^MGS2_OPT2_CYCLE=/{s/ffdff9ff/ff9ff9ff/g;s/fffff9ff/ffbff9ff/g}' | tr '\n' ' ')
K=$(python3 -I tools/sched44.py keys | tr '\n' ' ')
KN=$(python3 -I tools/sched44.py keys | sed '/^MGS2_OPT2_CYCLE=/s/fffff9ff/ffbff9ff/g' | tr '\n' ' ')
KV=$(python3 -I tools/sched44.py keys | sed '/^MGS2_OPT2_CYCLE=/{s/ff9ff9ff/ffdff9ff/g;s/ffbff9ff/fffff9ff/g}' | tr '\n' ' ')
KP=$(python3 -I tools/sched44.py pgokeys | tr '\n' ' ')
C2="MGS2_OPT2=ff9ff9ff MGS2_GL_RAIN_NEAR=1"; C2V="MGS2_OPT2=ffdff9ff MGS2_GL_RAIN_NEAR=1"
run() { t=$1; r=$2; shift 2; REF=$r sh tw_run44.sh $TW $t "$@" $EXTRA MGS2_HALF=10 MGS2_B43_SEEDLOG=23000:37800:$PWD/$TW/x86/seed_$t.txt \
          MGS2_ACTLOG=$PWD/$TW/x86/act_$t.txt:23000:37800 > ${G2}_$t.out 2>&1; gzip -f $TW/x86/out_$t.log $TW/x86/seed_$t.txt $TW/x86/act_$t.txt; }
one() { case $1 in
  off43) run off43 $R43 $X "$D" $K43 $C2 ;;
  s44n) run s44n $R43 $X "$D" $KN $C2 ;;
  s44) run s44 $R43 $X "$D" $K $C2 ;;
  von) run von $R43 $X "$D" $KV $C2V ;;
  pgo) run pgo $R43 $X "$D" $KP $C2 ;;
  pgo0) run pgo0 $R43 $X "$D" $(echo $KP | tr ' ' '\n' | sed '/^MGS2_NGE_CYCLE=/s/=.*/=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000/' | tr '\n' ' ') $C2 ;;
  *) echo "runs44a.sh: no arm $1"; return 1 ;; esac; }
A=${*:-off43 s44n s44 von}
for a in $A; do one $a & done; wait
for t in $A; do echo "$t: $(grep -a 'reference lines\|equals\|differ' ${G2}_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat $TW/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  content $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* content ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* content ')"
  echo "  $(zcat $TW/x86/out_$t.log.gz | grep -a '^gl44 study\|^native line_sph' | cut -c1-200 | tr '\n' '|')"
  echo "  seed log: sv $(zcat $TW/x86/seed_$t.txt.gz | grep -c '^sv ') lines md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^sv ' | md5sum | cut -c1-12); sd $(zcat $TW/x86/seed_$t.txt.gz | grep -c '^sd ') md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^sd ' | md5sum | cut -c1-12); st md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^st ' | md5sum | cut -c1-12)"
  echo "  act log: P $(zcat $TW/x86/act_$t.txt.gz | grep -c '^P ') lines md5 $(zcat $TW/x86/act_$t.txt.gz | grep '^P ' | md5sum | cut -c1-12)"; done
echo "runs44a.sh: done $(date -u +%H:%M:%S)"
