#!/bin/sh
# build_all.sh: the mainline tree (gl18me's build, rebuilt 2026-11-18 after the container reset) from its inputs to the generated headers,
# the x86 twin and the PSP EBOOT with its eight overlays, in one go.  README.md says what each step does and which checks it passed.
#   1 hdr    scripts/hdr.sh      tools/mk_headers.sh with v31's options -> gen/WP; the rain's w00a chain -> gen/wpl; patch_g0.py -> gen/wg
#   2 x86    scripts/x86.sh obj  the x86 game units and IOP objects (build/x86)
#   3 vu     scripts/x86.sh dump + scripts/vu.sh   an interpreted x86 run of route i collects the VU0 images; recomp/vupipe.py -> gen/vp22
#   4 twin   scripts/x86.sh twin (+ scripts/twin.sh: route i to vb 37,800 against prof/hashref_x86_in_gl18me.txt; NOCHECK=1 skips the run)
#   5 psp    scripts/psp.sh      build/psp: EBOOT.PBP, ovl_*.bin (no PGO: the profiles are owed)
#   6 emu    scripts/emu.sh      only with EMU=1: the EBOOT on the patched PPSSPP 1.20.4, route i, against the same reference (~8 min)
# Inputs (game data: personal use only, container only; env.sh's defaults): SL=SLES_503.83, STG=the extracted stages, PAK=gl18's
# mgs2disc.pak, PSPDEV=the pspdev toolchain, PPSSPP=the patched headless binary (tools/ppsspp_1204_mgs2.diff).  Outputs: gen/ and build/
# (game code: never copy them to the project folder).  build/psp and gen/vudump must not exist (the scripts never delete anything).
cd "$(dirname "$0")" || exit 1
M=$(pwd); export M
. $M/scripts/env.sh
[ -e $B/psp ] && { echo "build_all.sh: $B/psp exists: look at it and delete it by hand first"; exit 1; }
[ -e $G/vudump ] && { echo "build_all.sh: $G/vudump exists: look at it and delete it by hand first"; exit 1; }
ck() { [ "$(md5sum < "$1" | cut -c1-8)" = "$2" ] || { echo "build_all.sh: $1 is not $2"; exit 1; }; }
ck "$SL" e3f4c0df; ck "$PAK" 189ca53f; ck $PADF 7bc72ace
[ "$(find $STG -mindepth 1 -maxdepth 1 -type d | wc -l)" = 123 ] || { echo "build_all.sh: $STG does not hold 123 stage folders"; exit 1; }
[ -x $PSPDEV/bin/psp-gcc ] || { echo "build_all.sh: no psp-gcc in $PSPDEV/bin"; exit 1; }
mkdir -p $B
T0=$(date +%s)
step() {   # <name> <command...>: one step, timed; stops the build on a failure
  n=$1; shift; s=$(date +%s); echo "== $n ($(date +%H:%M:%S))"
  "$@" || { echo "build_all.sh: step $n failed"; exit 1; }
  echo "== $n done in $(( $(date +%s) - s )) s"
}
step hdr sh $M/scripts/hdr.sh
step x86 sh $M/scripts/x86.sh obj
grep -q " rc [1-9]" $B/x86/cc_*.log && { echo "build_all.sh: an x86 compile failed: $(grep -l ' rc [1-9]' $B/x86/cc_*.log)"; exit 1; }
step vu-dump sh $M/scripts/x86.sh dump
step vu sh $M/scripts/vu.sh
step twin-build sh $M/scripts/x86.sh twin
[ -n "$NOCHECK" ] || step twin-check sh $M/scripts/twin.sh r18a
step psp sh $M/scripts/psp.sh
[ -n "$EMU" ] && step emu sh $M/scripts/emu.sh
echo "build_all.sh: done in $(( $(date +%s) - T0 )) s"
echo "PSP files: $B/psp/EBOOT.PBP $B/psp/ovl_*.bin (with SLES_503.83, mgs2disc.pak, pad.txt, cfg/mgs2_gl18me.cfg as mgs2.cfg and"
echo "  prof/hashref_x86_in_gl18me.txt as hashref.txt next to them)"
