#!/bin/sh
# modelcs2.sh (task #165, charaspread): modelcs.sh repeated (tags cs_off2, cs_on2) to size the run-to-run spread; modelcs.sh: the bucket model build /home/claude/sbg0m/d rebuilt with charaspread's native sets (mkpscs.sh) on
# the indoor route with gl17's picture settings, the aft deck in the rain (30,810-30,890): G0H, H3 and NBS (OPT2 ffff7fdf) against G0H,
# H3, NBS and G0C (ffffffdf); the logs' state hashes (every 500 vblanks) must be equal.  Nice 19.
S=/home/claude/sb10; export GL3=/home/claude/sbg0m; L=/home/claude/sbg0m/modelcs2.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=1 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
echo "$(date +%H:%M:%S) start, bucket $PSPPROF_BUCKET, EBOOT $(md5sum $GL3/d/EBOOT.PBP | cut -c1-8), ovl_w00a $(md5sum $GL3/d/ovl_w00a.bin | cut -c1-8)" >> $L
nice -n 19 sh $S/model.sh cs_off2 30810 30890 in $H MGS2_OPT2=ffff7fdf &
nice -n 19 sh $S/model.sh cs_on2 30810 30890 in $H MGS2_OPT2=ffffffdf &
wait
echo "$(date +%H:%M:%S) done" >> $L
