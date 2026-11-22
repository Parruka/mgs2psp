#!/bin/sh
# mk.sh (gl30 G1 off the twin): gerl against gl29's gelist.c (v29), gl30's (v30) and gl30's with XS off (v30o), each gelist.c a copy
# with gxh_mark called before each gx_batch (sed; the build's own files are not touched), compiled with the twin's GF line (tw29.sh).
set -e
D=/home/claude/main/build_gl30/gerl; cd $D
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
mk() {   # <variant> <tree> [extra sed] [cflags]
  v=$1; t=$2; mkdir -p $v
  sed -e 's/^    if (gx_batch(i, 0)) {/    gxh_mark(i, 0, l->n, l->alow); if (gx_batch(i, 0)) {/' \
      -e 's/{ if (gx_batch(i, 1)) {/{ gxh_mark(i, 1, l->n, l->alow); if (gx_batch(i, 1)) {/' ${3:+-e "$3"} $t/ge/gelist.c > $v/gelist.c
  [ $(grep -c gxh_mark $v/gelist.c) = 2 ] || { echo "mk.sh: $v: the hooks did not go in"; exit 1; }
  echo 'void gxh_mark(unsigned i, int pass, unsigned n, unsigned alow);' > $v/hook.h
  gcc $GF -include $v/hook.h -I$t/ge -c $v/gelist.c -o $v/gelist.o
  gcc $GF $4 -I$t/ge -c gerl.c -o $v/gerl.o
  gcc -o $v/gerl $v/gerl.o $v/gelist.o -lm
  echo "$v: $(md5sum < $v/gelist.c | cut -c1-8) <- $t/ge/gelist.c ${3:+(sed $3)}"
}
mk v29 /home/claude/t29 "" -DGX_NO_NSUB
mk v30 /home/claude/t30
mk v30o /home/claude/t30 's/if (arm == GX_DZ_X \&\& !sx \&\& nb >= nd)/if (0 \&\& arm == GX_DZ_X \&\& !sx \&\& nb >= nd)/'
[ $(grep -c 'if (0 && arm == GX_DZ_X' v30o/gelist.c) = 1 ] || { echo "mk.sh: v30o: XS not off"; exit 1; }
