#!/bin/bash
# all-64 reference, second try: RC_WIDE=1 turns the ELF's inlining off, so the functions v27 inlines are called and must not be pruned:
# coverage = cov_x25 + the 326 ELF functions inlined in v27 (w3/cov_a64.txt).  Starts after chain9's headers (CPU).
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; L=/home/claude/lean
until grep -q '^headers' $S/chain9.txt 2>/dev/null; do sleep 15; done
rm -rf /home/claude/lean/hA64 /home/claude/lean/xA64
export RCPY=/mnt/project-files/mgs2/recomp/regcache.py RC_CONDCLEAN=1 RC_WIDE=1 COV=$L/w3/cov_a64.txt
s=$(date +%s); cd $L/w2 && J=${J:-1} sh dev/mkh_wpw.sh $L/w3/wpw_empty.json $L/hA64 > $S/mkh_a64.log 2>&1; echo "headers rc $? $(( $(date +%s) - s )) s" >> $S/chain12.txt
unset RC_WIDE RC_CONDCLEAN RCPY COV
s=$(date +%s); sh /mnt/project-files/mgs2/tools/dev/x86_ref.sh $L/hA64 $L/xA64 > $S/x86_a64.log 2>&1; echo "x86 $(( $(date +%s) - s )) s" >> $S/chain12.txt
sh $L/xrun27.sh $L/xA64 > $S/xrun_a64.txt 2>&1
echo chain12 done >> $S/chain12.txt
