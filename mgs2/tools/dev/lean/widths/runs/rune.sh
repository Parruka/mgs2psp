#!/bin/sh
# rune.sh <build dir> <d|w|f>: one x86 route (as runh.sh) with a hash at EVERY vblank; out <build dir>/E<r>/E<r>.all "vb ee iop vfb"
B=$1; R=$2; D=$B/E$R; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D
if [ "$R" = f ]; then PAD="$(cat /home/claude/v25f/pad_v25f.txt)"; DISC=/home/claude/v25f/pak/mgs2disc.pak; V=36500; POKE=
else PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"; DISC=/home/claude/psp5/mgs2disc.pak; V=25200
  POKE=21000:1190ed0:61313077,21000:1190f78:61313077,21000:1af8118:61313077,21000:1af81c0:61313077,22950:1190ed0:61313077,22950:1190f78:61313077,22950:1af8118:61313077,22950:1af81c0:61313077
  [ "$R" = w ] || POKE=
fi
s=$(date +%s)
env MGS2_DISC=$DISC MGS2_HASH_EVERY=1 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 ${POKE:+MGS2_POKE=$POKE} \
  ./boot /home/claude/mgs2/SLES_503.83 $V 2>&1 | awk '/^hash: vb/ {print $3, $5, $7, $9}' > E$R.all
echo "E$R: $(wc -l < E$R.all) lines $(( $(date +%s) - s )) s" > E$R.done
rm -f boot
