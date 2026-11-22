#!/bin/sh
# mkgen18c.sh <build dir> (task #180 step 4b-2): the five generated units that hold the 12 G0 functions compiled again in a copy of gl18's
# base (/home/claude/sbg18/base) from tools/dev/me18/patch_g0.py's headers (/home/claude/me18c/wg) through /home/claude/me18c/boot
# (gen_pre.h with the guards), with the base's flags (genf.txt; the ELF unit, w00a and w01a with the base's profile, as psp/Makefile
# PGO=use LAYOUT=1 PGO_STAGE="w00a w01a"; d00t and d01t without, as there).  Game data passes through it: personal use only.
D=$1; E=/home/claude/me18c
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
cd "$D" || exit 1
PUSE="-fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning"
GENF="$(cat genf.txt)"
cc() {   # <object> <pgo flags> <-D...> <source>
  s=$(date +%s); rm -f "$1"
  eval "psp-gcc $GENF $2 -ffunction-sections $3 -c $4 -o $1" > "cc_$1.log" 2>&1
  echo "$1 rc $? $(( $(date +%s) - s )) s" >> "cc_$1.log"
}
cc gen.o "$PUSE" "-DGEN='\"$E/wg/gen_elf.h\"'" $E/boot/gen.c &
cc gen_s_w00a.o "$PUSE" "-DSTAGE_GEN='\"$E/wg/gen_s_w00a.h\"'" $E/boot/gen_stage.c &
cc gen_s_w01a.o "$PUSE" "-DSTAGE_GEN='\"$E/wg/gen_s_w01a.h\"'" $E/boot/gen_stage.c &
wait
cc gen_s_d00t.o "" "-DSTAGE_GEN='\"$E/wg/gen_s_d00t.h\"'" $E/boot/gen_stage.c &
cc gen_s_d01t.o "" "-DSTAGE_GEN='\"$E/wg/gen_s_d01t.h\"'" $E/boot/gen_stage.c &
wait
for o in gen.o gen_s_w00a.o gen_s_w01a.o gen_s_d00t.o gen_s_d01t.o; do tail -n 1 cc_$o.log; done
md5sum gen.o gen_s_w00a.o gen_s_w01a.o gen_s_d00t.o gen_s_d01t.o
echo "mkgen18c done"
