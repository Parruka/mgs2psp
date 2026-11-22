#!/bin/sh
# gl15_chk.sh: gl15's emulator checks and model (task #140), gl14_chk.sh's: 1.20.4 "emu hw" (the delivery's cfg: gl15's nine-window
# turns) and NOPROF, the model windows (deck 30,810-30,890 and inside 33,410-33,490 on the indoor route, each with C2 off, the default,
# and with C2 on, MGS2_OPT2=ffffffff; 31,510-31,590 on the codec route with the default, C1 on), PPSSPP master on the delivery folder
# /home/claude/gl15.  Four at a time.
MG=/mnt/project-files/mgs2; X=/home/claude/sbg15; L=$X/chk.log; C="MGS2_GL_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
export GL3=$X
st "start"
( TAG=g15 sh $MG/psp/gl3.sh emu hw > $X/emu_hw.out 2>&1; st "emu hw rc $?"
  sh $MG/psp/gl3.sh model g15cod 31510 31590 all codec $C > $X/model_cod.out 2>&1; st "model g15cod rc $?" ) &
( NOPROF=1 TAG=g15n sh $MG/psp/gl3.sh emu hw > $X/emu_hwn.out 2>&1; st "emu hwn rc $?"
  sh $MG/psp/gl3.sh model g15ionc2 33410 33490 all in $C MGS2_OPT2=ffffffff > $X/model_ionc2.out 2>&1; st "model g15ionc2 rc $?" ) &
( sh $MG/psp/gl3.sh model g15on 30810 30890 all in $C > $X/model_on.out 2>&1; st "model g15on rc $?"
  sh $MG/psp/gl3.sh model g15onc2 30810 30890 all in $C MGS2_OPT2=ffffffff > $X/model_onc2.out 2>&1; st "model g15onc2 rc $?" ) &
( sh $MG/psp/gl3.sh model g15ion 33410 33490 all in $C > $X/model_ion.out 2>&1; st "model g15ion rc $?"
  G=/home/claude/gl15; M=/home/claude/msm/PSP/GAME/MGS2M_gl15; S14=/home/claude/msm/PSP/GAME/MGS2M_gl14
  mkdir -p $M/memcard0; cp $G/EBOOT.PBP $G/ovl_*.bin $M/; cp $S14/SLES_503.83 $S14/pad.txt $S14/hashref.txt $M/
  ln -sf /home/claude/v30/pak/mgs2disc.pak $M/mgs2disc.pak; sed 's/^MGS2_EXC=3$/MGS2_EXC=0/' $G/mgs2.cfg > $M/mgs2.cfg
  rm -f $M/log.txt $M/steps.csv $M/battery.txt $M/crash.txt $M/shot_*.bmp
  sh $MG/tools/dev/ppsspp_master_run.sh $M $X/master_run.out; st "master rc $?" ) &
wait
st "done"
