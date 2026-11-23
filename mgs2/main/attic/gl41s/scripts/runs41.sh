#!/bin/sh
# runs41.sh <twin folder> <arm...> (gl41s): x86 twin arms of route i to vb 37,800 against build_gl39/cfg/hashref_gl38.txt (c9ddb593, 685
# lines), with gl39's cfg schedule (build_gl39/tools/sched39.py keys and runs39c.sh's sch arm: X and D) and these MGS2_OPT2 words:
#  on   gl39's MGS2_OPT2_CYCLE (bits 19-21 set: P1, P3, E6 on in every turn) + probe (c) at the 5000th box cull + the gl41 counter lines
#  alt  gl39's cycle with bits 19-21 cleared by slot: slot i clears (i & 7) << 19 (all 8 on/off patterns twice in 16 turns)
#       + MGS2_E6_CHECK=1 (every E6 hit's hash computed by FNV-1a too and compared) + the counter lines
#  off  gl39's cycle and start word with bits 19-21 cleared in every turn (ffc7f9df)
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gl41s; M=/home/claude/main; R=$M/build_gl39/cfg/hashref_gl38.txt
B=$S/$1; shift
N="MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=1"; D="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"; X="$N MGS2_NAT_CULL=1"
K=$(cd $M/build_gl39 && python3 -I tools/sched39.py keys | grep -v '^MGS2_OPT2_CYCLE=' | tr '\n' ' ')
O39=$(cd $M/build_gl39 && python3 -I tools/sched39.py keys | grep '^MGS2_OPT2_CYCLE=' | cut -d= -f2)
cyc() { python3 -I -c "import sys; w=sys.argv[1].split(','); m=sys.argv[2]; print(','.join('%08x' % (int(x,16) & ~((((i & 7) << 19) if m == 'alt' else 0x380000))) for i, x in enumerate(w)))" $O39 $1; }
for a in "$@"; do
  case $a in
    on)  E="MGS2_OPT2_CYCLE=$O39 MGS2_PROBE41_CULL=5000 MGS2_L41_LOG=1" ;;
    alt) E="MGS2_OPT2_CYCLE=$(cyc alt) MGS2_E6_CHECK=1 MGS2_L41_LOG=1" ;;
    off) E="MGS2_OPT2_CYCLE=$(cyc off) MGS2_OPT2=ffc7f9df" ;;
    *) echo "runs41.sh: arm $a?"; exit 1 ;;
  esac
  echo "arm $a: $E"
  ( B=$B REF=$R nice -n 10 sh $M/scripts/twin.sh $a MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b $X "$D" $K $E > $B/g_$a.out 2>&1
    echo "$a: $(grep -a 'reference lines\|equals\|differ' $B/g_$a.out | tr '\n' ' ' | cut -c1-240)" >> $B/g_$a.out
    echo "  digests $(grep -a 'frame [0-9a-f]\{16\}' $B/x86/out_$a.log | md5sum | cut -c1-12) $(grep -a -c 'frame [0-9a-f]\{16\}' $B/x86/out_$a.log)" >> $B/g_$a.out ) &
done
wait; for a in "$@"; do tail -n 2 $B/g_$a.out; done; echo "runs41.sh: done $(date -u +%H:%M:%S)"
