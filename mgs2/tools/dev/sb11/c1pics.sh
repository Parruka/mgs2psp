#!/bin/sh
# c1pics.sh <shot vblanks> (B= the build, default sb11m/d; O= where the emulator outputs go): task #135, PPSSPP master pictures (tools/dev/ppsspp_master_run.sh: the exact picture reference) of the codec
# route with the sb11 PSP model build /home/claude/sb11m/d (mkpsp.sh, no PGO), C1 off (the default) and on (MGS2_OPT2=ffffffff), in the
# game folders /home/claude/msm/PSP/GAME/MGS2M_c1off and MGS2M_c1on: gl4c's delivery cfg with E vblanks (default 40,000, as the x86
# runs), rain and splash 1/2, no catcher, the shots given, each frame's
# list and frame digests in log.txt (MGS2_GL_DIG) for the check against the x86 runs.  The pictures stay in the container (game data: personal use only).
SH=$1; MG=/mnt/project-files/mgs2; B=${B:-/home/claude/sb11m/d}; O=${O:-/home/claude/sb11m}; GG=/home/claude/msm/PSP/GAME; E=${E:-40000}
for k in off on; do
  G=$GG/MGS2M_c1$k; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/shot_*.bmp $G/crash.txt
  cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_codec.txt $G/pad.txt; ln -sf /home/claude/v31/pak/mgs2disc.pak $G/mgs2disc.pak
  grep -v '^MGS2_GL_SHOT=\|^MGS2_EXC=\|^vblanks \|^deck \|^MGS2_HASH_EVERY=' $MG/prof/hw_gl4c/mgs2_gl4c.cfg > $G/mgs2.cfg
  { echo "vblanks $E"; echo "deck $((E - 1000))"; echo "MGS2_HASH_EVERY=500"; echo "MGS2_EXC=0"; echo "MGS2_GL_RAIN=2"; echo "MGS2_GL_SPLASH=2"; echo "MGS2_GL_SHOT=$SH"; echo "MGS2_GL_DIG=0:$E"; } >> $G/mgs2.cfg
  [ $k = on ] && echo "MGS2_OPT2=ffffffff" >> $G/mgs2.cfg
  cp $MG/prof/hashref_x86_codec_h2.txt $G/hashref.txt
done
( MAX=3600 sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_c1off $O/pic_off.out ) &
( MAX=3600 sh $MG/tools/dev/ppsspp_master_run.sh $GG/MGS2M_c1on $O/pic_on.out ) &
wait
echo "$(date +%H:%M:%S) pictures done" >> /home/claude/sb11/c1x.log
