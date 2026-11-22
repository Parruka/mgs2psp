#!/bin/sh
# tbg.sh <d | i | c> <tag> <X> <end vb> <keep>: me13 g (task #147) the take-back's x86 check in the xg twin (mk_g.sh): tbf.sh's run with
# MGS2_XLIVE_TBK=<keep> (bits: 1 the translator's state, 2 the GS map, 4 the live frame's depth window, 8 the texture cache); xg/out_<tag>.log
sh /home/claude/me13/run1g.sh $1 $2 $4 MGS2_XLIVE_DLF=1 MGS2_XLIVE_TB=$3 MGS2_XLIVE_TBK=$5
