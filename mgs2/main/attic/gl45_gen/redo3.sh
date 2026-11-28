#!/bin/sh
# gl45 batch 3: the deck unit's split headers and -g objects again, after tools/g45_split.py learned to leave out a #line mark
# inside a hot body (one survived and renamed every line after it: 49 hot deck functions had no line evidence).
# usage: redo3.sh <tag> <variant>...   (baseL = the base header compiled with -DGEN_LEV; name it after base in the same call)
M=/home/claude/main; G=$M/gen/g45; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; W=/home/claude/g45; O=$W/psp; cd $W
GENF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv -DK_HSTACK=(128*1024) -DIOP_HSTACK=(64*1024) -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I$M/boot -I$M/psp -I$M/runtime -I$M/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -ffunction-sections"
tag=$1; shift; : > out/redo3_$tag.txt
for v in "$@"; do
  D=$G/p_$v; LV=-DGEN_LEV
  if [ $v = base ]; then S=$M/gen/wg/gen_s_w00a.h; LV=; elif [ $v = baseL ]; then D=$G/p_base; S=; else S=$D/full_w00a.h; fi
  if [ -n "$S" ]; then python3 -I tools/g45_split.py $S $D/gen_s_w00a.h out/keys.json out/map_${v}_w00a.json >> out/redo3_$tag.txt 2>&1 || exit 1
  else cp out/map_base_w00a.json out/map_baseL_w00a.json; fi
  rm -f /home/claude/g45/out/size_${v}_w00a.json
  s=$(date +%s); psp-gcc $GENF $LV -g "-DSTAGE_GEN=\"$D/gen_s_w00a.h\"" -c $M/boot_g0/gen_stage.c -o $O/${v}_w00a_g.o > $O/cc_${v}_w00a_g.o.log 2>&1
  echo "${v}_w00a_g.o rc $? $(( $(date +%s) - s )) s" >> out/redo3_$tag.txt
done
echo "redo3 $tag done" >> out/redo3_$tag.txt
