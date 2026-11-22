#!/bin/sh
# gl3.sh <step> [<args>]: MGS2T_gl3 = gl2m (below) with the lit meshes on the GE (2026-10-27, task #63; renderer_notes 11.14,
# re/vu1_programs.md 3.14): the P06 calls (Snake, the crew, the guards: entries +0, +358, +738, +ab0) become GE batches.  ge/vu1conv.c
# vc_p06 computes each vertex as the micro program does (three lights and ambient, clamped; the guards' colour factor; two-matrix skin;
# the UVs of passes 2 and 3), the same bits as the PS2 on 933,883 captured vertices in both roundings; ge/xconv.c xc_lit puts the call's
# transform, a record per pass, the vertices and the strips in the frame (GERK_LIT), xc_pass gives passes 2 and 3 their texture and state
# when the translator reaches their GS packets (xl_on_pass; psp/xl_psp.c on_pass, ge/gelive.c gl_pass); ge/gelist.c lit_draw draws each
# pass with the call's world matrix.  On x86 the live frames equal gsref's PS2 reference draw for draw on ten frames of three routes (P06
# passes 7,033; 0 differ).  The game state is gl2m's: the hashes are prof/hashref_x86_*_h2.txt.
# Memory for the textures the guards add (the heaviest indoor frame needed 2,264 KB, x86 with a 2,688 KB pool): the host stacks of the
# other guest threads and of the IOP threads used at most 10,456 and 5,376 bytes in every PSP run so far (74 logs), so they are 48 KB
# and 24 KB here (128 and 64 KB before), in a pool of 512 KB (1,152 before; main_psp.c POOL_KB): 640 KB more heap, which the texture
# pool takes (2,688 KB: dl_cfg.txt MGS2_GL_TEXKB, and every emu run here).  main_psp.c also gives the deepest use of the main thread's
# stack above the pool (MGS2_STACK_MARK).  And fewer textures: a df0 call (the rooms) whose vertices' box lies wholly off one side of its
# scissor, by 2 GE pixels, is left out before its texture is asked for (ge/xconv.c xc_off, XcHost.cut; renderer_notes 11.15), its
# vertices still in the frame's depth range (xc_df0_zr), so the depth map is the same.  gsref's 8 captures: the 8,357 calls it leaves out
# are all calls the PS2 wrote no pixel from, and on 4 frames the exports with and without it differ only by those draws.  x86 from boot:
# 52% of the deck's df0 calls and 37% of the indoor route's left out, the heaviest indoor frame 1,998 KB, no texture ever refused for
# room.  g_opt OPT_GL_CUT (32768) off = every call converted (cfg ffff7fff; emu run incut0).  gl2m.sh's text:
# MGS2T_gl2m = gl2d (below) with the IOP -> EE data fix of runtime/sif.h (2026-10-27, task #65): data the IOP
# sends while it runs lands in EE memory at once (after the data queued ahead of it, never past a waiting command) instead of a copy
# in the heap until the run's end.  gl2d crashed at vb 26,108 when malloc returned 0 for a 32 KB piece of a 1,110 KB codec load held
# whole in the heap (34 x 32 KB in one IOP run; also at vb 15,681).  The game state is gl2d's (the same writes in the same order).
# emu run hwmem = hw with 5,120 KB more texture pool than dl_cfg.txt's, so PPSSPP's heap (11,728 KB headroom at the start against the
# PSP's 6,608) is the PSP's for everything else; B=<build dir> runs another build's EBOOT (gl2d's: /home/claude/gl2d/d) with TAG=<suffix>
# for its game folder and files.  gl2d.sh's text:
# MGS2T_gl2d = gl2 (below) with the system diagnostics of psp/v26me_bat.c -DSYS_DIAG (2026-10-27, task #62;
# me_switchoff_plan.md "The codec call"): in battery.txt, every power event the moment it comes (a power callback), and in each 5-s line
# the longest wait of a thread at priority 0x6F (below the game thread's 0x20: how long the system's own low-priority threads wait) and
# the thread manager's counts; the threads user mode can see at the start and every 60 s.  Reads only, user mode; the game state and the
# game thread's work are gl2's.  psp/gl1_glue.c takes the power callback out at the exit (me_bat_exit).  gl2.sh's text:
# MGS2T_gl2 = gl1 (below) with textures converted a GS block at a time (2026-10-27, task #59: ge/texconv.c
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
#                      incut0 (as in with the off-scissor cut off: MGS2_OPT=ffff7fff; its shots must equal in's, its digests x86's
#                      with MGS2_XLIVE_CUT=0), hw (the PSP delivery's mgs2.cfg: $X/dl_cfg.txt appended to the indoor route's), hwmem
#   model <tag> <a> <b> <mode> [route] [cfg lines...]   as xl5's (mode: xl, xg, gl, xgl or all)
# The digests (MGS2_GL_DIG) are compared with boot/xlive_chk.c MGS2_XLIVE_GX on the same route from boot (tools/dev/gl1_x86.sh), outside
# the profiled step (PPSSPP's interpreter there is not exact: one shown step after each digest window, its PspProf dump x_<run>); each run
# goes on to the route's end (steps.csv written), so the log has the totals and the memory lines; the shots (MGS2_GL_SHOT,
# shot_<vb>.bmp next to the EBOOT) are copied to $X/emu/shots_<run>.  NOPROF=1 leaves the profiled step out (PPSSPP's interpreter
# there changed gl4's vb 37,800 hash: renderer_notes 11.16); EXTRA='<cfg lines>' (no spaces inside a line) adds lines to every run's
# cfg (hw: before the delivery's; no name may be in both).  Work dir $GL3 or /home/claude/gl3; game folders MGS2GL3*.  Game
# data passes through all of it: personal use only.
# gl4c (2026-10-28, the codec test route): step d takes its base build from H2D (default /home/claude/h2/d) and its headers from WPD
# (default v30's WP); gl4c's are /home/claude/h2c/d (h2/d with gen_s_w00a.o built from /home/claude/v31/WPL: the 279 deck functions of
# Otacon's calls) and /home/claude/v31/WPL.  Route codec: play/pad_codec.txt, the pack $CPAK (default /home/claude/v31/pak), the
# delivery cfg $CCFG (default prof/hw_gl4c/mgs2_gl4c.cfg), prof/hashref_x86_codec_h2.txt; emu run "codec".
S=$1; X=${GL3:-/home/claude/gl3}; MG=/mnt/project-files/mgs2; T=$MG/tools
EXC=${EXC:-v26me4_exc}   # the crash catcher's sources psp/$EXC.c / .S (gl3: catcher 2; gl4: EXC=exc3, catcher 3); TTL: the XMB title
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
GG=/home/claude/ms_home/.ppsspp/PSP/GAME; V=/home/claude/v30
DIG_IN=37601:37789; SHOT_IN=32000,33500,35000,36500,37790
DIG_DECK=25001:25189; SHOT_DECK=24100,24600,25000,25190
TEXKB=2688   # the texture pool of every run (x86: MGS2_XLIVE_GX_TEXKB, the same, for the digests)
# the game folder <dir> for route <in | deck> with build <build dir>'s files, the route's hashref and <cfg lines> appended to the route's
# cfg (its own variable names: the callers' G and r stay as they are)
setup_game() {
  sg_g=$1; sg_r=$2; sg_b=$3; shift 3
  mkdir -p $sg_g/memcard0; rm -f $sg_g/log.txt $sg_g/steps.csv $sg_g/split.* $sg_g/battery.txt $sg_g/shot_*.bmp
  cp $sg_b/EBOOT.PBP $sg_b/ovl_*.bin $sg_g/; cp /home/claude/mgs2/SLES_503.83 $sg_g/   # (MGS2V5 is gone from GAME; same md5 e3f4c0df..)
  if [ $sg_r = in ]; then cp $MG/play/pad_w01af.txt $sg_g/pad.txt; ln -sf $V/pak/mgs2disc.pak $sg_g/mgs2disc.pak; cp $V/dl_draft/mgs2.cfg $sg_g/mgs2.cfg
  elif [ $sg_r = codec ]; then cp $MG/play/pad_codec.txt $sg_g/pad.txt; ln -sf ${CPAK:-/home/claude/v31/pak/mgs2disc.pak} $sg_g/mgs2disc.pak
    cp ${CCFG:-$MG/prof/hw_gl4c/mgs2_gl4c.cfg} $sg_g/mgs2.cfg
  # the deck pad: boot/pad_deck_idle.txt and a newline (md5 eab03409.., the gone GAME/MGS2V5/pad.txt; without it the deck run
  # leaves the route at vb 1,500 and the harness stops the machine at vb 2,767: gl3 deck, 2026-10-27)
  else { cat $MG/boot/pad_deck_idle.txt; echo; } > $sg_g/pad.txt; ln -sf /home/claude/psp5/mgs2disc.pak $sg_g/mgs2disc.pak; cp $MG/prof/emu_v24/mgs2_$sg_r.cfg $sg_g/mgs2.cfg; fi
  cp ${HREF:-$MG/prof/hashref_x86_${sg_r}_h2.txt} $sg_g/hashref.txt   # gl18 (task #179): HREF=<file> for a build whose state differs (the rain's skips)
  echo "MGS2_GL_TEXKB=$TEXKB" >> $sg_g/mgs2.cfg
  for l in "$@" $EXTRA; do case "$l" in prof*) [ -n "$NOPROF" ] && continue ;; esac; echo "$l" >> $sg_g/mgs2.cfg; done
}
case $S in
d)
  rm -rf $X/d && mkdir -p $X && cp -a ${H2D:-/home/claude/h2/d} $X/d || exit 1
  # the crash catcher, the battery log and their glue, with psp/v30me6.sh's flags for these units (BASEF, MACHF; no ME flags)
  SDK=/home/claude/pspsdk/pspdev/psp/sdk; HP=$MG/third_party/ps2rc/hle/psp
  BASEF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600"
  MACHF="-Dexit=mgs2_exit -Dabort=mgs2_abort -Dgetenv=mgs2_getenv"
  OS0=$(grep -o 'OVL_SIZE=0x[0-9a-f]*' $X/d/make.log | head -1 | cut -d= -f2)
  (cd $X/d && [ -n "$OS0" ] &&
   psp-gcc $BASEF -fno-merge-constants -I$SDK/include -c $MG/psp/gl1_glue.c -o gl1_glue.o &&
   psp-gcc $BASEF -fno-merge-constants -DME_DIAG3 -DME_DIAG4 -DME_DIAG5 -DME_EXC -DME_EXC_HOLD -DSYS_DIAG -I$SDK/include -c $MG/psp/v26me_bat.c -o v26me_bat.o &&
   psp-gcc $BASEF -fno-merge-constants $MACHF -DME_EXC -DMGS2_GL_EXC -DOVL_SIZE=$OS0 -I$SDK/include -I$HP -c $MG/psp/$EXC.c -o $EXC.o &&
   psp-gcc -I$SDK/include -c $MG/psp/$EXC.S -o ${EXC}_s.o &&
   [ "$(psp-objdump -h ${EXC}_s.o | awk '$2 == ".text" { print $7 }')" = "2**2" ] &&
   psp-gcc -I$SDK/include -c $HP/ps2rc_kcall.S -o ps2rc_kcall.o) > $X/d_build.out 2>&1 || { echo "gl3.sh d: the catcher's units failed"; exit 1; }
  XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o gl1_glue.o v26me_bat.o $EXC.o ${EXC}_s.o ps2rc_kcall.o" \
    MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
    RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=${WPD:-$V/WP} VP=/home/claude/psp/vp22 \
    TITLE="${TTL:-MGS2 harness gl3 Snake and guards}" sh $T/dev/rebuild_use.sh $X/d boot_rt.o main_psp.o >> $X/d_build.out 2>&1
  cd $X/d && psp-nm MGS2T.elf | grep -c ' xl_\| xlv_\| gu_\| gf_\| gl_\| gx_\| tk_' >> $X/d_build.out
  [ "$(grep -o 'OVL_SIZE=0x[0-9a-f]*' make.log | head -1 | cut -d= -f2)" = "$OS0" ] || echo "gl3.sh d: OVL_SIZE changed (the catcher's unit has $OS0)" >> $X/d_build.out
  for s in __wrap_host_vb_begin __wrap_mgs2_exit __wrap_sceKernelExitGame __wrap_mgs2_main mgs2_exc_handler xl_psp_gl_screen; do
    psp-nm MGS2T.elf | grep -q " T $s$" || echo "gl3.sh d: $s missing" >> $X/d_build.out; done ;;
