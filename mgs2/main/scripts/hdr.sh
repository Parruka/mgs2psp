#!/bin/sh
# hdr.sh: step 1, the generated headers (game data: container only, never in the project folder).
#  WP  = tools/mk_headers.sh with v31's options (psp_harness.md "v31"; mk_headers.sh's header comment; v30's options + cov_x31.txt):
#        RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_CONDCLEAN=1 RC_WPW=recomp/widths/wpd_all6.json MM_BASE=1
#        COV=recomp/cov/cov_x31.txt, and from gl19 also $M/recomp/cov/cov_gl19.txt (the functions MGS2_HALF's cut runs enter beyond
#        cov_x31: w01a's 2b2b28, entered at vb 37,431 once the sea is cut; the two lists are joined into $G/cov_gl19.txt).  Only the
#        pruning sees COV, so of WP's ten files only gen_s_w01a.h differs from v31's (and wg's copy of it); a WP made before gl19 fails
#        its w01a check below: FORCE=1
#  wpl = WP's w00a unit marked as v31/WPL's ('#line 1 "/home/claude/v30/WP/gen_s_w00a.h"', psp_harness.md gl4c) with the rain's chain
#        tools/dev/rain/g0/{h3,nbs,cs,h3r,g0r}_w00a.diff (each step's md5 as that folder's README.md: the G0R unit 923514f4)
#  wg  = tools/patch_g0.py: the five units holding the 12 G0 functions with their bodies opened by the guard's enum (gl18me's me18c/wg)
#  wh, wgh = gl19: WP's and wg's gen_elf.h with tools/patch_half.py's two lines (MGS2_HALF), for x86.sh and psp.sh
# FORCE=1 makes WP again even when it exists.
. "$(dirname "$0")/env.sh"
set -e
mkdir -p $G
ck() { [ "$(md5sum < "$1" | cut -c1-8)" = "$2" ] || { echo "hdr.sh: $1 is not $2"; exit 1; }; }
if [ -n "$FORCE" ] || [ ! -f $G/WP/gen_s_w01a.h ]; then
  ck "$SL" e3f4c0df
  gzip -dc $MG/recomp/widths/wpd_all6.json.gz > $G/wpd_all6.json
  ck $MG/recomp/cov/cov_x31.txt 4862068d; ck $M/recomp/cov/cov_gl19.txt 7a8ebbcb
  cat $MG/recomp/cov/cov_x31.txt $M/recomp/cov/cov_gl19.txt > $G/cov_gl19.txt
  s=$(date +%s)
  RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_CONDCLEAN=1 RC_WPW=$G/wpd_all6.json MM_BASE=1 J=${J:-3} \
    COV=$G/cov_gl19.txt SL="$SL" STG="$STG" sh $MG/tools/mk_headers.sh $G/WP > $G/hdr.out 2>&1
  echo "hdr.sh: mk_headers.sh $(( $(date +%s) - s )) s"
fi
# the md5s this rebuild got (2026-11-18); the w00a chain's md5s below prove WP's w00a equals v31's.  gl19: gen_s_w01a.h eb55ccb6
# (v31's, with cov_x31 alone, d9e3170e: the one difference is 2b2b28's body in place of its rt_missing line); the nine others as v31's
for p in "gen_elf.h e776f889" "gen_s_d00t.h bead1454" "gen_s_d01t.h c0b76420" "gen_s_init.h 4db8a443" "gen_s_n_title.h 660361ec" \
         "gen_s_r_title.h 43f405b1" "gen_s_r_tnk0.h 0ed78491" "gen_s_w00a.h 2fd83b2f" "gen_s_w01a.h eb55ccb6" "stages.h 672cd281"; do
  set -- $p; ck $G/WP/$1 $2; done
mkdir -p $G/wpl   # (its one file is written anew)
{ echo '#line 1 "/home/claude/v30/WP/gen_s_w00a.h"'; cat $G/WP/gen_s_w00a.h; } > $G/wpl/gen_s_w00a.h
ck $G/wpl/gen_s_w00a.h 7c4e54d9
for p in "h3 15140a0f" "nbs ed441b44" "cs 600b4378" "h3r 2e1047d2" "g0r 923514f4"; do
  set -- $p; patch -s -p0 --no-backup-if-mismatch $G/wpl/gen_s_w00a.h < $MG/tools/dev/rain/g0/$1_w00a.diff; ck $G/wpl/gen_s_w00a.h $2; done
mkdir -p $G/wg    # (patch_g0.py writes its five files anew)
python3 -I $M/tools/patch_g0.py $G/wg $G/WP $G/wpl > $G/wg.out   # gl19: gen_s_w01a.h 1681cd09 (from v31's w01a: 81abca76)
for p in "gen_elf.h 0ab126af" "gen_s_d00t.h df9aa8f3" "gen_s_d01t.h d48036b6" "gen_s_w00a.h eac6413f" "gen_s_w01a.h 1681cd09"; do
  set -- $p; ck $G/wg/$1 $2; done
# gl19 (MGS2_HALF, plan_deck_25fps.md Phase 1): tools/patch_half.py changes two lines of the actor scheduler P13f140 (its entry counts the
# passes; its call of each act at 0x13f1ac asks boot's rt_half_skip first while g_half is set): WP's gen_elf.h -> wh (x86), wg's -> wgh (PSP)
mkdir -p $G/wh $G/wgh   # (their one file each is written anew)
python3 -I $M/tools/patch_half.py $G/WP/gen_elf.h $G/wh/gen_elf.h > $G/wh.out
python3 -I $M/tools/patch_half.py $G/wg/gen_elf.h $G/wgh/gen_elf.h >> $G/wh.out
ck $G/wh/gen_elf.h 012150a6; ck $G/wgh/gen_elf.h 7706dd45
echo "hdr.sh: WP, wpl, wg, wh, wgh ok"
