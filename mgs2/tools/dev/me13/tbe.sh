#!/bin/sh
# tbe.sh <d | i | c> <tag> <X> <end vb> <keep>: me13 e (task #147) the take-back's x86 check in the xe twin (mk_e.sh): tb.sh's run with
# MGS2_XLIVE_TBK=<keep> (0 nothing kept, 1 the translator's state, 2 the GS map, 3 both); xe/out_<tag>.log
sh /home/claude/me13/run1e.sh $1 $2 $4 MGS2_XLIVE_DLF=1 MGS2_XLIVE_TB=$3 MGS2_XLIVE_TBK=$5
