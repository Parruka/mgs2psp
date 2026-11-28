#!/bin/sh
# pack43.sh [pkg, default pk_f43] (gl43; build_gl42/pack42.sh with gl43's names): build_gl43/deliver = the package's EBOOT.PBP, 8 overlays, mgs2.cfg,
# hashref.txt, pad.txt + rd/rd43pc_f.py as rd43pc.py + README.txt (written by hand) + MANIFEST.md5 (the 14 files, then SLES_503.83 and
# mgs2disc.pak as the stick has them).  15 files.  NOT delivered: the lead copies it when the lead decides.
. /home/claude/main/scripts/env.sh
R=/home/claude/main/build_gl43; K=$R/${1:-pk_f43}; D=$R/deliver; cd $R || exit 1
[ -e $D ] && { echo "pack43.sh: $D exists: look at it first"; exit 1; }
[ -f $R/README.txt ] && [ -f $R/rd/rd43pc_f.py ] || { echo "pack43.sh: README.txt or rd/rd43pc_f.py missing"; exit 1; }
mkdir -p $D && cp -p $K/EBOOT.PBP $K/ovl_*.bin $K/mgs2.cfg $K/hashref.txt $K/pad.txt $D/ && cp $R/rd/rd43pc_f.py $D/rd43pc.py && cp $R/README.txt $D/README.txt || exit 1
( cd $D && md5sum EBOOT.PBP ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin \
    mgs2.cfg hashref.txt pad.txt rd43pc.py README.txt; echo "$(md5sum < "$SL" | cut -c1-32)  SLES_503.83"; echo "$(md5sum < "$PAK" | cut -c1-32)  mgs2disc.pak" ) > $D/MANIFEST.md5
ls -la $D; echo "files: $(ls $D | wc -l)"; cat $D/MANIFEST.md5
( cd $D && head -n 14 MANIFEST.md5 | md5sum -c --quiet ) && echo "pack43.sh: every file checks against MANIFEST.md5"
cmp $D/EBOOT.PBP $K/EBOOT.PBP && echo "pack43.sh: EBOOT = the package's (the emulator runs' EBOOT)"
