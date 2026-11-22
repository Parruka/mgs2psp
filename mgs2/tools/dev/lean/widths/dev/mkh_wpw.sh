#!/bin/sh
# mkh_wpw.sh <widths json> <out dir> [unit ...]: tools/mk_headers.sh's per-unit passes with the v27 options plus RC_WPW=<widths json>, over the
# survey outputs kept in SV (default /home/claude/lean/w3/tmp) and v27's open lists (HDR/open): the survey is not run again.  The ELF first
# (its interprocedural sets go to the stages), then the stages J at a time.  Logs in <out dir>/log.
MG=/mnt/project-files/mgs2; SV=${SV:-/home/claude/lean/w3/tmp}; HDR=${HDR:-/home/claude/lean/hP}; J=${J:-3}
W=$1; O=$2; shift 2
[ -f "$W" ] && [ -n "$O" ] || { echo "usage: mkh_wpw.sh <widths json> <out dir> [unit ...]"; exit 1; }
W=$(cd $(dirname $W) && pwd)/$(basename $W)
U=${*:-elf init r_title r_tnk0 n_title d00t d01t w00a w01a}
mkdir -p $O/log $O/tmp $O/open && O=$(cd $O && pwd) || exit 1
cp $MG/recomp/cov/stages.h $O/stages.h; cp $HDR/open/open_*.txt $O/open/ || exit 1
for u in $U; do
  if [ $u = elf ]; then h=gen_elf.h; else h=gen_s_$u.h; fi
  mkdir -p $O/tmp/$u/a $O/tmp/$u/b $O/tmp/$u/l && cp $SV/$u/a/$h $O/tmp/$u/a/$h || exit 1
done
export RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_WPW=$W
st=""; e=0
for u in $U; do if [ $u = elf ]; then sh $MG/tools/mk_headers.sh --one $O elf || e=1; else st="$st $u"; fi; done
[ -n "$st" ] && { printf '%s\n' $st | xargs -P $J -I{} sh $MG/tools/mk_headers.sh --one $O {} || e=1; }
exit $e
