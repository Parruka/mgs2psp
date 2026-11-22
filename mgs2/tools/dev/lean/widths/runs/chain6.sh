#!/bin/bash
# after wpd_all6 (exact memory, regcache with RC_CONDCLEAN): statistics, hybrid widths, proof headers + set check + x86 + routes;
# hybrid headers + set check + x86 + routes.  Headers with the dev regcache (RCPY) and RC_CONDCLEAN=1.
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; L=/home/claude/lean
until grep -q '^exit' $S/wpd_all6.log 2>/dev/null; do sleep 5; done
grep -q '^exit 0' $S/wpd_all6.log || { echo "checker failed" > $S/chain6.txt; exit 1; }
export RCPY=$L/rcfix/regcache.py RC_CONDCLEAN=1
cd $L/w2
python3 tools/wstat.py wpd_all6.json elf w00a > $S/wstat_all6.txt 2>&1 &
python3 dev/hybrid.py wpd_all6.json $L/w3/wpw_hyb6.json > $S/hyb6.txt 2>&1
s=$(date +%s); sh dev/mkh_wpw.sh wpd_all6.json $L/hW6 > $S/mkh_w6.log 2>&1; echo "proof headers rc $? $(( $(date +%s) - s )) s" >> $S/chain6.txt
python3 dev/setcmp.py $L/hP $L/hW6 > $S/setcmp_w6.txt 2>&1
s=$(date +%s); sh /mnt/project-files/mgs2/tools/dev/x86_ref.sh $L/hW6 $L/xW6 > $S/x86_w6.log 2>&1; echo "proof x86 $(( $(date +%s) - s )) s" >> $S/chain6.txt
( s=$(date +%s); J=2 sh dev/mkh_wpw.sh $L/w3/wpw_hyb6.json $L/hH6 > $S/mkh_h6.log 2>&1; echo "hybrid headers rc $? $(( $(date +%s) - s )) s" >> $S/chain6.txt ) &
sh $L/xrun27.sh $L/xW6 > $S/xrun_w6.txt 2>&1; echo "proof routes done" >> $S/chain6.txt
wait
python3 dev/setcmp.py $L/hP $L/hH6 > $S/setcmp_h6.txt 2>&1
sh /mnt/project-files/mgs2/tools/dev/x86_ref.sh $L/hH6 $L/xH6 > $S/x86_h6.log 2>&1
sh $L/xrun27.sh $L/xH6 > $S/xrun_h6.txt 2>&1; echo "hybrid routes done" >> $S/chain6.txt
echo chain6 done >> $S/chain6.txt
