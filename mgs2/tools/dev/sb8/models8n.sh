#!/bin/sh
# models8n.sh: sb8's model windows on sb8n/d (no PGO; the eager straight copy + the lean ud_new path): deck 30,810-30,890, inside
# 33,410-33,490, codec route 31,510-31,590.  Everything on; on the deck also eager off (MGS2_OPT=bfffffff: OPT_XL_EAGER 0x40000000),
# mark off (7fffffff: OPT_XL_MARK 0x80000000, the lean path) and both off (3fffffff); inside and codec both off.  Three at a time.
S=/home/claude/sb8; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"
export GL3=/home/claude/sb8n   # model.sh's default is now sb8b (the final form)
sh $S/model.sh n8on 30810 30890 in $C & sh $S/model.sh n8ion 33410 33490 in $C & sh $S/model.sh n8cod 31510 31590 codec $C & wait
sh $S/model.sh n8off 30810 30890 in $C MGS2_OPT=3fffffff & sh $S/model.sh n8ioff 33410 33490 in $C MGS2_OPT=3fffffff &
sh $S/model.sh n8coff 31510 31590 codec $C MGS2_OPT=3fffffff & wait
sh $S/model.sh n8e0 30810 30890 in $C MGS2_OPT=bfffffff & sh $S/model.sh n8m0 30810 30890 in $C MGS2_OPT=7fffffff & wait
echo models done
