#!/bin/sh
# mk_reduced.sh <out dir> <cov files...>: the PSP-path reduced headers (union coverage) with the poll and idiom passes
# SRC: the regcached headers (default /home/claude/mgs2rc)
O=$1; shift; mkdir -p $O && cd $O || exit 1
sort -u "$@" > cov_u.txt
MG=/mnt/project-files/mgs2
SRC=${SRC:-/home/claude/mgs2rc}
python3 $MG/tools/prune_cov.py $SRC/gen_elf.h cov_u.txt elf gen_elf.h
for s in init r_title n_title r_tnk0 d00t d01t w00a; do python3 $MG/tools/prune_cov.py $SRC/gen_s_$s.h cov_u.txt gen_s_$s.h gen_s_$s.h; done
cp /home/claude/psp_pf/stages.h .
python3 $MG/tools/poll_fast.py gen_*.h | tail -1
for s in init r_title n_title r_tnk0 d00t d01t w00a; do python3 $MG/tools/idiom_apply.py gen_s_$s.h /home/claude/mgs2/stage/$s/nocache/$s.bin; done | grep -c QCOPY
