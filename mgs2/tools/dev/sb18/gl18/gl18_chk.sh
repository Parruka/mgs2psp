#!/bin/sh
# gl18_chk.sh: gl18's emulator checks and model (task #179), gl17_chk.sh's: 1.20.4 "emu hw" (the delivery's cfg: gl18's turns,
# hide-only and all skips) with gl18's x86 reference (HREF), NOPROF, hwmem; the model windows (deck 30,810-30,890 and inside
# 33,410-33,490 on the indoor route, 31,510-31,590 on the codec route) with every skip on (MGS2_OPT2=fffff9df), hide-only
# (fffd21df) and, on the deck, none (the default fffc01df: gl17's effects-off turns); the PPSSPP master on the delivery folder
# /home/claude/gl18.  Four at a time.  The rain as the delivery.
MG=/mnt/project-files/mgs2; X=/home/claude/sbg18; L=$X/chk.log
C="MGS2_GL_TEXKB=2688 MGS2_GL_RAIN=1 MGS2_GL_SPLASH=2 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
HR=$MG/prof/hashref_x86_in_gl18.txt
st() { echo "$(date +%H:%M:%S) $*" >> $L; }
export GL3=$X
st "start"
( HREF=$HR TAG=g18 sh $MG/psp/gl3.sh emu hw > $X/emu_hw.out 2>&1; st "emu hw rc $?"
  sh $MG/psp/gl3.sh model g18on 30810 30890 all in $C MGS2_OPT2=fffff9df > $X/model_on.out 2>&1; st "model g18on rc $?" ) &
( HREF=$HR NOPROF=1 TAG=g18n sh $MG/psp/gl3.sh emu hw > $X/emu_hwn.out 2>&1; st "emu hwn rc $?"
  sh $MG/psp/gl3.sh model g18hid 30810 30890 all in $C MGS2_OPT2=fffd21df > $X/model_hid.out 2>&1; st "model g18hid rc $?" ) &
( HREF=$HR TAG=g18 sh $MG/psp/gl3.sh emu hwmem > $X/emu_hwmem.out 2>&1; st "emu hwmem rc $?"
  sh $MG/psp/gl3.sh model g18off 30810 30890 all in $C > $X/model_off.out 2>&1; st "model g18off rc $?" ) &
( G=/home/claude/gl18; M=/home/claude/msm/PSP/GAME/MGS2M_gl18; S17=/home/claude/msm/PSP/GAME/MGS2M_gl17
  mkdir -p $M/memcard0; cp $G/EBOOT.PBP $G/ovl_*.bin $G/hashref.txt $M/; cp $S17/SLES_503.83 $S17/pad.txt $M/
  ln -sf /home/claude/v30/pak/mgs2disc.pak $M/mgs2disc.pak; sed 's/^MGS2_EXC=3$/MGS2_EXC=0/' $G/mgs2.cfg > $M/mgs2.cfg
  rm -f $M/log.txt $M/steps.csv $M/battery.txt $M/crash.txt $M/shot_*.bmp
  sh $MG/tools/dev/ppsspp_master_run.sh $M $X/master_run.out; st "master rc $?"
  sh $MG/psp/gl3.sh model g18ion 33410 33490 all in $C MGS2_OPT2=fffff9df > $X/model_ion.out 2>&1; st "model g18ion rc $?" ) &
wait
st "checks done"
( sh $MG/psp/gl3.sh model g18ihid 33410 33490 all in $C MGS2_OPT2=fffd21df > $X/model_ihid.out 2>&1; st "model g18ihid rc $?" ) &
( sh $MG/psp/gl3.sh model g18cod 31510 31590 all codec $C MGS2_OPT2=fffff9df > $X/model_cod.out 2>&1; st "model g18cod rc $?" ) &
( sh $MG/psp/gl3.sh model g18codhid 31510 31590 all codec $C MGS2_OPT2=fffd21df > $X/model_codhid.out 2>&1; st "model g18codhid rc $?" ) &
wait
st "done"
