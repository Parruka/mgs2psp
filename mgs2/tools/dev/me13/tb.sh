#!/bin/sh
# tb.sh <d | i | c> <tag> <X> <end vb>: me13 d (task #147) the take-back's x86 check: the split as in runs.sh (the deadline copy, the filtered
# guard log, the late walk with the lanes) with the picture started afresh at the first frame from vb X (MGS2_XLIVE_TB=X: a new GS map and
# texture cache, the live frame and the GE list set up again, the translator reset), as the main CPU does when it takes the picture back
# from a stopped ME; xd/out_<tag>.log.  (tb_v1.sh started the translator cold with today's walk, which MGS2_XLIVE_GX does not allow.)
sh /home/claude/me13/run1.sh $1 $2 $4 MGS2_XLIVE_DLF=1 MGS2_XLIVE_TB=$3
