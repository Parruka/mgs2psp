#!/bin/bash
# after chain10's headers: the hybrid hyb7 as a plain PSP build, model and emulator hashes (pspb.sh)
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad
until grep -q '^headers' $S/chain10.txt 2>/dev/null; do sleep 15; done
grep -q '^headers rc 0' $S/chain10.txt || exit 1
bash $S/pspb.sh H7 /home/claude/lean/hH7 1
