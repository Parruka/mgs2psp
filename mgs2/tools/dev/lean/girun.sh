#!/bin/sh
# girun.sh (tools/dev/lean) <x86 GI build dir> <tag> <a> <b>: route c (tools/dev/lean/runh.sh's env) from boot to vb b+2 with MGS2_GI=a:b: counts of vblanks
# a+1..b in <dir>/gi_<tag>.txt; the hash lines in <dir>/run_<tag>.log (they must equal prof/hashes_x86_codec_h2.txt at the shared points).
D=$1; T=$2; A=$3; B=$4
cd $D || exit 1
PAD="$(cat /mnt/project-files/mgs2/play/pad_codec.txt)"; DISC=/home/claude/mgs2/disc; V=$((B + 2))
env MGS2_DISC=$DISC MGS2_HASH_EVERY=100 MGS2_HASH_DENSE=15700:16010 MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 MGS2_PAD="$PAD" MGS2_SIF_LOG=0 \
  MGS2_GI=$A:$B:$D/gi_$T.txt ./boot /home/claude/mgs2/SLES_503.83 $V > out_$T.log 2>&1
echo "rc $?" > rc_$T.txt
grep -a "^hash\|^vuh\|^gi:\|error\|^split\|missing\|break at\|unhandled\|not found" out_$T.log > run_$T.log
awk '/^hash: vb/ && $3 % 100 == 0 {print $3, $5, $7, $9}' run_$T.log > $T.ref
touch $T.done
