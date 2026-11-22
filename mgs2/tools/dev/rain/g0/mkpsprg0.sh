#!/bin/sh
# mkpsprg0.sh <new dir> (task #165, G0: mkpsprk2.sh with the w00a unit rebuilt from the private G0 tree /home/claude/sbg0/nat (line_sph's
# native form with g_opt2 G0 2048 and G0H 4096; MGS2_OPT2 sets them: ffffffdf on, ffffe7df off)).  As mkpsprk2.sh <new dir> (task #164: sbrk2 = gl17's sources (sb17 now, with the rain sprite filter) + sb17k.diff's buckets + the kill switch boot.c; from mkpsp17k.sh, task #160, Ffm 2026-11-03 16:32 "what cost more?"): a measurement-only PSP model build of the gl17 tree with the
# emulator profiler's buckets: the tree /home/claude/sb17k = sb17 (+ patch_calls) + ge/fxbk.c (the picture's calls by kind and owner,
# -DFX_BUCKET in gelive.c, xl_psp.c, fxbk.c), psp/main_psp.c's timed regions as keys (-DHOST_BUCKET) and gen_dispatch.o rebuilt with
# -DGEN_ACTOR_PROF (each actor's act under its act function; no PGO profile for that unit).  Otherwise mkpsp17.sh (gl7/d's game objects,
# no PGO for the picture code).  Never shipped.  Game data passes through it: personal use only.
D=$1; T=/home/claude/sbrk2; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ ! -e "$D" ] && cp -a /home/claude/gl7/d "$D" && cd "$D" || { echo "mkpsp17k.sh: give a new directory"; exit 1; }
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
for f in xlive xlate gsfront texconv texcache gebatch xconv vu1conv gelist fxthin; do psp-gcc $GF -c $T/ge/$f.c -o ge_$f.o || exit 1; done
psp-gcc $GF -DFX_BUCKET -c $T/ge/gelive.c -o ge_gelive.o || exit 1
psp-gcc $GF -DFX_BUCKET -c $T/ge/fxbk.c -o ge_fxbk.o || exit 1
psp-gcc $GF -c $T/ge/gsup2.c -o ge_gsup2.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DPOOL_KB=512 -DFX_BUCKET -I$SDK/include -I$T/runtime -I$MG/runtime -c $T/psp/xl_psp.c -o xl_psp.o || exit 1
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/boot/boot.c|$T/boot/boot.c|; s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$T/runtime &|")
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp17k.sh: boot_rt.o failed"; exit 1; }
HT=${HT:-MGS2 harness rain G0 buckets}
L=$(grep -o "psp-gcc [^&]*-o main_psp.o" make.log | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$T/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$HT\"'|; s| -c | -DHOST_BUCKET -c |")
eval "$L" > main_psp.out 2>&1 || { echo "mkpsp17k.sh: main_psp.o failed"; exit 1; }
grep -q 'HOST_BUCKET' main_psp.out 2>/dev/null; echo "main_psp line has HOST_BUCKET: $(echo "$L" | grep -c HOST_BUCKET)"
# gen_dispatch.o again with GEN_ACTOR_PROF (make rebuilds the missing object with genf.txt's flags; its profile left out)
cp genf.txt genf_orig.txt; printf '%s -DGEN_ACTOR_PROF\n' "$(cat genf_orig.txt | sed 's|-I/mnt/project-files/mgs2/psp |-I/home/claude/sbg0/nat &|')" > genf.txt; rm -f gen_dispatch.o gen_dispatch.gcda gen_s_w00a.o
grep -c "sbg0/nat" genf.txt
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_fxbk.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness rain G0 buckets" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; grep -c 'psp-gcc.* -c ' make.log; grep -o 'psp-gcc [^&]*-o gen_dispatch.o' make.log | grep -c GEN_ACTOR_PROF; ls -l EBOOT.PBP && md5sum EBOOT.PBP
psp-nm MGS2T.elf | grep -E ' (g_emuactor|g_emuprof)$'
