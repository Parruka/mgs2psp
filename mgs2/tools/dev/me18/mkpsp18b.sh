#!/bin/sh
# me18b mkpsp18b.sh <dir> (task #180, the ME picture's build order step 4b-1: the split on the main CPU): me18's mkpsp18.sh (step 4a) with
# the picture files and psp/xl_psp.c from /home/claude/me18b (ge/xlview.h the copy's view, ge/xlme.c the late walk and the copy, built
# ME-safe and linked now; psp/xl_psp.c the split MGS2_ME_PIC: records, marks, the copy as each transfer ends, the late walk at the next
# vblank hook) and boot_rt.o with /home/claude/me18b/rt/ee_io.h (sb18's with XL_IMG and xlive_chk_end) ahead of sb18's runtime/.
# Base /home/claude/sbg18/base (gl18's tree).  No PGO (a model build).  Game data passes through it: personal use only.
D=$1; T=/home/claude/sb18; E=/home/claude/me18b; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ ! -e "$D" ] && cp -a /home/claude/sbg18/base "$D" && cd "$D" || { echo "mkpsp18b.sh: give a new directory"; exit 1; }
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
MEF="-fno-jump-tables -DMGS2_ME_SAFE"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme me_rt; do
  psp-gcc $GF $MEF -c $E/ge/$f.c -o ge_$f.o || exit 1
  [ $f = me_rt ] || psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_$f.o || exit 1
done
psp-gcc $GF -c $E/ge/fxthin.c -o ge_fxthin.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -c $E/psp/xl_psp.c -o xl_psp.o || exit 1
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/boot/boot.c|$T/boot/boot.c|; s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$E/rt -I$T/runtime &|; s|-fprofile-use .*-fno-predictive-commoning||")   # (no profile: a model build)
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp18b.sh: boot_rt.o failed"; cat boot_rt.out | head -n 20; exit 1; }
HT=${HT:-MGS2 harness me18b ME picture split}
L=$(grep -o "psp-gcc [^&]*-o main_psp.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$T/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$HT\"'|")
eval "$L" > main_psp.out 2>&1 || { echo "mkpsp18b.sh: main_psp.o failed"; exit 1; }
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_xlme.o ge_fxthin.o ge_me_rt.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness me18b" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; ls -l EBOOT.PBP && md5sum EBOOT.PBP
echo "undefined memcpy/memset/memcmp/malloc family left in the ME-safe objects:"; for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme; do psp-nm -u ge_$f.o | grep -w "memcpy\|memset\|memcmp\|malloc\|calloc\|realloc\|free\|printf\|fprintf\|puts\|floorf\|ceilf" | sed "s/^/  ge_$f.o: /"; done
