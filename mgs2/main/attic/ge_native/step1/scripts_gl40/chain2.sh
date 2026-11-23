#!/bin/sh
# chain2.sh (st1g): the rest of chain.sh after its job was stopped at its time limit during g1: g1 (on), b0 (gl40's own EBOOT with
# nge0.cfg: the heap and shot A/B against g0), the twin late-walk arm lw (MGS2_NAT_LW=2: the PSP's walk in ge/xlme.c, with gl40's XP
# check and step 1's check mode), then g1b (on again).  One at a time.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st1g; cd $S || exit 1
sh em.sh g1 cfg/nge1.cfg 5400 > em_g1.out 2>&1; cat em_g1.out
PKG=$S/pk40 sh em_b.sh b0 cfg/nge0.cfg 5400 > em_b0.out 2>&1; cat em_b0.out
sh run.sh lw MGS2_NAT_LW=2 MGS2_XP_CHECK=1 MGS2_NGE=1 MGS2_NGE_CHK=1 > run_lw.out 2>&1; cat bld/rc_lw.txt
sh em.sh g1b cfg/nge1.cfg 5400 > em_g1b.out 2>&1; cat em_g1b.out
echo "chain2: done $(date -u +%H:%M:%S)"
