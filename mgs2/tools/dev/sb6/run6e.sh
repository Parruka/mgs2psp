#!/bin/sh
# run6e.sh: run6.sh's batch E, after the fix in boot/xlive_chk.c (glf.ram set at each transfer the translator takes: with the translator
# started late the first frame's thinning scan read RAM through a null pointer; batch B's digest runs from vb 30,800 stopped there, rc 139,
# lane 3 off and on alike).  On the twin x1 built again with the fix: the per-call digest files on the indoor route's aft deck in the rain
# (vb 30,800-30,900, lane 3 off and on, dig2_x86.sh), and the indoor route from boot with lane 3 on and chk2's settings (i3on) against
# batch A's i2on (the binary before the fix): the same log but for the host's timings.  The results in x1/res_E.txt.
S=/home/claude/sb6; X=$S/x1; MG=/mnt/project-files/mgs2; PY="env PYTHONDONTWRITEBYTECODE=1 python3 -I"
sh $S/dig2_x86.sh g3 30800 30900 > $X/dig_g3.out 2>&1 &
sh $MG/tools/dev/gl1_x86.sh $X i3on i 0:37800 MGS2_XLIVE_GB=1 MGS2_XLIVE_GX_TEXKB=2688 MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 \
  MGS2_HASH_DENSE=15700:16010 MGS2_SB_LANE=15 &
wait
{ echo "== g3 dig"; cat $X/dig_g3.out; cat $X/rc_g3off.txt $X/rc_g3on.txt
  for t in off on; do printf "dig_g3$t: calls %s, vblank lines %s, bytes %s, md5 %s\n" "$(grep -c '^c ' $X/dig_g3$t.txt)" \
    "$(grep -c '^v ' $X/dig_g3$t.txt)" "$(wc -c < $X/dig_g3$t.txt)" "$(md5sum < $X/dig_g3$t.txt | cut -c1-32)"; done
  echo "calls by entry (lane 3 on):"; grep '^c ' $X/dig_g3on.txt | awk '{print $4}' | sort | uniq -c | sort -rn | head -16
  echo "prims (v lines' sum): off $(awk '/^v /{s+=$6} END{print s}' $X/dig_g3off.txt), on $(awk '/^v /{s+=$6} END{print s}' $X/dig_g3on.txt)"
  $PY $S/census6.py $X/out_g3on.log $X/out_g3off.log
  echo "== g3: on vs off logs"; $PY $S/cmp6.py $X/out_g3on.log $X/out_g3off.log $X/out_g3off.log
  echo "== i3on (after the fix) vs i2on (batch A, before the fix)"; cat $X/rc_i3on.txt
  $PY $S/cmp6.py $X/out_i3on.log $X/out_i2on.log.gz $X/out_i2on.log.gz
  $PY $S/tl6.py $X/out_i2on.log.gz $X/out_i3on.log; } > $X/res_E.txt 2>&1
gzip -9 $X/out_g3*.log $X/dig_g3*.txt $X/out_i3on.log
echo "batch E done $(date +%T)"
