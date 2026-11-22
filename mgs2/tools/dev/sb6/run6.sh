#!/bin/sh
# run6.sh: sb6's x86 checks of lane 3 (renderer_notes 11.33) one batch after another on the twin x1, each batch's logs compared, then
# gzipped (disk): A chk2 indoor + deck, B chk2 codec + the per-call digest files on the indoor route's aft deck in the rain (vb 30,800-30,900),
# C MGS2_THIN_CHK at 1/2 (indoor, deck), D the same at 1/1 (no thinning).  The results in x1/res_<batch>.txt.  References: sb5's x1 runs
# with lanes 1, 2 and 4 on (sb6's off configuration), prof/hashref_x86_{in,deck,codec}_h2.txt.
S=/home/claude/sb6; X=$S/x1; R5=/home/claude/sb5/x1; P=/mnt/project-files/mgs2/prof; PY="env PYTHONDONTWRITEBYTECODE=1 python3 -I"
ana() {   # ana <prefix> <reference log> <hashref>: on vs off vs reference; chk vs off; hashes; census
  p=$1; r=$2; h=$3
  echo "== $p: on vs off (reference $r)"; $PY $S/cmp6.py $X/out_${p}on.log $X/out_${p}off.log $r
  if [ -f $X/out_${p}chk.log ]; then echo "== $p: chk vs off"; $PY $S/cmp6.py $X/out_${p}chk.log $X/out_${p}off.log $r; fi
  echo "== $p: hashes"; $PY $S/href6.py $P/$h $X/out_${p}*.log
  echo "== $p: census"; $PY $S/census6.py $X/out_${p}*.log
  echo "== $p: rc"; cat $X/rc_${p}*.txt
  grep -h "^xlive gb: thinned\|^xlive gb: thin check\|^xlive lazy" $X/out_${p}*.log
}
case $1 in
A) sh $S/chk2_x86.sh i i2 & sh $S/chk2_x86.sh d d2 & wait
   { ana i2 $R5/out_i2on.log.gz hashref_x86_in_h2.txt; ana d2 $R5/out_d2on.log.gz hashref_x86_deck_h2.txt; } > $X/res_A.txt 2>&1
   gzip -9 $X/out_i2*.log $X/out_d2*.log ;;
B) sh $S/chk2_x86.sh c c2 & sh $S/dig2_x86.sh g3 30800 30900 > $X/dig_g3.out 2>&1 & wait
   { ana c2 $R5/out_c2on.log.gz hashref_x86_codec_h2.txt; echo "== g3 dig"; cat $X/dig_g3.out; for t in off on; do printf "dig_g3$t: calls %s, prims lines %s, bytes %s\n" \
     "$(grep -c '^c ' $X/dig_g3$t.txt)" "$(grep -c '^v ' $X/dig_g3$t.txt)" "$(wc -c < $X/dig_g3$t.txt)"; done
     grep '^c ' $X/dig_g3on.txt | awk '{print $4}' | sort | uniq -c | sort -rn | head -12
     $PY $S/census6.py $X/out_g3on.log $X/out_g3off.log; $PY $S/cmp6.py $X/out_g3on.log $X/out_g3off.log $X/out_g3off.log | tail -15; } > $X/res_B.txt 2>&1
   gzip -9 $X/out_c2*.log $X/out_g3*.log $X/dig_g3*.txt ;;
C) sh $S/thin_x86.sh i t2i 2 & sh $S/thin_x86.sh d t2d 2 & wait
   { ana t2i $R5/out_i1on.log.gz hashref_x86_in_h2.txt; ana t2d $X/out_t2doff.log hashref_x86_deck_h2.txt; } > $X/res_C.txt 2>&1
   gzip -9 $X/out_t2i*.log $X/out_t2d*.log ;;
D) sh $S/thin_x86.sh i t1i 1 & sh $S/thin_x86.sh d t1d 1 & wait
   { ana t1i $X/out_t1ioff.log hashref_x86_in_h2.txt; ana t1d $X/out_t1doff.log hashref_x86_deck_h2.txt; } > $X/res_D.txt 2>&1
   gzip -9 $X/out_t1i*.log $X/out_t1d*.log ;;
esac
echo "batch $1 done $(date +%T)"
