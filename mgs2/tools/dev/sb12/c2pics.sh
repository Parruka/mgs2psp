#!/bin/sh
# c2pics.sh <shot vblanks> (B= the build, default /home/claude/sb12m/d; O= where the emulator outputs go): task #140, PPSSPP master
# pictures (tools/dev/ppsspp_master_run.sh: the exact picture reference) of the indoor route (play/pad_w01af.txt, gl14's delivery route,
# the x86 route i to vb 37,800) with the sb12 PSP model build (mkpsp.sh, no PGO), C1 on in both (the next builds' default) and C2 off
# (MGS2_OPT2=ffffffdf) and on (ffffffff), in the game folders /home/claude/msm/PSP/GAME/MGS2M_c2off and MGS2M_c2on: gl14's delivery cfg
# without its turns, no catcher, the shots given, each frame's list and frame digests in log.txt (MGS2_GL_DIG) for the check against the
# x86 runs out_it1.log and out_it3.log.  The pictures stay in the container (game data: personal use only).
SH=$1; MG=/mnt/project-files/mgs2; B=${B:-/home/claude/sb12m/d}; O=${O:-/home/claude/sb12m}; GG=/home/claude/msm/PSP/GAME
for k in off on; do
  G=$GG/MGS2M_c2$k; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/shot_*.bmp $G/crash.txt
  cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_w01af.txt $G/pad.txt; ln -sf /home/claude/v30/pak/mgs2disc.pak $G/mgs2disc.pak   # (the indoor route's pack, as MGS2M_gl14)
  grep -v '^#\|^MGS2_GL_SHOT=\|^MGS2_EXC=\|^MGS2_OPT_CYCLE=\|^MGS2_OPT2_CYCLE=\|^MGS2_GL_DIG=' /home/claude/gl14/mgs2.cfg > $G/mgs2.cfg
  { echo "MGS2_EXC=0"; echo "MGS2_GL_SHOT=$SH"; echo "MGS2_GL_DIG=0:37800"; } >> $G/mgs2.cfg
  if [ $k = on ]; then echo "MGS2_OPT2=ffffffff" >> $G/mgs2.cfg; else echo "MGS2_OPT2=ffffffdf" >> $G/mgs2.cfg; fi
  cp $MG/prof/hashref_x86_in_h2.txt $G/hashref.txt
done
( MAX=3600 sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_c2off $O/pic_off.out ) &
( MAX=3600 sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_c2on $O/pic_on.out ) &
wait
echo "$(date +%H:%M:%S) pictures done" >> /home/claude/sb12/c2x.log
