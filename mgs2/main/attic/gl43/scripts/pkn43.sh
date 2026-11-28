#!/bin/sh
# pkn43.sh [folder] [pkg] (gl43: build_gl42/pkn42.sh with build_gl43, gl43's cfg and hashref; gl42: build_gl41/pkn41.sh with build_gl43, gl42's cfg and hashref, nice 19): gl42's no-PGO, no-order build
# (m/scripts/psp.sh with PSPD=build_gl43/<folder, default n43>) and its emulator package (<pkg, default pk_n43>: EBOOT.PBP, the 8
# overlays, cfg/mgs2_gl43.cfg as mgs2.cfg, cfg/hashref_gl43.txt as hashref.txt, build_gl38/pk_f38/pad.txt) with <pkg>.md5.
# Game-derived files: container only.
R=/home/claude/main/build_gl43; F=${1:-n43}; K=${2:-pk_n43}; cd $R || exit 1
[ -e $R/$F ] || [ -e $R/$K ] && { echo "pkn43.sh: $F or $K exists: look at it and delete it first"; exit 1; }
PSPD=$R/$F nice -n 19 sh $R/m/scripts/psp.sh > $R/$F.out 2>&1; r=$?; echo "psp.sh rc $r $(date -u +%H:%M:%S)" >> $R/$F.out
[ $r = 0 ] && [ -f $R/$F/EBOOT.PBP ] || { echo "pkn43.sh: build failed"; tail -n 20 $R/$F.out; exit 1; }
mkdir -p $R/$K && cp $R/$F/EBOOT.PBP $R/$F/ovl_*.bin $R/$K/ && cp $R/cfg/mgs2_gl43.cfg $R/$K/mgs2.cfg && cp $R/cfg/hashref_gl43.txt $R/$K/hashref.txt \
  && cp /home/claude/main/build_gl38/pk_f38/pad.txt $R/$K/pad.txt || exit 1
( cd $R/$K && md5sum * ) > $R/$K.md5; cat $R/$K.md5; echo "pkn43.sh: done $(date -u +%H:%M:%S)"
