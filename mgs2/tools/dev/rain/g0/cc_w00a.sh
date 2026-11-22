#!/bin/sh
# cc_w00a.sh <out dir> <nat dir> [flags]: the x86 stage unit w00a as tools/dev/x86_ref.sh compiles it (v31's headers /home/claude/v31/WPL),
# with <nat dir> ahead of the shared boot/ so its native_w00a.c / .h are taken (task #165, G0)
O=$1; N=$2; shift 2; MG=/mnt/project-files/mgs2; W=${W:-/home/claude/v31/WPL}   # W=/home/claude/sbg0/wpl: the H3 copy
CF="-O1 -g0 -w -I$N -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"
mkdir -p $O && cd $O || exit 1; s=$(date +%s)
gcc $CF "$@" "-DSTAGE_GEN=\"$W/gen_s_w00a.h\"" -c $MG/boot/gen_stage.c -o gen_s_w00a.o > cc_w00a.log 2>&1
echo "w00a rc $? $(( $(date +%s) - s )) s"
