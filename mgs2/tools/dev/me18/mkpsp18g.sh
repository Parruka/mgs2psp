#!/bin/sh
# me18e mkpsp18g.sh <dir> (task #180 step 4d-5, memory for the PSP: me_step4_notes.md 4d-5): mkpsp18f.sh with main_psp.o from
# /home/claude/me18e/psp/main_psp.c (MAIN_KB 256: the main thread's stack 768 KB smaller, to newlib's heap) and boot_rt.o with gl18's
# profile (sbg18/pg/gcda/boot_rt.gcda, gl18's PUSE flags kept from the base's line; the guarded functions lose theirs).  From mkpsp18f.sh:
# me18e mkpsp18f.sh <dir> (task #180 step 4d-4, the loop on the PSP's Media Engine: me_step4_notes.md 4d): mkpsp18e.sh with
# psp/xl_psp.c built -DMGS2_ME_REAL (the ME's start, the arena and the view's buffers in its eDRAM, its take-back and stop), the ME's
# side of the loop psp/xme_me.c (its entry, waits and cache steps) built as the ME-safe objects are and checked here (the only
# privileged instructions in it: Count reads, cache 0x14 / 0x19, sync, HALT, ctc1 $31, the one $gp write; no jalr, no jr but jr ra,
# no VFPU, no syscall, no trap), and v30me6c's ME bring-up psp/me/hle_me_mgs2.c (-DME_REST -DME_SONY -DME_SONY_BOOT -DME_KADDR, its
# reset stub moved to .data, as psp/gl16ep2.sh).  From mkpsp18e.sh:
# me18e mkpsp18e.sh <dir> (task #180 step 4d-1, mode 4 on the emulator, 4d-2, the ME's heap requests and the view's fixed buffers, and
# 4d-3, the ME's memory on 64-byte lines of its own: me_step4_notes.md 4d): the ME-safe objects' .data and .bss renamed .me_data and
# .me_bss and psp/xl_psp.c's .bss.me renamed .me_bss (psp-objcopy), placed on whole lines by psp/me_sec.ld (-Wl,-T: INSERT, the default
# script kept); tools/dev/me18/mesec_chk.py checks the ELF.  From 4d-2: mkpsp18d.sh with the picture files and
# psp/xl_psp.c from /home/claude/me18e (ge/xme.c the cache steps and SYNC, psp/xl_psp.c MGS2_ME_PIC=4).  From mkpsp18d.sh:
# me18d mkpsp18d.sh <dir> (task #180 step 4c, the ME's loop as a thread of the main CPU on the emulator: me_step4_notes.md 4c): me18c's
# mkpsp18c.sh with the picture files and psp/xl_psp.c from /home/claude/me18d (ge/xme.c the ME's loop, ge/xlme.c xm_lay, psp/xl_psp.c
# MGS2_ME_PIC=3) and ge/xme.c among the ME-safe objects.  From mkpsp18c.sh:
# me18c mkpsp18c.sh <dir> (task #180 step 4b-2, the copy at its deadline with the guards: me_step4_notes.md): me18b's mkpsp18b.sh
# (4b-1) on a directory made by mkgen18c.sh (gl18's base with the five game units holding the 12 G0 functions compiled again with
# their guards), with the picture files and psp/xl_psp.c from /home/claude/me18c (ge/xlme.c xm_plan / xm_fill, psp/xl_psp.c
# MGS2_ME_PIC=2: the log, the filter, the deadline copy, the check) and boot_rt.o built with -DME_GUARD_RT and /home/claude/me18c/rt
# (ee_io.h, sif.h, iop_host.h, kernel.h with the runtime's guards) ahead of sb18's runtime/.  boot_rt.o without PGO, as 4b-1 (a model
# build); the game units keep the base's profile.  Game data passes through it: personal use only.
D=$1; T=/home/claude/sb18; E=/home/claude/me18e; MG=/mnt/project-files/mgs2; SDK=/home/claude/pspsdk/pspdev/psp/sdk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
[ -n "$D" ] && [ -f "$D/cc_gen_s_w01a.o.log" ] && grep -q "rc 0" "$D/cc_gen_s_w01a.o.log" && cd "$D" || { echo "mkpsp18c.sh: give a directory made by mkgen18c.sh"; exit 1; }
ML=/home/claude/sbg18/base/make.log   # the base's compile lines (a relink in $D writes its own make.log)
BC=${BOOTC:-$T/boot/boot.c}   # b3: BOOTC=/home/claude/me18c/boot/boot.c (the checkpoints); WATCH=1: -DME_WATCH (diagnostic b3)
WD=; [ "$WATCH" = 1 ] && WD=-DME_WATCH
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
MEF="-fno-jump-tables -DMGS2_ME_SAFE"
HP=$MG/third_party/ps2rc/hle/psp
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme xme me_rt; do
  psp-gcc $GF $MEF -c $E/ge/$f.c -o ge_$f.o || exit 1
  [ $f = me_rt ] || psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_$f.o || exit 1
  psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_$f.o || exit 1   # 4d-3
