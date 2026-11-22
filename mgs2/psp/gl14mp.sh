#!/bin/sh
# gl14mp.sh <out dir> [<gl14 build dir, default /home/claude/sbg14/d>]: MGS2T_gl14mp (2026-11-02, task #132; mgs2/me_probe_notes.md,
# psp_harness.md "Build gl14mp"), gl14's build d with the Media Engine probe linked after its objects the way psp/v30me6.sh linked the
# ME sound mix after v30's: every gl14 object as it is (the PGO profiles, the code order layout_elf.ld and the overlays' layouts);
# main_psp.o compiled again with a title of the same length (first with gl14's title, which must give gl14's object byte for byte);
# gl14's gl1_glue.o with its five functions renamed (objcopy --redefine-sym: psp/me_probe.c gives __wrap_host_vb_begin,
# __wrap_mgs2_exit, __wrap_sceKernelExitGame, me_mix_state and me_mix_diag now; gl1_glue.o's code stays where it was, unused, and its
# me_mix_last_vb stays the word of the latest vblank); the new units after gl14's objects with pads so that gl14's code keeps its
# addresses and its data their offsets modulo 8 KB (tools/dev/padfit.py, psp/v26me_pad.S); the overlays relinked at the arena's
# address (each the size of gl14's).  The ME's bring-up is v30me6c's (psp/me/hle_me_mgs2.c with -DME_REST -DME_SONY -DME_SONY_BOOT
# -DME_KADDR, its stub moved to .data as there; psp/me_rest.S); the ME's unit (psp/me_probe_me.c + psp/me_probe_kern.c + me_rest.S,
# joined with ld -r) is checked as v30me6.sh checks its own: nothing undefined (no libc, no system call), no relocation in its read-only
# data (no table of code addresses), no small-data access.  New link items: --wrap=hw_probe (main_psp.o's call of the hardware
# probe goes to me_probe.c, which calls it first) and -lpspdmac (sceDmacMemcpy, sceDmacTryMemcpy).  Game data passes through the
# build: personal use only.
set -e
O=$1; B=${2:-/home/claude/sbg14/d}; MG=/mnt/project-files/mgs2; T=/home/claude/sb11; D0=/home/claude/gl7/d
T14="MGS2 harness gl14 codec faces"; TITLE="MGS2 harness gl14mp, ME probe"; [ ${#TITLE} = ${#T14} ]
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; SDK=/home/claude/pspsdk/pspdev/psp/sdk
BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
MACHF="-Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv"
HP=$MG/third_party/ps2rc/hle/psp
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
[ -n "$O" ] && [ ! -e "$O" ]
mkdir -p $O; cd $O
for f in $B/*.o $B/layout_*.ld $B/make.log; do cp $f .; done
for st in $STAGES; do cp $B/ovl_$st.ld .; done
OS0=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' make.log | head -1 | cut -d= -f2); [ -n "$OS0" ]
# 0. gl14's EBOOT is its ELF stripped and packed (so step 3 makes this build's the same way)
mkdir -p b14pk; (cd b14pk && unpack-pbp $B/EBOOT.PBP > /dev/null && psp-strip $B/MGS2T.elf -o b14_strip.elf && cmp DATA.PSP b14_strip.elf)

# 1. main_psp.o with the new title, as sb11's pgo_gl.sh "use" compiled gl14's (gl7's make.log line, sb11's tree)
L0=$(grep -o "psp-gcc [^&]*-o main_psp.o" $D0/make.log | head -1 | sed "s|/mnt/project-files/mgs2/psp/main_psp.c|$T/psp/main_psp.c|; s|-I/mnt/project-files/mgs2/runtime|-I$T/runtime &|")
echo "$L0" | grep -q "HARNESS_TITLE="
mpc() { L1=$(echo "$L0" | sed "s|HARNESS_TITLE='\"[^\"]*\"'|HARNESS_TITLE='\"$1\"'|; s| -o main_psp.o\$| -o $2|"); eval "$L1"; }
dump() { psp-objdump -s -r $1 | tail -n +3; }
mpc "$T14" main_psp_t14.o; dump main_psp_t14.o > mp_a.txt; dump $B/main_psp.o > mp_b.txt; cmp mp_a.txt mp_b.txt
mpc "$TITLE" main_psp.o
[ "$(psp-size -A main_psp.o | tail -n +2 | md5sum)" = "$(psp-size -A $B/main_psp.o | tail -n +2 | md5sum)" ]

# 2. gl1_glue.o: its five functions out of the way (the same bytes, other names)
psp-objcopy --redefine-sym __wrap_host_vb_begin=gl1_unused_vb_begin --redefine-sym __wrap_mgs2_exit=gl1_unused_mgs2_exit \
  --redefine-sym __wrap_sceKernelExitGame=gl1_unused_exit_game --redefine-sym me_mix_state=gl1_unused_mix_state \
  --redefine-sym me_mix_diag=gl1_unused_mix_diag $B/gl1_glue.o gl1_glue.o
[ "$(psp-size -A gl1_glue.o | tail -n +2 | md5sum)" = "$(psp-size -A $B/gl1_glue.o | tail -n +2 | md5sum)" ]
[ "$(psp-objdump -d gl1_glue.o | tail -n +4 | sed 's/<[^>]*>//g' | md5sum)" = "$(psp-objdump -d $B/gl1_glue.o | tail -n +4 | sed 's/<[^>]*>//g' | md5sum)" ]

# 3. the new units
cc() { psp-gcc $BASEF -fno-merge-constants "$@"; }
cc $MACHF -DME_REST -DME_SONY -I$SDK/include -I$HP -I$MG/psp/me -I$MG/psp -c $MG/psp/me_probe.c -o me_probe.o
cc $MACHF -DHLE_PSP -DHLE_HAVE_AUDIO_MODEL -DME_REST -DME_SONY -DME_SONY_BOOT -DME_KADDR -I$SDK/include -I$HP -I$MG/psp/me \
  -c $MG/psp/me/hle_me_mgs2.c -o hle_me.o
# (hle_me.c's reset stub and the ME's exception handler into .data, as v30me6.sh: in .text they would raise its alignment to 16)
psp-objcopy --rename-section .text.hle_me_stub=.data.hle_me_stub,alloc,load,contents,data hle_me.o
MEF="-fno-jump-tables -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns"
cc $MEF -I$HP -I$MG/psp -c $MG/psp/me_probe_me.c -o me_probe_me0.o
cc $MEF -I$HP -I$MG/psp -c $MG/psp/me_probe_kern.c -o me_probe_kern0.o
for u in me_probe_me me_probe_kern; do psp-objcopy --redefine-sym memset=me_memset --redefine-sym memcpy=me_memcpy ${u}0.o ${u}1.o; done
psp-gcc -c $MG/psp/me_rest.S -o me_rest.o
[ "$(psp-objdump -h me_rest.o | awk '$2 == ".text" { print $7 }')" = "2**2" ]
psp-ld -r -o me_probe_u.o me_probe_me1.o me_probe_kern1.o me_rest.o
# the Media Engine's unit (hle_me.h's rules)
[ -z "$(psp-nm -u me_probe_u.o)" ]
[ -z "$(psp-objdump -r -j .rodata me_probe_u.o 2>/dev/null | grep -i 'R_MIPS')" ]
[ -z "$(psp-objdump -r me_probe_u.o | grep -i 'GPREL\|GOT')" ]
for u in me_probe.o hle_me.o me_probe_u.o; do echo "$u: .text $(psp-objdump -h $u | awk '$2 == ".text" { print $3, $7 }')"; done
# the new units call no library function that gl14's own objects do not call (a new one, a libgcc double helper say, is taken from
# its archive earlier than in gl14's link and moves the libraries' code and data by different amounts, which no pad can undo); the
# system's import stubs (sce*) are exempt: their tables are only reported by padfit.py
for o in $B/*.o; do psp-nm -u $o; done | awk 'NF == 2 { print $2 }' | sort -u > b14_u.txt
for o in $B/*.o me_probe.o hle_me.o me_probe_u.o; do psp-nm --defined-only $o; done | awk 'NF == 3 { print $3 }' | sort -u > b14_def.txt
psp-nm -u me_probe.o hle_me.o | awk 'NF == 2 { print $2 }' | sort -u | comm -23 - b14_u.txt | comm -23 - b14_def.txt \
  | grep -v '^sce\|^__real_' > new_lib.txt || true
[ ! -s new_lib.txt ] || { echo "new library calls: $(cat new_lib.txt | tr '\n' ' ')"; exit 1; }

# 4. the link: gl14's (its make.log) with the new units after gl14's objects, the new wrap, -lpspdmac and a map; gl14's own link again
#    for its map (in a directory of its own with gl14's objects: the layout file names them)
L14=$(grep '^psp-gcc .* -o MGS2T.elf$' make.log | tail -1)
NEW="me_probe.o hle_me.o me_probe_u.o pad.o"
WR="--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main"
L=$(echo "$L14" | sed "s| ps2rc_kcall.o | ps2rc_kcall.o $NEW |; s|$WR|$WR,--wrap=hw_probe|; s| -o MGS2T.elf\$| -lpspdmac -o MGS2T.elf -Wl,-Map,MGS2T.map|")
echo "$L" | grep -q " ps2rc_kcall.o $NEW "
echo "$L" | grep -q -- "$WR,--wrap=hw_probe "
echo "$L" | grep -q 'MGS2T.map'
mkdir -p base; for f in $B/*.o $B/layout_*.ld; do ln -sf $f base/; done
(cd base && eval "$L14 -Wl,-Map,MGS2T.map" && psp-fixup-imports MGS2T.elf && cmp MGS2T.elf $B/MGS2T.elf)
pads=${PADS:-"0 0 0 0"}; n=0
while :; do
  set -- $pads
  psp-gcc -c -DPAD_TEXT=$1 -DPAD_RODATA=$2 -DPAD_DATA=$3 -DPAD_BSS=$4 $MG/psp/v26me_pad.S -o pad.o
  eval "$L"
  python3 $MG/tools/dev/padfit.py base/MGS2T.map MGS2T.map $pads > padfit_$n.txt
  ! grep -q 'TEXT MOVED' padfit_$n.txt
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

# 5. the overlays at the arena's address (gl14's scripts, objects and OVL_SIZE), each the size of gl14's
A=0x$(psp-nm MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }'); LG=$(psp-gcc -print-libgcc-file-name)
for st in $STAGES; do
  psp-ld -T ovl_$st.ld --defsym OVL_BASE=$A --defsym OVL_SIZE=$OS0 -R MGS2T.elf -o ovl_$st.elf ovl_hdr_$st.o gen_s_$st.o $LG
  psp-objcopy -O binary -j .ovl ovl_$st.elf ovl_$st.bin
  [ $(stat -c %s ovl_$st.bin) = $(stat -c %s $B/ovl_$st.bin) ]
done

# 6. checks: gl14's symbols (code at gl14's addresses, data moved by multiples of 8 KB; the five renamed ones "not found" in .text and
#    at gl14's addresses under their new names), the arena, the probe's symbols, md5s
python3 $MG/tools/dev/elfcmp.py $B/MGS2T.elf MGS2T.elf
sym() { psp-nm $1 | awk -v s=$2 '$3 == s { print $1 }'; }
for p in __wrap_host_vb_begin:gl1_unused_vb_begin __wrap_mgs2_exit:gl1_unused_mgs2_exit __wrap_sceKernelExitGame:gl1_unused_exit_game \
         me_mix_state:gl1_unused_mix_state me_mix_diag:gl1_unused_mix_diag; do
  [ "$(sym $B/MGS2T.elf ${p%%:*})" = "$(sym MGS2T.elf ${p##*:})" ]
done
for x in me_mix_last_vb hw_probe host_vb_begin mgs2_exit; do [ "$(sym $B/MGS2T.elf $x)" = "$(sym MGS2T.elf $x)" ] || echo "moved: $x"; done
for x in __wrap_hw_probe __wrap_host_vb_begin __wrap_mgs2_exit __wrap_sceKernelExitGame me_mix_state me_mix_diag mep_work mep_fp_run \
         me_rest me_rest_halt hle_me_start ps2rc_me_block; do [ $(sym MGS2T.elf $x | wc -l) = 1 ]; done
echo "ovl_arena $A (gl14 0x$(sym $B/MGS2T.elf ovl_arena)); mep_work 0x$(sym MGS2T.elf mep_work), the ME's stub in .data at 0x$(sym MGS2T.elf hle_me_stub 2>/dev/null)"
md5sum EBOOT.PBP ovl_*.bin
