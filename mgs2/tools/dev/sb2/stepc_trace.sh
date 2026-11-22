#!/bin/sh
# stepc_trace.sh prep|sim|emu <build dir> <out dir>: step c's sizing (renderer_notes 11.24 "Next" 1).  The indoor route in PspProf with an
# instruction trace of eight 10-step windows (the harness's most; aft deck 29,601, 30,101, 30,701, 31,101 in w00a: Snake waiting and
# walking; inside 31,901, 32,651, 33,401, 34,151 in w01a), streamed through a FIFO into stepc_sim, so no trace lands on disk (a 40-step
# trace is ~420 MB).  The run ends 10 vb after the last window and prof_run.sh waits for its dump (PPSSPP left running after the
# harness's end races through idle time and battery.txt grows by tens of MB a minute: 2026-10-30, a full disk).
# prep: the game folder, the cfg, stepc_sim and the FIFO; then sim and emu, each as its own background job (sim first: it waits on the
# FIFO until the emulator opens it).  out: <out dir>/stepc.out, the window dumps t, t.1 .. t.7 (each window's PspProf count, which A's
# misses must equal), log_t.txt
S=$1; B=$2; O=$3; MG=/mnt/project-files/mgs2; GG=/home/claude/ms_home/.ppsspp/PSP/GAME; V=/home/claude/v30; G=$GG/MGS2SC_t
export PPSSPP_VFPU_RN=1 PATH=/home/claude/pspsdk/pspdev/bin:$PATH
case $S in
prep)
  mkdir -p $G/memcard0 $O; rm -f $G/log.txt $G/steps.csv $G/battery.txt $G/crash.txt
  cp $B/EBOOT.PBP $B/ovl_*.bin $G/; cp /home/claude/mgs2/SLES_503.83 $G/
  cp $MG/play/pad_w01af.txt $G/pad.txt; ln -sf $V/pak/mgs2disc.pak $G/mgs2disc.pak
  cp $V/dl_draft/mgs2.cfg $G/mgs2.cfg; cp $MG/prof/hashref_x86_in_h2.txt $G/hashref.txt
  sed -i '/^MGS2_HASH_EVERY=/d; /^vblanks /d' $G/mgs2.cfg; e=0
  { echo 'MGS2_GL_TEXKB=2688'; echo 'MGS2_HASH_EVERY=500'
    for a in 29601 30101 30701 31101 31901 32651 33401 34151; do echo "prof $a $((a + 10)) all"; e=$((a + 20)); done
    echo "vblanks $e"; } >> $G/mgs2.cfg
  PYTHONDONTWRITEBYTECODE=1 python3 $MG/tools/dev/sb2/stepc_cfg.py $B > $O/stepc.cfg || exit 1
  gcc -O2 -o $O/stepc_sim $MG/tools/dev/sb2/stepc_sim.c || exit 1
  rm -f $O/t.fifo; mkfifo $O/t.fifo; grep -c '^prof' $G/mgs2.cfg ;;
sim)
  $O/stepc_sim $O/stepc.cfg $O/t.fifo 10 4 > $O/stepc.out 2>&1; echo done > $O/done_sim ;;
emu)
  F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  PSPPROF_ITRACE=$O/t.fifo PSPPROF_ITRACE_ALL=1 sh $MG/tools/dev/prof_run.sh $G 0x$F $O/t ${MAXS:-1500} $O/t.7.calls > $O/run_t.out 2>&1
  cp $G/log.txt $O/log_t.txt; cp $G/mgs2.cfg $O/cfg_t.txt; echo done > $O/done_emu ;;
*) echo "stepc_trace.sh: prep | sim | emu <build dir> <out dir>"; exit 1 ;;
esac
