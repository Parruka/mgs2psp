#!/bin/sh
# rh_all33.sh (gl33 G2): the read-hash runs to vb 31,400: base (no NAT key), native every turn, native + the in-place key by turns,
# native + the in-place key in every turn
cd /home/claude/main/build_gl33 || exit 1
sh rh33.sh rb 31400 &
sh rh33.sh rn 31400 MGS2_NAT_CYCLE=3,3,3,3 &
sh rh33.sh ri 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL_CYCLE=0,0,1,1 &
wait
sh rh33.sh r1 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1
echo done
