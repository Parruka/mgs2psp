#!/bin/sh
# mk_g.sh: me14 g (task #149, the ME picture's build order step 2) in /home/claude/me14/xg: step 1's split twin (me13 mk_g.sh, xg) with the
# picture code made ME-safe: xa's ME-safe objects (mk.sh), ge_e/xlate.c (me13's take-back xlate.c + patch_me14.py's xlate edits) built
# the same way, and boot/xlive_chk_g.c = me13's + scratchpad me14/patch_me14g_glue.py (the picture heap; the thinning's scan by the host
# at the frame's first queued transfer, handed to the late walk with the record).  Game, runtime and other objects: me13 xd's.
M=/home/claude/me13; E=/home/claude/me14; O=$E/xg; mkdir -p $O && cd $O || exit 1
for o in boot_rt gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a iop_main iop_rt iop_rt_libs \
  iop_rt_ucontext jsifman cdbios spu2_lite ge_xldig ge_fxthin ge_c1model ge_c2model ge_c3model; do ln -sf $M/xd/$o.o $o.o; done
for o in ge_xlive ge_gsfront ge_gsup ge_texconv ge_texcache ge_gelive ge_gebatch ge_xconv ge_vu1conv ge_gelist ge_me_rt; do ln -sf $E/xa/$o.o $o.o; done
GF="-O2 -g0 -ffp-contract=off -Wall -Wextra -Wno-missing-field-initializers -DGU_SENT -DXL_CENSUS"
MS="-DMGS2_ME_SAFE -fno-jump-tables -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
gcc $GF $MS -I$E/ge -c $E/ge_e/xlate.c -o ge_xlate.o || exit 1
objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xlate.o || exit 1
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -I$M/boot -c $E/boot/xlive_chk_g.c -o xlive_chk.o || exit 1
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o \
  iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o \
  ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o ge_me_rt.o -lm || exit 1
cp $M/xg/g0.txt g0.txt   # the G0 functions' ranges: boot_rt and the game objects are xd's, so the ranges are the same if the link puts them
nm -S boot > nm.txt      # first; checked below
python3 -I -c '
G0 = "S_d00t_P299fb8 P11de00 S_d01t_P2a1200 S_w00a_P31c0d0 S_w00a_P3a4138 S_w00a_P11e0a8 P16d1ac S_w01a_P30c8b8 S_w01a_P322198 S_w01a_P30d6f8 S_w01a_P350f40 S_w01a_P3926e0".split()
sym = {}
for l in open("nm.txt"):
    f = l.split()
    if len(f) == 4: sym[f[3]] = (int(f[0], 16), int(f[1], 16))
    elif len(f) == 3: sym[f[2]] = (int(f[0], 16), 0)
b = sym["__executable_start"][0]
for g in G0: a, n = sym[g]; print("%x %x %s" % (a - b, n, g))
' > g0.txt
echo "g0 $(wc -l < g0.txt) functions; the same as me13 xg's: $(cmp -s g0.txt $M/xg/g0.txt && echo yes || echo NO)"
echo built $O
