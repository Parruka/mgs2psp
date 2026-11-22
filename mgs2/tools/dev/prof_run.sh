#!/bin/sh
# prof_run.sh <game dir> <flag hex> <out> <max seconds>: headless PPSSPP (JIT, the interpreter + PspProf while the flag is set); stops once the dump exists
G=$1; F=$2; OUT=$3; MAX=$4; WAIT=${5:-$OUT.calls}
rm -f $OUT $OUT.calls $OUT.1 $OUT.1.calls $G/log.txt $G/steps.csv
cd /home/claude
HOME=/home/claude/ms_home PSPPROF_FLAG=$F PSPPROF_OUT=$OUT PSPPROF_SWITCH=1 PSPPROF_FA=$FA /home/claude/ppsspp_src/ppsspp-1.20.4/build/PPSSPPHeadless $G/EBOOT.PBP --graphics=software --timeout=$MAX > $OUT.run 2>&1 &
P=$!
i=0; while [ $i -lt $MAX ] && kill -0 $P 2>/dev/null; do if [ -f $WAIT ]; then sleep 5; break; fi
  # a run the harness stopped (missing function, no memory: "end: the machine stopped") never writes $WAIT and waits for X, while
  # PPSSPP races through the idle time and battery.txt grows by tens of MB a minute (gl3 deck, 2026-10-27): stop it at once
  if grep -q "^end: the machine stopped" $G/log.txt 2>/dev/null; then echo "STOPPED: the harness stopped the machine (log.txt)"; break; fi
  sleep 5; i=$((i+5)); done
kill $P 2>/dev/null; wait $P 2>/dev/null
echo "DONE after $i s"; grep PspProf $OUT.run | head -c 2000
