#!/bin/sh
# pgo.sh <step> ...: gl20's profile and code-order runs on the headless PPSSPP (2026-11-18; tools/dev/pgo_run.sh, sb17/pgo_gl.sh and
# psp/gl3.sh "model" mapped onto this tree).  Each run gets its own memory-stick root HOME=$B/ms/<tag> (ms0:/ = $HOME/.ppsspp, the game
# folder PSP/GAME/MGS2G20, ms0:/pgo for the counts), so runs can go side by side.  The game folder holds game data (the EBOOT, the
# overlays, SLES_503.83, pad.txt, the pack's link): its log, steps and crash files are copied to $B/<step dir>, then delete it by hand.
#   train <psp dir> <tag> <a> <b> [vblanks [cfg lines...]]   a PGO=gen build (psp.sh PGO=gen) on route i with cfg/mgs2_gl20pgo.cfg (gl19fb with cut b in
#        every turn from vb 24,000, no shots) and hashref cfg/hashref_gl20pgo.txt, plus "pgo a b" (main_psp.c pgo_vb: counts zeroed at a,
#        written at b); the *.gcda files to $B/pg/<tag>, the log to $B/pg/log_<tag>.txt.  Stops at [vblanks] (default 37,800).  The cfg
#        lines go last (they win): e.g. MGS2_ME_PIC=0 and MGS2_GL_TEXKB=1024 free about 5.5 MB for an instrumented build that does not
#        fit otherwise (the picture's memory only: the game's state and code paths are the same, so a game unit's counts are too).
#   trace <psp dir> <tag> <a> ...             PspProf's instruction trace (the second emulator, PPF) of 5-step windows ("prof a a+5 all",
#        up to 8), the training cfg: $B/lay/<tag>.it (PSPPROF_ITRACE_ALL, PSPPROF_ITRACE_MARK) and the windows' counts $B/lay/<tag>[.N]
#   model <psp dir> <tag> <cfg> <ref> <a:b> ...  PspProf counts ("prof a b all", up to 8 windows; PSPPROF_FA=1: the fully associative
#        I-cache bound too) with the given cfg and hashref: $B/model/<tag>[.N], the log $B/model/log_<tag>.txt
#   order <psp dir> <seed>   (no emulator) tools/layout_sim.c's order search (sb17/pgo_gl.sh order) on $B/lay/t0.it (the deck route's
#        8 windows: 2 in d00t, 2 in d01t, whose fetches in the overlay arena move up 16 MB, out of every function; 4 in w00a) and
#        t1.it (8 in w01a), the windows reordered so that the last two of each are held out (-test 2): t0 call 2 and the waiting deck,
#        t1 35,651 and 36,401; -wl 1 -wd 1, 40,000 moves, one main ELF order for both: $B/lay/order_s<seed>.txt.  IN=<order file>
#        starts the search from that order too (layout_sim -in), OPT=<moves> (default 40,000), JS=<threads> (default 2)
#   new <psp dir> <tag>      (no emulator) layout_sim -new: the misses of a relinked build's real addresses on both traces
. "$(dirname "$0")/env.sh"
PPF=${PPF:-/home/claude/ppsspp_prof/ppsspp-1.20.4/build/PPSSPPHeadless}   # PspProf as tools/ppsspp_pspprof (ITRACE, cache ops)
S=$1; D=$2; T=$3; shift 3
[ -f $D/EBOOT.PBP ] || { echo "pgo.sh: no $D/EBOOT.PBP"; exit 1; }
HM=$B/ms/$T; GD=$HM/.ppsspp/PSP/GAME/MGS2G20
case $S in train|trace|model) [ -e $HM ] && { echo "pgo.sh: $HM exists: look at it and delete it first"; exit 1; } ;; esac
LS=$B/layout_sim; [ -x $LS ] || gcc -O2 -pthread -o $LS $M/tools/layout_sim.c -lm || exit 1
bases() {   # <psp dir>: B0 = the main ELF's .text start, B1 = the overlay code's start (the arena + its 0x34-byte header)
  B0=$(psp-nm $1/MGS2T.elf | awk '$3 == "_ftext" { print $1 }')
  B1=$(printf %08x $((0x$(psp-nm $1/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
}
funcs() {   # <psp dir> <suffix>: layout_funcs.py's lists for the deck (w00a) and inside (w01a)
  python3 -I $M/tools/layout_funcs.py $1/MGS2T.elf $1/ovl_w00a.elf $1 w00a > $B/lay/funcs_deck$2.txt 2> $B/lay/funcs_deck$2.err
  python3 -I $M/tools/layout_funcs.py $1/MGS2T.elf $1/ovl_w01a.elf $1 w01a > $B/lay/funcs_w01a$2.txt 2> $B/lay/funcs_w01a$2.err
}
setup() {   # <cfg> <hashref> <extra cfg lines...>: the game folder
  c=$1; r=$2; shift 2; mkdir -p $GD/memcard0 $HM/.ppsspp/pgo || exit 1
  cp $D/EBOOT.PBP $D/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $PADF $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
    && sed '/^vblanks /d' $c > $GD/mgs2.cfg && cp $r $GD/hashref.txt || exit 1
  for l in "$@"; do echo "$l" >> $GD/mgs2.cfg; done
}
go() {   # <emulator> <max s> <wait file> [env...]: run until the wait file or steps.csv appears or the harness stops the machine
  em=$1; mx=$2; wf=$3; shift 3; s=$(date +%s); cd /home/claude || exit 1
  env HOME=$HM PPSSPP_VFPU_RN=1 "$@" $em $GD/EBOOT.PBP --graphics=software --timeout=$mx > $O/run_$T.out 2>&1 &
  pid=$!; i=0
  while [ $i -lt $mx ] && kill -0 $pid 2>/dev/null; do
    { [ -f $GD/steps.csv ] || { [ -n "$wf" ] && [ -f $wf ]; }; } && { sleep 5; break; }
    grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "pgo.sh: STOPPED: the harness stopped the machine"; break; }
    # the run's end before vb 31,800 ("deck") writes no steps.csv, and PPSSPP then races through the end screen while battery.txt
    # grows by tens of MB a minute (the first w00a training run: 360 MB): stop at the end line
    grep -q "^end: end of the run" $GD/log.txt 2>/dev/null && { sleep 10; echo "pgo.sh: the run's end"; break; }
    sleep 5; i=$((i + 5)); done
  kill $pid 2>/dev/null; wait $pid 2>/dev/null
  echo "pgo.sh: $T $(( $(date +%s) - s )) s"
  for f in log.txt steps.csv crash.txt battery.txt; do [ -f $GD/$f ] && cp $GD/$f $O/${f%.*}_$T.${f##*.}; done
}
F=$(psp-nm $D/MGS2T.elf | awk '$3 == "g_emuprof" { print $1 }')
case $S in
train)
  a=$1; b=$2; vb=${3:-37800}; O=$B/pg; mkdir -p $O; shift 2; [ $# -gt 0 ] && shift
  setup $M/cfg/mgs2_gl20pgo.cfg $M/cfg/hashref_gl20pgo.txt "vblanks $vb" "pgo $a $b" "$@"
  go $PPSSPP 9000 ""
  mkdir -p $O/$T && cp $HM/.ppsspp/pgo/*.gcda $O/$T/; ls -l $O/$T | tail -n +2 | awk '{ print $5, $9 }'
  grep "^pgo:" $O/log_$T.txt; python3 -I $M/tools/refcmp.py --partial $M/cfg/hashref_gl20pgo.txt $O/log_$T.txt ;;
trace)
  O=$B/lay; mkdir -p $O; L=; e=0; n=0
  for a in "$@"; do L="$L|prof $a $((a + 5)) all"; e=$((a + 10)); n=$((n + 1)); done
  IFS='|'; set -- $L; unset IFS; shift   # (the lines, the first field empty)
  setup $M/cfg/mgs2_gl20pgo.cfg $M/cfg/hashref_gl20pgo.txt "$@" "vblanks $e"
  go $PPF 9000 $O/$T.$((n - 1)).calls PSPPROF_FLAG=0x$F PSPPROF_OUT=$O/$T PSPPROF_SWITCH=1 PSPPROF_ITRACE=$O/$T.it PSPPROF_ITRACE_ALL=1 PSPPROF_ITRACE_MARK=1
  ls -l $O/$T.it; python3 -I $M/tools/refcmp.py --partial $M/cfg/hashref_gl20pgo.txt $O/log_$T.txt ;;
model)
  c=$1; r=$2; shift 2; O=$B/model; mkdir -p $O; L=; e=0; n=0
  for w in "$@"; do L="$L|prof ${w%:*} ${w#*:} all"; e=$((${w#*:} + 10)); n=$((n + 1)); done
  IFS='|'; set -- $L; unset IFS; shift
  setup $c $r "$@" "vblanks $e"; sed -i '/^MGS2_GL_SHOT=/d' $GD/mgs2.cfg
  W=$O/$T.calls; [ $n -gt 1 ] && W=$O/$T.$((n - 1)).calls
  go $PPF 9000 $W PSPPROF_FLAG=0x$F PSPPROF_OUT=$O/$T PSPPROF_SWITCH=1 PSPPROF_FA=1
  for f in $O/$T $O/$T.[0-9]*; do case $f in *.calls) ;; *) [ -f $f ] && echo "$(basename $f): $(head -n 1 $f)" ;; esac; done
  python3 -I $M/tools/refcmp.py --partial $r $O/log_$T.txt ;;
order)
  sd=$T; cd $B/lay || exit 1; bases $D
  [ -f funcs_deck.txt ] && [ -f funcs_w01a.txt ] || funcs $D
  A0=$(psp-nm $D/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $D/make.log | head -n 1 | cut -d= -f2)
  [ -f t0m.it ] && [ -f t1m.it ] || python3 -I -c "
import numpy as np
def wins(p):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == 9, (p, len(m)); return [a[m[k]:m[k + 1]] for k in range(8)]
d = wins('t0.it'); lo, hi = 0x$A0, 0x$A0 + $OS
for k in (0, 1, 2, 3):
    x = d[k].copy(); s = (x[:, 0] >= lo) & (x[:, 0] < hi); x[s, 0] += 0x1000000; d[k] = x
    print('t0 window', k, 'arena fetch runs moved:', int(s.sum()), 'of', len(x))
np.concatenate([d[k] for k in (0, 2, 4, 6, 1, 7, 3, 5)]).tofile('t0m.it'); del d
w = wins('t1.it'); np.concatenate([w[k] for k in (0, 1, 2, 3, 4, 7, 5, 6)]).tofile('t1m.it'); del w" > merge.out 2>&1
  echo "base0 $B0 base1 $B1" > bases.txt
  $LS t0m.it funcs_deck.txt -and t1m.it funcs_w01a.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -opt ${OPT:-40000} -j ${JS:-2} -seed $sd \
    ${IN:+-in $IN} -out order_s$sd.txt > ls_s$sd.out 2>&1; echo "order_s$sd rc $?"; tail -n 6 ls_s$sd.out ;;
new)
  cd $B/lay || exit 1; bases $D; funcs $D _$T
  $LS t0m.it funcs_deck.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -new funcs_deck_$T.txt > new_${T}_0.log 2>&1
  $LS t1m.it funcs_w01a.txt -wl 1 -wd 1 -test 2 -base0 $B0 -base1 $B1 -new funcs_w01a_$T.txt > new_${T}_1.log 2>&1
  tail -n 4 new_${T}_0.log new_${T}_1.log ;;
*) echo "pgo.sh train | trace | model | order | new"; exit 1 ;;
esac
