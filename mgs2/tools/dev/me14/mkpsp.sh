#!/bin/sh
# me14 mkpsp.sh <dir> (task #149, the ME picture's build order step 2 on the PSP's main CPU): sb14 mkpsp.sh (gl16's tree, no PGO: the model
# build) with the picture files made ME-safe as the ME unit is built (scratchpad me14/mkme_psp.sh): ge/ from /home/claude/me14/ge with
# -fno-jump-tables -DMGS2_ME_SAFE and the compiler's memcpy/memset/memcmp calls renamed to me_rt's; ge/me_rt.c (the picture heap, the
# me_memcpy family) added; fxthin (the host's scan of the rain and splash) built as before; psp/xl_psp.c from me14/psp
# (scratchpad me14/patch_me14_psp.py: the heap's region, the scan by the host at the frame's first walked transfer, the heap's log line),
# built without MGS2_ME_SAFE (the main CPU's side: newlib's malloc, printing).  ge/xlme.c (the ME's loop) is not in it: the main CPU keeps
# xl_psp.c's own callbacks.  Game data passes through it: personal use only.
D=$1; T=/home/claude/sb14; E=/home/claude/me14; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ ! -e "$D" ] && cp -a /home/claude/gl7/d "$D" && cd "$D" || { echo "mkpsp.sh: give a new directory"; exit 1; }
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
MEF="-fno-jump-tables -DMGS2_ME_SAFE"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 me_rt; do
  psp-gcc $GF $MEF -c $E/ge/$f.c -o ge_$f.o || exit 1
  [ $f = me_rt ] || psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_$f.o || exit 1
done
psp-gcc $GF -c $E/ge/fxthin.c -o ge_fxthin.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -c $E/psp/xl_psp.c -o xl_psp.o || exit 1
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/boot/boot.c|$T/boot/boot.c|; s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$T/runtime &|")
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp.sh: boot_rt.o failed"; exit 1; }
HT=${HT:-MGS2 harness me14 ME-safe picture}
L=$(grep -o "psp-gcc [^&]*-o main_psp.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$T/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$HT\"'|")
eval "$L" > main_psp.out 2>&1 || { echo "mkpsp.sh: main_psp.o failed"; exit 1; }
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_me_rt.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness me14" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; grep -c 'psp-gcc.* -c ' make.log; ls -l EBOOT.PBP && md5sum EBOOT.PBP
echo "undefined memcpy/memset/memcmp left in the ME-safe objects:"; for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2; do psp-nm -u ge_$f.o | grep -w "memcpy\|memset\|memcmp\|malloc\|calloc\|realloc\|free" | sed "s/^/  ge_$f.o: /"; done
