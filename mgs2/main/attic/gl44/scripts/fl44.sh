#!/bin/sh
# fl44.sh <order folder name, s1 or s2> (gl44's final link, gl43's procedure, attic/gl43/state.md B7): a trial link psp_t with gl43's pads
# (RO_PAD 0x15d9, ME_PRE 0x1180, ME_PAD 0x1580, XL_PAD 0x210, END_PAD 0x19c0) and the chosen order; padfit42.py against gl43's final ELF
# (build_gl43/psp: .rodata, the .bss head, every .me_bss group at its offsets mod 0x2000, the image end); the final link psp with the
# fitted pads (RO_PAD: .rodata at the target's offset) and the same link again as psp_b: the 10 files byte for byte; rt_new44.sh
# (real addresses against the search's); g4_44.sh psp.  psp_t and psp_b are left for a look (deleted by hand).
R=/home/claude/main/build_gl44; O=${1:-s2}; cd $R || exit 1; T43=/home/claude/main/build_gl43/psp/MGS2T.elf
for d in psp psp_t psp_b; do [ -e $R/$d ] && { echo "fl44.sh: $R/$d exists"; exit 1; }; done
[ -d $R/ord/$O ] || { echo "fl44.sh: no $R/ord/$O"; exit 1; }
RO_PAD=0x15d9 ME_PRE=0x1180 ME_PAD=0x1580 XL_PAD=0x210 END_PAD=0x19c0 ORD=$R/ord/$O sh rl44.sh psp_t > $R/fl44_t.out 2>&1; echo "trial rc $?"; tail -n 4 $R/fl44_t.out | cut -c1-200
python3 -I /home/claude/main/build_gl42/tools/padfit42.py $R/psp_t/MGS2T.elf $R/u 0x15d9 0x1180 0x1580 0x210 0x19c0 $T43 > $R/padfit44.out 2>&1 || { cat $R/padfit44.out; exit 1; }; cat $R/padfit44.out
RO=$(grep '^PADS: RO_PAD' $R/padfit44.out | awk '{ print $3 }'); L=$(grep '^ *ME_PRE ' $R/padfit44.out)
PRE=$(echo "$L" | awk '{ print $2 }'); MEP=$(echo "$L" | awk '{ print $4 }'); XLP=$(echo "$L" | awk '{ print $6 }'); END=$(echo "$L" | awk '{ print $8 }')
echo "fl44.sh: pads RO_PAD $RO ME_PRE $PRE ME_PAD $MEP XL_PAD $XLP END_PAD $END"
for v in "$RO" "$PRE" "$MEP" "$XLP" "$END"; do case $v in 0x*) ;; *) echo "fl44.sh: a pad not read: '$v'"; exit 1 ;; esac; done
for d in psp psp_b; do RO_PAD=$RO ME_PRE=$PRE ME_PAD=$MEP XL_PAD=$XLP END_PAD=$END ORD=$R/ord/$O sh rl44.sh $d > $R/fl44_$d.out 2>&1; echo "$d rc $?"; done
n=0; for f in MGS2T.elf EBOOT.PBP ovl_d00t.bin ovl_d01t.bin ovl_init.bin ovl_n_title.bin ovl_r_title.bin ovl_r_tnk0.bin ovl_w00a.bin ovl_w01a.bin; do
  cmp -s $R/psp/$f $R/psp_b/$f && n=$((n + 1)) || echo "fl44.sh: $f DIFFERS between psp and psp_b"; done; echo "fl44.sh: linked twice: $n of 10 files byte-equal"
python3 -I /home/claude/main/build_gl42/tools/padfit42.py $R/psp/MGS2T.elf $R/u $RO $PRE $MEP $XLP $END $T43 | tail -n 4
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH; psp-size $T43 $R/psp/MGS2T.elf; psp-nm $T43 | awk '$3 == "_end"'; psp-nm $R/psp/MGS2T.elf | awk '$3 == "_end"'
psp-readelf -l $R/psp/MGS2T.elf | grep -c LOAD | sed 's/^/LOAD segments: /'
sh rt_new44.sh > $R/rt_new44.out 2>&1; cat $R/rt_new44.out | cut -c1-200
sh g4_44.sh $R/psp > $R/g4_44_psp.out 2>&1; cat $R/g4_44_psp.out | cut -c1-250
md5sum $R/psp/EBOOT.PBP; echo "fl44.sh: done $(date -u +%H:%M:%S)"
