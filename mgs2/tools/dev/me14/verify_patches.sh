#!/bin/sh
# verify_patches.sh: each patch script remakes its file from its base byte for byte (task #149 step 2's files in /home/claude/me14)
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/me14; E=/home/claude/me14; B=/home/claude/sb14; M=/home/claude/me13
T=$S/vp_tmp; rm -rf $T; mkdir -p $T/ge || exit 1
P="env PYTHONDONTWRITEBYTECODE=1 python3 -I"
$P $S/patch_me14.py $B/ge $T/ge $M/ge_e/xlate.c $T/ge_e_xlate.c > $T/p1.out 2>&1 || { echo "patch_me14.py failed"; cat $T/p1.out; }
$P $S/patch_me14_lc.py $T/ge/gsup2.c $T/gsup2_lc.c > $T/p2.out 2>&1 && mv $T/gsup2_lc.c $T/ge/gsup2.c || echo "patch_me14_lc.py failed"
for f in xlive.c xlate.c gsfront.c texconv.c texcache.c gelive.c gebatch.c xconv.c vu1conv.c gelist.c gsup2.c gelive.h gebatch.h xconv.h; do
  cmp -s $T/ge/$f $E/ge/$f && echo "same ge/$f" || echo "DIFFERENT ge/$f"; done
cmp -s $T/ge_e_xlate.c $E/ge_e/xlate.c && echo "same ge_e/xlate.c" || echo "DIFFERENT ge_e/xlate.c"
$P $S/patch_me14_glue.py $B/boot/xlive_chk.c $T/xlive_chk.c > /dev/null 2>&1; cmp -s $T/xlive_chk.c $E/boot/xlive_chk.c && echo "same boot/xlive_chk.c" || echo "DIFFERENT boot/xlive_chk.c"
$P $S/patch_me14g_glue.py $M/boot/xlive_chk_g.c $T/xlive_chk_g.c > /dev/null 2>&1; cmp -s $T/xlive_chk_g.c $E/boot/xlive_chk_g.c && echo "same boot/xlive_chk_g.c" || echo "DIFFERENT boot/xlive_chk_g.c"
$P $S/patch_me14g2_glue.py $E/boot/xlive_chk_g.c $T/xlive_chk_g2.c > /dev/null 2>&1; cmp -s $T/xlive_chk_g2.c $E/boot/xlive_chk_g2.c && echo "same boot/xlive_chk_g2.c" || echo "DIFFERENT boot/xlive_chk_g2.c"
$P $S/patch_me14_psp.py $B/psp/xl_psp.c $T/xl_psp.c > /dev/null 2>&1; cmp -s $T/xl_psp.c $E/psp/xl_psp.c && echo "same psp/xl_psp.c" || echo "DIFFERENT psp/xl_psp.c"
$P $S/patch_me14_psp_v7.py $B/psp/xl_psp.c $T/xl_psp7.c > /dev/null 2>&1; cmp -s $T/xl_psp7.c $E/ge_e/xl_psp_v7.c && echo "same v7 xl_psp.c (me14p8's)" || echo "DIFFERENT v7"
$P $S/patch_me14_psp_v6.py $B/psp/xl_psp.c $T/xl_psp6.c > /dev/null 2>&1; cmp -s $T/xl_psp6.c $E/ge_e/xl_psp_v6.c && echo "same v6 xl_psp.c (me14p7's)" || echo "DIFFERENT v6"
$P $S/patch_me14_psp_v5.py $B/psp/xl_psp.c $T/xl_psp5.c > /dev/null 2>&1; cmp -s $T/xl_psp5.c $E/ge_e/xl_psp_v5.c && echo "same v5 xl_psp.c (me14p6's)" || echo "DIFFERENT v5"
rm -rf $T
