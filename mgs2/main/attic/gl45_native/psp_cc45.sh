#!/bin/sh
# gl45 study: the hooked ELF unit compiled for the PSP as scripts/psp.sh compiles gen.o (no profile): gen45.o, for instruction counts
M=/home/claude/main; G=$M/gen; export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
cd /home/claude/n45/psp; s=$(date +%s)
psp-gcc -O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 \
  -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv '-DK_HSTACK=(128*1024)' '-DIOP_HSTACK=(64*1024)' -DGEN_MM_FAST -DGUEST_SPR0 -DGEN_PIN23 \
  -DHOST_STACK_POOL -DHOST_BIG_ALLOC -DVIF0_BUF_WORDS=0x4000 -I$M/boot -I$M/psp -I$M/runtime -I$M/boot -DVU_MCL_RZ -DVU_QMOVE_VFPU -DK_COUNTDOWN \
  -DGUEST_SPRSTATE -DGEN_LITE -DGEN_HB_SP -ffunction-sections "-DGEN=\"$G/wgh45/gen_elf.h\"" -c $M/boot_g0/gen.c -o gen45.o > cc_gen45.log 2>&1
echo "gen45.o rc $? $(( $(date +%s) - s )) s" >> cc_gen45.log
