#!/bin/sh
# gl45 batch 3: one variant of the two hot units compiled for the PSP as scripts/psp.sh compiles them (no profile), from headers
# split by tools/g45_split.py: psp/<v>_elf.o, psp/<v>_w00a.o (the shipping flags: sizes) and psp/<v>_elf_g.o, psp/<v>_w00a_g.o
# (-g: the line of each instruction, for the weights).  usage: psp_var.sh <v> <levers | base> <selection (all, all-b2, a .json)> [extra flags]
M=/home/claude/main; G=$M/gen/g45; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
v=$1; lev=$2; sel=$3; XF=$4; D=$G/p_$v; O=/home/claude/g45/psp; W=/home/claude/g45; mkdir -p $D $O; cd $W
if [ "$lev" = base ]; then E=$M/gen/wgh/gen_elf.h; S=$M/gen/wg/gen_s_w00a.h; LV=$XF
else
  python3 -I tools/gen_levers.py $M/gen/wgh/gen_elf.h $D/full_elf.h $lev $sel out/rep_p${v}_elf.json > out/psp_$v.txt 2>&1 || exit 1
  python3 -I tools/gen_levers.py $M/gen/wg/gen_s_w00a.h $D/full_w00a.h $lev $sel out/rep_p${v}_w00a.json >> out/psp_$v.txt 2>&1 || exit 1
  E=$D/full_elf.h; S=$D/full_w00a.h; LV="-DGEN_LEV $XF"
fi
python3 -I tools/g45_split.py $E $D/gen_elf.h out/keys.json out/map_${v}_elf.json >> out/psp_$v.txt 2>&1 || exit 1
python3 -I tools/g45_split.py $S $D/gen_s_w00a.h out/keys.json out/map_${v}_w00a.json >> out/psp_$v.txt 2>&1 || exit 1
GENF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv -DK_HSTACK=(128*1024) -DIOP_HSTACK=(64*1024) -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I$M/boot -I$M/psp -I$M/runtime -I$M/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -ffunction-sections $LV"
cc() { o=$1; shift; s=$(date +%s); psp-gcc $GENF "$@" -o $O/$o > $O/cc_$o.log 2>&1; echo "$o rc $? $(( $(date +%s) - s )) s" >> out/psp_$v.txt; }
( cc ${v}_elf.o "-DGEN=\"$D/gen_elf.h\"" -c $M/boot_g0/gen.c; cc ${v}_elf_g.o -g "-DGEN=\"$D/gen_elf.h\"" -c $M/boot_g0/gen.c ) &
( cc ${v}_w00a.o "-DSTAGE_GEN=\"$D/gen_s_w00a.h\"" -c $M/boot_g0/gen_stage.c; cc ${v}_w00a_g.o -g "-DSTAGE_GEN=\"$D/gen_s_w00a.h\"" -c $M/boot_g0/gen_stage.c ) &
wait
echo "psp_var $v done" >> out/psp_$v.txt
