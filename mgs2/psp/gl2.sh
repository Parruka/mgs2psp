#!/bin/sh
# gl2.sh <step> [<args>]: MGS2T_gl2 = gl1 (below) with textures converted a GS block at a time (2026-10-27, task #59: ge/texconv.c
# tc_texture_blk, ge/gsup2.c gu_block, ge/texcache.c tk_blk; the same bytes as gl1's texel by texel, checked on x86 with
# MGS2_XLIVE_TKBLK=2); g_opt OPT_TK_BLK (16384, runtime/host_time.h) off = gl1's way (cfg 16 in the timing windows).  gl1.sh's text:
# MGS2T_gl1, the live picture (2026-10-27; renderer_notes 11.13 "3e", task #56): xl5 (psp/xl5.sh: the one walk,
# shown frames only) with psp/xl_psp.c and main_psp.c built with -DMGS2_GL: each shown step's transfers become a frame of GE batches
# (ge/gelive.c; textures from ge/texcache.c in a pool of their own), its GE list (ge/gelist.c) is drawn by the GE into one of two frame
# buffers while the game runs on, and a frame the GE has drawn is shown (sceDisplaySetFrameBuf); the harness's text is on a third buffer.
# main_psp.c times the picture's work (HOST_GL) and the waits for the GE (HOST_GW): the log's windows and summary, steps.csv gl_us and
# gw_us.  The game state is not touched: the hashes are xl5's (prof/hashref_x86_*_h2.txt).  ME off, as xl5.  For the first runs on the
# PSP (2026-10-27) the build also has the Media Engine builds' crash catcher (psp/v26me4_exc.c / .S with -DMGS2_GL_EXC: the record goes
# on the frame on the display) and battery log (psp/v26me_bat.c, as v30me6's flags without the ME's), joined by psp/gl1_glue.c and the
# link's wraps in place of psp/me_mix.c; mgs2.cfg MGS2_EXC=1 turns the catcher on (the emulator keeps it off).  Every wait for the GE
# gives up after 2 s (psp/xl_psp.c gl_sync).  Steps:
#   d                  the build (h2's objects, as xl5's d)
#   emu [runs...]      PPSSPP runs (software GE): in, deck (each with its digest window and shots, below), incyc (the picture off and
#                      the GE overlap off take turns with all on, every 100 steps from 31,800), insync (the GE overlap off throughout),
#                      hw (the PSP delivery's mgs2.cfg: $X/dl_cfg.txt appended to the indoor route's)
#   model <tag> <a> <b> <mode> [route] [cfg lines...]   as xl5's (mode: xl, xg, gl, xgl or all)
# The digests (MGS2_GL_DIG) are compared with boot/xlive_chk.c MGS2_XLIVE_GX on the same route from boot (tools/dev/gl1_x86.sh), outside
# the profiled step (PPSSPP's interpreter there is not exact: one shown step after each digest window, its PspProf dump x_<run>); each run
# goes on to the route's end (steps.csv written), so the log has the totals and the memory lines; the shots (MGS2_GL_SHOT,
# shot_<vb>.bmp next to the EBOOT) are copied to $X/emu/shots_<run>.  Work dir $GL2 or /home/claude/gl2; game folders MGS2GL2*.  Game
# data passes through all of it: personal use only.
S=$1; X=${GL2:-/home/claude/gl2}; MG=/mnt/project-files/mgs2; T=$MG/tools
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; V=/home/claude/v30
DIG_IN=37601:37789; SHOT_IN=32000,33500,35000,36500,37790
DIG_DECK=25001:25189; SHOT_DECK=24100,24600,25000,25190
# the game folder <dir> for route <in | deck> with build <build dir>'s files, the route's hashref and <cfg lines> appended to the route's
# cfg (its own variable names: the callers' G and r stay as they are)
setup_game() {
  sg_g=$1; sg_r=$2; sg_b=$3; shift 3
  mkdir -p $sg_g/memcard0; rm -f $sg_g/log.txt $sg_g/steps.csv $sg_g/split.* $sg_g/battery.txt $sg_g/shot_*.bmp
  cp $sg_b/EBOOT.PBP $sg_b/ovl_*.bin $sg_g/; cp $GG/MGS2V5/SLES_503.83 $sg_g/
  if [ $sg_r = in ]; then cp $MG/play/pad_w01af.txt $sg_g/pad.txt; ln -sf $V/pak/mgs2disc.pak $sg_g/mgs2disc.pak; cp $V/dl_draft/mgs2.cfg $sg_g/mgs2.cfg
  else cp $GG/MGS2V5/pad.txt $sg_g/; ln -sf /home/claude/psp5/mgs2disc.pak $sg_g/mgs2disc.pak; cp $MG/prof/emu_v24/mgs2_$sg_r.cfg $sg_g/mgs2.cfg; fi
  cp $MG/prof/hashref_x86_${sg_r}_h2.txt $sg_g/hashref.txt
  for l in "$@"; do echo "$l" >> $sg_g/mgs2.cfg; done
}
case $S in
d)
  rm -rf $X/d && mkdir -p $X && cp -a /home/claude/h2/d $X/d || exit 1
  # the crash catcher, the battery log and their glue, with psp/v30me6.sh's flags for these units (BASEF, MACHF; no ME flags)
  SDK=/home/claude/pspsdk/pspdev/psp/sdk; HP=$MG/third_party/ps2rc/hle/psp
  BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
  MACHF="-Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv"
  OS0=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $X/d/make.log | head -1 | cut -d= -f2)
  (cd $X/d && [ -n "$OS0" ] &&
   psp-gcc $BASEF -fno-merge-constants -I$SDK/include -c $MG/psp/gl1_glue.c -o gl1_glue.o &&
   psp-gcc $BASEF -fno-merge-constants -DME_DIAG3 -DME_DIAG4 -DME_DIAG5 -DME_EXC -DME_EXC_HOLD -I$SDK/include -c $MG/psp/v26me_bat.c -o v26me_bat.o &&
   psp-gcc $BASEF -fno-merge-constants $MACHF -DME_EXC -DMGS2_GL_EXC -DOVL_SIZE=$OS0 -I$SDK/include -I$HP -c $MG/psp/v26me4_exc.c -o v26me4_exc.o &&
   psp-gcc -I$SDK/include -c $MG/psp/v26me4_exc.S -o v26me4_exc_s.o &&
   [ "$(psp-objdump -h v26me4_exc_s.o | awk '$2 == ".text" { print $7 }')" = "2**2" ] &&
   psp-gcc -I$SDK/include -c $HP/ps2rc_kcall.S -o ps2rc_kcall.o) > $X/d_build.out 2>&1 || { echo "gl2.sh d: the catcher's units failed"; exit 1; }
  XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o gl1_glue.o v26me_bat.o v26me4_exc.o v26me4_exc_s.o ps2rc_kcall.o" \
    MAINX="-DMGS2_XL -DMGS2_GL" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
    RTX18=-DMGS2_XLIVE W=$V/WP VP=/home/claude/psp/vp22 \
    TITLE="MGS2 harness gl2 texture blocks" sh $T/dev/rebuild_use.sh $X/d boot_rt.o main_psp.o >> $X/d_build.out 2>&1
  cd $X/d && psp-nm MGS2T.elf | grep -c ' xl_\| xlv_\| gu_\| gf_\| gl_\| gx_\| tk_' >> $X/d_build.out
  [ "$(grep -o 'OVL_SIZE=0x[0-9a-f]*' make.log | head -1 | cut -d= -f2)" = "$OS0" ] || echo "gl2.sh d: OVL_SIZE changed (the catcher's unit has $OS0)" >> $X/d_build.out
  for s in __wrap_host_vb_begin __wrap_mgs2_exit __wrap_sceKernelExitGame __wrap_mgs2_main mgs2_exc_handler xl_psp_gl_screen; do
    psp-nm MGS2T.elf | grep -q " T $s$" || echo "gl2.sh d: $s missing" >> $X/d_build.out; done ;;
