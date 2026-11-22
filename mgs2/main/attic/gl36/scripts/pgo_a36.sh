#!/bin/sh
# pgo_a36.sh (gl36's fresh profile, gl35's method on t36; the training cfg has MGS2_NAT_REUSE_CYCLE=1,1,0,0 in place of gl35's keep cycle): the five instrumented builds one after the other (m/scripts/psp.sh PGO=gen:
# g_game = gen.o and gen_dispatch.o, g_rtb = boot_rt.o, g_pic = the picture objects and xl_psp.o, g_w00a and g_w01a = one overlay each),
# each one's training run started as soon as it is built (m/scripts/pgo.sh train: cfg/mgs2_gl36pgo.cfg = the delivery cfg without the
# shots, so native, in place, fast and MGS2_NAT_KEEP=1 in every turn, MGS2_NAT_REUSE_CYCLE=1,1,0,0: both arms of the reuse are in the counts; every
# hash line checked against cfg/hashref_gl36.txt), the runs side by side with the next build.  The runs' last cfg lines are gl33's (the
# picture's memory cut so that the instrumented build fits the emulator's memory: MGS2_GL_TEXKB, and no ME picture where the picture
# code is not the trained part).  Then the counts into build_gl36/gcda.
R=/home/claude/main/build_gl36; cd $R/m || exit 1
bld() { # <tag> <PGO_ELF or empty> <PGO_STAGE or empty>
  [ -e $R/g_$1 ] && { echo "pgo_a36.sh: $R/g_$1 exists"; exit 1; }
  env PSPD=$R/g_$1 PGO=gen ${2:+PGO_ELF=$2} ${3:+PGO_STAGE=$3} sh scripts/psp.sh > $R/g_$1.out 2>&1; r=$?; echo "g_$1 rc $r $(date -u +%H:%M:%S)"
  [ $r = 0 ] || exit 1; ( cd $R/g_$1 && md5sum EBOOT.PBP ovl_*.bin ) > $R/g_$1.md5; }
bld game game ""; sh scripts/pgo.sh train $R/g_game game 15000 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=1024 > $R/train_game.out 2>&1 &
bld rtb rtb "";   sh scripts/pgo.sh train $R/g_rtb rtb 15000 37790 37800 MGS2_GL_TEXKB=1024 > $R/train_rtb.out 2>&1 &
bld pic pic "";   sh scripts/pgo.sh train $R/g_pic pic 15000 37790 37800 MGS2_GL_TEXKB=1664 > $R/train_pic.out 2>&1 &
bld w00a "" w00a; sh scripts/pgo.sh train $R/g_w00a w00a 29100 31550 31600 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0 > $R/train_w00a.out 2>&1 &
bld w01a "" w01a; sh scripts/pgo.sh train $R/g_w01a w01a 31600 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0 > $R/train_w01a.out 2>&1 &
wait
for t in game rtb pic w00a w01a; do echo "== train_$t"; cat $R/train_$t.out | cut -c1-220; done
mkdir -p $R/gcda && cp $R/pg/game/gen.gcda $R/pg/rtb/boot_rt.gcda $R/pg/rtb/gen_dispatch.gcda $R/pg/w00a/gen_s_w00a.gcda $R/pg/w01a/gen_s_w01a.gcda $R/gcda/ \
  && for f in $R/pg/pic/*.gcda; do case $(basename $f) in gen_dispatch.gcda) ;; *) cp $f $R/gcda/ ;; esac; done
( cd $R/gcda && md5sum *.gcda > md5.txt; ls | wc -l ); echo "pgo_a36.sh: done $(date -u +%H:%M:%S)"
