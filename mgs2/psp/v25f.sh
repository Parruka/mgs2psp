#!/bin/sh
# v25f.sh <out dir>: the free-play timing build MGS2T_v25f (audit H1, 2026-10-23; psp_harness.md "Build v25f") from v24's build.
# Same code as v24 at the same addresses: every v24 object is linked as it is (V24 = v24's d build: the generated units, boot_rt.o with its
# PGO profile, the code order layout_*.ld); new are main_psp.o (v24's source, only the title differs, at the same length), free play's
# extra functions as separate units (tools/xunit.py: gen_x.o in the program, an extra part after each overlay's own end, psp/ovl_x.ld),
# psp/xhook.c (the stubs' hook through --wrap of rt_missing, start and end through --wrap of mgs2_main / mgs2_exit, the battery log) and
# pad.o, linked after v24's objects so that v24's read-only data and data keep their offsets modulo 8 KB (the caches' way size).
# Inputs in the container (ephemeral): V24, the full translation (FULL_ELF, FULL_W00A, FULL_D01T), the coverage lists (play/run).
set -e
O=$1; MG=/mnt/project-files/mgs2; V24=/home/claude/v24/d; R=/home/claude/play/run; CU=/home/claude/vglz/W/cov_u.txt
FULL_ELF=/home/claude/h1size/full_l/gen_elf.h; FULL_W00A=/home/claude/h1size/full_l/gen_s_w00a.h; FULL_D01T=/home/claude/play/st/d01t_l/gen_s_d01t.h
TITLE="MGS2T v25f free play"
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; P=/home/claude/pspsdk/pspdev/psp; SDK=$P/sdk
BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
GENF="$(cat $V24/genf.txt)"
mkdir -p $O/X; cd $O
for f in $V24/*.o $V24/layout_*.ld $V24/genf.txt; do cp $f .; done

# 1. free play's extra functions: E = (prefork + mix coverage) - v24's coverage (cov_u), per unit
CL=$R/cov_prefork.txt,$R/cov_mix.txt
python3 $MG/tools/xunit.py $FULL_ELF elf $CL $CU X/gen_xelf.h
python3 $MG/tools/xunit.py $FULL_W00A w00a $CL $CU X/gen_s_w00a_x.h
python3 $MG/tools/xunit.py $FULL_D01T d01t $CL $CU X/gen_s_d01t_x.h
# compiled as v24's units (genf.txt), without PGO; no merged constants (nothing joins v24's merged blocks)
cc() { eval psp-gcc $GENF -ffunction-sections -fno-merge-constants "$@"; }
cc -DGEN="'\"$O/X/gen_xelf.h\"'" -c $MG/boot/gen.c -o gen_x.o &
cc -DSTAGE_GEN="'\"$O/X/gen_s_w00a_x.h\"'" -c $MG/boot/gen_stage.c -o gen_s_w00a_x.o &
cc -DSTAGE_GEN="'\"$O/X/gen_s_d01t_x.h\"'" -c $MG/boot/gen_stage.c -o gen_s_d01t_x.o &
wait

# 2. the harness: v24's main_psp.c (psp/v25f/main_psp.c: psp/main_psp.c as it was for v25f; main_psp.c has moved on to v26) with its own title; the hook; the pads
psp-gcc $BASEF -ffunction-sections -DGUEST_SPRSTATE -DOVL_SIZE=0x460000 "-DHARNESS_TITLE=\"$TITLE\"" -I$SDK/include -I$MG/runtime -c $MG/psp/v25f/main_psp.c -o main_psp.o
psp-gcc $BASEF -Wall -ffunction-sections -fno-merge-constants -DOVL_SIZE=0x460000 -I$SDK/include -c $MG/psp/xhook.c -o xhook.o
# fills found from the first link (text: .rodata on v24's offset mod 8 KB after .text and the import tables; rodata: .data; data: the
# libraries' data and .bss): see psp_harness.md "Build v25f"
psp-gcc -c -DPAD_TEXT=0x17c8 -DPAD_RODATA=0x1c80 -DPAD_DATA=0xa0 $MG/psp/v25f_pad.S -o pad.o

# 3. the link: v24's (make.log) with the new units after v24's objects and the three wraps; xh_k_vblanks = boot.c's static k_vblanks
lnk() { psp-gcc -I. -I$P/include -I$P/sdk/include $BASEF -D_PSP_FW_VERSION=600 -L. -L$P/lib -L$P/sdk/lib -Wl,--section-ordering-file=layout_elf.ld \
  $MG/psp/spr_state.ld -Wl,-zmax-page-size=128 -Wl,--wrap=rt_missing,--wrap=mgs2_main,--wrap=mgs2_exit "$@" \
  main_psp.o hwprobe.o psp_ctx.o boot_rt.o gen.o gen_dispatch.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o iop_main.o spu2_lite.o gen_x.o xhook.o pad.o \
  -lpspfpu -lpsppower -lpspdebug -lpspctrl -lpspdisplay -lm -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspnet -lpspnet_apctl -o MGS2T.elf -Wl,-Map,MGS2T.map; }
lnk
K=$(psp-nm MGS2T.elf | awk '$3 == "k_vblanks" { print $1 }'); [ $(echo "$K" | wc -w) = 1 ]
lnk -Wl,--defsym,xh_k_vblanks=0x$K
cp MGS2T.elf MGS2T_prefix.elf
mksfoex -d MEMSIZE=1 "$TITLE" PARAM.SFO
psp-fixup-imports MGS2T.elf
psp-strip MGS2T.elf -o MGS2T_strip.elf
pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL MGS2T_strip.elf NULL

# 4. the overlays at the arena's new address (+128 KB: a multiple of 8 KB), every stub call wrapped; w00a and d01t with their extra part
B=0x$(psp-nm MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); LG=$(psp-gcc -print-libgcc-file-name)
for st in init r_title r_tnk0 n_title d00t w01a w00a d01t; do
  sed '/KEEP(\*(.ovl_hdr))/r layout_'$st'.ld' $MG/psp/ovl.ld > ovl_$st.ld
  psp-ld -T ovl_$st.ld --wrap=rt_missing --defsym OVL_BASE=$B --defsym OVL_SIZE=0x460000 -R MGS2T.elf -o ovl_${st}_plain.elf ovl_hdr_$st.o gen_s_$st.o $LG
  psp-objcopy -O binary -j .ovl ovl_${st}_plain.elf ovl_${st}_plain.bin
done
for st in init r_title r_tnk0 n_title d00t w01a; do cp ovl_${st}_plain.bin ovl_$st.bin; done
# d01t's extra part needs libgcc's _ucmpdi2, which the reduced part does not: it goes into the extra part (else the reduced part moves)
sed '/KEEP(\*(.ovl_hdr))/r layout_w00a.ld' $MG/psp/ovl_x.ld > ovl_x_w00a.ld
sed -e 's/EXCLUDE_FILE(\*_x\.o)/EXCLUDE_FILE(*_x.o *libgcc.a:_ucmpdi2.o)/' -e 's/^\(    \)\*_x\.o(\(.*\))$/&\n\1*libgcc.a:_ucmpdi2.o(\2)/' $MG/psp/ovl_x.ld |
  sed '/KEEP(\*(.ovl_hdr))/r layout_d01t.ld' > ovl_x_d01t.ld
for st in w00a d01t; do
  psp-ld -T ovl_x_$st.ld --wrap=rt_missing --defsym OVL_BASE=$B --defsym OVL_SIZE=0x460000 -R MGS2T.elf -o ovl_$st.elf ovl_hdr_$st.o gen_s_$st.o gen_s_${st}_x.o $LG -Map ovl_$st.map
  psp-objcopy -O binary -j .ovl ovl_$st.elf ovl_$st.bin
  n=$(stat -c %s ovl_${st}_plain.bin); cmp -n $n ovl_${st}_plain.bin ovl_$st.bin   # the reduced part as in the plain link
done

# 5. checks: v24's symbols (tools/elfcmp.py: code at v24's addresses, data moved by multiples of 8 KB); every plain overlay the size of v24's
python3 $MG/tools/dev/elfcmp.py $V24/MGS2T.elf MGS2T.elf
for st in init r_title r_tnk0 n_title d00t w01a w00a d01t; do [ $(stat -c %s ovl_${st}_plain.bin) = $(stat -c %s $V24/ovl_$st.bin) ]; done
md5sum EBOOT.PBP ovl_*.bin | grep -v _plain
