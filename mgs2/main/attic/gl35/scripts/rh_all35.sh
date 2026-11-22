#!/bin/sh
# rh_all35.sh (gl35 G2): the read-hash runs to vb 31,400: base (no NAT key), native + in place + fast in every turn (gl35's no-keep arm =
# gl34's rf), the same + the keeps in every turn (rk: MGS2_NAT_KEEP=15), the same + the keeps by turns (rkc: the delivered keys)
cd /home/claude/main/build_gl35 || exit 1
sh rh35.sh rb 31400 &
sh rh35.sh rf 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 &
wait
sh rh35.sh rk 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP=15 &
sh rh35.sh rkc 31400 MGS2_NAT_CYCLE=3,3,3,3 MGS2_NAT_INPL=1 MGS2_NAT_FAST=1 MGS2_NAT_KEEP_CYCLE=15,15,0,0 &
wait
echo done
