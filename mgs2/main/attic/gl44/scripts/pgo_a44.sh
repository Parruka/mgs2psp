#!/bin/sh
# pgo_a44.sh (gl44: build_gl43/pgo_a43.sh with build_gl44, t44, cfg/mgs2_gl44pgo.cfg and cfg/hashref_gl44pgo.txt (the twin's pgo arm); the
# ME_PIC=0 trainings (game, w00a, w01a) also compared with cfg/hashref_gl44pgo0.txt (the twin's pgo0 arm); gl43: build_gl42/pgo_a42.sh;
# ONE HEAVY JOB AT A TIME: each instrumented build, then its training, then the next, nice 19; the trainings' last cfg lines are gl41/gl42's
# (memory cut: game, w00a, w01a match hashref_gl38 / hashref_gl43pgo as gl42 B5 explains).  Then the counts into build_gl43/gcda.
R=/home/claude/main/build_gl44; cd $R/m || exit 1
bld() { # <tag> <PGO_ELF or empty> <PGO_STAGE or empty>
  [ -e $R/g_$1 ] && { echo "pgo_a44.sh: $R/g_$1 exists"; exit 1; }
  env PSPD=$R/g_$1 PGO=gen ${2:+PGO_ELF=$2} ${3:+PGO_STAGE=$3} nice -n 19 sh scripts/psp.sh > $R/g_$1.out 2>&1; r=$?; echo "g_$1 rc $r $(date -u +%H:%M:%S)"
  [ $r = 0 ] || exit 1; ( cd $R/g_$1 && md5sum EBOOT.PBP ovl_*.bin ) > $R/g_$1.md5; }
trn() { t=$1; shift; nice -n 19 sh scripts/pgo.sh train $R/g_$t $t "$@" > $R/train_$t.out 2>&1; echo "train_$t rc $? $(date -u +%H:%M:%S)"; }
bld game game ""; trn game 15000 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=1024
bld rtb rtb "";   trn rtb 15000 37790 37800 MGS2_GL_TEXKB=1024
bld pic pic "";   trn pic 15000 37790 37800 MGS2_GL_TEXKB=1664
bld w00a "" w00a; trn w00a 29100 31550 31600 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0
bld w01a "" w01a; trn w01a 31600 37790 37800 MGS2_ME_PIC=0 MGS2_GL_TEXKB=256 MGS2_ME_PIC_GE2=0
for t in game rtb pic w00a w01a; do echo "== train_$t"; cat $R/train_$t.out | cut -c1-220; done
for t in game w00a w01a; do echo "== train_$t against hashref_gl44pgo0 (MGS2_ME_PIC=0: nothing native)"; python3 -I /home/claude/main/tools/refcmp.py --partial $R/cfg/hashref_gl44pgo0.txt $R/pg/log_$t.txt | tail -n 2; done
mkdir -p $R/gcda && cp $R/pg/game/gen.gcda $R/pg/rtb/boot_rt.gcda $R/pg/rtb/gen_dispatch.gcda $R/pg/w00a/gen_s_w00a.gcda $R/pg/w01a/gen_s_w01a.gcda $R/gcda/ \
  && for f in $R/pg/pic/*.gcda; do case $(basename $f) in gen_dispatch.gcda) ;; *) cp $f $R/gcda/ ;; esac; done
( cd $R/gcda && md5sum *.gcda > md5.txt; ls | wc -l ); echo "pgo_a44.sh: done $(date -u +%H:%M:%S)"
