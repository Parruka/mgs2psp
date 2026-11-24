#!/bin/sh
# step 2b: the carve after the opening. grun.sh TAG KB2 [more VAR=...]: c3's switches through the PSP's walk (MGS2_NAT_LW=2), check mode
# on, MGS2_NGE_KB2=KB2 (the growth at vb 24,000: the default).  One at a time.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/st2
t=$1; k=$2; shift 2
E8=fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff,fffff9ff
REF=$S/bld/hashref_a3.txt sh $S/run.sh $t MGS2_OPT2=fffff9ff MGS2_OPT2_CYCLE=$E8 MGS2_NGE=2 MGS2_NGE_CHK=1 MGS2_NAT_LW=2 MGS2_NGE_KB2=$k "$@"
