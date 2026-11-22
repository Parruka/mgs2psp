#!/bin/sh
# rh_all34.sh (gl34 G2): the read-hash runs to vb 31,400: base (no NAT key), native + in place in every turn (gl34's plain = gl33's
# r1), the same + the fast key in every turn, the same + the fast key by turns (the delivered keys)
cd /home/claude/main/build_gl34 || exit 1
sh rh34.sh rb 31400 &
sh rh34.sh r1 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 &
sh rh34.sh rf 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 &
wait
sh rh34.sh rfc 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST_CYCLE=1,1,0,0
echo done
