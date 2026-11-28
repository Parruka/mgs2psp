#!/bin/sh
# gl45 batch 3: a selection on the x86 headers, built and run over the whole route.  usage: selx2.sh <tag> <selection.json>.  Results: out/selx_<tag>.txt
M=/home/claude/main; G=$M/gen/g45; cd /home/claude/g45; t=$1; s=$2; mkdir -p $G/x_$t; : > out/selx_$t.txt
python3 -I tools/gen_levers.py $M/gen/wh/gen_elf.h $G/x_$t/gen_elf.h abc $s out/rep_x${t}_elf.json >> out/selx_$t.txt 2>&1 || exit 1
python3 -I tools/gen_levers.py $M/gen/wpl/gen_s_w00a.h $G/x_$t/gen_s_w00a.h abc $s out/rep_x${t}_w00a.json >> out/selx_$t.txt 2>&1 || exit 1
XCF=-DGEN_LEV sh buildx.sh x$t $G/x_$t/gen_elf.h $G/x_$t/gen_s_w00a.h >> out/selx_$t.txt 2>&1
sh runx.sh x$t boot_x$t >> out/selx_$t.txt 2>&1
echo "selx $t done" >> out/selx_$t.txt
