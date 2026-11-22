#!/bin/sh
# mkpsp.sh <dir>: gl7's PSP build (/home/claude/gl7/d: EBOOT 61127531) with this tree (sb3: gl9's Stage B lanes 1 and 2 and the exact trims of
# renderer_notes 11.28, task #111): boot_rt.o again with runtime/ee_io.h from here, every picture object from ge/ here, xl_psp.o from psp/
# here (g_opt bits in runtime/host_time.h here), relinked as gl3.sh d links (rebuild_use.sh with nothing to drop: make finds these newer).
# No PGO for those (the model build).  Only boot_rt.o, xl_psp.o and ge_*.o change; the game's own objects are gl7's.  Game data passes
# through it: personal use only.
D=$1; T=/home/claude/sb3; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ ! -e "$D" ] && cp -a /home/claude/gl7/d "$D" && cd "$D" || { echo "mkpsp.sh: give a new directory"; exit 1; }
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist; do psp-gcc $GF -c $T/ge/$f.c -o ge_$f.o || exit 1; done
psp-gcc $GF -c $T/ge/gsup2.c -o ge_gsup2.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -c $T/psp/xl_psp.c -o xl_psp.o || exit 1
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" make.log | head -1 | sed "s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$T/runtime &|")
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp.sh: boot_rt.o failed"; exit 1; }
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness sb3 trims" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; grep -c 'psp-gcc.* -c ' make.log; ls -l EBOOT.PBP && md5sum EBOOT.PBP
