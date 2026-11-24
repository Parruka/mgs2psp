#!/bin/sh
# step 2: the turn words' twin hashrefs.  Blocks of 5 turns of 100 vblanks from vb 24,000 (25 blocks, 125 turns, to vb 36,500):
# K-A (MGS2_NGE_CYCLE digit 0), K-N1 (1), K-N2 (2), K-N2R (2 + bound kind 9), K7 (0 + bound kind 7); C2 on in every turn; the carve
# after the opening (KB2 416).  kc: the NGE cycle alone (its state hash: hashref_kc); kr: the same with MGS2_BOUND_CYCLE, against
# hashref_kc (the bound kinds are picture only).  Through the PSP's walk (MGS2_NAT_LW=2).  One after the other.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st2
E8=fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff
NG=01220012200122001220012200122001220012200122001220012200122001220012200122001220012200122001220012200122001220012200122001220
BD=00097000970009700097000970009700097000970009700097000970009700097000970009700097000970009700097000970009700097000970009700097
REF=/home/claude/main/build_gl39/cfg/hashref_gl38.txt sh $S/run.sh kc MGS2_OPT2=fffff9ff MGS2_OPT2_CYCLE=$E8 MGS2_NGE_CYCLE=$NG MGS2_NAT_LW=2 MGS2_NGE_KB2=416
REF=$S/bld/hashref_kc.txt sh $S/run.sh kr MGS2_OPT2=fffff9ff MGS2_OPT2_CYCLE=$E8 MGS2_NGE_CYCLE=$NG MGS2_BOUND_CYCLE=$BD MGS2_NAT_LW=2 MGS2_NGE_KB2=416
