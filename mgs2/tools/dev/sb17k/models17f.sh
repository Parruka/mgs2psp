#!/bin/sh
# models17f.sh (task #160): per-function profiles of one part each (PSPPROF_BUCKET_ONLY = the part's key, psp/main_psp.c HOST_BUCKET) on the
# bucket build /home/claude/sb17km/d, aft deck in the rain (30,810-30,890), effects as today's plan: the draw pipeline (ff020000), the GS
# walk (ff010000), the picture's frame work (ff040000).  Three at a time.
S=/home/claude/sb10; export GL3=/home/claude/sb17km; L=/home/claude/sb17km/models17k.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=2"; ON="MGS2_OPT2=ffffffdf MGS2_GL_STREAKS=2"
echo "$(date +%H:%M:%S) f start, bucket $PSPPROF_BUCKET" >> $L
PSPPROF_BUCKET_ONLY=ff020000 sh $S/model.sh f_dg 30810 30890 in $H $ON &
PSPPROF_BUCKET_ONLY=ff010000 sh $S/model.sh f_gs 30810 30890 in $H $ON &
PSPPROF_BUCKET_ONLY=ff040000 sh $S/model.sh f_gl 30810 30890 in $H $ON & wait
echo "$(date +%H:%M:%S) f done" >> $L
