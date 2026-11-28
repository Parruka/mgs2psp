#!/bin/sh
# g4_38.sh [psp dir] (gl38's static gate, as gl36's g4_36.sh): the names the final ELF imports (sce* stubs) against gl36's delivered
# build (build_gl36/psp) and gl37's no-PGO build, the stub sections' sizes, the ME checks' last lines, the ELF's sizes, and the new
# symbols' definitions.
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
R=/home/claude/main/build_gl38; B=${1:-$R/psp}; A=/home/claude/main/build_gl36/psp; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/g4_38; mkdir -p $S
echo "gl38 G4 on $B (EBOOT $(md5sum < $B/EBOOT.PBP | cut -c1-8), $(wc -c < $B/EBOOT.PBP) bytes) against gl36's $A (EBOOT $(md5sum < $A/EBOOT.PBP | cut -c1-8), $(wc -c < $A/EBOOT.PBP) bytes)"
psp-nm $A/MGS2T.elf | awk '{ print $3 }' | grep '^sce' | sort -u > $S/ia.txt; psp-nm $B/MGS2T.elf | awk '{ print $3 }' | grep '^sce' | sort -u > $S/ib.txt
echo "imports of the ELF (sce* names) gl36 / gl38: $(wc -l < $S/ia.txt) / $(wc -l < $S/ib.txt) names; differences: $(diff $S/ia.txt $S/ib.txt | grep -c '^[<>]')"; diff $S/ia.txt $S/ib.txt | grep '^[<>]'
for s in .lib.stub .lib.ent .rodata.sceResident .rodata.sceNid .sceStub.text; do
  printf "  section %s: gl36 %s bytes, gl38 %s bytes\n" $s "$(psp-objdump -h $A/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')" "$(psp-objdump -h $B/MGS2T.elf | awk -v s=$s '$2 == s { print $3 }')"; done
psp-size $A/MGS2T.elf $B/MGS2T.elf
n=0; und=0; for o in $B/*.o; do b=$(basename $o); case $b in ovl_hdr_*) continue ;; esac; n=$((n + 1))
  [ -f $A/$b ] || { echo "  $b: not in gl36's"; continue; }
  psp-nm -u $A/$b | awk '{ print $2 }' | sort -u > $S/a.txt; psp-nm -u $o | awk '{ print $2 }' | sort -u > $S/b.txt
  d=$(diff $S/a.txt $S/b.txt | grep '^>' | cut -c3- | grep '^sce' | tr '\n' ' '); [ -n "$d" ] && { echo "  $b: new sce* references: $d"; und=$((und + 1)); }
done; echo "objects $n; objects with a sce* reference gl36's object does not have: $und"
tail -n 1 $B/xme_me_chk.out 2>/dev/null; tail -n 1 $B/xme_rest_chk.out 2>/dev/null; grep -i "mesec_chk\|me_call_chk" $B/../$(basename $B).out $R/rl38.out $R/rt_b38.out 2>/dev/null | tail -4
