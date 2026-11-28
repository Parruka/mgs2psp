#!/bin/sh
# pack44.sh [pkg, default pk_f44] (gl44; build_gl43/pack43.sh with gl44's names, and block A's black-box reader): build_gl44/deliver = the
# package's EBOOT.PBP, 8 overlays, mgs2.cfg, hashref.txt, pad.txt + rd/rd44pc_f.py as rd44pc.py + rd/rdbbox.py + README.txt (written by
# hand) + MANIFEST.md5 (the 15 files, then SLES_503.83 and mgs2disc.pak as the stick has them).  16 files.  NOT delivered: the lead copies it.
. /home/claude/main/scripts/env.sh
R=/home/claude/main/build_gl44; K=$R/${1:-pk_f44}; D=$R/deliver; cd $R || exit 1
[ -e $D ] && { echo "pack44.sh: $D exists: look at it first"; exit 1; }
[ -f $R/README.txt ] && [ -f $R/rd/rd44pc_f.py ] && [ -f $R/rd/rdbbox.py ] || { echo "pack44.sh: README.txt, rd/rd44pc_f.py or rd/rdbbox.py missing"; exit 1; }
mkdir -p $D && cp -p $K/EBOOT.PBP $K/ovl_*.bin $K/mgs2.cfg $K/hashref.txt $K/pad.txt $D/ && cp $R/rd/rd44pc_f.py $D/rd44pc.py && cp $R/rd/rdbbox.py $D/rdbbox.py \
  && cp $R/README.txt $D/README.txt || exit 1
( cd $D && md5sum EBOOT.PBP ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin \
    mgs2.cfg hashref.txt pad.txt rd44pc.py rdbbox.py README.txt; echo "$(md5sum < "$SL" | cut -c1-32)  SLES_503.83"; echo "$(md5sum < "$PAK" | cut -c1-32)  mgs2disc.pak" ) > $D/MANIFEST.md5
ls -la $D; echo "files: $(ls $D | wc -l)"; cat $D/MANIFEST.md5
( cd $D && head -n 15 MANIFEST.md5 | md5sum -c --quiet ) && echo "pack44.sh: every file checks against MANIFEST.md5"
cmp $D/EBOOT.PBP $K/EBOOT.PBP && echo "pack44.sh: EBOOT = the package's (the emulator runs' EBOOT)"
cmp $D/mgs2.cfg $R/cfg/mgs2_gl44.cfg && cmp $D/hashref.txt $R/cfg/hashref_gl44.txt && echo "pack44.sh: mgs2.cfg = cfg/mgs2_gl44.cfg, hashref.txt = cfg/hashref_gl44.txt"
