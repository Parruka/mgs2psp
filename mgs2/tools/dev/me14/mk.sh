#!/bin/sh
# mk.sh <out dir>: me14 (task #149, the ME picture's build order step 2) plain x86 twin: sb14's twin (mk.sh, x1) with the picture code made
# ME-safe: ge/ xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 from /home/claude/me14/ge (sb14's +
# scratchpad me14/patch_me14.py) built with -DMGS2_ME_SAFE -fno-jump-tables and their memcpy / memset / memcmp calls renamed to me_rt's
# (objcopy), ge/me_rt.c, and boot/xlive_chk.c = sb14's + patch_me14_glue.py (the picture heap, the thinning's scan on the host's side).
# The other objects (game, runtime, xldig, fxthin, c1-c3model) are sb14 x1's.
O=$1; shift; T=/home/claude/sb14; E=/home/claude/me14; R=/home/claude/v31/x86
mkdir -p $O && cd $O || exit 1
for o in gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a iop_main iop_rt iop_rt_libs iop_rt_ucontext jsifman cdbios spu2_lite; do
  ln -sf $R/$o.o $o.o; done
ln -sf $T/x1/boot_rt.o boot_rt.o
for f in xldig fxthin c1model c2model c3model; do ln -sf $T/x1/ge_$f.o ge_$f.o; done
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
MS="-DMGS2_ME_SAFE -fno-jump-tables -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"   # (no __memcpy_chk: every copy through me_rt, as on the PSP)
RN="--redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2; do
  o=ge_$f.o; [ $f = gsup2 ] && o=ge_gsup.o
  gcc $GF $MS -c $E/ge/$f.c -o $o || exit 1
  objcopy $RN $o || exit 1
done
gcc $GF -c $E/ge/me_rt.c -o ge_me_rt.o || exit 1
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -c $E/boot/xlive_chk.c -o xlive_chk.o || exit 1
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o ge_me_rt.o -lm || exit 1
echo "outside symbols of the ME-safe files:"; for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup; do nm -u ge_$f.o | awk '{print $2}'; done | sort | uniq -c | tr '\n' ' '; echo
echo built $O
