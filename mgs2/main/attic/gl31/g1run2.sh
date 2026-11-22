#!/bin/sh
# g1run2.sh (gl31 G1, after the lead's cfg update: arm X on every frame): two twin runs of build_gl31/tw with the PSP cfg's schedule
# (tw_run.sh) and the delivered MGS2_GL_DZ:
#  d0: NAT key absent (the whole-route census of X; against a2 = gl30's twin)
#  d1: MGS2_NAT_CYCLE=0,0,3,3 (the delivered keys): its hashes and digests are the build's references
M=/home/claude/main; R=$M/build_gl31; DZ="MGS2_GL_DZ=0:29489:X,29490:31400:X,31401:999999999:X"
( sh $R/tw_run.sh tw d0 "$DZ" > $R/tw/twin_d0.out 2>&1 ) &
( sh $R/tw_run.sh tw d1 "$DZ" MGS2_NAT_CYCLE=0,0,3,3 > $R/tw/twin_d1.out 2>&1 ) &
wait
