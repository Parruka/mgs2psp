#!/bin/bash
# runs43a.sh [arms] (gl43, THE TWIN'S ARMS on build_gl43/tw, one at a time, nice 19): every arm has gl42's native keys (N, X) and the
# digests' cfg (D), as build_gl42/runs42a.sh; the seed log (MGS2_B43_SEEDLOG, boot_x86.c b43_seedlog) from vb 28,900 in each arm.
#  off  gl42's schedule keys with B (bit 21) out of every OPT2 word: gl43's code with B never on must be gl42's twin (hashref_gl42 byte for byte)
#  chk  gl43's schedule keys (tools/sched43.py) and MGS2_B43_CHECK=1: in the B turns every call of the four functions runs as translated and
#       is compared with the shadow's prediction (0 different); the state = off's but the prims' hide bits in the B turns
#  sch  gl43's schedule keys as the delivery cfg has them: ITS hashref is gl43's (the B turns change the state hash); seed log = off's
#  bon  sch with B on in every turn from t50 (vb 29,000): the most B; seed log = off's
#  pgo  the PGO training's keys (tools/sched43.py pgokeys: 128 turns): the trainings' hashref (hashref_gl43pgo)
#  pgo0 pgo with MGS2_NGE_CYCLE all 0: the reference of the trainings that run with MGS2_ME_PIC=0 (game, w00a, w01a: nothing native, B in its turns)
cd /home/claude/main/build_gl43; TW=${TW:-tw}; G2=${G2:-g1}; R42=$PWD/cfg/hashref_gl42.txt; R43=${R43:-$PWD/cfg/hashref_gl43.txt}
N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"
D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K42=$(python3 -I ../build_gl42/tools/sched42.py keys | sed 's/fffff9ff/ffdff9ff/g' | tr '\n' ' ')
K=$(python3 -I tools/sched43.py keys | tr '\n' ' ')
KP=$(python3 -I tools/sched43.py pgokeys | tr '\n' ' ')
KB=$(python3 -I tools/sched43.py keys | python3 -I -c "
import sys
for l in sys.stdin.read().split('\n'):
    if l.startswith('MGS2_OPT2_CYCLE='): w = l.split('=')[1].split(','); w = w[:50] + ['fffff9ff'] * (len(w) - 50); l = 'MGS2_OPT2_CYCLE=' + ','.join(w)
    if l: print(l)" | tr '\n' ' ')
C2="MGS2_OPT2=ffdff9ff MGS2_GL_RAIN_NEAR=1"
run() { t=$1; r=$2; shift 2; REF=$r sh tw_run43.sh $TW $t "$@" $EXTRA MGS2_B43_SEEDLOG=28900:37800:$PWD/$TW/x86/seed_$t.txt > ${G2}_$t.out 2>&1; gzip -f $TW/x86/out_$t.log $TW/x86/seed_$t.txt; }
A=${*:-off chk sch bon}
for a in $A; do case $a in
  off|offd) run $a $R42 $X "$D" $K42 $C2 ;;
  chk) run chk $R42 $X "$D" $K $C2 MGS2_B43_CHECK=1 ;;
  sch|schd) run $a $R42 $X "$D" $K $C2 ;;
  bon) run bon $R42 $X "$D" $KB $C2 ;;
  pgo) run pgo $R42 $X "$D" $KP $C2 ;;
  pgo0) run pgo0 $R42 $X "$D" $(echo $KP | tr ' ' '\n' | sed '/^MGS2_NGE_CYCLE=/s/=.*/=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000/' | tr '\n' ' ') $C2 ;;
esac; done
for t in $A; do echo "$t: $(grep -a 'reference lines\|equals\|differ' ${G2}_$t.out | tr '\n' ' ' | cut -c1-200)"
  echo "  digest lines $(zcat $TW/x86/out_$t.log.gz | grep -a 'frame [0-9a-f]\{16\}' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'frame [0-9a-f]\{16\}')"
  echo "  content $(zcat $TW/x86/out_$t.log.gz | grep -a 'picture: vb [0-9]* content ' | md5sum | cut -c1-12) $(zcat $TW/x86/out_$t.log.gz | grep -a -c 'picture: vb [0-9]* content ')"
  echo "  $(zcat $TW/x86/out_$t.log.gz | grep -a '^b43: \|^b43chk' | cut -c1-260 | tr '\n' '|')"
  echo "  seed log: sv $(zcat $TW/x86/seed_$t.txt.gz | grep -c '^sv ') lines md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^sv ' | md5sum | cut -c1-12); sd $(zcat $TW/x86/seed_$t.txt.gz | grep -c '^sd ') md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^sd ' | md5sum | cut -c1-12); st md5 $(zcat $TW/x86/seed_$t.txt.gz | grep '^st ' | md5sum | cut -c1-12)"; done
echo "runs43a.sh: done $(date -u +%H:%M:%S)"
