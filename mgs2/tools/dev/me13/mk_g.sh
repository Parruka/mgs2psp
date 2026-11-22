#!/bin/sh
# mk_g.sh: me13 g (task #147, the take-back x86 check) in /home/claude/me13/xg: xf's twin (mk_f.sh) with boot/xlive_chk_g.c
# (boot/xlive_chk_f.c + tools/patch_me13g.py: MGS2_XLIVE_TBK bit 8, the texture cache); every other object is xe's (ge_xlate.o) or xd's.
# Then xg/g0.txt as mk.sh's.
M=/home/claude/me13; O=$M/xg; mkdir -p $O && cd $O || exit 1
s=$(date +%s); : > cc.status
cp $M/boot/xlive_chk_f.c $M/boot/xlive_chk_g.c && python3 -I $M/tools/patch_me13g.py $M/boot/xlive_chk_g.c >> cc.status || exit 1
for o in boot_rt gen gen_dispatch gen_s_d00t gen_s_d01t gen_s_init gen_s_n_title gen_s_r_title gen_s_r_tnk0 gen_s_w00a gen_s_w01a iop_main iop_rt iop_rt_libs \
  iop_rt_ucontext jsifman cdbios spu2_lite ge_xlive ge_gsfront ge_gsup ge_xldig ge_texconv ge_texcache ge_gelive ge_gebatch ge_xconv ge_vu1conv ge_gelist \
  ge_fxthin ge_c1model ge_c2model ge_c3model; do ln -sf $M/xd/$o.o $o.o; done
ln -sf $M/xe/ge_xlate.o ge_xlate.o
gcc -O2 -g0 -Wall -Wextra -DGU_SENT -I$M/boot -c $M/boot/xlive_chk_g.c -o xlive_chk.o > cc_chk.log 2>&1; echo "chk $? $(( $(date +%s) - s )) s" >> cc.status
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o gen_s_w00a.o gen_s_w01a.o \
  iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o \
  ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_c1model.o ge_c2model.o ge_c3model.o -lm > link.log 2>&1
echo "link $? $(( $(date +%s) - s )) s" >> cc.status
nm -S boot > nm.txt
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
echo "g0 $(wc -l < g0.txt) functions; the same as xd's: $(cmp -s g0.txt $M/xd/g0.txt && echo yes || echo NO)" >> cc.status; cat cc.status
