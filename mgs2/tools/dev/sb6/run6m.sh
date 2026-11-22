#!/bin/sh
# run6m.sh: sb6's six model windows one after another (model.sh; lane 3 off = MGS2_OPT=dfffffff, on = the build's default), gl10's
# settings (MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2), then fitcmp.py by pair (80 steps each) into /home/claude/sb6m/model/fitcmp_{c,d,i}.txt.
# Each run's PPSSPP game folder (ms_home/.ppsspp/PSP/GAME/MGS2GL3M_<tag>, about 25 MB) is removed once gl3.sh has copied its log and cfg.
M=/home/claude/sb6m/model; S=/home/claude/sb6
fit() { PYTHONDONTWRITEBYTECODE=1 python3 -I $S/fitcmp.py $M/s6$1of $M/s6$1on 80 /home/claude/sb6m/d > $M/fitcmp_$1.txt 2>&1
        head -n 1 $M/fitcmp_$1.txt; df -m /home/claude | tail -n 1; }
sh $S/model.sh s6cof 31510 31590 codec MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_OPT=dfffffff; tail -n 1 $M/run_s6cof.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6cof
sh $S/model.sh s6con 31510 31590 codec MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2; tail -n 1 $M/run_s6con.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6con
fit c
sh $S/model.sh s6dof 30810 30890 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_OPT=dfffffff; tail -n 1 $M/run_s6dof.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6dof
sh $S/model.sh s6don 30810 30890 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2; tail -n 1 $M/run_s6don.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6don
fit d
sh $S/model.sh s6iof 33410 33490 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 MGS2_OPT=dfffffff; tail -n 1 $M/run_s6iof.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6iof
sh $S/model.sh s6ion 33410 33490 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2; tail -n 1 $M/run_s6ion.out
rm -r /home/claude/ms_home/.ppsspp/PSP/GAME/MGS2GL3M_s6ion
fit i
