#!/bin/sh
# pgo_stagegen.sh <from dir> <new dir> <stage> <objects to drop...>: (v26) copy a build dir, drop the named objects and the link outputs,
# rebuild them with PGO=gen PGO_ELF= (plain main ELF objects) PGO_STAGE=<stage> (that overlay instrumented), relink.  W (headers, default
# /home/claude/v26/WP), VP (VU programs, default /home/claude/psp/vp22), J (jobs, default 3) from the environment.
F=$1; B=$2; ST=$3; shift 3
rm -rf "$B"; cp -a "$F" "$B" || exit 1
cd "$B" || exit 1
for o in "$@"; do rm -f "$B/$o"; done
rm -f "$B"/*.gcda "$B"/MGS2T.elf "$B"/EBOOT.PBP "$B"/ovl_*.bin "$B"/ovl_*.elf "$B"/ovl_*.ld "$B"/PARAM.SFO
OO=$(for o in *.o; do printf -- "-o %s " $o; done)
RTXX="-Wno-error=coverage-mismatch -DVP_VFPU" VP=${VP:-/home/claude/psp/vp22} J=${J:-3} timeout 3600 sh /mnt/project-files/mgs2/tools/dev/mk_psp.sh "$B" ${W:-/home/claude/v26/WP} PGO=gen PGO_ELF= PGO_STAGE=$ST LAYOUT=1 STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a" PSP_EBOOT_TITLE="MGS2 timing harness v26 pgo $ST" $OO
tail -n 1 make.log; echo "compiles: $(grep -c 'psp-gcc.* -c ' make.log)"; grep -o ' -o [a-z_0-9]*\.o' make.log | tr '\n' ' '; echo