emu)
  export PPSSPP_VFPU_RN=1; mkdir -p $X/emu; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}'); shift; [ $# -gt 0 ] || set -- in deck
  for r in "$@"; do
    B=${B:-$X/d}; G=$GG/MGS2GL3_$r$TAG; rm -f $X/emu/done_$r$TAG
    case $r in in) setup_game $G in $B "MGS2_GL_DIG=$DIG_IN" "MGS2_GL_SHOT=$SHOT_IN" 'prof 37795 37796' ;;
                deck) setup_game $G deck $B "MGS2_GL_DIG=$DIG_DECK" "MGS2_GL_SHOT=$SHOT_DECK" 'prof 25195 25196' ;;
                codec) setup_game $G codec $B ;;
                incyc) setup_game $G in $B 'MGS2_OPT_CYCLE=31800:100:ffffffff,fffff7ff,ffffefff' 'prof 37795 37796' ;;
                insync) setup_game $G in $B 'MGS2_OPT=ffffefff' "MGS2_GL_DIG=$DIG_IN" 'prof 37795 37796' ;;
                incut0) setup_game $G in $B 'MGS2_OPT=ffff7fff' "MGS2_GL_DIG=$DIG_IN" "MGS2_GL_SHOT=$SHOT_IN" 'prof 37795 37796' ;;
                hw) [ -f $X/dl_cfg.txt ] || { echo "gl3.sh emu hw: no $X/dl_cfg.txt"; continue; }
                    setup_game $G in $B; cat $X/dl_cfg.txt >> $G/mgs2.cfg
                    [ -n "$NOPROF" ] || echo 'prof 37795 37796' >> $G/mgs2.cfg ;;
                hwmem) [ -f $X/dl_cfg.txt ] || { echo "gl3.sh emu hwmem: no $X/dl_cfg.txt"; continue; }
                    setup_game $G in $B; cat $X/dl_cfg.txt >> $G/mgs2.cfg; echo "MGS2_GL_TEXKB=$(( $(grep '^MGS2_GL_TEXKB=' $X/dl_cfg.txt | tail -n 1 | cut -d= -f2) + 5120 ))" >> $G/mgs2.cfg; echo 'prof 37795 37796' >> $G/mgs2.cfg ;;
                *) echo "gl3.sh emu: no run $r"; continue ;; esac
    rm -f $G/crash.txt
    k=$r$TAG; F=$(psp-nm $B/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
    (sh $T/dev/prof_run.sh $G 0x$F $X/emu/x_$k ${MAXS:-9000} $G/steps.csv > $X/emu/run_$k.out 2>&1; cp $G/log.txt $X/emu/log_$k.txt
     cp $G/steps.csv $X/emu/steps_$k.csv; cp $G/battery.txt $X/emu/battery_$k.txt; cp $G/crash.txt $X/emu/crash_$k.txt
     mkdir -p $X/emu/shots_$k; rm -f $X/emu/shots_$k/*.bmp; cp $G/shot_*.bmp $X/emu/shots_$k/ 2>/dev/null; echo done > $X/emu/done_$k) &
  done
  wait ;;
model)
  t=$2; a=$3; b=$4; m=$5; r=${6:-in}; if [ $# -ge 6 ]; then shift 6; else shift $#; fi
  mkdir -p $X/model; export PPSSPP_VFPU_RN=1; F=$(psp-nm $X/d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  G=$GG/MGS2GL3M_$t; setup_game $G $r $X/d "vblanks $b" "MGS2_HASH_EVERY=500" "prof $a $b $m" "$@"
  sed -i '/^MGS2_HASH_EVERY=500,31800:100$/d; /^vblanks 37800$/d' $G/mgs2.cfg
  sh $T/dev/prof_run.sh $G 0x$F $X/model/$t 4000 $X/model/$t.calls > $X/model/run_$t.out 2>&1
  cp $G/log.txt $X/model/log_$t.txt; cp $G/mgs2.cfg $X/model/cfg_$t.txt; touch $X/model/done_$t ;;
*) echo "gl3.sh: step d | emu | model"; exit 1 ;;
esac
