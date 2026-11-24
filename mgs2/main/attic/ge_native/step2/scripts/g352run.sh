#!/bin/sh
# step 2: the carve's size.  g352: MGS2_NGE=2 for the whole run (ship + people native, people from 24,100), regions 256 KB until vb 24,000,
# then 352 KB + the people's 76 KB (carve 428 KB: frame data room 1696 - 264 - 428 = 1004 KB against kc's 939 KB nothing-native peak),
# C2 on, the PSP's walk, check mode.  REF: g416's.  After kn.sh (one at a time).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st2
E8=fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff
REF=$S/bld/hashref_g416.txt sh $S/run.sh g352 MGS2_OPT2=fffff9ff MGS2_OPT2_CYCLE=$E8 MGS2_NGE=2 MGS2_NGE_CHK=1 MGS2_NAT_LW=2 MGS2_NGE_KB2=352
