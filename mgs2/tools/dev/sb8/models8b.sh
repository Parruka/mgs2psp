#!/bin/sh
# models8b.sh: sb8's model windows on sb8b/d (no PGO; the eager straight copy + the inline marking, the final form): deck 30,810-30,890, inside
# 33,410-33,490, codec route 31,510-31,590.  Everything on; on the deck also eager off (MGS2_OPT=bfffffff: OPT_XL_EAGER 0x40000000),
# mark off (7fffffff: OPT_XL_MARK 0x80000000) and both off (3fffffff); inside and codec both off.  Three at a time.
S=/home/claude/sb8; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"
GL3=/home/claude/sb8b sh $S/model.sh b8on 30810 30890 in $C & GL3=/home/claude/sb8b sh $S/model.sh b8ion 33410 33490 in $C & GL3=/home/claude/sb8b sh $S/model.sh b8cod 31510 31590 codec $C & wait
GL3=/home/claude/sb8b sh $S/model.sh b8off 30810 30890 in $C MGS2_OPT=3fffffff & GL3=/home/claude/sb8b sh $S/model.sh b8ioff 33410 33490 in $C MGS2_OPT=3fffffff &
GL3=/home/claude/sb8b sh $S/model.sh b8coff 31510 31590 codec $C MGS2_OPT=3fffffff & wait
GL3=/home/claude/sb8b sh $S/model.sh b8e0 30810 30890 in $C MGS2_OPT=bfffffff & GL3=/home/claude/sb8b sh $S/model.sh b8m0 30810 30890 in $C MGS2_OPT=7fffffff & wait
echo models done
