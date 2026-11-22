#!/bin/sh
# pgochk.sh: why the guarded units' functions moved out of .text.hot (task #180 4b-2): (1) w01a, gen.o and w00a guarded, compiled
# without -w (with -Wno-error=coverage-mismatch) so GCC names each function whose profile it drops; (2) w01a unguarded with mkgen18c's
# flags, to see if a fresh compile gives the base's object.  Objects and gcda copies in /dev/shm/pgochk (removed by hand after).
E=/home/claude/me18c; B=$E/b1; O=/dev/shm/pgochk
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
mkdir -p $O/w $O/r
cp $B/gen.gcda $B/gen_s_w00a.gcda $B/gen_s_w01a.gcda $O/w/; cp $B/gen_s_w01a.gcda $O/r/
cd $B
PUSE="-fprofile-use -fprofile-partial-training -Wno-missing-profile -fno-unroll-loops -fno-peel-loops -fno-tracer -fno-ipa-cp-clone -fno-unswitch-loops -fno-split-loops -fno-predictive-commoning"
GENF="$(cat genf.txt)"; GENFW=$(echo "$GENF" | sed 's/ -w / /')
cw() {  # <object> <-D...> <source>: warnings kept, filtered
  eval "psp-gcc $GENFW -Wno-error=coverage-mismatch $PUSE -ffunction-sections $2 -c $3 -o $O/w/$1" 2>&1 | grep -i "profile\|control flow\|counters\|checksum\|source locations" > $O/warn_$1.txt
  echo "$1 done $(grep -c '' $O/warn_$1.txt) lines" >> $O/status.txt
}
cw gen_s_w01a.o "-DSTAGE_GEN='\"$E/wg/gen_s_w01a.h\"'" $E/boot/gen_stage.c &
cw gen.o "-DGEN='\"$E/wg/gen_elf.h\"'" $E/boot/gen.c &
cw gen_s_w00a.o "-DSTAGE_GEN='\"$E/wg/gen_s_w00a.h\"'" $E/boot/gen_stage.c &
( eval "psp-gcc $GENF $PUSE -ffunction-sections -DSTAGE_GEN='\"/home/claude/v31/WPL/gen_s_w01a.h\"' -c /mnt/project-files/mgs2/boot/gen_stage.c -o $O/r/gen_s_w01a.o" > $O/r/cc.log 2>&1; echo "ref w01a rc $?" >> $O/status.txt ) &
wait
echo "all done" >> $O/status.txt
