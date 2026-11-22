#!/bin/sh
# rl32.sh [ROOT] (gl32 = gl31 + the ship's vertex cache, 2026-11-21: ge_xconv.o and ge_vu1conv.o are the tree's now (ge/natvc.h), gen.o has
# three more functions behind wrappers (boot/nat_stub.h's events); below, gl31's recipe: gl31 = gl30 + native step 1, 2026-11-20; build_gl30n/rl30.sh's recipe + build_gl30/relink29.sh's gelist part, on
# /home/claude/t32): ROOT (default /home/claude/main/build_gl32)/stage = gl29's stage objects (build_gl29/stage) with these made again
# from the tree (gl20 gcda3 profiles):
#  - ge_xme.o, ge_xme_me.o, ge_xlme.o (build_gl20/u2's lines) and xl_psp.o (build_gl22's line) on the tree's files, as relink29.sh and
#    rl30.sh (both parents changed what they include: ge/gelist.h's GxDzCount.nsub, and step 1's xme / xlme / xl_psp);
#  - step 1's two: ge_xlate.o (ge/xlate.c) and boot_rt.o (boot/boot.c + nat_stub.h): rl30.sh's relabelled compile (-E -fdirectives-only,
#    the .i's markers relabelled from the tree to /home/claude/main, then -fpreprocessed -fdirectives-only -c);
#  - gl30's three: ge_gelist.o, ge_gelive.o, ge_xconv.o: relink29.sh's relabelled compile (plain -E, the .i's markers relabelled from
#    TREE/ge/ to /home/claude/main/ge/, then -c of the .i).  Checked here: ge_gelist.o must equal gl30's stage object (the one in the
#    EBOOT on the PSP now), ge_gelive.o and ge_xconv.o gl23's (= gl29's = gl30's);
#  - gen.o: gl29's object with P1214a0 made weak and P1214a0_orig added at its place (psp-objcopy; not compiled again), as rl30.sh;
#  - v26me_bat.o and xme_rest.o: gl29's stage objects kept (the tree's psp/v26me_bat.c and psp/xme_rest.S are t29's: checked here by cmp);
# the ME-safe renames as psp.sh; tools/xme_me_chk.py; then psp.sh relinks into ROOT/psp (its gates: xme_gate with xme_rest_chk,
# mesec_chk, me_call_chk).  CHECK=1 first proves rl30.sh's relabel recipe on main's own boot.c and xlate.c (gl29's stage objects).
# Nothing outside ROOT is written (psp.sh's shadow folder build_mg exists: links only).
set -e
T=${T:-/home/claude/t32}; M=/home/claude/main; R=${1:-$M/build_gl32}; B=$M/build_gl29/stage
TITLE=${TITLE:-MGS2 harness gl32 vertex cache}
{ [ -e $R/stage ] || [ -e $R/psp ]; } && { echo "rl32.sh: $R/stage or $R/psp exists: look at it first"; exit 1; }
[ -z "$(find $T -type l)" ] || { echo "rl32.sh: $T holds a symlink"; exit 1; }
for f in v26me_bat.c xme_rest.S xme_me.c xme_me.h; do cmp -s $T/psp/$f /home/claude/t29/psp/$f || { echo "rl32.sh: psp/$f is not t29's"; exit 1; }; done
mkdir -p $R/stage
cp $B/*.o $B/genf.txt $B/ge_xme_me.dis $B/xme_me_chk.out $R/stage/
cd $R/stage; rm -f -- ge_xme.o ge_xme_me.o ge_xlme.o xl_psp.o ge_xlate.o boot_rt.o gen.o ge_gelist.o ge_gelive.o ge_xconv.o ge_vu1conv.o
for g in ge_xlme ge_xme xl_psp ge_xlate boot_rt ge_gelist ge_gelive ge_xconv ge_vu1conv; do cp $M/build_gl20/gcda3/$g.gcda .; done
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
merename() { psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp $1
             psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data $1; }
# relab <object> <main's source> <the tree's source> <log>: rl30.sh's relabelled compile (directives only)
relab() { o=$1; f=$2; s=$3; c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //")
  [ -f "$s" ] || { echo "FAIL $o: no $s"; return 1; }
  e=$(echo "$c" | sed "s# -c $f -o $o.o# -E -fdirectives-only $s -o $o.i#"); k=$(echo "$c" | sed "s# -c $f -o $o.o# -fpreprocessed -fdirectives-only -c $o.i -o $o.o#")
  [ "$e" != "$c" ] && [ "$k" != "$c" ] || { echo "FAIL $o: its line is not '... -c $f -o $o.o'"; return 1; }
  { echo "cmd: $e"; sh -c "$e"; sed -i "s#$T/#/home/claude/main/#g" $o.i; echo "relabel: $T/ -> /home/claude/main/";
    echo "cmd: $k"; sh -c "$k"; } > $4 2>&1 || { echo FAIL $o; head -20 $4; return 1; }; }
if [ -n "$CHECK" ]; then   # the recipe on main's own sources: gl23's objects (gl29's stage holds them)
  mkdir -p chk; cd chk; cp ../ge_xlate.gcda ../boot_rt.gcda .
  relab ge_xlate /home/claude/main/ge/xlate.c /home/claude/main/ge/xlate.c cc_ge_xlate.o.log; merename ge_xlate.o
  relab boot_rt /home/claude/main/boot/boot.c /home/claude/main/boot/boot.c cc_boot_rt.o.log
  for o in ge_xlate boot_rt; do cmp -s $o.o $B/$o.o && echo "recipe check: $o.o from main's source equals gl29's stage object" || { echo "recipe check FAILED: $o.o"; exit 1; }; done
  cd ..
fi
for o in ge_xme ge_xme_me ge_xlme; do
  c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //; s#/home/claude/main/ge/#$T/ge/#; s#/home/claude/main/psp/xme_me.c#$T/psp/xme_me.c#")
  echo "$o <- $(echo "$c" | grep -o "$T/[^ ]*")"
  sh -c "$c" > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
echo "ge_xlate <- $T/ge/xlate.c (relabelled, directives only)"; relab ge_xlate /home/claude/main/ge/xlate.c $T/ge/xlate.c cc_ge_xlate.o.log
echo "boot_rt <- $T/boot/boot.c (relabelled, directives only)"; relab boot_rt /home/claude/main/boot/boot.c $T/boot/boot.c cc_boot_rt.o.log
for o in ge_gelist ge_gelive ge_xconv ge_vu1conv; do   # relink29.sh's relabelled compile (plain -E)
  c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //")
  f=$(echo "$c" | grep -o "/home/claude/main/ge/[a-z0-9_]*\.c"); s=$(echo "$f" | sed "s#^/home/claude/main/ge/#$T/ge/#")
  [ -f "$s" ] || { echo "FAIL $o: no $s"; exit 1; }
  e=$(echo "$c" | sed "s# -c $f -o $o.o# -E $s -o $o.i#"); k=$(echo "$c" | sed "s# -c $f -o $o.o# -c $o.i -o $o.o#")
  [ "$e" != "$c" ] && [ "$k" != "$c" ] || { echo "FAIL $o: its line is not '... -c $f -o $o.o'"; exit 1; }
  echo "$o <- $s (relabelled, plain -E)"
  { echo "cmd: $e"; sh -c "$e"; sed -i "s#$T/ge/#/home/claude/main/ge/#g" $o.i; echo "relabel: $T/ge/ -> /home/claude/main/ge/";
    echo "cmd: $k"; sh -c "$k"; } > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
c=$(head -1 $M/build_gl22/stage/cc_xl_psp.o.log | sed "s/^cmd: //; s#/home/claude/main/psp/xl_psp.c#$T/psp/xl_psp.c#")
echo "xl_psp <- $(echo "$c" | grep -o "$T/[^ ]*")"
sh -c "$c" > cc_xl_psp.o.log 2>&1 || { echo FAIL xl_psp; head -20 cc_xl_psp.o.log; exit 1; }
for o in ge_xme ge_xme_me ge_xlme ge_xlate ge_gelist ge_gelive ge_xconv ge_vu1conv; do merename $o.o; done   # psp.sh's ME-safe renames
echo "xme_me undef: $(psp-nm -u ge_xme_me.o | awk '{ print $2 }' | tr '\n' ' ')"
psp-objdump -d ge_xme_me.o > ge_xme_me.dis
python3 -I $M/tools/xme_me_chk.py ge_xme_me.dis > xme_me_chk.out 2>&1 || { cat xme_me_chk.out; exit 1; }
tail -1 xme_me_chk.out
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o
# gen.o: the builder behind the wrapper
OC=   # gl32: and the vertex cache's three event functions (0x129128, 0x12cf28, 0x12cd78)
for fn in P1214a0 P129128 P12cf28 P12cd78; do
  sec=$(psp-objdump -t $B/gen.o | awk -v f=$fn '$NF == f { print $(NF-2) }'); val=$(psp-nm $B/gen.o | awk -v f=$fn '$3 == f && $2 == "T" { print $1 }')
  [ -n "$sec" ] && [ -n "$val" ] || { echo "rl32.sh: no global $fn in $B/gen.o"; exit 1; }
  OC="$OC --add-symbol ${fn}_orig=$sec:0x$val,global,function --weaken-symbol $fn"; echo "gen.o: $fn in $sec at 0x$val"; done
psp-objcopy $OC $B/gen.o gen.o
echo "gen.o: $(psp-nm gen.o | grep 'P1214a0\|P129128\|P12cf28\|P12cd78' | tr '\n' ';')"
rm -f -- ge_xme.gcda ge_xlme.gcda xl_psp.gcda ge_xlate.gcda boot_rt.gcda ge_gelist.gcda ge_gelive.gcda ge_xconv.gcda ge_vu1conv.gcda
# the parents' objects that must come out again
G30=$M/build_gl30/stage; G30N=$M/build_gl30n/stage
cmp -s ge_gelist.o $G30/ge_gelist.o && echo "ge_gelist.o equals gl30's stage object" || { echo "rl32.sh: ge_gelist.o is NOT gl30's"; exit 1; }
for o in ge_gelive; do cmp -s $o.o $M/build_gl23/stage/$o.o && cmp -s $o.o $G30/$o.o && echo "$o.o equals gl23's and gl30's stage object" || { echo "rl32.sh: $o.o is NOT gl23's"; exit 1; }; done
for o in ge_xconv ge_vu1conv; do cmp -s $o.o $G30/$o.o && echo "$o.o equals gl30's stage object (NOT expected in gl32)" || echo "$o.o differs from gl30's stage object (gl32: the vertex cache)"; done
echo "against the parents (cmp): $(for o in ge_xme ge_xme_me ge_xlme xl_psp; do cmp -s $o.o $G30/$o.o && printf '%s =gl30 ' $o; cmp -s $o.o $G30N/$o.o && printf '%s =gl30n ' $o; done; for o in ge_xlate boot_rt gen; do cmp -s $o.o $G30N/$o.o && printf '%s =gl30n ' $o || printf '%s differs-from-gl30n ' $o; done)"
cat cc_ge_xme.o.log cc_ge_xme_me.o.log cc_ge_xlme.o.log cc_ge_xlate.o.log cc_boot_rt.o.log cc_ge_gelist.o.log cc_ge_gelive.o.log cc_ge_xconv.o.log cc_ge_vu1conv.o.log cc_xl_psp.o.log | grep -v '^cmd: \|^relabel: ' | head -30
cd $M
set +e
RELINK=$R/stage PGO=use GCDA=$M/build_gl20/gcda3 ORDER=$M/build_gl20/psp PSPD=$R/psp TITLE="$TITLE" sh scripts/psp.sh > $R/psp.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -16 $R/psp.out; grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more' $R/psp.out | head
exit $r
