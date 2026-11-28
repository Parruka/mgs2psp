#!/bin/sh
# gl45 batch 3: the deck unit's split header again (without the #line marks) and its -g object, for the variants named
M=/home/claude/main; G=$M/gen/g45; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; W=/home/claude/g45; O=$W/psp; cd $W
GENF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv -DK_HSTACK=(128*1024) -DIOP_HSTACK=(64*1024) -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I$M/boot -I$M/psp -I$M/runtime -I$M/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -ffunction-sections"
for v in "$@"; do
  D=$G/p_$v; if [ $v = base ]; then S=$M/gen/wg/gen_s_w00a.h; LV=; else S=$D/full_w00a.h; LV=-DGEN_LEV; fi
  python3 -I tools/g45_split.py $S $D/gen_s_w00a.h out/keys.json out/map_${v}_w00a.json > out/resplit_$v.txt 2>&1 || exit 1
  s=$(date +%s); psp-gcc $GENF $LV -g "-DSTAGE_GEN=\"$D/gen_s_w00a.h\"" -c $M/boot_g0/gen_stage.c -o $O/${v}_w00a_g.o > $O/cc_${v}_w00a_g.o.log 2>&1
  echo "${v}_w00a_g.o rc $? $(( $(date +%s) - s )) s" >> out/resplit_$v.txt
done
