#!/bin/bash
# pspb.sh <tag> <header dir> [J]: plain PSP build (v27plain's flags) into /home/claude/v27/p<tag>, then the free-play model (model1.sh) and
# the emulator hash comparison; results appended to scratchpad/pspb_<tag>.txt
S=/tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad; L=/home/claude/lean; MG=/mnt/project-files/mgs2; t=$1; H=$2
mkdir -p /home/claude/v27/p$t && cp /home/claude/v24/d/genf.txt /home/claude/v27/p$t/ && cd /home/claude/v27 && s=$(date +%s) && \
  RTXX="-DVP_VFPU" VP=/home/claude/psp/vp22 J=${3:-1} sh $MG/tools/dev/mk_psp.sh /home/claude/v27/p$t $H \
  STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a" PSP_EBOOT_TITLE="MGS2 lean $t" > /dev/null 2>&1
echo "psp $t: $(tail -1 /home/claude/v27/p$t/make.log) $(( $(date +%s) - s )) s" >> $S/pspb_$t.txt
s=$(date +%s); sh $L/w2/dev/model1.sh /home/claude/v27/p$t p$t > $S/model_p$t.log 2>&1
echo "model $t: $(cat $S/model_p$t.log) $(( $(date +%s) - s )) s; $(sh $L/w2/dev/emucmp.sh $L/model/log_p$t.txt 2>&1 | tail -2)" >> $S/pspb_$t.txt
echo "pspb $t done" >> $S/pspb_$t.txt
