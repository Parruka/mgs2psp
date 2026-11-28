#!/bin/sh
# gl45 batch 3: the x86 twins of each lever alone, all three, and the fallback test; each over the whole route.  Results: out/batch_x.txt
M=/home/claude/main; G=$M/gen/g45; cd /home/claude/g45; T=tools/gen_levers.py; : > out/batch_x.txt
for v in a b c abc; do
  mkdir -p $G/x_$v
  python3 -I $T $M/gen/wh/gen_elf.h $G/x_$v/gen_elf.h $v all out/rep_x${v}_elf.json >> out/batch_x.txt 2>&1
  python3 -I $T $M/gen/wpl/gen_s_w00a.h $G/x_$v/gen_s_w00a.h $v all out/rep_x${v}_w00a.json >> out/batch_x.txt 2>&1
done
for v in a b c abc; do XCF=-DGEN_LEV sh buildx.sh x$v $G/x_$v/gen_elf.h $G/x_$v/gen_s_w00a.h >> out/batch_x.txt 2>&1; done
XCF="-DGEN_LEV -DGEN_LEV_FBTEST=3" sh buildx.sh xfb $G/x_abc/gen_elf.h $G/x_abc/gen_s_w00a.h /home/claude/g45/out/g45fb.o >> out/batch_x.txt 2>&1
( sh runx.sh xa boot_xa >> out/batch_x.txt 2>&1 ) & ( sh runx.sh xb boot_xb >> out/batch_x.txt 2>&1 ) & wait
( sh runx.sh xc boot_xc >> out/batch_x.txt 2>&1 ) & ( sh runx.sh xabc boot_xabc >> out/batch_x.txt 2>&1 ) & wait
sh runx.sh xfb boot_xfb >> out/batch_x.txt 2>&1
echo "batch_x done" >> out/batch_x.txt
