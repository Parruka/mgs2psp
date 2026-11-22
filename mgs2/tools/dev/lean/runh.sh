#!/bin/sh
# runr.sh <dir> <tag> <d|w|f|i|c> [env...]: a from-boot x86 run for the reference checks.  d: the deck tutorial route (pad_deck_idle, v21's
# pack psp5) to vb 25200; w: the same with the crew-deck redirect (MGS2_POKE, as tools/dev runq.sh); f: the v25f free route (pad_v25f,
# the v25f pack) to vb 36500.  A hash every 100 vblanks and at every vblank of 15700-16010 (with the VU0 state hash).
# i (v30): the indoor free route (play/pad_w01af.txt: free play's start, into Deck-A crew's quarters, play/w01a_route.md) to vb 37800,
# from the disc folder (the pack is made from this run: pass MGS2_DISC_LOG=1; the "disc:" table and read lines go to run_<tag>.log).
# c (gl4c, 2026-10-28): the codec test route (play/pad_codec.txt: free play's start to vb 30,500, then SELECT opens the codec on the aft
# deck and O calls Otacon 15 times, vb 30,620-56,866) to vb 60000, from the disc folder as i.
# Out: <tag>.ref "vb ee iop vfb" (every 100), <tag>.dense "vb ee vfb [vuh]", run_<tag>.log (the hash and error lines), <tag>.done
D=$1; T=$2; R=$3; shift 3
cd $D || exit 1
if [ "$R" = f ]; then PAD="$(cat /home/claude/v25f/pad_v25f.txt)"; DISC=/home/claude/v25f/pak/mgs2disc.pak; V=36500; POKE=
elif [ "$R" = i ]; then PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; DISC=/home/claude/mgs2/disc; V=37800; POKE=
elif [ "$R" = c ]; then PAD="$(cat /mnt/project-files/mgs2/play/pad_codec.txt)"; DISC=/home/claude/mgs2/disc; V=60000; POKE=
else PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"; DISC=/home/claude/psp5/mgs2disc.pak; V=25200
  POKE=21000:1190ed0:61313077,21000:1190f78:61313077,21000:1af8118:61313077,21000:1af81c0:61313077,22950:1190ed0:61313077,22950:1190f78:61313077,22950:1af8118:61313077,22950:1af81c0:61313077
  [ "$R" = w ] || POKE=
fi
env MGS2_DISC=$DISC MGS2_HASH_EVERY=100 MGS2_HASH_DENSE=15700:16010 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 ${POKE:+MGS2_POKE=$POKE} "$@" \
  ./boot /home/claude/mgs2/SLES_503.83 $V > out_$T.log 2>&1
echo "rc $?" > rc_$T.txt
grep -a "^disc:\|^mix: ck\|^khandler\|^stage\|^hash\|^vuh\|JALR\|error\|^split\|^speedups\|g_opt\|missing\|break at\|unhandled\|not found\|stage code" out_$T.log > run_$T.log
awk '/^hash: vb/ && $3 % 100 == 0 {print $3, $5, $7, $9}' run_$T.log > $T.ref
awk '/^hash: vb/ {h[$3] = $5 " " $9; o[++n] = $3} /^vuh: vb/ {u[$3] = $4} END {for (i = 1; i <= n; i++) {v = o[i]; print v, h[v] (v in u ? " " u[v] : "")}}' run_$T.log > $T.dense
rm -f out_$T.log
touch $T.done
