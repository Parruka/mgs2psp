#!/bin/sh
# mk.sh: me9's x86 twin (task #130) in /home/claude/me9/xd: sb9's x1 objects (symlinked) with me9's boot/xlive_chk.c (sb9's
# boot/xlive_chk.c + me9.diff), compiled as sb9's (-DGU_SENT).  Make xd first: mkdir xd; ln -s /home/claude/sb9/x1/*.o xd/; rm xd/xlive_chk.o
cd /home/claude/me9/xd && gcc -O2 -g0 -Wall -Wextra -DGU_SENT -I/home/claude/sb9/boot -c /home/claude/me9/boot/xlive_chk.c -o xlive_chk.o && \
gcc -o boot boot_rt.o gen.o gen_dispatch.o gen_s_d00t.o gen_s_d01t.o gen_s_init.o gen_s_n_title.o gen_s_r_title.o gen_s_r_tnk0.o \
  gen_s_w00a.o gen_s_w01a.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o xlive_chk.o ge_xlive.o \
  ge_xlate.o ge_gsfront.o ge_gsup.o ge_xldig.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o \
  ge_fxthin.o -lm
