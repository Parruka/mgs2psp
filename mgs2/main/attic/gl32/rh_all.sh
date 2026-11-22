#!/bin/sh
# rh_all.sh (gl32 G2): the three read-hash runs to vb 31,400, one after the other: base (no NAT key), native every turn, native + the vertex cache keys
cd /home/claude/main/build_gl32 || exit 1
sh rh32.sh rb 31400
sh rh32.sh rn 31400 MGS2_NAT_CYCLE=3,3,3,3
sh rh32.sh rv 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_VC_CYCLE=0,0,1,1,3,3 MGS2_NAT_VC_KB=256