emu)
  export PPSSPP_VFPU_RN=1; mkdir -p $X/emu; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}'); shift; [ $# -gt 0 ] || set -- in deck
  for r in "$@"; do
    G=$GG/MGS2GL2_$r; rm -f $X/emu/done_$r
    case $r in in) setup_game $G in $X/d "MGS2_GL_DIG=$DIG_IN" "MGS2_GL_SHOT=$SHOT_IN" 'prof 37795 37796' ;;
                deck) setup_game $G deck $X/d "MGS2_GL_DIG=$DIG_DECK" "MGS2_GL_SHOT=$SHOT_DECK" 'prof 25195 25196' ;;
                incyc) setup_game $G in $X/d 'MGS2_OPT_CYCLE=31800:100:ffffffff,fffff7ff,ffffefff' 'prof 37795 37796' ;;
                insync) setup_game $G in $X/d 'MGS2_OPT=ffffefff' "MGS2_GL_DIG=$DIG_IN" 'prof 37795 37796' ;;
                hw) [ -f $X/dl_cfg.txt ] || { echo "gl2.sh emu hw: no $X/dl_cfg.txt"; continue; }
                    setup_game $G in $X/d; cat $X/dl_cfg.txt >> $G/mgs2.cfg; echo 'prof 37795 37796' >> $G/mgs2.cfg ;;
                *) echo "gl2.sh emu: no run $r"; continue ;; esac
    rm -f $G/crash.txt
    (sh $T/dev/prof_run.sh $G 0x$F $X/emu/x_$r ${MAXS:-9000} $G/steps.csv > $X/emu/run_$r.out 2>&1; cp $G/log.txt $X/emu/log_$r.txt
     cp $G/steps.csv $X/emu/steps_$r.csv; cp $G/battery.txt $X/emu/battery_$r.txt; cp $G/crash.txt $X/emu/crash_$r.txt
     mkdir -p $X/emu/shots_$r; rm -f $X/emu/shots_$r/*.bmp; cp $G/shot_*.bmp $X/emu/shots_$r/ 2>/dev/null; echo done > $X/emu/done_$r) &
  done
  wait ;;
model)
  t=$2; a=$3; b=$4; m=$5; r=${6:-in}; if [ $# -ge 6 ]; then shift 6; else shift $#; fi
  mkdir -p $X/model; export PPSSPP_VFPU_RN=1; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2GL2M_$t; setup_game $G $r $X/d "vblanks $b" "MGS2_HASH_EVERY=500" "prof $a $b $m" "$@"
  sed -i '/^MGS2_HASH_EVERY=500,31800:100$/d; /^vblanks 37800$/d' $G/mgs2.cfg
  sh $T/dev/prof_run.sh $G 0x$F $X/model/$t 4000 $X/model/$t.calls > $X/model/run_$t.out 2>&1
  cp $G/log.txt $X/model/log_$t.txt; cp $G/mgs2.cfg $X/model/cfg_$t.txt; touch $X/model/done_$t ;;
*) echo "gl2.sh: step d | emu | model"; exit 1 ;;
esac
