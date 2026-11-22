#!/bin/sh
# mk_gl18me.sh: gl18me's delivery folder /home/claude/gl18me (task #180 step 4e; gl18_mk.sh's way): d3's EBOOT (4d-5, 596b36a6) re-titled
# "MGS2 harness gl18me ME picture" (pk/: unpack, mksfoex MEMSIZE=1, pack, and an unpack check in pk/chk), d3's 8 overlays, mgs2.cfg and
# README.txt from /home/claude/me18e/dl, hashref.txt = prof/hashref_x86_in_gl18me.txt (the x86 twin r18a: the rain's 7 skips always on).
# The shared files (SLES_503.83, mgs2disc.pak, pad.txt, memcard0) are copied on the PC.  Game data passes through it: personal use only.
D=/home/claude/me18e/d3; G=/home/claude/gl18me; MG=/mnt/project-files/mgs2; DL=/home/claude/me18e/dl
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ "$(md5sum < $D/EBOOT.PBP | cut -c1-32)" = 596b36a661e11154409d4e59958c84fd ] || { echo "mk_gl18me.sh: d3's EBOOT is not 596b36a6"; exit 1; }
[ -f $MG/prof/hashref_x86_in_gl18me.txt ] && [ -f $DL/mgs2.cfg ] && [ -f $DL/README.txt ] || { echo "mk_gl18me.sh: reference, cfg or README missing"; exit 1; }
[ ! -e $G ] && mkdir -p $G/pk/chk || { echo "mk_gl18me.sh: $G exists"; exit 1; }
cd $G/pk && unpack-pbp $D/EBOOT.PBP > unpack.out 2>&1 && mv PARAM.SFO PARAM_build.SFO && mksfoex -d MEMSIZE=1 "MGS2 harness gl18me ME picture" PARAM.SFO \
  && pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL DATA.PSP NULL > pack.out 2>&1 || { echo "mk_gl18me.sh: re-title failed"; exit 1; }
cd $G/pk/chk && unpack-pbp ../EBOOT.PBP > /dev/null 2>&1 && cmp DATA.PSP ../DATA.PSP && cmp PARAM.SFO ../PARAM.SFO || { echo "mk_gl18me.sh: re-title check failed"; exit 1; }
cp $G/pk/EBOOT.PBP $G/ && cp $D/ovl_init.bin $D/ovl_r_title.bin $D/ovl_r_tnk0.bin $D/ovl_n_title.bin $D/ovl_d00t.bin $D/ovl_d01t.bin $D/ovl_w00a.bin $D/ovl_w01a.bin $G/ \
  && cp $DL/mgs2.cfg $DL/README.txt $G/ && cp $MG/prof/hashref_x86_in_gl18me.txt $G/hashref.txt || { echo "mk_gl18me.sh: copies failed"; exit 1; }
cd $G && md5sum EBOOT.PBP ovl_*.bin mgs2.cfg hashref.txt README.txt > md5_container.txt && cat md5_container.txt
echo "folder: $(ls $G | tr '\n' ' ')"
