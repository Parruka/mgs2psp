#!/bin/sh
# emu.sh [max seconds]: step 6, the PSP build ($B/psp) on the headless PPSSPP 1.20.4 with the project's patch (PPSSPP_VFPU_RN=1,
# software graphics, the emulator's profiler off: NOPROF, no "prof" line), route i to vb 37,800 with gl18me's delivered cfg (cfg/,
# md5 9f46ad61: MGS2_OPT2=fffff9df, MGS2_ME_PIC=4, MGS2_ME_PIC_TURN=100, MGS2_HASH_REF=hashref.txt) and hashref.txt =
# prof/hashref_x86_in_gl18me.txt, as psp/gl3.sh "emu hw" ran gl18me (hwme5e).  PPSSPP has no Media Engine: in mode 4 the ME's loop runs
# as the "me_pic" thread of the main CPU (me_step4_notes.md 4c).  The game folder (a copy of the build, SLES_503.83, pad.txt, a link to the
# pack: game data) is HOME=/home/claude/ms_home's PSP/GAME/MGS2MAIN; its log, steps and crash files are copied to $B/emu and checked with
# tools/refcmp.py --partial (the harness hashes every 500 vblanks, every 100 from 31,800 and every vblank of 15,700-16,010: 434 lines,
# 311 vuh lines), and with tools/emucmp.py (frame digests and vuh lines against the x86 twin's log).  Afterwards delete the game folder
# by hand (about 30 MB with its log and the cfg's 16 shots: game pictures).
. "$(dirname "$0")/env.sh"
# (gl20: EMU_HOME= another memory-stick root, for a run while another session's game folder is still in /home/claude/ms_home)
MAX=${1:-9000}; HM=${EMU_HOME:-/home/claude/ms_home}; GD=$HM/.ppsspp/PSP/GAME/MGS2MAIN; E=$B/emu; P=$B/psp
# gl19: CFG= another cfg, REF= its hashref (an x86 twin run with the same settings: twin.sh's hashref_<tag>.txt), TW= that twin's log
CFG=${CFG:-$M/cfg/mgs2_gl18me.cfg}; REF=${REF:-$MG/prof/hashref_x86_in_gl18me.txt}; TW=${TW:-$B/x86/out_r18a.log}
[ -f $P/EBOOT.PBP ] || { echo "emu.sh: no $P/EBOOT.PBP (psp.sh)"; exit 1; }
[ -e $GD ] && { echo "emu.sh: $GD exists: look at it and delete it first"; exit 1; }
mkdir -p $GD/memcard0 $E || exit 1
cp $P/EBOOT.PBP $P/ovl_*.bin $GD/ && cp "$SL" $GD/SLES_503.83 && cp $PADF $GD/pad.txt && ln -s "$PAK" $GD/mgs2disc.pak \
  && cp $CFG $GD/mgs2.cfg && cp $REF $GD/hashref.txt || exit 1
s=$(date +%s); cd /home/claude || exit 1
HOME=$HM PPSSPP_VFPU_RN=1 $PPSSPP $GD/EBOOT.PBP --graphics=software --timeout=$MAX > $E/run.out 2>&1 &
pid=$!; i=0
while [ $i -lt $MAX ] && kill -0 $pid 2>/dev/null; do
  [ -f $GD/steps.csv ] && { sleep 5; break; }   # the harness writes steps.csv at the route's end, then waits on its end screen
  grep -q "^end: the machine stopped" $GD/log.txt 2>/dev/null && { echo "emu.sh: STOPPED: the harness stopped the machine"; break; }
  sleep 5; i=$((i + 5)); done
kill $pid 2>/dev/null; wait $pid 2>/dev/null
echo "emu.sh: $(( $(date +%s) - s )) s"
for f in log.txt steps.csv crash.txt battery.txt; do [ -f $GD/$f ] && cp $GD/$f $E/$f; done
python3 -I $M/tools/refcmp.py --partial $REF $E/log.txt; r=$?
echo "emu.sh: split lines: $(grep -c '^split: first\|^split: VU0' $E/log.txt)"
# the frame digests of the cfg's window (31,561-31,959) and the 311 vuh lines against the x86 twin's (twin.sh's log, when it is there)
[ -f $TW ] && { python3 -I $M/tools/emucmp.py $E/log.txt $TW || r=1; }
grep -i "headroom\|^end:" $E/log.txt | tail -n 4
exit $r
