#!/bin/bash
# rf.sh <build dir> <tag> <last vb> [VAR=value ...]: free route (v25f pad and pack) to <last vb> in /home/claude/lean/dmp/rf_<tag>, no ASLR
# (host addresses repeat), extra variables passed on; hashes every ${HE:-100} vblanks; done.txt says the exit code and seconds
B=$1; T=$2; V=$3; shift 3; D=/home/claude/lean/dmp/rf_$T; mkdir -p $D/memcard0; cp $B/boot $D/boot || exit 1; cd $D; rm -f done.txt
s=$(date +%s)
env MGS2_DISC=/home/claude/v25f/pak/mgs2disc.pak MGS2_HASH_EVERY=${HE:-100} MGS2_VFPU_STAT=1 MGS2_FPU_RZ=1 MGS2_SKIP_MOVIES=1 \
  MGS2_PAD="$(cat /home/claude/v25f/pad_v25f.txt)" MGS2_SIF_LOG=0 "$@" \
  setarch -R ./boot /home/claude/mgs2/SLES_503.83 $V > out.log 2>&1
echo "$T exit $? $(( $(date +%s) - s )) s" > done.txt
