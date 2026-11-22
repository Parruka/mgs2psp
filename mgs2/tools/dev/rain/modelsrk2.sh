#!/bin/sh
# modelsrk.sh (task #164, the rain study): the bucket model build /home/claude/sbrkm/d (sbrk = sb17k + the kill switch MGS2_KILL_ACT) on
# the indoor route with gl17's picture settings (OPT2 ffffffdf = gl17's "on" turn, no streaks, drop sheets whole, the rain's sprites left
# out of the picture): the aft deck in the rain (30,810-30,890) as gl17 runs and with
# the four sprite actors of the rain destroyed on the deck (16,000-31,499: line_sph, body_sph, charaspread, rain_gas_pers).  Nice 19: they
# take only the CPU gl17's build and checks leave.
S=/home/claude/sb10; export GL3=/home/claude/sbrkm2; L=/home/claude/sbrkm2/modelsrk2.log
export PSPPROF_BUCKET=$(/home/claude/pspsdk/pspdev/bin/psp-nm $GL3/d/MGS2T.elf | awk '$3=="g_emuactor"{print $1}')
H="MGS2_GL_SPLASH=2 MGS2_GL_RAIN=1 MGS2_GL_STREAKS=0 MGS2_GL_RAIN_SPRITES=0 MGS2_OPT2=ffffffdf"
K="MGS2_KILL_ACT=16000-31499:line_sph.c,body_sph.c,charaspread.c,rain_gas_pers.c"
echo "$(date +%H:%M:%S) start, bucket $PSPPROF_BUCKET" >> $L
nice -n 19 sh $S/model.sh r2_g17 30810 30890 in $H &
nice -n 19 sh $S/model.sh r2_kill 30810 30890 in $H $K &
wait
echo "$(date +%H:%M:%S) done" >> $L
