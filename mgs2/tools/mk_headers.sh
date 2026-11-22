#!/bin/sh
# mk_headers.sh <out dir> [unit ...]: the timing harness's generated headers (PSP and x86 builds) from the disc files in one run
# (recomp/lean_plan.md step 0).  Units: elf and the stage names (default: elf init r_title r_tnk0 n_title d00t d01t w00a w01a).
# Per unit, in this order (the order that made v26's headers: psp/v26.sh hdr over v25f's W25 and vglz/W):
#   survey.py (VU_VFPU=1; a stage with STAGE / SPLIT) -> regcache.py (RC_KEEPMASK=1, plus any RC_* option set in the environment) ->
#   poll_fast -> idiom_apply -> sp_fast -> vu_hoist -> vgrp_bvt -> ldu_fuse -> fpu_zero -> vgrp_fcl -> (elf: jalr_tab) -> prune_cov (COV) ->
#   elf: native_elf_apply;  w00a: native_apply, stage_tab;  other stages: stage_tab
# regcache.py sees the whole (unpruned) translation, so its interprocedural sets do not depend on the coverage list.  survey.py runs for
# every unit first (J at a time); with RC_RETLIVE set, tools/ret_open.py then lists each unit's open functions from those outputs
# (<out>/open/open_<unit>.txt, regcache.py's RC_RET_OPEN; the return sets go to <out>/ret_<unit>.json).  Then the ELF's regcache.py and
# later passes run alone (RC_IPO_SAVE / RC_IPO_LOAD can hand its sets to the stages), then the stages', J at a time.  RC_RETLIVE needs
# all units in one run, or OPEN=<dir> with the lists of such a run.
# Inputs: SL (SLES_503.83), STG (<stage>/nocache/<stage>.bin), COV (coverage list, recomp/cov/cov_x25.txt: v26's), STAGES_H; RCPY (regcache.py).
# Each unit's interprocedural sets go to <out>/ipo_<unit>.json (RC_IPO_SAVE); the stages read the ELF's (RC_IPO_LOAD; used with RC_XUNIT=1).
# Out: <out>/gen_*.h, <out>/stages.h, logs <out>/log/<unit>.log; work files in <out>/tmp (emptied at the end unless KEEP=1).  REF=<dir>: each header
# compared with REF's (cmp).  Takes about 10 minutes for all units (4 cores).
# The options of the v27 headers (lean_plan.md 1b, 1c, 1e; runtime/kernel.h's cleared handler start):
#   RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1
# Every set after v27 also takes RC_CONDCLEAN=1 (regcache.py's fix of a dropped store before a call, recomp/widths_step2.md; v27's
# hashes stay with it, its headers change at 35 call sites).  A set without proved widths also takes RC_SQWIDE=1 (v27's quadword copies:
# with it free play's hashes change from vb 33,531 and equal the proved widths'; deck and crew deck stay); a set with RC_WPW leaves it off.
# MM_BASE=1 (lean_plan.md step 3): tools/mm_base.py on each finished header (guest accesses through a register base take one I/O test
# and host base per register; needs gen_pre.h's base forms).
MG=/mnt/project-files/mgs2; T=${T:-$MG/tools}
SL=${SL:-/home/claude/mgs2/SLES_503.83}; STG=${STG:-/home/claude/mgs2/stage}
COV=${COV:-$MG/recomp/cov/cov_x25.txt}; STAGES_H=${STAGES_H:-$MG/recomp/cov/stages.h}; J=${J:-3}; RCPY=${RCPY:-$MG/recomp/regcache.py}
unit() {   # sets w, L, h, img, pu, sv for unit $u of out dir $O
  w=$O/tmp/$u; L=$O/log/$u.log
  if [ $u = elf ]; then h=gen_elf.h; img=$SL; pu=elf; sv="SLUS=$SL"
  else h=gen_s_$u.h; img=$STG/$u/nocache/$u.bin; pu=$h; sv="SLUS=$SL STAGE=$img SPLIT=$u"; fi
}
if [ "$1" = --survey ]; then
  O=$2; u=$3; unit; s=$(date +%s)
  mkdir -p $w/a $w/b $w/l || exit 1
  [ -f $img ] || { echo "$u: no image $img"; exit 1; }
  { cd $MG/recomp && env $sv VU_VFPU=1 python3 survey.py $w/a/$h; } > $L 2>&1 || { echo "$u: survey.py failed (log $L)"; exit 1; }
  echo "$u: survey.py $(( $(date +%s) - s )) s"
  exit 0
