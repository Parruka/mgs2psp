#!/bin/sh
# v30me6.sh <out dir> [<v30 build dir, default /home/claude/v30/d>]: MGS2T_v30me6a / v30me6b (2026-10-26; psp_harness.md "Build v30me6"),
# v30me5 (psp/v30me5.sh, whose text below applies) with the Media Engine started the way Sony's own ME driver starts it, after the study
# mgs2/me_startup_study.md (-DME_SONY on psp/me/hle_me_mgs2.c, psp/me_mix.c and psp/v26me_bat.c): the stub at 0xBFC00040, the kernel's
# start code at 0xBFC00000-0x3F left as it is (checked word by word first; if it differs the ME is not started and the main CPU mixes),
# the reset bit changed with interrupts held off, and reads for the log (the start code, the ME's 0xBC100000 as found, the main CPU's
# 0xBC100000 and reset register in the battery lines).  No new register write.  SONYBOOT=1 (build b) adds the rest of Sony's ME boot
# code (-DME_SONY_BOOT): 0xBC100050 = 7 (was 6) and the VME's reset and set-up (0xBCC00010, 0xBCC00070, 0xBCC00030, 0xBCC00040), which
# are new writes and ship only with Ffm's OK.  KADDR=1 (with SONYBOOT=1: build c, v30me6c, 2026-10-27) adds -DME_KADDR on
# psp/me/hle_me_mgs2.c: its kernel-mode helpers run at their kernel address, as the crash catcher's handler does (v30me6b runs 2 and 3
# stopped in k_kick: an interrupt in kernel mode at a user address is a break for Sony's interrupt manager); no new register write.
# Without -DME_SONY the sources give v30me5's objects.  v30me5.sh's text:
# v30me5.sh <out dir> [<v30 build dir, default /home/claude/v30/d>]: MGS2T_v30me5 (2026-10-26; psp_harness.md "Build v30me5"), v30's
# Media Engine build (psp/v30me.sh, whose text below applies) with one change, the resting Media Engine (Ffm chose "Build it",
# decision card 2026-10-26 20:43): when it has nothing to mix the ME halts instead of polling at full speed, and the main CPU wakes
# it with its interrupt when it queues commands (-DME_REST on psp/me_mix.c, psp/me_mix_me.c and psp/me/hle_me_mgs2.c; the ME's unit
# is psp/me_mix_me.c and the new psp/me_rest.S joined with ld -r and checked as one), plus diagnostics: the rest counts, the ME's
# interrupt flags and the words of Sony's ME requests in the log and battery.txt, and battery.txt's capacity and suspend reads
# (-DME_REST -DME_DIAG5 on psp/v26me_bat.c).  The rest are v30me.sh's units, flags, pads search and checks; the title has the length
# of v30's.  MGS2_ME_REST=0 in mgs2.cfg keeps the ME polling as in v30 (the same build).  v30me.sh's text:
# v30me.sh <out dir> [<v30 build dir, default /home/claude/v30/d>]: MGS2T_v30 (2026-10-26; psp_harness.md "Build v30"), the Media Engine
# sound mix, crash catcher 2 and battery log of v26me4 layered on v30's build d exactly as psp/v29me.sh layered them on v29's: the same
# units, flags, pads search and checks; only the base and the titles differ from v29me.sh.  v29me.sh's text:
# v29me.sh <out dir> [<v29 build dir, default /home/claude/v29/d>]: MGS2T_v29 (2026-10-25; psp_harness.md "Build v29"), the Media Engine
# sound mix, crash catcher 2 and battery log of v26me4 layered on v29's build d exactly as psp/v28me.sh layered them on v28's: the same
# units, flags, pads search and checks; only the base and the titles differ from v28me.sh.  v28me.sh's text:
# v28me.sh <out dir> [<v28 build dir, default /home/claude/v28/d>]: MGS2T_v28 (2026-10-25; psp_harness.md "Build v28"), the Media Engine
# sound mix, crash catcher 2 and battery log of v26me4 (psp/v26me4.sh, whose text below applies) layered on v28's build d the way
# v26me4 layered them on v26's: the same units, flags and checks, main_psp.o with the new title (psp/main_psp.c, which is v26's
# psp/v26/main_psp.c byte for byte), the new units after the base's objects with pads so that the base's code keeps its addresses and
# its data their offsets modulo 8 KB (tools/dev/padfit.py), the overlays relinked at the arena's new address.  Only the base, the
# titles and OVL_SIZE (from the base's make.log) differ from v26me4.sh.  v26me4.sh's text:
# v26me4.sh <out dir> [<v26 build dir>]: MGS2T_v26me4 (2026-10-24; psp_harness.md "Build v26me4"), v26me3 build e with two changes, both
# diagnostics: battery.txt's lines also give the time since power-on, the CPU / bus clocks, the program break and the free memory
# (-DME_DIAG4 on psp/v26me_bat.c; after v26me3's run 2 stopped 39.5 minutes in, v26me2 38.6), and the
# crash catcher's second form, psp/v26me4_exc.c and psp/v26me4_exc.S, in place of psp/v26me_exc.c / .S (always in: EXC=1 of
# v26me3.sh).  v26me3's catcher returned from the exception into a trap that made system calls, and its self-test switched Ffm's PSP off
# (run 1); this one draws the record on the screen from the exception itself (no system call) and spins (PSPLink's handler too runs
# its own code in kernel mode, not back in the thread).  The game's and the Media Engine's objects are v26me3 build e's (the same sources and flags); the battery
# log's too, plus me_bat_hold / me_bat_unhold (-DME_EXC_HOLD: the self-test holds battery.txt's semaphore, so no write of it is under
# way when the PSP stops in the test).  MGS2_EXC=1 in mgs2.cfg turns the catcher on and offers the self-test on the screen at the start;
# psp/v26me4_test.sh prints the record as the screen shows it (x86).  v26me3's own text follows.
# v26me3.sh <out dir> [<v26 build dir>]: MGS2T_v26me3 (2026-10-24; psp_harness.md "Build v26me3"), v26me2 with diagnostics after v26me2's
# PSP run ended in a power-off at vb 33,600-33,700 (no cause in its log): -DME_DIAG3 on psp/me_mix.c, psp/me_mix_me.c (both builds) and
# psp/v26me_bat.c.  battery.txt gets a line every 5 s instead of every 60 s, each with the Media Engine's idle polls, the last command it
# applied, the EPC of an exception it took and the power callback's counts and last flags; the "mix vb" lines add the Media Engine's busy
# time from its idle polls (polls per second with an empty ring measured in the 100 ms after its start).  Without -DME_DIAG3 the sources
# give v26me2's objects byte for byte.  EXC=1 (Ffm chose to add it, decision card 2026-10-24 14:41): the crash catcher, psp/v26me_exc.c
# and psp/v26me_exc.S (a default exception handler registered in kernel mode through KUBridge, as tt_port's psp_bench; the record on the
# screen, in crash.txt and log.txt; waits for X), -DME_EXC on psp/me_mix.c and psp/v26me_bat.c (the release before every exit, the
# install at the start, the dry run's vblank hook) and -lpspsystemctrl_user (sctrlHENFindFunction); without EXC=1 the commands are
# those of build d.  v26me2's own text follows.
# v26me2.sh <out dir> [<v26 build dir>]: MGS2T_v26me2 (2026-10-24; psp_harness.md "Build v26me2"), v26me with one change: the Media
# Engine's bring-up is the port's copy psp/me/hle_me_mgs2.c (the reset stub writes the ME's memory range, bus clock enables and NMI flags
# before any access to main RAM, and leaves progress words in the ME's boot memory that the main CPU reads back and logs: "me:" line),
# me_mix.c with -DHLE_ME_PROGRESS (that line), me_mix_me.c's ME build with -DME_PROG_MARK (the loop's mark).  v26me's own text follows.
# v26me.sh <out dir> [<v26 build dir>]: the Media Engine sound probe MGS2T_v26me (2026-10-23; Ffm chose the Media Engine for the game's
# sound at 18:28; psp_harness.md "Build v26me") from v26's build (default /home/claude/v26/d, ephemeral).  v26's code at v26's addresses:
# every v26 object is linked as it is (the generated units with their PGO profiles, the code order layout_*.ld); new are main_psp.o
# (psp/v26/main_psp.c: main_psp.c as it was for v26, only the title differs, at the same length), the split (boot/spu_split.c, PS2RC's
# audio/spu2_split.c), the PSP side (psp/me_mix.c), the Media Engine's loop (psp/me_mix_me.c, twice: for the Media Engine, and with
# -DME_SIM for the emulator check MGS2_SPU_ME=2), PS2RC's bring-up (hle/psp/hle_me.c, ps2rc_kcall.S, unchanged), the battery log
# (psp/v26me_bat.c: battery.txt with the Media Engine's state) and pad.o (psp/v26me_pad.S).  They are linked after v26's objects
# and built with -fno-merge-constants (nothing joins v26's merged string and constant blocks), so v26's read-only data and data, and
# the libraries' data, keep their offsets modulo 8 KB (the caches' way size): tools/dev/padfit.py compares the link maps with v26's
# own link and sizes the pads.  Seven --wrap (boot/spu_split.c, psp/me_mix.c, psp/v26me_bat.c); the overlays are v26's objects
# relinked at the arena's new address.
set -e
O=$1; V26=${2:-/home/claude/v30/d}; MG=/mnt/project-files/mgs2   # (V26: the base build, named as in v26me4.sh)
SB=${SONYBOOT:-0}; if [ "$SB" = 1 ]; then TITLE="MGS2 v30 ME Sony (me6b)"; SD=" -DME_SONY -DME_SONY_BOOT"; else TITLE="MGS2 v30 ME Sony (me6a)"; SD=" -DME_SONY"; fi; T26="MGS2 timing harness v30"
KD=""; if [ "${KADDR:-0}" = 1 ]; then [ "$SB" = 1 ]; TITLE="MGS2 v30 ME Sony (me6c)"; KD=" -DME_KADDR"; fi
[ ${#TITLE} = ${#T26} ]
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; P=/home/claude/pspsdk/pspdev/psp; SDK=$P/sdk
BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
MACHF="-Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv"
AU=$MG/third_party/ps2rc/audio; RT=$MG/third_party/ps2rc/ps2rc/iop/rt; HP=$MG/third_party/ps2rc/hle/psp
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
mkdir -p $O; cd $O
for f in $V26/*.o $V26/layout_*.ld $V26/make.log $V26/genf.txt; do cp $f .; done
for st in $STAGES; do cp $V26/ovl_$st.ld .; done

OS0=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' make.log | head -1 | cut -d= -f2); [ -n "$OS0" ]
# 1. main_psp.o with the new title; first with v26's title, which must give v26's object (the same flags; only the file name differs)
# v30me5: psp/v30/main_psp.c, main_psp.c as it was for v30 (psp/main_psp.c has the translator's measurement lines since)
mpc() { psp-gcc $BASEF -ffunction-sections -DGUEST_SPRSTATE -DOVL_SIZE=$OS0 "-DHARNESS_TITLE=\"$1\"" -I$SDK/include -I$MG/runtime -c $MG/psp/v30/main_psp.c -o $2; }
dump() { psp-objdump -s -r $1 | tail -n +3; }
mpc "$T26" main_psp_t26.o; dump main_psp_t26.o > mp_a.txt; dump $V26/main_psp.o > mp_b.txt; cmp mp_a.txt mp_b.txt
mpc "$TITLE" main_psp.o

# 2. the new units (EXC=1: XD adds the crash catcher's hooks to me_mix.c and v26me_bat.c; XO its objects; XL its library)
XD=" -DME_EXC"; XO=" v26me4_exc.o v26me4_exc_s.o"; XL=" -lpspsystemctrl_user"
cc() { psp-gcc $BASEF -fno-merge-constants "$@"; }
cc $MACHF -I$MG/psp -I$RT -I$AU -I$MG/boot -c $MG/boot/spu_split.c -o spu_split.o
cc $MACHF -I$AU -Dspu2_bind_ops=__real_spu2_bind_ops -Dspu2_render48=__real_spu2_render48 -c $AU/spu2_split.c -o spu2_split.o
cc $MACHF -DHLE_ME_PROGRESS -DME_DIAG3 -DME_REST$XD$SD -I$SDK/include -I$HP -I$MG/psp/me -I$MG/psp -I$AU -I$MG/boot -c $MG/psp/me_mix.c -o me_mix.o
cc $MACHF -DHLE_PSP -DHLE_HAVE_AUDIO_MODEL -DME_REST$SD$KD -I$SDK/include -I$HP -I$MG/psp/me -c $MG/psp/me/hle_me_mgs2.c -o hle_me.o
# hle_me.c's reset stub and the ME's exception handler (section .text.hle_me_stub, 16-byte aligned: the handler's address goes to the
# ME's CP0) into .data, which is 16-byte aligned already: in .text they would raise its alignment from 4 to 16 and move all of v26's
# code by 4 bytes.  The main CPU never runs them: the stub is copied to the ME's reset vector, the handler runs on the ME.
psp-objcopy --rename-section .text.hle_me_stub=.data.hle_me_stub,alloc,load,contents,data hle_me.o
psp-gcc -I$SDK/include -c $HP/ps2rc_kcall.S -o ps2rc_kcall.o
cc -DME_PROG_MARK -DME_DIAG3 -DME_REST -fno-jump-tables -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -I$AU -I$HP -I$MG/boot -I$MG/psp -c $MG/psp/me_mix_me.c -o me_mix_me0.o
psp-objcopy --redefine-sym memset=me_memset me_mix_me0.o me_mix_me1.o
# v30me5: the rest, its handler and the ME's interrupt set-up (psp/me_rest.S), joined to the loop's unit: the checks below see both
psp-gcc -c $MG/psp/me_rest.S -o me_rest.o
[ "$(psp-objdump -h me_rest.o | awk '$2 == ".text" { print $7 }')" = "2**2" ]
psp-ld -r -o me_mix_me.o me_mix_me1.o me_rest.o
cc -DME_SIM -DME_DIAG3 -DME_REST -I$SDK/include -I$AU -I$HP -I$MG/boot -I$MG/psp -c $MG/psp/me_mix_me.c -o me_sim.o
cc -DME_DIAG3 -DME_DIAG4 -DME_DIAG5 -DME_REST$XD$SD -DME_EXC_HOLD -I$SDK/include -c $MG/psp/v26me_bat.c -o v26me_bat.o
cc $MACHF -DME_EXC -DOVL_SIZE=$OS0 -I$SDK/include -I$HP -c $MG/psp/v26me4_exc.c -o v26me4_exc.o
psp-gcc -I$SDK/include -c $MG/psp/v26me4_exc.S -o v26me4_exc_s.o
# the handler's .text must stay 4-byte aligned (16 would move all of v26's code, as hle_me's stub would: see below)
[ "$(psp-objdump -h v26me4_exc_s.o | awk '$2 == ".text" { print $7 }')" = "2**2" ]
# the Media Engine's unit (hle_me.h's rules): nothing undefined (no libc, no system call), no relocation in its read-only data (no table
# of code addresses), no small-data access
[ -z "$(psp-nm -u me_mix_me.o)" ]
[ -z "$(psp-objdump -r -j .rodata me_mix_me.o 2>/dev/null | grep -i 'R_MIPS')" ]
[ -z "$(psp-objdump -r me_mix_me.o | grep -i 'GPREL\|GOT')" ]

# 3. the link: v26's (its make.log) with the new units after v26's objects, the wraps and a map; v26's own link again for its map
L26=$(grep '^psp-gcc .* -o MGS2T.elf$' make.log | tail -1)
NEW="spu_split.o spu2_split.o me_mix.o hle_me.o ps2rc_kcall.o me_mix_me.o me_sim.o v26me_bat.o$XO pad.o"
WR="-Wl,--wrap=spu2_bind_ops,--wrap=spu2_render48,--wrap=iop_rt_create,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main"
L=$(echo "$L26" | sed "s| spu2_lite.o | spu2_lite.o $NEW |; s|-Wl,-zmax-page-size=128|-Wl,-zmax-page-size=128 $WR|; s| -o MGS2T.elf\$|$XL -o MGS2T.elf -Wl,-Map,MGS2T.map|")
echo "$L" | grep -q " spu2_lite.o $NEW "     # (one check per line: set -e ignores a failure before the last command of an && list)
echo "$L" | grep -q -- "$WR"
echo "$L" | grep -q 'MGS2T.map'
# (in a directory of its own: the layout file names v26's objects, and ld opens a file a script names, so main_psp.o must be v26's there)
mkdir -p base; for f in $V26/*.o $V26/layout_*.ld; do ln -sf $f base/; done
(cd base && eval "$L26 -Wl,-Map,MGS2T.map" && psp-fixup-imports MGS2T.elf && cmp MGS2T.elf $V26/MGS2T.elf)   # the map is v26's link
# v26me2: PADS="0x128 0x1364 0x1de0 0x1c54" (the fit from 0 0 0 0 gave a text pad of 0x130, which left the libraries' code 8 bytes from
# v26me's): with it every symbol v26me had keeps v26me's address but the changed units' own (elfcmp against v26me's ELF, step 5)
pads=${PADS:-"0 0 0 0"}; n=0
while :; do
  set -- $pads
  psp-gcc -c -DPAD_TEXT=$1 -DPAD_RODATA=$2 -DPAD_DATA=$3 -DPAD_BSS=$4 $MG/psp/v26me_pad.S -o pad.o
  eval "$L"
  python3 $MG/tools/dev/padfit.py base/MGS2T.map MGS2T.map $pads > padfit_$n.txt
  pads=$(awk '/^PADS/ {print $2, $3, $4, $5}' padfit_$n.txt)
  grep -q '^FIT' padfit_$n.txt && break
  n=$((n + 1)); [ $n -lt 8 ]
done
echo "pads $pads after $n refits"
cp MGS2T.elf MGS2T_prefix.elf
mksfoex -d MEMSIZE=1 "$TITLE" PARAM.SFO
psp-fixup-imports MGS2T.elf
psp-strip MGS2T.elf -o MGS2T_strip.elf
pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL MGS2T_strip.elf NULL

# 4. the overlays at the arena's new address (v26's scripts, objects and OVL_SIZE); each the size of v26's
B=0x$(psp-nm MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); LG=$(psp-gcc -print-libgcc-file-name)
OS=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' make.log | head -1 | cut -d= -f2)
for st in $STAGES; do
  psp-ld -T ovl_$st.ld --defsym OVL_BASE=$B --defsym OVL_SIZE=$OS -R MGS2T.elf -o ovl_$st.elf ovl_hdr_$st.o gen_s_$st.o $LG
  psp-objcopy -O binary -j .ovl ovl_$st.elf ovl_$st.bin
  [ $(stat -c %s ovl_$st.bin) = $(stat -c %s $V26/ovl_$st.bin) ]
done

# 5. checks: v26's symbols (code at v26's addresses, data moved by multiples of 8 KB); the arena; md5s
python3 $MG/tools/dev/elfcmp.py $V26/MGS2T.elf MGS2T.elf
[ -n "$V26ME" ] && python3 $MG/tools/dev/elfcmp.py $V26ME/MGS2T.elf MGS2T.elf
echo "ovl_arena $B (v26 0x$(psp-nm $V26/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'))"
# the crash catcher's symbols, once each, its handler and the self-test's load in the EBOOT's code
sym() { psp-nm MGS2T.elf | awk -v s=$1 '$3 == s { print $1 }'; }
for x in mgs2_exc_handler mgs2_exc_screen mgs2_exc_test_pc mgs2_exc_regs mgs2_exc_stack; do [ $(sym $x | wc -l) = 1 ]; done
echo "crash catcher: handler 0x$(sym mgs2_exc_handler), screen 0x$(sym mgs2_exc_screen), self-test load 0x$(sym mgs2_exc_test_pc)"
md5sum EBOOT.PBP ovl_*.bin
