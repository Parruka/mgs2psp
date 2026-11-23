#!/bin/sh
# pspb.sh (st1g): native step 1 on gl40's tree, PSP build (emulator only; NOT for delivery): gl40's PGO=use objects (build_gl40/u)
# with the six objects step 1 changes compiled again from st1g/t (t40 + step 1) with gl40's exact lines (u/cc_*.log; the paths st1g/m
# in place of build_gl40/m, the profiles build_gl40/gcda), then linked by psp.sh's RELINK branch with gl40's code order (ord/final),
# gl40's final pads (rl40.sh's pf: RO_PAD 0x260, ME_PAD 0x15c0, XL_PAD 0x210, ME_PRE 0x1d00 (me_sec_pad3.ld), END_PAD 0x1a00) and
# its gates (xme_gate, mesec_chk40, me_call_chk) into st1g/psp.  st1g/m: links to st1g/t's folders (main's gen, cfg, tools read only)
# and a copy of gl40's scripts.  Each of the six is preprocessed and its paths given back as build_gl40/m's before the compile, so that
# GCC finds gl40's profile for the functions step 1 did not change (the id of a static function hashes its source path).  nice 19.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1g; R=/home/claude/main/build_gl40; U=$S/u; P=$S/psp; T=$S/t; MM=$S/m
. /home/claude/main/scripts/env.sh
[ -e $U ] && { echo "pspb.sh: $U exists"; exit 1; }; [ -e $P ] && { echo "pspb.sh: $P exists"; exit 1; }
mkdir -p $MM/scripts || exit 1
for d in boot boot_g0 ge psp rt runtime; do [ -e $MM/$d ] || ln -s $T/$d $MM/$d; done
for d in cfg gen tools; do [ -e $MM/$d ] || ln -s /home/claude/main/$d $MM/$d; done
cp $R/m/scripts/psp.sh $R/m/scripts/env.sh $MM/scripts/ || exit 1
mkdir -p $U && cp $R/u/*.o $R/u/genf.txt $R/u/ge_xme_me.dis $R/u/xme_me_chk.out $R/u/gen_compiled.md5 $U/ && cp $R/gcda/*.gcda $U/ || exit 1
cd $U || exit 1
cc() { co=$1; shift; cs=$(date +%s); { echo "cmd: $*"; eval "nice -n 19 $*"; } > cc_$co.log 2>&1; r=$?; echo "$co rc $r $(( $(date +%s) - cs )) s" >> cc_$co.log; tail -n 1 cc_$co.log; return $r; }
line() { head -n 1 $R/u/cc_$1.log | sed "s|^cmd: ||; s|/home/claude/main/build_gl40/m/|$MM/|g"; }
for o in boot_rt.o xl_psp.o ge_xconv.o ge_gelist.o ge_gelive.o ge_xlme.o; do
  L=$(line $o); [ -n "$L" ] || { echo "pspb.sh: no line for $o"; exit 1; }
  I=${o%.o}.i; LI=$(echo "$L" | sed "s| -c \([^ ]*\) -o [^ ]*\$| -E \1 -o $I|"); LC=$(echo "$L" | sed "s| -c \([^ ]*\) -o \([^ ]*\)\$| -c $I -o \2|")
  [ "$LI" != "$L" ] && [ "$LC" != "$L" ] || { echo "pspb.sh: cannot split the line of $o"; exit 1; }
  rm -f $o $I; cc $I "$LI" || { tail -n 20 cc_$I.log; exit 1; }
  sed -i "s|$MM/|/home/claude/main/build_gl40/m/|g" $I
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
PD=$R/padld; X="-Wl,--defsym,ME_PAD=0x15c0 -Wl,--defsym,XL_PAD=0x210 -Wl,-T,$PD/ro_pad2.ld -Wl,--defsym,RO_PAD=0x260 -Wl,-T,$PD/end_pad.ld -Wl,--defsym,END_PAD=0x1a00 -Wl,--defsym,ME_PRE=0x1d00"
MESEC=$PD/me_sec_pad3.ld XLD="$X" RELINK=$U PGO=use GCDA=$R/gcda ORDER=$R/ord/final PSPD=$P MGS=$MM/build_mg TITLE="MGS2 harness gl40 nge1 (emulator only)" nice -n 19 sh $MM/scripts/psp.sh > $S/psp.out 2>&1
r=$?; echo "psp.sh rc $r"; tail -n 14 $S/psp.out | cut -c1-240
grep -i 'mesec\|FAIL\|warn\|xme_rest_chk\|gate\|names more\|me_call' $S/psp.out | head; exit $r
