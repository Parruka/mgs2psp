#!/bin/sh
# exc3_test.sh [work dir] (x86 dev check, 2026-10-27, task #71): builds psp/exc3_test.c over the part of psp/exc3.c before x_find
# (the record, the handler's screen code, the writer thread's save and the dry run; without its #include lines) and prints the crash
# records as the PSP's debug screen shows them, with what the writer appends to crash.txt.
# C=exc3b (2026-10-28): catcher 3b's psp/exc3b.c with psp/exc3b_test.c (default C=exc3: gl4's).
MG=/mnt/project-files/mgs2; T=${1:-/home/claude/exc3_test}; C=${C:-exc3}
mkdir -p $T/inc
for h in pspkernel.h pspctrl.h pspdebug.h psppower.h systemctrl.h ps2rc_kcall.h; do : > $T/inc/$h; done
awk '/^#include/ { next } /^static uint32_t x_find/ { exit } { print }' $MG/psp/$C.c > $T/exc_part.c
gcc -O2 -no-pie -I$T/inc -I$T -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-pointer-to-int-cast \
  -Wno-int-to-pointer-cast -Wl,--defsym,t_exec=0x08804000 -Wl,--defsym,t_fdata=0x08be4fd0 -Wl,--defsym,t_ovl=0x08bee000 \
  -Wl,--defsym,mgs2_exc_test_pc=0x08b90288 $MG/psp/${C}_test.c -o $T/${C}_test && $T/${C}_test
