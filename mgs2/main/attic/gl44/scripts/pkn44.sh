#!/bin/sh
# pkn44.sh [folder] [pkg] (gl44: build_gl43/pkn43.sh with build_gl44, gl44's cfg and gl43's hashref): the no-PGO, no-order build
# (m/scripts/psp.sh with PSPD=build_gl44/<folder, default n44>) and its emulator package (<pkg, default pk_n44>: EBOOT.PBP, the 8
# overlays, cfg/mgs2_gl44.cfg as mgs2.cfg, cfg/hashref_gl43.txt as hashref.txt, build_gl38/pk_f38/pad.txt) with <pkg>.md5.
# HREF= another hashref (gl44 block B: cfg/hashref_gl44.txt); PKONLY=1 packages an existing build folder.  Game-derived files: container only.
R=/home/claude/main/build_gl44; F=${1:-n44}; K=${2:-pk_n44}; cd $R || exit 1
if [ -z "$PKONLY" ]; then [ -e $R/$F ] || [ -e $R/$K ] && { echo "pkn44.sh: $F or $K exists: look at it and delete it first"; exit 1; }
PSPD=$R/$F nice -n 19 sh $R/m/scripts/psp.sh > $R/$F.out 2>&1; r=$?; echo "psp.sh rc $r $(date -u +%H:%M:%S)" >> $R/$F.out
else [ -e $R/$K ] && { echo "pkn44.sh: $K exists"; exit 1; }; r=0; fi
[ $r = 0 ] && [ -f $R/$F/EBOOT.PBP ] || { echo "pkn44.sh: build failed"; tail -n 20 $R/$F.out; exit 1; }
mkdir -p $R/$K && cp $R/$F/EBOOT.PBP $R/$F/ovl_*.bin $R/$K/ && cp $R/cfg/mgs2_gl44.cfg $R/$K/mgs2.cfg && cp ${HREF:-$R/cfg/hashref_gl43.txt} $R/$K/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $R/$K/pad.txt || exit 1
( cd $R/$K && md5sum * ) > $R/$K.md5; cat $R/$K.md5; echo "pkn44.sh: done $(date -u +%H:%M:%S)"
