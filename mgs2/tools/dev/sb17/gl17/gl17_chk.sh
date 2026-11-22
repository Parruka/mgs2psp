#!/bin/sh
# gl17_chk.sh: gl17's emulator checks and model (task #156), gl16_chk.sh's: 1.20.4 "emu hw" (the delivery's cfg: gl17's two-window
# turns, the effects off and on) and NOPROF, the model windows (deck 30,810-30,890 and inside 33,410-33,490 on the indoor route,
# 31,510-31,590 on the codec route, each with the effects on, MGS2_OPT2=ffffffdf, and off, the default fffff9df), PPSSPP master on
# the delivery folder /home/claude/gl17.  Four at a time.  The rain as the delivery: drop sheets whole, splash 1/2, no streaks, the
# rain's own sprites left out.
MG=/mnt/project-files/mgs2; X=/home/claude/sbg17; L=$X/chk.log
C="MGS2_GL_TEXKB=2688 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
export GL3=$X
st "start"
( TAG=g17 sh $MG/psp/gl3.sh emu hw > $X/emu_hw.out 2>&1; st "emu hw rc $?"
  sh $MG/psp/gl3.sh model g17cod 31510 31590 all codec $C MGS2_OPT2=ffffffdf > $X/model_cod.out 2>&1; st "model g17cod rc $?" ) &
( NOPROF=1 TAG=g17n sh $MG/psp/gl3.sh emu hw > $X/emu_hwn.out 2>&1; st "emu hwn rc $?"
  sh $MG/psp/gl3.sh model g17ioff 33410 33490 all in $C > $X/model_ioff.out 2>&1; st "model g17ioff rc $?" ) &
( sh $MG/psp/gl3.sh model g17on 30810 30890 all in $C MGS2_OPT2=ffffffdf > $X/model_on.out 2>&1; st "model g17on rc $?"
  sh $MG/psp/gl3.sh model g17off 30810 30890 all in $C > $X/model_off.out 2>&1; st "model g17off rc $?" ) &
( sh $MG/psp/gl3.sh model g17ion 33410 33490 all in $C MGS2_OPT2=ffffffdf > $X/model_ion.out 2>&1; st "model g17ion rc $?"
  G=/home/claude/gl17; M=/home/claude/msm/PSP/GAME/MGS2M_gl17; S16=/home/claude/msm/PSP/GAME/MGS2M_gl16
  mkdir -p $M/memcard0; cp $G/EBOOT.PBP $G/ovl_*.bin $M/; cp $S16/SLES_503.83 $S16/pad.txt $S16/hashref.txt $M/
  ln -sf /home/claude/v30/pak/mgs2disc.pak $M/mgs2disc.pak; sed 's/^MGS2_EXC=3$/MGS2_EXC=0/' $G/mgs2.cfg > $M/mgs2.cfg
  rm -f $M/log.txt $M/steps.csv $M/battery.txt $M/crash.txt $M/shot_*.bmp
  sh $MG/tools/dev/ppsspp_master_run.sh $M $X/master_run.out; st "master rc $?" ) &
wait
st "checks done"
sh $MG/psp/gl3.sh model g17codoff 31510 31590 all codec $C > $X/model_codoff.out 2>&1; st "model g17codoff rc $?"
st "done"
