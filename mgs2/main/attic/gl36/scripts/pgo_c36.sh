#!/bin/sh
# pgo_c36.sh (gl36, after the emulator gate failed on the first final build psp_a: e2 and e3 stopped at vb 0, code 7, "disc: no memory
# for 177 sectors (361520 bytes of \MODULES\MODULE.IPK)"): t36 + the one memory fix in runtime/iop_host.h (disc_file_read without its
# whole-read buffer); the use build u again with the same profiles (build_gl36/gcda) and the same code order (ord/final: no new traces,
# no new search), the lost-profile check, the relink into psp; then each object of u against u_a (the first use build) and each
# function's size and address of psp's ELF against psp_a's (the order's figures hold where nothing moved).
R=/home/claude/main/build_gl36; cd $R/m || exit 1
[ -e $R/u ] && { echo "pgo_c36.sh: $R/u exists"; exit 1; }
PSPD=$R/u PGO=use GCDA=$R/gcda sh scripts/psp.sh > $R/u.out 2>&1; r=$?; echo "u rc $r $(date -u +%H:%M:%S)"; [ $r = 0 ] || exit 1
sh scripts/pgochk.sh $R/u $R/pgochk_u > $R/pgochk_u.out 2>&1; echo "pgochk rc $?"; tail -3 $R/pgochk_u.out | cut -c1-200
cd $R && sh rl36.sh psp > $R/rl36_c.out 2>&1; echo "relink rc $? $(date -u +%H:%M:%S)"; tail -6 $R/rl36_c.out | cut -c1-240
n=0; d=0; for o in $R/u/*.o; do n=$((n + 1)); cmp -s $o $R/u_a/$(basename $o) || { d=$((d + 1)); echo "  object differs from u_a: $(basename $o)"; }; done; echo "objects of u: $n, differing from u_a: $d"
NM=/home/claude/pspsdk/pspdev/bin/psp-nm
for e in psp_a psp; do $NM -S --defined-only $R/$e/MGS2T.elf | awk 'NF == 4 && $3 ~ /[tTwW]/ { print $4, $1, $2 }' | sort > $R/fn_$e.txt; done
echo "ELF functions (name address size): psp_a $(wc -l < $R/fn_psp_a.txt), psp $(wc -l < $R/fn_psp.txt); lines differing: $(diff $R/fn_psp_a.txt $R/fn_psp.txt | grep -c '^[<>]')"
diff $R/fn_psp_a.txt $R/fn_psp.txt | grep '^[<>]' | head -20
for f in ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin; do cmp -s $R/psp/$f $R/psp_a/$f && echo "  $f = psp_a's" || echo "  $f differs from psp_a's"; done
echo "pgo_c36.sh: done $(date -u +%H:%M:%S)"
