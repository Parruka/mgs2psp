#!/bin/sh
# pgo15.sh <build dir> <out dir> <route>...: sequential profile runs (route deck, w01a or free); libgcov merges each run's counts into
# ms0:/pgo (empty it before the first run of a new profile); copies after each run.  deck / w01a: counts from vb 24000 to 25150 (the
# tutorial's timed steps; w01a with the crew-deck redirect).  free (v26): the v25f route (its pad and disc pack), counts from vb 30500 to
# 36490 (the 2 minutes of free play: stand, walk, crawl, punch and fire, first person, the last tour).  in (v30): the indoor free route
# (play/pad_w01af.txt, the pack from psp/v30.sh pak), counts from vb 31800 to 37790
B=$1; O=$2; shift 2
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; P=/home/claude/ms_home/.ppsspp/pgo
export PPSSPP_VFPU_RN=1
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
mkdir -p $O $P
F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
for r in "$@"; do
  G=$GG/MGS2V15P_${r}_$(basename $B); mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv
  cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $G/
  if [ $r = free ]; then
    cp /home/claude/v25f/dl/pad.txt $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
    printf 'clock 333\nMGS2_OPT=ffffffff\nMGS2_HASH_EVERY=500,30500:100\nvblanks 36500\ndeck 30500\npgo 30500 36490\n' > $G/mgs2.cfg
  elif [ $r = in ]; then   # v30: the indoor free route (play/w01a_route.md) and its pack (psp/v30.sh pak); counts from vb 31800 to 37790
    cp /mnt/project-files/mgs2/play/pad_w01af.txt $G/pad.txt; ln -sf /home/claude/v30/pak/mgs2disc.pak $G/mgs2disc.pak
    printf 'clock 333\nMGS2_OPT=ffffffff\nMGS2_HASH_EVERY=500,31800:100\nvblanks 37800\ndeck 31800\npgo 31800 37790\n' > $G/mgs2.cfg
  else
    cp $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
    printf 'clock 333\nMGS2_OPT=ffffffff\nMGS2_HASH_EVERY=500\nvblanks 25160\npgo 24000 25150\n' > $G/mgs2.cfg
  fi
  [ $r = w01a ] && echo 'MGS2_POKE=21000:1190ed0:61313077,21000:1190f78:61313077,21000:1af8118:61313077,21000:1af81c0:61313077,22950:1190ed0:61313077,22950:1190f78:61313077,22950:1af8118:61313077,22950:1af81c0:61313077' >> $G/mgs2.cfg
  sh /mnt/project-files/mgs2/tools/dev/prof_run.sh $G 0x$F $O/prof_$r.txt ${MAXS:-5000} $G/steps.csv > $O/run_$r.out 2>&1
  cp $G/log.txt $O/log_$r.txt; mkdir -p $O/after_$r; cp $P/*.gcda $O/after_$r/
done
echo ALLDONE > $O/done
