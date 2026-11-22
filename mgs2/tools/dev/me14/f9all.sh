#!/bin/sh
# f9all.sh (task #149 step 2, glue v8: each region the request's own size by default, ME_HEAP_GROW_KB 0; v7's trim after a resize at
# newlib's top): me14p9 on the emulator, hw (no profile) and hwmem.  Log f9all.log
L=/home/claude/me14p9/f9all.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
G=/mnt/project-files/mgs2/psp/gl3.sh
st start
(cd /home/claude/me14p9 && GL3=/home/claude/me14p9 TAG=f9 NOPROF=1 sh $G emu hw > /home/claude/me14p9/emu_f9.out 2>&1; st "emu hw f9 done") &
(cd /home/claude/me14p9 && GL3=/home/claude/me14p9 TAG=f9 sh $G emu hwmem > /home/claude/me14p9/emu_mf9.out 2>&1; st "emu hwmem f9 done") &
wait; st "all done"
