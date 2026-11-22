#!/bin/sh
# modelg0r.sh (task #167, rain.c): the bucket model build /home/claude/sbg0m/d rebuilt with H3R and G0R (mkpsg0r.sh) on the indoor route
# with gl17's picture settings, the aft deck in the rain (30,810-30,890), G0H, H3, NBS and G0C on in all three: H3R and G0R off (OPT2
# fffcffdf), H3R on (fffdffdf), H3R and G0R on (ffffffdf); each log's state hashes (every 500 vblanks) must equal the x86 twin's of the
# same mode.  Nice 19.
S=/home/claude/sb10; export GL3=/home/claude/sbg0m; L=/home/claude/sbg0m/modelg0r.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=1 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
echo "$(date +%H:%M:%S) start, bucket $PSPPROF_BUCKET, EBOOT $(md5sum $GL3/d/EBOOT.PBP | cut -c1-8), ovl_w00a $(md5sum $GL3/d/ovl_w00a.bin | cut -c1-8)" >> $L
nice -n 19 sh $S/model.sh rr_off 30810 30890 in $H MGS2_OPT2=fffcffdf &
nice -n 19 sh $S/model.sh rr_h3r 30810 30890 in $H MGS2_OPT2=fffdffdf &
nice -n 19 sh $S/model.sh rr_g0r 30810 30890 in $H MGS2_OPT2=ffffffdf &
wait
echo "$(date +%H:%M:%S) done" >> $L
