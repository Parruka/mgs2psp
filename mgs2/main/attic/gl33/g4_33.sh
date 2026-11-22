#!/bin/sh
# g4_33.sh (gl33 G4): the undefined symbols of every object of the final build (build_gl33/psp; all compiled again from t33 with gl33's
# profiles) against gl31's stage objects (build_gl31/stage), the names the ELF imports (sce* stubs) against gl31's ELF, the ME checks'
# last lines, and ge_xme_me.o's instructions against gl31's.
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
R=/home/claude/main/build_gl33; A=/home/claude/main/build_gl31/stage; B=$R/psp; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g4_33; mkdir -p $S
echo "gl33 G4: undefined symbols of each object against gl31's stage object (psp-nm -u); < only gl31's, > only gl33's"
n=0; same=0; new=
for o in $B/*.o; do b=$(basename $o); case $b in ovl_hdr_*) continue ;; esac; n=$((n + 1))
  [ -f $A/$b ] || { echo "  $b: not in gl31's stage"; continue; }
  cmp -s $o $A/$b && same=$((same + 1))
  psp-nm -u $A/$b | awk '{ print $2 }' | sort -u > $S/a.txt; psp-nm -u $o | awk '{ print $2 }' | sort -u > $S/b.txt
  d=$(diff $S/a.txt $S/b.txt | grep '^[<>]' | tr '\n' ' '); [ -n "$d" ] && { echo "  $b: $d"; new="$new $(diff $S/a.txt $S/b.txt | grep '^>' | cut -c3- | tr '\n' ' ')"; }
done
echo "objects: $n; byte-identical to gl31's stage objects: $same"
psp-nm $B/MGS2T.elf > $S/elf.nm
printf "new names and where the ELF defines them:"; for s in $new; do printf " %s(%s)" $s "$(awk -v s=$s '$3 == s { print $2 }' $S/elf.nm | head -1)"; done; echo
for e in /home/claude/main/build_gl31/psp/MGS2T.elf $B/MGS2T.elf; do psp-nm $e | awk '{ print $3 }' | grep '^sce' | sort -u; done > /dev/null
psp-nm /home/claude/main/build_gl31/psp/MGS2T.elf | awk '{ print $3 }' | grep '^sce' | sort -u > $S/ia.txt
awk '{ print $3 }' $S/elf.nm | grep '^sce' | sort -u > $S/ib.txt
echo "imports of the ELF (sce* names) against gl31's: $(wc -l < $S/ia.txt) / $(wc -l < $S/ib.txt) names; differences: $(diff $S/ia.txt $S/ib.txt | grep -c '^[<>]')"; diff $S/ia.txt $S/ib.txt | grep '^[<>]'
for s in .lib.stub .lib.ent .rodata.sceResident .rodata.sceNid .sceStub.text; do
  printf "  section %s: gl31 %s bytes, gl33 %s bytes\n" $s "$(psp-objdump -h /home/claude/main/build_gl31/psp/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')" "$(psp-objdump -h $B/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')"; done
psp-objdump -d $A/ge_xme_me.o | sed 1,3d | cut -f2- > $S/ma.txt; psp-objdump -d $B/ge_xme_me.o | sed 1,3d | cut -f2- > $S/mb.txt
echo "ge_xme_me.o: gl31 $(md5sum < $A/ge_xme_me.o | cut -c1-8), gl33 $(md5sum < $B/ge_xme_me.o | cut -c1-8); instruction lines differing: $(diff $S/ma.txt $S/mb.txt | grep -c '^[<>]')"
tail -n 1 $B/xme_me_chk.out; tail -n 1 $B/xme_rest_chk.out; grep -i "mesec_chk\|me_call_chk" $R/psp.out | tail -3
