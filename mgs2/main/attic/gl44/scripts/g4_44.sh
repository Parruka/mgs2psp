#!/bin/sh
# g4_44.sh [psp dir] (gl44: build_gl43/g4_43.sh against gl43's final build build_gl43/psp, A= another; gl43: against gl42's): the names the final ELF imports (sce* stubs),
# build (build_gl39/psp), the stub sections' sizes, the ME checks' last lines, the ELF's sizes, and the new
# symbols' definitions.
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
R=/home/claude/main/build_gl44; B=${1:-$R/psp}; A=${A:-/home/claude/main/build_gl43/psp}; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g4_44; mkdir -p $S
echo "gl44 G4 on $B (EBOOT $(md5sum < $B/EBOOT.PBP | cut -c1-8), $(wc -c < $B/EBOOT.PBP) bytes) against gl43's $A (EBOOT $(md5sum < $A/EBOOT.PBP | cut -c1-8), $(wc -c < $A/EBOOT.PBP) bytes)"
psp-nm $A/MGS2T.elf | awk '{ print $3 }' | grep '^sce' | sort -u > $S/ia.txt; psp-nm $B/MGS2T.elf | awk '{ print $3 }' | grep '^sce' | sort -u > $S/ib.txt
echo "imports of the ELF (sce* names) gl43 / gl44: $(wc -l < $S/ia.txt) / $(wc -l < $S/ib.txt) names; differences: $(diff $S/ia.txt $S/ib.txt | grep -c '^[<>]')"; diff $S/ia.txt $S/ib.txt | grep '^[<>]'
for s in .lib.stub .lib.ent .rodata.sceResident .rodata.sceNid .sceStub.text; do
  printf "  section %s: gl43 %s bytes, gl44 %s bytes\n" $s "$(psp-objdump -h $A/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')" "$(psp-objdump -h $B/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')"; done
psp-size $A/MGS2T.elf $B/MGS2T.elf
n=0; und=0; for o in $B/*.o; do b=$(basename $o); case $b in ovl_hdr_*) continue ;; esac; n=$((n + 1))
  [ -f $A/$b ] || { echo "  $b: not in gl43's"; continue; }
  psp-nm -u $A/$b | awk '{ print $2 }' | sort -u > $S/a.txt; psp-nm -u $o | awk '{ print $2 }' | sort -u > $S/b.txt
  d=$(diff $S/a.txt $S/b.txt | grep '^>' | cut -c3- | grep '^sce' | tr '\n' ' '); [ -n "$d" ] && { echo "  $b: new sce* references: $d"; und=$((und + 1)); }
done; echo "objects $n; objects with a sce* reference gl43's object does not have: $und"
tail -n 1 $B/xme_me_chk.out 2>/dev/null; tail -n 1 $B/xme_rest_chk.out 2>/dev/null; grep -i "mesec_chk\|me_call_chk" $B/../$(basename $B).out $R/psp.out 2>/dev/null | tail -4
