#!/bin/sh
# f7all.sh (task #149 step 2, glue v6: the resize hook grows newlib's top by the shortfall first; the events keep the request): me14p7 on
# the emulator, hw (no profile) and hwmem, and hw with MGS2_ME_HEAP_GROW_KB=0 (each region the request's own size).  Log f7all.log
L=/home/claude/me14p7/f7all.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
G=/mnt/project-files/mgs2/psp/gl3.sh
st start
(cd /home/claude/me14p7 && GL3=/home/claude/me14p7 TAG=f7 NOPROF=1 sh $G emu hw > /home/claude/me14p7/emu_f7.out 2>&1; st "emu hw f7 done") &
(cd /home/claude/me14p7 && GL3=/home/claude/me14p7 TAG=f7 sh $G emu hwmem > /home/claude/me14p7/emu_mf7.out 2>&1; st "emu hwmem f7 done") &
(cd /home/claude/me14p7 && GL3=/home/claude/me14p7 TAG=f7g0 NOPROF=1 EXTRA='MGS2_ME_HEAP_GROW_KB=0' sh $G emu hw > /home/claude/me14p7/emu_f7g0.out 2>&1; st "emu hw f7g0 done") &
wait; st "all done"
