#!/bin/sh
# pgochk.sh <PGO=use psp dir> <out dir>: (gl20, FF8's lesson 2026-11-18 and me18/pgochk.sh) every profiled object of the build compiled
# again with its own command (psp.sh's cc_<object>.log "cmd:" line; gen_dispatch.o's from make.log) but with the warnings on
# (-w taken out; -Wno-error=coverage-mismatch is in psp.sh's PGO=use flags), from <out dir> holding a copy of the build's .gcda files, so
# GCC names each function whose profile it drops ("control flow checksum mismatch", "number of counters", "source locations", ...).
# Writes <out dir>/warn_<object>.txt (the lines about profiles) and checks that each object of the check is the build's byte for byte
# (the objects psp.sh renames sections in after the compile are compared as compiled, so only the ones it leaves alone).
# -Wno-missing-profile is taken out too: GCC then names each function it finds no profile for ("profile for function X not found in
# profile data", silent in the build).  Each object is also compiled once more with -fprofile-generate instead of -fprofile-use into
# <out dir>/gcno with -ftest-coverage, for its .gcno file (-fprofile-generate alone writes none): the function names of the .gcda
# idents, so tools/pgo_lost.py names the functions whose counts are all zero (not trained: -fprofile-partial-training compiles them with
# the static guess).  It runs inside gcno/ with the same -o name (the commands' paths are absolute): a static function's ident hashes
# the object's base name with its path, so "-o gcno/x.o" would give other idents than the training build's.
# Then tools/pgo_lost.py lists the functions that ran without a profile.  <out dir> must not exist (game code inside: container only).
. "$(dirname "$0")/env.sh"
D=$1; O=$2
[ -f $D/cc_gen.o.log ] && grep -q "^cmd: .*-fprofile-use" $D/cc_gen.o.log || { echo "pgochk.sh: $D is not a PGO=use build of psp.sh"; exit 1; }
[ -e $O ] && { echo "pgochk.sh: $O exists"; exit 1; }
mkdir -p $O/gcno && cp $D/*.gcda $O/ && cd $O || exit 1
OBJ="gen.o gen_s_w00a.o gen_s_w01a.o boot_rt.o xl_psp.o ge_fxthin.o"
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme xme; do OBJ="$OBJ ge_$f.o"; done
chk() {   # <object> <command>
  o=$1; c=$(echo "$2" | sed 's/ -w / /; s/ -Wno-missing-profile / /')
  g=$(echo "$2" | sed 's/-fprofile-use -fprofile-partial-training -Wno-missing-profile\( -Wno-error=coverage-mismatch\)*/-fprofile-generate -fprofile-update=single -ftest-coverage/')
  s=$(date +%s); eval "$c" > all_$o.txt 2>&1; r=$?
  ( cd gcno && eval "$g" ) > gcno/all_$o.txt 2>&1 || echo "$o: the -fprofile-generate compile failed (gcno/all_$o.txt)"
  grep -i "profile\|control flow\|counters\|checksum\|source location\|coverage" all_$o.txt > warn_$o.txt
  case $o in gen.o) [ "$(md5sum < $o | cut -c1-32)" = "$(cut -c1-32 $D/gen_compiled.md5)" ] && e="equal to the build's as compiled (gen_compiled.md5; gl33: before the wrapper step)" || e="DIFFERS from the build's" ;;
    gen_s_*|boot_rt.o|gen_dispatch.o) cmp -s $o $D/$o && e="equal to the build's" || e="DIFFERS from the build's" ;; *) e="(renamed after compile in the build: not compared)" ;; esac
  echo "$o rc $r $(( $(date +%s) - s )) s, $(grep -c '' warn_$o.txt) profile lines, object $e"
}
n=0
for o in $OBJ; do
  c=$(sed -n 's/^cmd: //p' $D/cc_$o.log | head -n 1); [ -n "$c" ] || { echo "pgochk.sh: no command for $o"; continue; }
  chk $o "$c" & n=$((n + 1)); [ $((n % ${J:-3})) -eq 0 ] && wait
done
# (psp/Makefile's PGO=use line lacks -Wno-error=coverage-mismatch: added here, as psp.sh's flags have it.  gen_dispatch.c is compiled
# through psp.sh's shadow folder; while that folder was <psp dir>/mg (before gl20's fix) the training and use builds saw other paths:
# "source locations changed" (a warning, the profile used) for its public functions, and no profile at all for the static ones
# jalr_find, guest_call, stage_call, gen_mm, whose ids hash the path)
c=$(grep -o "psp-gcc [^&]*-o gen_dispatch.o" $D/make.log | head -n 1 | sed 's/ -w / -w -Wno-error=coverage-mismatch /')
[ -n "$c" ] && chk gen_dispatch.o "$c"
wait
echo "pgochk.sh: done"
