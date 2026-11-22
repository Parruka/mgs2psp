#!/bin/sh
# f8all.sh (task #149 step 2, glue v7: after a resize of a region that ended at newlib's top chunk, the free top is given back at once):
# me14p8 on the emulator, hw (no profile) and hwmem, and hwmem with MGS2_ME_HEAP_GROW_KB=0 (each region the request's own size).  Log f8all.log
L=/home/claude/me14p8/f8all.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
G=/mnt/project-files/mgs2/psp/gl3.sh
st start
(cd /home/claude/me14p8 && GL3=/home/claude/me14p8 TAG=f8 NOPROF=1 sh $G emu hw > /home/claude/me14p8/emu_f8.out 2>&1; st "emu hw f8 done") &
(cd /home/claude/me14p8 && GL3=/home/claude/me14p8 TAG=f8 sh $G emu hwmem > /home/claude/me14p8/emu_mf8.out 2>&1; st "emu hwmem f8 done") &
(cd /home/claude/me14p8 && GL3=/home/claude/me14p8 TAG=f8g0 EXTRA='MGS2_ME_HEAP_GROW_KB=0' sh $G emu hwmem > /home/claude/me14p8/emu_mf8g0.out 2>&1; st "emu hwmem f8g0 done") &
wait; st "all done"
