#!/bin/sh
# stepc7_trace.sh prep|sim|emu <build dir> <out dir>: task #133 (Stage C step 1) T2's sizing, tools/dev/sb2/stepc_trace.sh's method on the
# codec route: PspProf with an instruction trace of four 10-step windows in a codec call (vb 31,411, 31,461, 31,511, 31,561; the model's
# window is 31,510-31,590), streamed through a FIFO into stepc7_sim (no trace on disk).  The cfg is the codec model's (gl3.sh model codec:
# prof/hw_gl4c/mgs2_gl4c.cfg, the texture pool 2,688 KB, rain and splash 1/2) without its shots.  Game data passes through it: personal use.
S=$1; B=$2; O=$3; MG=/mnt/project-files/mgs2; GG=/home/claude/ms_home/.ppsspp/PSP/GAME; G=$GG/MGS2SC7_t; H=$(dirname "$0")
export PPSSPP_VFPU_RN=1 PATH=/home/claude/pspsdk/pspdev/bin:$PATH
case $S in
prep)
  mkdir -p $G/memcard0 $O; rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/crash.txt
  cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_codec.txt $G/pad.txt; ln -sf /home/claude/v31/pak/mgs2disc.pak $G/mgs2disc.pak
  cp $MG/prof/hw_gl4c/mgs2_gl4c.cfg $G/mgs2.cfg; cp $MG/prof/hashref_x86_codec_h2.txt $G/hashref.txt
  sed -i '/^MGS2_HASH_EVERY=/d; /^vblanks /d; /^MGS2_GL_SHOT=/d' $G/mgs2.cfg; e=0
  { echo 'MGS2_GL_TEXKB=2688'; echo 'MGS2_HASH_EVERY=500'; echo 'MGS2_GL_RAIN=2'; echo 'MGS2_GL_SPLASH=2'
    for a in 31411 31461 31511 31561; do echo "prof $a $((a + 10)) all"; e=$((a + 20)); done
    echo "vblanks $e"; } >> $G/mgs2.cfg
  PYTHONDONTWRITEBYTECODE=1 python3 -I $H/stepc7_cfg.py $B > $O/stepc.cfg || exit 1
  gcc -O2 -o $O/stepc7_sim $H/stepc7_sim.c || exit 1
  rm -f $O/t.fifo; mkfifo $O/t.fifo; grep -c '^prof' $G/mgs2.cfg ;;
sim)
  $O/stepc7_sim $O/stepc.cfg $O/t.fifo 10 > $O/stepc.out 2>&1; echo done > $O/done_sim ;;
emu)
  F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  PSPPROF_ITRACE=$O/t.fifo PSPPROF_ITRACE_ALL=1 sh $MG/tools/dev/prof_run.sh $G 0x$F $O/t ${MAXS:-2400} $O/t.3.calls > $O/run_t.out 2>&1
  cp $G/log.txt $O/log_t.txt; cp $G/mgs2.cfg $O/cfg_t.txt; echo done > $O/done_emu ;;
*) echo "stepc7_trace.sh: prep | sim | emu <build dir> <out dir>"; exit 1 ;;
esac
