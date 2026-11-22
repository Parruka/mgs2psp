#!/bin/sh
# modelh3.sh (task #165, H3): the bucket model build /home/claude/sbg0m/d (sbrk2 + the private nat/ with G0 / G0H, mkpsprg0.sh)
# rebuilt with H3 (mkpsph3.sh) on the indoor route with gl17's picture settings, the aft deck in the rain (30,810-30,890): all off (OPT2
# ffffc7df), G0H only (ffffdfdf), G0H and H3 (ffffffdf).  Nice 19.
S=/home/claude/sb10; export GL3=/home/claude/sbg0m; L=/home/claude/sbg0m/modelh3.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=1 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
echo "$(date +%H:%M:%S) start, bucket $PSPPROF_BUCKET" >> $L
nice -n 19 sh $S/model.sh h3_off 30810 30890 in $H MGS2_OPT2=ffffc7df &
nice -n 19 sh $S/model.sh h3_g0h 30810 30890 in $H MGS2_OPT2=ffffdfdf &
nice -n 19 sh $S/model.sh h3_on 30810 30890 in $H MGS2_OPT2=ffffffdf &
wait
echo "$(date +%H:%M:%S) done" >> $L
