#!/bin/sh
# trail.sh: why gl15's master shot of vb 31,112 differs from the C2-on master's in 36 pixels (same GE list): the delivered gl15 build in
# PPSSPP master twice, with gl15's turns (MGS2M_g15t) and with C2 on all the time (MGS2M_g15a, MGS2_OPT2=ffffffff, no OPT2 turns), shots of
# the frames 31,100-31,198 (window 301 is C2's first on-turn after an off-turn) and each frame's digests over 31,000-31,300.
MG=/mnt/project-files/mgs2; G=/home/claude/gl15; GG=/home/claude/msm/PSP/GAME; S14=$GG/MGS2M_gl14; O=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g15r
SH=31102,31104,31106,31108,31110,31112,31114,31122,31132,31142,31162,31182,31200
for k in t a; do
  M=$GG/MGS2M_g15$k; mkdir -p $M/memcard0; cp $G/EBOOT.PBP $G/ovl_*.bin $M/; cp $S14/SLES_503.83 $S14/pad.txt $S14/hashref.txt $M/
  ln -sf /home/claude/v30/pak/mgs2disc.pak $M/mgs2disc.pak
  sed -e 's/^MGS2_EXC=3$/MGS2_EXC=0/' -e "s/^MGS2_GL_SHOT=.*/MGS2_GL_SHOT=$SH/" -e 's/^MGS2_GL_DIG=.*/MGS2_GL_DIG=31000:31300/' $G/mgs2.cfg > $M/mgs2.cfg
  if [ $k = a ]; then sed -i '/^MGS2_OPT2_CYCLE=/d' $M/mgs2.cfg; echo "MGS2_OPT2=ffffffff" >> $M/mgs2.cfg; fi
  rm -f $M/log.txt $M/steps.csv $M/battery.txt $M/crash.txt $M/shot_*.bmp
done
echo "$(date +%H:%M:%S) start" > $O/trail.log
( sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_g15t $O/trail_t.out; echo "$(date +%H:%M:%S) t rc $?" >> $O/trail.log ) &
( sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_g15a $O/trail_a.out; echo "$(date +%H:%M:%S) a rc $?" >> $O/trail.log ) &
wait
echo "$(date +%H:%M:%S) done" >> $O/trail.log
