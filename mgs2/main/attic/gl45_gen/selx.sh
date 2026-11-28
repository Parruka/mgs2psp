#!/bin/sh
# gl45 batch 3: the chosen mix (out/sel_final.json) on the x86 headers, built and run over the whole route.  Results: out/selx.txt
M=/home/claude/main; G=$M/gen/g45; cd /home/claude/g45; mkdir -p $G/x_sel; : > out/selx.txt
python3 -I tools/gen_levers.py $M/gen/wh/gen_elf.h $G/x_sel/gen_elf.h abc out/sel_final.json out/rep_xsel_elf.json >> out/selx.txt 2>&1 || exit 1
python3 -I tools/gen_levers.py $M/gen/wpl/gen_s_w00a.h $G/x_sel/gen_s_w00a.h abc out/sel_final.json out/rep_xsel_w00a.json >> out/selx.txt 2>&1 || exit 1
XCF=-DGEN_LEV sh buildx.sh xsel $G/x_sel/gen_elf.h $G/x_sel/gen_s_w00a.h >> out/selx.txt 2>&1
sh runx.sh xsel boot_xsel >> out/selx.txt 2>&1
echo "selx done" >> out/selx.txt
