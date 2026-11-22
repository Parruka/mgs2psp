#!/bin/sh
# chk4d.sh <run name, e.g. hwme18e1>: hash lines vs prof/hashref_x86_in_gl18.txt, vuh lines vs hwme18bn, frames vs hwme18bn (piccmp.sh)
R=$1; L=/home/claude/me18e/emuw/emu/log_$R.txt; S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad
[ -f "$L" ] || { echo "no log $L"; exit 1; }
grep -a '^hash: vb' "$L" | awk 'NR == FNR {r[$1] = $2; next} {n++; if (!($3 in r)) {m++; next} if (r[$3] == $5) e++; else {d++; if (d <= 3) print "hash differs: " $0 " want " r[$3]}} END {printf "hash lines %d: %d equal, %d differ, %d not in the reference\n", n, e, d, m}' /mnt/project-files/mgs2/prof/hashref_x86_in_gl18.txt -
grep -a '^vuh: vb' "$L" | awk 'NR == FNR {r[$3] = $4; next} {n++; if (!($3 in r)) {m++; next} if (r[$3] == $4) e++; else {d++; if (d <= 3) print "vuh differs: " $0 " want " r[$3]}} END {printf "vuh lines %d: %d equal, %d differ, %d not in the reference\n", n, e, d, m}' $S/me18e/vuh_bn.txt -
sh /home/claude/me18d/tools/piccmp.sh "$L" $S/pv_hwme18bn.txt
grep -a '^hash: vb' "$L" | tail -1 | cut -c1-60
grep -a -c "^split: vb\|^SPLIT" "$L"
