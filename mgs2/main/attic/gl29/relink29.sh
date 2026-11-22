#!/bin/sh
# relink29.sh TREE NAME [ROOT] (gl29, 2026-11-20; relink28.sh + the objects that include the headers gl29 changes): ROOT (default
# /home/claude/main/build_NAME)/stage = gl23's stage objects with these recompiled from TREE (gl20 gcda3 profiles):
#  - ge_xme.o, ge_xme_me.o, ge_xlme.o (build_gl20/u2's lines on TREE's files) and xl_psp.o (build_gl22's line on TREE's file), as
#    relink28.sh: the profiles of their static functions keyed by TREE's path, so with TREE=/home/claude/t28 they are gl28's objects;
#  - gl29: ge_gelist.o, ge_gelive.o, ge_xconv.o (gl23's objects in gl28; gelist.h changed, and gelive.c includes it; xconv.c does not,
#    recompiled as the plan says, as a check: it must come out as gl23's): build_gl20/u2's lines on TREE's files, preprocessed (-E), the
#    .i's line markers relabelled from TREE/ge/ to /home/claude/main/ge/ (GCC's profile id of a static function hashes its source
#    file's name: scripts/psp.sh), then compiled from the .i, so the gcda3 profiles meet them as they met gl23's (TREE=t28: gl23's
#    objects byte for byte);
#  - v26me_bat.o (psp.sh's line) and xme_rest.o (+ tools/xme_rest_chk.py), as relink28.sh;
# the ME-safe renames as psp.sh; tools/xme_me_chk.py; then psp.sh relinks into ROOT/psp with TITLE (default "MGS2 harness gl29 shadow
# depth test"; psp.sh's gates: xme_gate, mesec_chk, me_call_chk).  The recipe's check: TREE=/home/claude/t28 TITLE="MGS2 harness
# mainline gl28" must give gl28's EBOOT (ee11e43f).  Nothing outside ROOT is written (psp.sh's shadow folder build_mg exists: links only).
set -e
T=$1; N=$2; M=/home/claude/main; R=${3:-$M/build_$N}
TITLE=${TITLE:-MGS2 harness gl29 shadow depth test}
[ -n "$T" ] && [ -n "$N" ] || { echo "relink29.sh TREE NAME [ROOT]"; exit 1; }
{ [ -e $R/stage ] || [ -e $R/psp ]; } && { echo "relink29.sh: $R/stage or $R/psp exists: look at it first"; exit 1; }
mkdir -p $R/stage
cp $M/build_gl23/stage/*.o $M/build_gl23/stage/genf.txt $M/build_gl23/stage/ge_xme_me.dis $M/build_gl23/stage/xme_me_chk.out $R/stage/
cd $R/stage; rm -f -- ge_xme.o ge_xme_me.o ge_xlme.o xl_psp.o v26me_bat.o ge_gelist.o ge_gelive.o ge_xconv.o
for g in ge_xlme ge_xme xl_psp ge_gelist ge_gelive ge_xconv; do cp $M/build_gl20/gcda3/$g.gcda .; done
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
SDK=/home/claude/pspsdk/pspdev/psp/sdk
for o in ge_xme ge_xme_me ge_xlme; do
  c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //; s#/home/claude/main/ge/#$T/ge/#; s#/home/claude/main/psp/xme_me.c#$T/psp/xme_me.c#")
  echo "$o <- $(echo "$c" | grep -o "$T/[^ ]*")"
  sh -c "$c" > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
for o in ge_gelist ge_gelive ge_xconv; do   # gl29: relabelled to /home/claude/main/ge/ (above)
  c=$(head -1 $M/build_gl20/u2/cc_$o.o.log | sed "s/^cmd: //")
  f=$(echo "$c" | grep -o "/home/claude/main/ge/[a-z0-9_]*\.c"); s=$(echo "$f" | sed "s#^/home/claude/main/ge/#$T/ge/#")
  [ -f "$s" ] || { echo "FAIL $o: no $s"; exit 1; }
  e=$(echo "$c" | sed "s# -c $f -o $o.o# -E $s -o $o.i#"); k=$(echo "$c" | sed "s# -c $f -o $o.o# -c $o.i -o $o.o#")
  [ "$e" != "$c" ] && [ "$k" != "$c" ] || { echo "FAIL $o: its line is not '... -c $f -o $o.o'"; exit 1; }
  echo "$o <- $s (relabelled)"
  { echo "cmd: $e"; sh -c "$e"; sed -i "s#$T/ge/#/home/claude/main/ge/#g" $o.i; echo "relabel: $T/ge/ -> /home/claude/main/ge/";
    echo "cmd: $k"; sh -c "$k"; } > cc_$o.o.log 2>&1 || { echo FAIL $o; head -20 cc_$o.o.log; exit 1; }
done
c=$(head -1 $M/build_gl22/stage/cc_xl_psp.o.log | sed "s/^cmd: //; s#/home/claude/main/psp/xl_psp.c#$T/psp/xl_psp.c#")
echo "xl_psp <- $(echo "$c" | grep -o "$T/[^ ]*")"
sh -c "$c" > cc_xl_psp.o.log 2>&1 || { echo FAIL xl_psp; head -20 cc_xl_psp.o.log; exit 1; }
# gl28: the battery unit with psp.sh's own line (scripts/psp.sh: BASEF + the catcher's defines), from the tree
BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
c="psp-gcc $BASEF -fno-merge-constants -DME_DIAG3 -DME_DIAG4 -DME_DIAG5 -DME_EXC -DME_EXC_HOLD -DSYS_DIAG -I$SDK/include -c $T/psp/v26me_bat.c -o v26me_bat.o"
echo "v26me_bat <- $T/psp/v26me_bat.c"
{ echo "cmd: $c"; sh -c "$c"; } > cc_v26me_bat.o.log 2>&1 || { echo FAIL v26me_bat; head -20 cc_v26me_bat.o.log; exit 1; }
# gl28: the ME's rest, as psp.sh assembles it (v30me5.sh's me_rest.o line), only when the tree has it
if [ -f $T/psp/xme_rest.S ]; then
  c="psp-gcc -c $T/psp/xme_rest.S -o xme_rest.o"; echo "xme_rest <- $T/psp/xme_rest.S"
  { echo "cmd: $c"; sh -c "$c"; } > cc_xme_rest.o.log 2>&1 || { echo FAIL xme_rest; head -20 cc_xme_rest.o.log; exit 1; }
  python3 -I $M/tools/xme_rest_chk.py xme_rest.o /mnt/project-files/mgs2/psp/me_rest.S > xme_rest_chk.out 2>&1 || { cat xme_rest_chk.out; exit 1; }
  tail -1 xme_rest_chk.out
fi
for o in ge_xme ge_xme_me ge_xlme ge_gelist ge_gelive ge_xconv; do   # psp.sh's ME-safe renames (gl29: and the three relabelled ones)
  psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp $o.o
  psp-objcopy --rename-section .bss=.me_bss --rename-section .data=.me_data $o.o
done
echo "xme_me undef: $(psp-nm -u ge_xme_me.o | awk '{ print $2 }' | tr '\n' ' ')"
psp-objdump -d ge_xme_me.o > ge_xme_me.dis
python3 -I $M/tools/xme_me_chk.py ge_xme_me.dis > xme_me_chk.out 2>&1 || { cat xme_me_chk.out; exit 1; }
tail -1 xme_me_chk.out
psp-objcopy --rename-section .bss.me=.me_bss xl_psp.o
rm -f -- ge_xme.gcda ge_xlme.gcda xl_psp.gcda ge_gelist.gcda ge_gelive.gcda ge_xconv.gcda
echo "gl23's objects equal (cmp): $(for o in ge_gelist ge_gelive ge_xconv; do cmp -s $o.o $M/build_gl23/stage/$o.o && printf '%s yes ' $o || printf '%s no ' $o; done)"
cat cc_ge_xme.o.log cc_ge_xme_me.o.log cc_ge_gelist.o.log cc_ge_gelive.o.log cc_ge_xconv.o.log cc_xl_psp.o.log cc_v26me_bat.o.log | grep -v '^cmd: \|^relabel: ' | head -20
cd $M
set +e
RELINK=$R/stage PGO=use GCDA=$M/build_gl20/gcda3 ORDER=$M/build_gl20/psp PSPD=$R/psp TITLE="$TITLE" sh scripts/psp.sh > $R/psp.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -16 $R/psp.out; grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more' $R/psp.out | head
exit $r
