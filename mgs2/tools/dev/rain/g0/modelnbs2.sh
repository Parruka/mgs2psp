#!/bin/sh
# modelnbs2.sh (task #165, body_sph): the null pair for the picture side's spread: modelnbs.sh's nbs_on (OPT2 ffffffdf) run again on the
# same build (its first outputs renamed nbs_on1); nbs_on1 against nbs_on = the same build, words and game.  Nice 19.
S=/home/claude/sb10; export GL3=/home/claude/sbg0m; L=/home/claude/sbg0m/modelnbs.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=1 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0"
echo "$(date +%H:%M:%S) null run start, bucket $PSPPROF_BUCKET, EBOOT $(md5sum $GL3/d/EBOOT.PBP | cut -c1-8), ovl_w00a $(md5sum $GL3/d/ovl_w00a.bin | cut -c1-8)" >> $L
nice -n 19 sh $S/model.sh nbs_on 30810 30890 in $H MGS2_OPT2=ffffffdf
echo "$(date +%H:%M:%S) null run done" >> $L
