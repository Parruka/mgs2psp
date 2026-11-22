#!/bin/sh
# g7_34.sh (gl33 G7): a second build from the tree and the profiles (PGO=use into build_gl34/u2, then the relink with the order into
# build_gl34/psp2) against the first (u, psp) and the deliver folder.
R=/home/claude/main/build_gl34; cd $R/m || exit 1
[ -e $R/u2 ] || [ -e $R/psp2 ] && { echo "g7_34.sh: u2 or psp2 exists"; exit 1; }
PSPD=$R/u2 PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u2.out 2>&1; echo "second use build rc $?"
RELINK=$R/u2 PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$R/psp2 TITLE="MGS2 harness gl34" sh scripts/psp.sh > $R/psp2.out 2>&1; echo "second relink rc $?"
echo "second build (PGO=use into u2, relinked with ord/final into psp2) against the first:"
for f in EBOOT.PBP MGS2T.elf ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin; do
  cmp -s $R/psp/$f $R/psp2/$f && echo "  same psp/$f $(md5sum < $R/psp/$f | cut -c1-32)" || echo "  DIFFERENT psp/$f"; done
n=0; d=0; for o in $R/u/*.o; do n=$((n + 1)); cmp -s $o $R/u2/$(basename $o) || { d=$((d + 1)); echo "  DIFFERENT object $(basename $o)"; }; done
echo "  objects of the use build: $n, differing $d"
for f in EBOOT.PBP ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin; do
  cmp -s $R/deliver/$f $R/psp/$f && echo "  deliver/$f = psp/$f" || echo "  DIFFERENT deliver/$f"; done
