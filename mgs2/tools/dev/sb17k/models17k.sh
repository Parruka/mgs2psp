#!/bin/sh
# models17k.sh (task #160): the bucket model build /home/claude/sb17km/d (mkpsp17k.sh) on the indoor route: the aft deck in the rain
# (30,810-30,890) with the effects as today's plan and with them off, the deck while Snake waits (29,810-29,890) with them on, and inside
# (33,410-33,490) with them on.  PSPPROF_BUCKET = g_emuactor.  Four at a time.
S=/home/claude/sb10; export GL3=/home/claude/sb17km; L=/home/claude/sb17km/models17k.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=2"; ON="MGS2_OPT2=ffffffdf MGS2_GL_STREAKS=2"; OFF=MGS2_OPT2=fffff9df
echo "$(date +%H:%M:%S) start, bucket $PSPPROF_BUCKET" >> $L
sh $S/model.sh k_now 30810 30890 in $H $ON &
sh $S/model.sh k_off 30810 30890 in $H $OFF &
sh $S/model.sh k_wait 29810 29890 in $H $ON &
sh $S/model.sh k_in 33410 33490 in $H $ON & wait
echo "$(date +%H:%M:%S) done" >> $L
