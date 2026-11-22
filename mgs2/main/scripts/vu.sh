#!/bin/sh
# vu.sh: step 3, the AOT VU0 header gen/vp22/gen_vu.h (psp_harness.md "VU0 header vp22"): the micro program images from an interpreted
# run of route i (x86.sh dump's boot_dump: boot_x86.c vu_prof, MGS2_VU_PROF=gen/vudump:0, writes gen/vudump/<hash>.bin and entries.txt),
# then recomp/vupipe.py with prof/vu_spec_v22.txt, VP_VFPU = prof/vu_vfpu_list_v22.txt (vp16n's 18 entries) and VP_MEM=all.
# vp22 was made with VP_KEEP=/home/claude/psp/vp9/gen_vu.h (lost): that only moves the functions vp9 lacked into gen_vu_tail.h so a PGO
# profile keeps matching; without it every function is in gen_vu.h (the same functions and dispatch; the order differs).
# The images are game data: container only.  SKIPRUN=1 reuses gen/vudump.
. "$(dirname "$0")/env.sh"
X=$B/x86; D=$G/vudump
if [ -z "$SKIPRUN" ]; then
  [ -x $X/boot_dump ] || { echo "vu.sh: no $X/boot_dump (x86.sh dump)"; exit 1; }
  [ -e $D ] && { echo "vu.sh: $D exists: look at it and delete it by hand, or SKIPRUN=1 to reuse it"; exit 1; }
  mkdir -p $D || exit 1
  s=$(date +%s)
  ( cd $X && env MGS2_DISC="$PAK" MGS2_HASH_EVERY=100 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$(cat $PADF)" MGS2_SIF_LOG=0 \
      MGS2_PKT_HASH= MGS2_VU_PROF=$D:0 ./boot_dump "$SL" ${VEND:-37800} > out_dump.log 2>&1; echo "rc $?" > rc_dump.txt )
  echo "vu.sh: dump run $(cat $X/rc_dump.txt), $(( $(date +%s) - s )) s, $(ls $D | grep -c '\.bin$') images"
fi
# vp22's spec names 26 images; route i runs 23 of them.  fe0033d5, b641370c and de078118 (speed_notes.md: found at vb 24,666 of route d,
# the deck tutorial) never run on route i, and route d's disc pack (/home/claude/psp5/mgs2disc.pak) is lost, so their images cannot be
# collected here: they are left out of the spec (they stay on the interpreter, which is the exact model) unless STRICT=1.  Route i's
# state cannot depend on them: the dump run interprets every call and meets no such image.
mkdir -p $G/vp22; : > $G/vp22/spec_used.txt; miss=""
for sp in $(cat $MG/prof/vu_spec_v22.txt); do h=${sp%%:*}; if [ -f $D/$h.bin ]; then printf '%s ' $sp >> $G/vp22/spec_used.txt; else miss="$miss $sp"; fi; done
[ -z "$miss" ] || { echo "vu.sh: images not met on route i, left out of the spec:$miss"; [ -n "$STRICT" ] && exit 1; }
for e in $(cat $MG/prof/vu_vfpu_list_v22.txt | tr ',' ' '); do grep -q "${e%%:*}:" $G/vp22/spec_used.txt || { echo "vu.sh: VFPU entry $e has no image"; exit 1; }; done
( cd $G && VP_VFPU="$(cat $MG/prof/vu_vfpu_list_v22.txt)" VP_MEM=all python3 -I $MG/recomp/vupipe.py $D $G/vp22/gen_vu.h $(cat $G/vp22/spec_used.txt) > $G/vp22/gen.log 2>&1 )
echo "vu.sh: vupipe rc $?, $(grep -c 'pairs emitted' $G/vp22/gen.log) entries, $(grep -c 'VFPU form,' $G/vp22/gen.log) VFPU forms, refused: $(grep -c 'not compiled\|no VFPU form' $G/vp22/gen.log)"
md5sum $G/vp22/gen_vu.h
