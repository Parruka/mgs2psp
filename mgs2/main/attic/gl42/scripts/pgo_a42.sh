#!/bin/sh
# pgo_a42.sh (gl42: build_gl41/pgo_a41.sh with build_gl42, t42 and cfg/mgs2_gl42pgo.cfg (the delivery cfg without the shots; the cycles
# to 128 turns, every kind past t73: the people native (K-N2, K-N2R) and the ship native in the counts) and cfg/hashref_gl42pgo.txt
# (the twin's pgo arm).  ONE HEAVY JOB AT A TIME (the lead's gl42 brief): each instrumented build, then its training, then the next build,
# all under nice 19 (gl41 ran a training beside the next build).  The trainings' last cfg lines are gl41's (the picture's memory cut so the
# instrumented build fits the emulator's memory).  Then the counts into build_gl42/gcda.
R=/home/claude/main/build_gl42; cd $R/m || exit 1
bld() { # <tag> <PGO_ELF or empty> <PGO_STAGE or empty>
  [ -e $R/g_$1 ] && { echo "pgo_a42.sh: $R/g_$1 exists"; exit 1; }
  env PSPD=$R/g_$1 PGO=gen ${2:+PGO_ELF=$2} ${3:+PGO_STAGE=$3} nice -n 19 sh scripts/psp.sh > $R/g_$1.out 2>&1; r=$?; echo "g_$1 rc $r $(date -u +%H:%M:%S)"
  [ $r = 0 ] || exit 1; ( cd $R/g_$1 && md5sum EBOOT.PBP ovl_*.bin ) > $R/g_$1.md5; }
trn() { t=$1; shift; nice -n 19 sh scripts/pgo.sh train $R/g_$t $t "$@" > $R/train_$t.out 2>&1; echo "train_$t rc $? $(date -u +%H:%M:%S)"; }
bld game game ""; trn game 15000 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=1024
bld rtb rtb "";   trn rtb 15000 37790 37800 MGS2_GL_TEXKB=1024
bld pic pic "";   trn pic 15000 37790 37800 MGS2_GL_TEXKB=1664
bld w00a "" w00a; trn w00a 29100 31550 31600 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0
bld w01a "" w01a; trn w01a 31600 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0
for t in game rtb pic w00a w01a; do echo "== train_$t"; cat $R/train_$t.out | cut -c1-220; done
mkdir -p $R/gcda && cp $R/pg/game/gen.gcda $R/pg/rtb/boot_rt.gcda $R/pg/rtb/gen_dispatch.gcda $R/pg/w00a/gen_s_w00a.gcda $R/pg/w01a/gen_s_w01a.gcda $R/gcda/ \
  && for f in $R/pg/pic/*.gcda; do case $(basename $f) in gen_dispatch.gcda) ;; *) cp $f $R/gcda/ ;; esac; done
( cd $R/gcda && md5sum *.gcda > md5.txt; ls | wc -l ); echo "pgo_a42.sh: done $(date -u +%H:%M:%S)"
