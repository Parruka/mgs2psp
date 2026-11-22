#!/bin/bash
# after the narrow-in checker: hybrid widths hyb7 (proof in v27's 64-bit functions, narrow-in in its 32-bit ones), headers, x86, routes
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; L=/home/claude/lean
until grep -q '^exit' $S/wpd_nin6.log 2>/dev/null; do sleep 10; done
grep -q '^exit 0' $S/wpd_nin6.log || { echo "checker failed" >> $S/chain10.txt; exit 1; }
cd $L/w2 && python3 dev/hybrid2.py wpd_all6.json wpd_nin6.json $L/w3/wpw_hyb7.json > $S/hyb7.txt 2>&1
export RCPY=$L/rcfix/regcache.py RC_CONDCLEAN=1
s=$(date +%s); J=${J:-2} sh dev/mkh_wpw.sh $L/w3/wpw_hyb7.json $L/hH7 > $S/mkh_h7.log 2>&1; echo "headers rc $? $(( $(date +%s) - s )) s" >> $S/chain10.txt
unset RCPY RC_CONDCLEAN
python3 dev/setcmp.py $L/hH6 $L/hH7 > $S/setcmp_h7.txt 2>&1
s=$(date +%s); sh /mnt/project-files/mgs2/tools/dev/x86_ref.sh $L/hH7 $L/xH7 > $S/x86_h7.log 2>&1; echo "x86 $(( $(date +%s) - s )) s" >> $S/chain10.txt
sh $L/xrun27.sh $L/xH7 > $S/xrun_h7.txt 2>&1
echo chain10 done >> $S/chain10.txt
