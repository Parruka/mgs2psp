#!/bin/sh
# pms.sh <PspProf dump> [steps]: per-step instructions, I-misses, D-misses and the cost-model ms (1.4 cycles per instruction, 70 per miss, 333 MHz)
head -1 $1 | awk -v n=${2:-100} -v f=$1 '{printf "%-36s %7.3f M instr %6.1f k I-miss %6.1f k D-miss = %6.2f ms\n", f, $3/n/1e6, $5/n/1e3, $7/n/1e3, (1.4*$3 + 70*($5+$7))/n/333000}'
