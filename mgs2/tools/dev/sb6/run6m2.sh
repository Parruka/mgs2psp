#!/bin/sh
# run6m2.sh: run6m.sh's deck and inside windows, the four runs side by side (PspProf counts the emulated machine's instructions and cache
# misses, so the host's load does not change them; each run has its own PPSSPP game folder), then fitcmp.py for d and i.  Used for sb6
# after run6m.sh had done the codec pair.
M=/home/claude/sb6m/model; S=/home/claude/sb6
fit() { PYTHONDONTWRITEBYTECODE=1 python3 -I $S/fitcmp.py $M/s6$1of $M/s6$1on 80 /home/claude/sb6m/d > $M/fitcmp_$1.txt 2>&1
        head -n 1 $M/fitcmp_$1.txt; df -m /home/claude | tail -n 1; }
{ sh $S/model.sh s6dof 30810 30890 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_OPT=dfffffff; tail -n 1 $M/run_s6dof.out
  rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6dof; } &
{ sh $S/model.sh s6don 30810 30890 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2; tail -n 1 $M/run_s6don.out
  rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6don; } &
{ sh $S/model.sh s6iof 33410 33490 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_OPT=dfffffff; tail -n 1 $M/run_s6iof.out
  rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6iof; } &
{ sh $S/model.sh s6ion 33410 33490 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2; tail -n 1 $M/run_s6ion.out
  rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6ion; } &
wait
fit d
fit i
