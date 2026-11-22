#!/bin/sh
# (task #121's first version, before the straight copy and the leaf path: run with GL3=/home/claude/sb8m; models8n.sh is the final one)
# models8.sh: sb8's model windows (sb8m/d, no PGO): deck 30,810-30,890, inside 33,410-33,490, codec route 31,510-31,590; everything on,
# then eager + mark off (MGS2_OPT=3fffffff: OPT_XL_EAGER 0x40000000, OPT_XL_MARK 0x80000000), three at a time
S=/home/claude/sb8; C="MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2"
sh $S/model.sh s8on 30810 30890 in $C & sh $S/model.sh s8ion 33410 33490 in $C & sh $S/model.sh s8cod 31510 31590 codec $C & wait
sh $S/model.sh s8on0 30810 30890 in $C MGS2_OPT=3fffffff & sh $S/model.sh s8ion0 33410 33490 in $C MGS2_OPT=3fffffff &
sh $S/model.sh s8cod0 31510 31590 codec $C MGS2_OPT=3fffffff & wait
echo models done
