#!/bin/sh
# tbf.sh <d | i | c> <tag> <X> <end vb> <keep>: me13 f (task #147) the take-back's x86 check in the xf twin (mk_f.sh): tbe.sh's run with
# MGS2_XLIVE_TBK=<keep> (bits: 1 the translator's state, 2 the GS map, 4 the live frame's depth window); xf/out_<tag>.log
sh /home/claude/me13/run1f.sh $1 $2 $4 MGS2_XLIVE_DLF=1 MGS2_XLIVE_TB=$3 MGS2_XLIVE_TBK=$5
