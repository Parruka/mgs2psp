#!/bin/sh
# chain3.sh (st1g): after the late walk's index fix (ge/xlme.c: nge_walk(b, x->nat_i0[b] + i)): the twin's xlme and link, the late-walk
# arm lw3 (MGS2_NAT_LW=2, check mode, XP check); the PSP build again (pspb.sh, gl40's pads) and its pad-less relink (rl_np.sh); the
# emulator g1np (on, the pad-less package).  One at a time.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1g; cd $S || exit 1
ONLY="xlme" sh mk.sh > mk3.out 2>&1; grep "rc\|link" mk3.out | head -3
sh run.sh lw3 MGS2_NAT_LW=2 MGS2_XP_CHECK=1 MGS2_NGE=1 MGS2_NGE_CHK=1 > run_lw3.out 2>&1; cat bld/rc_lw3.txt
sh pspb.sh > pspb3.out 2>&1; echo "pspb rc $?"; tail -4 pspb3.out
sh rl_np.sh > rl_np3.out 2>&1; echo "rl_np rc $?"; cat rl_np3.out
cp psp_np/EBOOT.PBP psp_np/ovl_*.bin pk_np/ && (cd pk_np && md5sum EBOOT.PBP)
PKG=$S/pk_np sh em_b.sh g1np cfg/nge1.cfg 5400 > em_g1np.out 2>&1; cat em_g1np.out
echo "chain3: done $(date -u +%H:%M:%S)"
