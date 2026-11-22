#!/bin/sh
# v26me4_test.sh [work dir] (x86 dev check, 2026-10-24): builds psp/v26me4_test.c over the part of psp/v26me4_exc.c that runs in the
# exception (everything before x_find, without its #include lines) and prints the crash records as the PSP's debug screen shows them.
MG=/mnt/project-files/mgs2; T=${1:-/home/claude/v26me4_test}
mkdir -p $T/inc
for h in pspkernel.h pspctrl.h pspdebug.h systemctrl.h ps2rc_kcall.h; do : > $T/inc/$h; done
awk '/^#include/ { next } /^static uint32_t x_find/ { exit } { print }' $MG/psp/v26me4_exc.c > $T/exc_part.c
gcc -O2 -no-pie -I$T/inc -I$T -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-pointer-to-int-cast \
  -Wno-int-to-pointer-cast -Wl,--defsym,t_exec=0x08804000 -Wl,--defsym,t_fdata=0x08be4fd0 -Wl,--defsym,t_ovl=0x08bee000 \
  -Wl,--defsym,mgs2_exc_test_pc=0x08b90288 $MG/psp/v26me4_test.c -o $T/v26me4_test && $T/v26me4_test
