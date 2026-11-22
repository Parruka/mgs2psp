#!/bin/sh
# openret_iter.sh <iteration dir> <OPENR> <OPENF> (lean 1c step 2, dev): one round of the open-return fixpoint.  Each unit's survey output
# (/home/claude/lean/sv/<header>, made once by --survey) goes through rc6's rc_jsif.py + regcache.py in probe-only mode with
# RC_RET_OPENR / OPENF; the reads after calls through a register or into another unit land in <dir>/sites_<unit>.json, their union in
# <dir>/union.txt ("R <list>" / "F <list>").  The rounds start from empty sets and stop when union.txt equals the sets given.
MG=/mnt/project-files/mgs2; SV=/home/claude/lean/sv; SL=/home/claude/mgs2/SLES_503.83; STG=/home/claude/mgs2/stage
U="elf init r_title r_tnk0 n_title d00t d01t w00a w01a"
hdr() { [ $1 = elf ] && echo gen_elf.h || echo gen_s_$1.h; }
if [ "$1" = --survey ]; then
  mkdir -p $SV
  printf '%s\n' $U | xargs -P 3 -I{} sh $0 --survey1 {}; exit 0
fi
if [ "$1" = --survey1 ]; then u=$2; h=$(hdr $u)
  if [ $u = elf ]; then sv="SLUS=$SL"; else sv="SLUS=$SL STAGE=$STG/$u/nocache/$u.bin SPLIT=$u"; fi
  cd $MG/recomp && env $sv VU_VFPU=1 python3 survey.py $SV/$h > $SV/$u.log 2>&1; echo "$u $?" >> $SV/status; exit 0
fi
if [ "$1" = --one ]; then D=$2; R=$3; F=$4; u=$5; h=$(hdr $u); xl=""; [ $u = elf ] || xl=RC_IPO_LOAD=/home/claude/lean/hR/ipo_elf.json
  mkdir -p $D/t_$u; cp $SV/$h $D/t_$u/$h
  env RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_KEEPMASK=1 RC_RET_OPEN=/home/claude/lean/ret/open/open_$u.txt \
    RC_RET_OPENR="$R" RC_RET_OPENF="$F" RC_RET_ITER=1 RC_RET_PROBE_ONLY=1 RC_RET_SITES_SAVE=$D/sites_$u.json $xl \
    python3 /home/claude/lean/rc6/rc_jsif.py $D/t_$u/$h > $D/$u.log 2>&1
  echo "$u $?" >> $D/status; rm -f $D/t_$u/$h; rmdir $D/t_$u; exit 0
fi
D=$1; R=$2; F=$3; mkdir -p $D
: > $D/status
printf '%s\n' $U | xargs -P ${J:-3} -I{} sh $0 --one $D "$R" "$F" {}
python3 - $D << 'PY'
import json, sys, glob
d = sys.argv[1]; R = set(); F = set()
for f in glob.glob(d + '/sites_*.json'): j = json.load(open(f)); R |= set(j['R']); F |= set(j['F'])
open(d + '/union.txt', 'w').write('R %s\nF %s\n' % (','.join(map(str, sorted(R))), ','.join(map(str, sorted(F)))))
PY
cat $D/status; cat $D/union.txt
