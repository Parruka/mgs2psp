#!/bin/sh
# h3log.sh <batch> <end vb> <MGS2_READ_LOG ranges>: the read-check route on xt3 for H3 (task #165), all off against G0H + H3, with the
# loads of the given vblanks logged one by one (gzipped), then rlz_cmp.py on the pair (out in /home/claude/sbg0/xt3/rlz_<batch>.txt)
b=$1; E=$2; R=$3; XS=f51bb5:f51bb6,13004ee:13004ef; cd /home/claude/sbg0 || exit 1
X=/home/claude/sbg0/xt3 sh runtsz.sh ${b}off ffffc1df,ffffc7df $E $XS MGS2_READ_LOG=$R &
X=/home/claude/sbg0/xt3 sh runtsz.sh ${b}h3 fffff9df,ffffffdf $E $XS MGS2_READ_LOG=$R &
wait
cd /home/claude/sbg0/xt3 && python3 -I /mnt/project-files/mgs2/tools/dev/rlz_cmp.py out_${b}off.log.gz out_${b}h3.log.gz ./boot > rlz_$b.txt 2>&1
echo "$b compared $(date +%H:%M:%S)"; ls -la out_${b}off.log.gz out_${b}h3.log.gz
