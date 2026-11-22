#!/bin/sh
# f6all.sh (task #149 step 2, pools v4: the resize with a free block after the realloc's): me14p6 on the emulator (hw and hwmem, the
# delivery's cfg with the build's heap defaults: 768 KB, regions of 32 KB at least, resize on), the model's deck and inside windows of
# me14p6 (to compare with sb14m2's c3wd1 / c3wi1: gl16's code, the same model build without the ME-safe files), and the final x86
# checks (me14/runs_f.sh).  Log /home/claude/me14p6/f6all.log
L=/home/claude/me14p6/f6all.log; st() { echo "$(date -u +%H:%M:%S) $*" >> $L; }
G=/mnt/project-files/mgs2/psp/gl3.sh
st start
(cd /home/claude/me14p6 && GL3=/home/claude/me14p6 TAG=f6 NOPROF=1 sh $G emu hw > /home/claude/me14p6/emu_f6.out 2>&1; st "emu hw f6 done") &
(cd /home/claude/me14p6 && GL3=/home/claude/me14p6 TAG=f6 sh $G emu hwmem > /home/claude/me14p6/emu_mf6.out 2>&1; st "emu hwmem f6 done") &
(GL3=/home/claude/me14p6 sh /home/claude/sb10/model.sh m6d 30810 30890 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 > /home/claude/me14p6/model_m6d.out 2>&1; st "model deck done") &
(GL3=/home/claude/me14p6 sh /home/claude/sb10/model.sh m6i 33410 33490 in MGS2_GL_RAIN=2 MGS2_GL_SPLASH=2 > /home/claude/me14p6/model_m6i.out 2>&1; st "model inside done") &
(sh /home/claude/me14/runs_f.sh; st "x86 runs_f done") &
wait; st "all done"
