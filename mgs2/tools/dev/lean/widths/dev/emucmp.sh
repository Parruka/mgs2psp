#!/bin/sh
# emucmp.sh <model log>: the emulator run's hash lines (every 500 vblanks) against prof/hashes_x86_free_v27.txt
grep -a '^hash: vb' $1 | awk '{print $3, $5, $7, $9}' > /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/emu_$$.txt
awk 'NR == FNR {r[$1] = $0; next} ($1 in r) {n++; if (r[$1] == $0) e++; else if (!f) f = $0 " (want " r[$1] ")"} END {printf "%d / %d hash lines equal to the x86 free reference; first difference: %s\n", e, n, f ? f : "none"}' \
  /mnt/project-files/mgs2/prof/hashes_x86_free_v27.txt /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/emu_$$.txt
