#!/bin/sh
# h3rlog.sh <batch> <end vb> <MGS2_READ_LOG ranges>: the read-check route on xt5 for H3R (task #167), H3R off against on (G0H, H3, NBS and
# G0C on in both), the given vblanks logged load by load (gzipped), then rlz_cmp.py on the pair (out in /home/claude/sbg0/xt5/rlz_<batch>.txt)
b=$1; E=$2; R=$3; cd /home/claude/sbg0 || exit 1
X=/home/claude/sbg0/xt5 sh runtsz.sh ${b}off fffef9df,fffeffdf $E 1:2 MGS2_READ_LOG=$R &
X=/home/claude/sbg0/xt5 sh runtsz.sh ${b}on fffff9df,ffffffdf $E 1:2 MGS2_READ_LOG=$R &
wait
cd /home/claude/sbg0/xt5 && python3 -I /mnt/project-files/mgs2/tools/dev/rlz_cmp.py out_${b}off.log.gz out_${b}on.log.gz ./boot > rlz_$b.txt 2>&1
echo "$b compared $(date +%H:%M:%S)"; ls -la out_${b}off.log.gz out_${b}on.log.gz
