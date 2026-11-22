#!/bin/sh
# rebuild_use.sh <build dir> <objects to rebuild...>: drop those objects and the link outputs, rebuild them with the v17 flags (genf.txt: the build's GENF; v18 adds -DGUEST_SPRSTATE there) (PGO=use with
# the gcda files in the dir, LAYOUT=1 with its layout files), keep every other object, relink
B=$1; shift
cd $B || exit 1
for o in "$@"; do rm -f "$B/$o"; done
rm -f MGS2T.elf EBOOT.PBP ovl_*.bin ovl_*.elf PARAM.SFO
OO=$(for o in *.o; do printf -- "-o %s " $o; done)
RTXX="-Wno-error=coverage-mismatch -DVP_VFPU ${RTX18}" VP=${VP:-/home/claude/psp/vp16n} J=4 timeout 1800 sh /mnt/project-files/mgs2/tools/dev/mk_psp.sh $B ${W:-/home/claude/vglz/W} PGO=use LAYOUT=1 PGO_STAGE="w00a w01a" STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a" PSP_EBOOT_TITLE="${TITLE:-MGS2 timing harness v18}" $OO
tail -n 1 make.log; echo "compiles: $(grep -c 'psp-gcc.* -c ' make.log)"