fi
if [ "$1" = --one ]; then
  O=$2; u=$3; unit; s=$(date +%s)
  xl=""; [ $u = elf ] || xl=RC_IPO_LOAD=$O/ipo_elf.json   # the ELF's interprocedural sets, for RC_XUNIT
  [ -n "$RC_RETLIVE" ] && xl="$xl RC_RET_OPEN=$O/open/open_$u.txt RC_RET_SAVE=$O/ret_$u.json"
  { cd $MG/recomp && env RC_KEEPMASK=1 RC_IPO_SAVE=$O/ipo_$u.json $xl python3 $RCPY $w/a/$h &&
    cd $w/a && python3 $T/poll_fast.py $h && python3 $T/idiom_apply.py $h $img && python3 $T/sp_fast.py $h && python3 $T/vu_hoist.py $h &&
    python3 $T/vgrp_bvt.py $w/a/$h $w/b/$h && python3 $T/ldu_fuse.py $w/b $w/l && python3 $T/fpu_zero.py $w/l/$h && python3 $T/vgrp_fcl.py $w/l/$h &&
    { [ $u != elf ] || python3 $T/jalr_tab.py $w/l/$h; } && python3 $T/prune_cov.py $w/l/$h $COV $pu $w/p.h &&
    case $u in
      elf) python3 $T/native_elf_apply.py $w/p.h $O/$h ;;
      w00a) python3 $T/native_apply.py $w/p.h $w/n.h && python3 $T/stage_tab.py $w/n.h $O/$h ;;
      *) python3 $T/stage_tab.py $w/p.h $O/$h ;;
    esac && { [ -z "$MM_BASE" ] || python3 $T/mm_base.py $O/$h; }; } >> $L 2>&1 || { echo "$u: failed (log $L)"; exit 1; }
  r=""; [ -n "$REF" ] && { cmp -s $O/$h $REF/$h && r=", equal to $REF/$h" || r=", DIFFERS from $REF/$h"; }
  echo "$u: $(( $(date +%s) - s )) s$r"
  [ -n "$KEEP" ] || for f in $w/a/$h $w/b/$h $w/l/$h $w/p.h; do : > $f; done   # emptied, not deleted
  exit 0
fi
O=$1; shift; [ -n "$O" ] || { echo "usage: mk_headers.sh <out dir> [unit ...]"; exit 1; }
ALL="elf init r_title r_tnk0 n_title d00t d01t w00a w01a"; U=${*:-$ALL}
mkdir -p $O/log $O/tmp && O=$(cd $O && pwd) || exit 1
cp $STAGES_H $O/stages.h
printf '%s\n' $U | xargs -P $J -I{} sh $0 --survey $O {} || exit 1
if [ -n "$RC_RETLIVE" ] && [ -n "$OPEN" ]; then mkdir -p $O/open && cp $OPEN/open_*.txt $O/open/ || exit 1   # an earlier run's lists
elif [ -n "$RC_RETLIVE" ]; then   # the lists need every unit's text at once
  [ "$U" = "$ALL" ] || { echo "RC_RETLIVE: all units in one run (or OPEN=<dir> with the lists of such a run)"; exit 1; }
  hs=""; for u in $U; do unit; hs="$hs $w/a/$h"; done
  python3 $T/ret_open.py $O/open $SL $STG $hs > $O/log/ret_open.log 2>&1 || { echo "ret_open.py failed (log $O/log/ret_open.log)"; exit 1; }
  tail -1 $O/log/ret_open.log
fi
st=""; e=0
for u in $U; do if [ $u = elf ]; then sh $0 --one $O elf || e=1; else st="$st $u"; fi; done
[ -n "$st" ] && { printf '%s\n' $st | xargs -P $J -I{} sh $0 --one $O {} || e=1; }
exit $e
