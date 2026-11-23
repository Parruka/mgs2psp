#!/bin/sh
# pspb.sh: native step 1's PSP build (emulator only; NOT for delivery): gl39's PGO=use objects (build_gl39/u) with the six objects
# step 1 changes compiled again from st1/t with gl39's exact lines (u/cc_*.log; the paths st1/m in place of build_gl39/m, the
# profiles build_gl39/gcda), then linked by psp.sh's RELINK branch with gl39's code order (ord/final) and its gates (xme_gate,
# mesec_chk, me_call_chk) into st1/psp.  st1/m: links to st1/t's folders (main's gen, cfg, tools read only) and a copy of gl39's
# scripts.  Each of the six is preprocessed and its paths given back as build_gl39/m's before the compile, so that GCC finds gl39's
# profile for the functions step 1 did not change (the id of a static function hashes its source path).  Emulator only; nice 19.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1; R=/home/claude/main/build_gl39; U=$S/u; P=$S/psp; T=$S/t; MM=$S/m
. /home/claude/main/scripts/env.sh
[ -e $U ] && { echo "pspb.sh: $U exists"; exit 1; }; [ -e $P ] && { echo "pspb.sh: $P exists"; exit 1; }
mkdir -p $MM/scripts || exit 1
for d in boot ge psp rt runtime; do [ -e $MM/$d ] || ln -s $T/$d $MM/$d; done
[ -e $MM/boot_g0 ] || ln -s /home/claude/t39/boot_g0 $MM/boot_g0
for d in cfg gen tools; do [ -e $MM/$d ] || ln -s /home/claude/main/$d $MM/$d; done
cp $R/m/scripts/psp.sh $R/m/scripts/env.sh $MM/scripts/ || exit 1
mkdir -p $U && cp $R/u/*.o $R/u/genf.txt $R/u/ge_xme_me.dis $R/u/xme_me_chk.out $R/u/gen_compiled.md5 $U/ && cp $R/gcda/*.gcda $U/ || exit 1
cd $U || exit 1
cc() { co=$1; shift; cs=$(date +%s); { echo "cmd: $*"; eval "nice -n 19 $*"; } > cc_$co.log 2>&1; r=$?; echo "$co rc $r $(( $(date +%s) - cs )) s" >> cc_$co.log; tail -n 1 cc_$co.log; return $r; }
line() { head -n 1 $R/u/cc_$1.log | sed "s|^cmd: ||; s|/home/claude/main/build_gl39/m/|$MM/|g"; }
for o in boot_rt.o xl_psp.o ge_xconv.o ge_gelist.o ge_gelive.o ge_xlme.o; do
  L=$(line $o); [ -n "$L" ] || { echo "pspb.sh: no line for $o"; exit 1; }
  # the profile ids of static functions hash the source path (scripts/psp.sh's note): preprocess, give every path of st1/m back as
  # build_gl39/m's (the training build's), compile the result; the code of the functions step 1 did not change then takes gl39's profile
  I=${o%.o}.i; LI=$(echo "$L" | sed "s| -c \([^ ]*\) -o [^ ]*\$| -E \1 -o $I|"); LC=$(echo "$L" | sed "s| -c \([^ ]*\) -o \([^ ]*\)\$| -c $I -o \2|")
  [ "$LI" != "$L" ] && [ "$LC" != "$L" ] || { echo "pspb.sh: cannot split the line of $o"; exit 1; }
  rm -f $o $I; cc $I "$LI" || { tail -n 20 cc_$I.log; exit 1; }
  sed -i "s|$MM/|/home/claude/main/build_gl39/m/|g" $I
  grep -q "$S" $I && { echo "pspb.sh: $I still names the scratch folder"; exit 1; }
  cc $o "$LC" || { tail -n 20 cc_$o.log; exit 1; }; rm -f $I
  case $o in
  ge_*) psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp $o || exit 1
        psp-objcopy --rename-section .bss.h39=.me_bss.h39 --rename-section .bss=.me_bss --rename-section .data=.me_data $o || exit 1 ;;
  xl_psp.o) psp-objcopy --rename-section .bss.me=.me_bss $o || exit 1 ;;
  esac
done
rm -f $U/*.gcda
cd $MM || exit 1
RELINK=$U PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$P MGS=$MM/build_mg TITLE="MGS2 harness gl39 nge1 (emulator only)" nice -n 19 sh $MM/scripts/psp.sh > $S/psp.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -n 14 $S/psp.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $S/psp.out | head; exit $r
