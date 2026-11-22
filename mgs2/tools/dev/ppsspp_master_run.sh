#!/bin/sh
# ppsspp_master_run.sh <game dir> <out>: PPSSPP master (patched, /home/claude/ppm/build) on a game folder under /home/claude/msm, software GE,
# PPSSPP_VFPU_RN=1 and PPSSPP_FPU_FS=1 (the PSP-3000's VFPU rounding and FPU denormal rules); stops it once the harness has written its end lines (it then waits on its end screen, as on the PSP), if the harness
# stops the machine, or if battery.txt passes 64 MB.  The build (2026-10-29): master 83e616ea, a shallow clone with submodules (the
# test-data ones pspautotests and frametests removed), tools/ppsspp_master_vfpu_rn.diff and (2026-10-30) tools/ppsspp_master_fpu_fs.diff applied, then tools/ppsspp_master_vfpu_rn.diff's
# cmake line and ninja PPSSPPHeadless (needs libgl1-mesa-dev).  The game folder holds a delivery's files with MGS2_EXC=0 in mgs2.cfg.
G=$1; OUT=$2; MAX=${MAX:-3600}
cd /home/claude
PPSSPP_VFPU_RN=1 PPSSPP_FPU_FS=1 /home/claude/ppm/build/PPSSPPHeadless $G/EBOOT.PBP --memstick=/home/claude/msm --graphics=software --timeout=$MAX > $OUT 2>&1 &
P=$!; s=$(date +%s)
while kill -0 $P 2>/dev/null; do
  if grep -q "^end: the machine stopped" $G/log.txt 2>/dev/null; then echo "STOPPED: the harness stopped the machine"; kill $P; break; fi
  # the harness waits on its end screen (never an auto-exit on the PSP): stop PPSSPP once the log's end lines are written
  if grep -q "^disc pack: " $G/log.txt 2>/dev/null; then sleep 5; echo "END: the run's end lines are in log.txt"; kill $P; break; fi
  if [ "$(stat -c %s $G/battery.txt 2>/dev/null || echo 0)" -gt 67108864 ]; then echo "STOPPED: battery.txt over 64 MB"; kill $P; break; fi
  sleep 10
done
wait $P 2>/dev/null
echo "DONE after $(( $(date +%s) - s )) s"
