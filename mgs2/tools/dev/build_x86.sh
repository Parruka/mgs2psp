#!/bin/sh
# build_x86.sh <dir> [make args, e.g. RTX=...]: the boot-path x86 probe with the flags of the reference build (GEN_MM_FAST GUEST_SPR0);
# the generated units are not rebuilt (-o) when only runtime headers changed
D=$1; shift; cd $D || exit 1
MG=/mnt/project-files/mgs2
OLD=""; for o in gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o; do [ -f $o ] && OLD="$OLD -o $o"; done
[ -n "$FULL" ] && OLD=""
make -f $MG/boot/Makefile.probe -j4 W=${W:-/home/claude/psp} $OLD CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0" "$@" boot 2>&1 | grep -v "^gcc" | head -20
