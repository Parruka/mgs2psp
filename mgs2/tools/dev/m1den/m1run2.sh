#!/bin/sh
# m1run2.sh <dir> <tag> <w|d|f|i> [env...]: runref2.sh (the v21 reference run, scratchpad) plus the m1: report lines and MGS2_M1_KVB
D=$1; T=$2; R=$3; shift 3
cd $D || exit 1
PAD="$(cat /home/claude/mgs2/boot/pad_deck_idle.txt)"
POKE=21000:1190ed0:61313077,21000:1190f78:61313077,21000:1af8118:61313077,21000:1af81c0:61313077,22950:1190ed0:61313077,22950:1190f78:61313077,22950:1af8118:61313077,22950:1af81c0:61313077
[ "$R" = w ] || POKE=
DISC=/home/claude/psp5/mgs2disc.pak
# v30: f = the v25f free route (to 36500), i = the indoor route (play/pad_w01af.txt, psp/v30.sh's pack; to 37800)
if [ "$R" = f ]; then PAD="$(cat /home/claude/v25f/pad_v25f.txt)"; DISC=/home/claude/v25f/pak/mgs2disc.pak; STOP=${STOP:-36500}; fi
if [ "$R" = i ]; then PAD="$(cat /mnt/project-files/mgs2/play/pad_w01af.txt)"; DISC=/home/claude/v30/pak/mgs2disc.pak; STOP=${STOP:-37800}; fi
K=$(nm ./boot | awk '$3=="k_vblanks"{print $1}'); G=$(nm ./boot | awk '$3=="g_hash_vb"{print $1}'); KV=$(( 0x$K - 0x$G ))
env MGS2_M1_KVB=$KV MGS2_HASH_EVERY=100 MGS2_HASH_DENSE=15700:16010 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_DISC=$DISC MGS2_PAD="$PAD" MGS2_SIF_LOG=0 ${POKE:+MGS2_POKE=$POKE} "$@" ./boot /home/claude/mgs2/SLES_503.83 ${STOP:-25200} 2>&1 | grep -a "^hash\|^vuh\|^vpv\|JALR\|error\|^split\|^speedups\|g_opt\|^m1:\|^m1vg\|^pkt:\|missing\|break at\|unhandled" > run_$T.log
awk '/^hash: vb/ && $3 % 100 == 0 {print $3, $5, $7, $9}' run_$T.log > $T.ref
awk '/^hash: vb/ {h[$3] = $5 " " $9; o[++n] = $3} /^vuh: vb/ {u[$3] = $4} END {for (i = 1; i <= n; i++) {v = o[i]; print v, h[v] (v in u ? " " u[v] : "")}}' run_$T.log > $T.dense
touch $T.done
