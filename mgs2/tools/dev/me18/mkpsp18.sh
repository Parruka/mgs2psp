#!/bin/sh
# me18 mkpsp18.sh <dir> (task #180, the ME picture's build order step 4, part a): me14's mkpsp.sh (step 2: the picture files ME-safe on the
# PSP's main CPU) on gl18's tree: base /home/claude/sbg18/base (gl7/d with the rain's exact skips in the w00a unit), the picture files
# from /home/claude/me18/ge (sb18's = gl17's, made ME-safe by the scratchpad's me18/patch_me18.py) with -fno-jump-tables -DMGS2_ME_SAFE
# and the compiler's memcpy/memset/memcmp calls renamed to me_rt's; ge/me_rt.c added; fxthin (the host's scan) built as before;
# psp/xl_psp.c from /home/claude/me18/psp (me14's patch_me14_psp.py on sb18's, with gl17's scan arguments and the counts the ME-safe
# gl_call keeps on the frame); boot_rt.o and main_psp.o as gl18's (sb18's boot.c, runtime/ and psp/main_psp.c).  No PGO (a model build).
# ge/xlme.c (the ME's loop) is not in it.  Game data passes through it: personal use only.
D=$1; T=/home/claude/sb18; E=/home/claude/me18; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ ! -e "$D" ] && cp -a /home/claude/sbg18/base "$D" && cd "$D" || { echo "mkpsp18.sh: give a new directory"; exit 1; }
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
MEF="-fno-jump-tables -DMGS2_ME_SAFE"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 me_rt; do
  psp-gcc $GF $MEF -c $E/ge/$f.c -o ge_$f.o || exit 1
  [ $f = me_rt ] || psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_$f.o || exit 1
done
psp-gcc $GF -c $E/ge/fxthin.c -o ge_fxthin.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -c $E/psp/xl_psp.c -o xl_psp.o || exit 1
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/boot/boot.c|$T/boot/boot.c|; s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|-fprofile-use .*-fno-predictive-commoning||")   # (no profile: a model build)
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp18.sh: boot_rt.o failed"; exit 1; }
HT=${HT:-MGS2 harness me18 ME-safe picture}
L=$(grep -o "psp-gcc [^&]*-o main_psp.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$T/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$HT\"'|")
eval "$L" > main_psp.out 2>&1 || { echo "mkpsp18.sh: main_psp.o failed"; exit 1; }
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_me_rt.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness me18" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; ls -l EBOOT.PBP && md5sum EBOOT.PBP
echo "undefined memcpy/memset/memcmp/malloc family left in the ME-safe objects:"; for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2; do psp-nm -u ge_$f.o | grep -w "memcpy\|memset\|memcmp\|malloc\|calloc\|realloc\|free\|printf\|fprintf\|puts\|floorf\|ceilf" | sed "s/^/  ge_$f.o: /"; done