done
# 4d-4: the ME's side of the loop, as the ME-safe objects (no data of its own), and its check
psp-gcc $GF $MEF -fstack-usage -I$HP -c $E/psp/xme_me.c -o ge_xme_me.o || exit 1
psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp ge_xme_me.o || exit 1
psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data ge_xme_me.o || exit 1
[ "$(psp-nm -u ge_xme_me.o | awk '{ print $2 }' | tr '\n' ' ')" = "xme_run " ] || { echo "mkpsp18f.sh: ge_xme_me.o names more than xme_run"; exit 1; }
psp-objdump -d ge_xme_me.o > ge_xme_me.dis
python3 -I $E/tools/xme_me_chk.py ge_xme_me.dis || exit 1
cat ge_xme_me.su
# 4d-4: v30me6c's ME bring-up (psp/gl16ep2.sh's line)
psp-gcc -O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 \
  -fno-merge-constants -Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv -DHLE_PSP -DHLE_HAVE_AUDIO_MODEL -DME_REST -DME_SONY \
  -DME_SONY_BOOT -DME_KADDR -I$SDK/include -I$HP -I$MG/psp/me -c $MG/psp/me/hle_me_mgs2.c -o hle_me.o || exit 1
psp-objcopy --rename-section .text.hle_me_stub=.data.hle_me_stub,alloc,load,contents,data hle_me.o || exit 1
psp-gcc $GF -c $E/ge/fxthin.c -o ge_fxthin.o || exit 1
psp-gcc $GF -DMGS2_XL -DMGS2_GL -DMGS2_ME_REAL -DME_REST -DME_SONY $WD -DPOOL_KB=512 -I$SDK/include -I$T/runtime -I$MG/runtime -I$HP -I$MG/psp/me -c $E/psp/xl_psp.c -o xl_psp.o || exit 1
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o || exit 1   # 4d-3: its objects the ME writes (ME_LN)
# 4d-5: boot_rt.o with gl18's own profile (its PUSE flags are the base line's, kept)
GC=/home/claude/sbg18/pg/gcda/boot_rt.gcda; [ "$(md5sum < $GC | cut -c1-32)" = 7a733edf53a0a547e9d90ccc49bee9a4 ] && cp $GC boot_rt.gcda || { echo "mkpsp18g.sh: gl18's boot_rt.gcda not found"; exit 1; }
L=$(grep -o "psp-gcc [^&]*-o boot_rt.o" "$ML" | head -1 | sed "s|/mnt/project-files/mgs2/boot/boot.c|$BC|; s|-I/mnt/project-files/mgs2/psp -I/mnt/project-files/mgs2/runtime|-I$E/rt -I$T/runtime &|; s|-DGUEST_SPLIT_RT|-DGUEST_SPLIT_RT -DME_GUARD_RT $WD|")
echo "$L" | grep -q "fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops" || { echo "mkpsp18g.sh: no profile flags in the boot_rt.o line"; exit 1; }
echo "$L" | grep -q ME_GUARD_RT || { echo "mkpsp18c.sh: no -DME_GUARD_RT in the boot_rt.o line"; exit 1; }
eval "$L" > boot_rt.out 2>&1 || { echo "mkpsp18c.sh: boot_rt.o failed"; cat boot_rt.out | head -n 20; exit 1; }
HT=${HT:-MGS2 harness me18e ME picture 4d-5}
L=$(grep -o "psp-gcc [^&]*-o main_psp.o" "$ML" | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$E/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|; s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$HT\"'|")
eval "$L" > main_psp.out 2>&1 || { echo "mkpsp18c.sh: main_psp.o failed"; exit 1; }
echo "$L" | grep -q "$E/psp/main_psp.c" || { echo "mkpsp18g.sh: main_psp.o not from $E/psp"; exit 1; }
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_xlme.o ge_xme.o ge_xme_me.o ge_fxthin.o ge_me_rt.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o hle_me.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user -Wl,-T,$E/psp/me_sec.ld" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/v31/WPL VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness me18e" sh $MG/tools/dev/rebuild_use.sh "$D" > relink.out 2>&1
tail -n 3 relink.out; ls -l EBOOT.PBP && md5sum EBOOT.PBP
echo "undefined memcpy/memset/memcmp/malloc family left in the ME-safe objects:"; for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme xme xme_me; do psp-nm -u ge_$f.o | grep -w "memcpy\|memset\|memcmp\|malloc\|calloc\|realloc\|free\|printf\|fprintf\|puts\|floorf\|ceilf" | sed "s/^/  ge_$f.o: /"; done
