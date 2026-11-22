#!/bin/sh
# v30.sh <step> [<work dir, default /home/claude/v30>]: the timing build MGS2T_v30 (2026-10-26; psp_harness.md "Build v30").  New against
# v29: the delivered run is the indoor free route (play/w01a_route.md: free play's start on the aft deck, into Deck-A crew's quarters
# unseen, then 2 minutes inside: stand, walk, crawl, punch, fire, first person, sneak, a real alert).  The code is v29's (the same
# generator and options: lean steps 1-3); the coverage adds the functions that route runs (recomp/cov/cov_x30.txt = cov_x25.txt + 41 ELF,
# 91 w00a and 557 w01a functions), and the profiles and the code order take the indoor route as a fourth route ("in").  Then the Media
# Engine sound mix, crash catcher 2 and battery log as v29 (psp/v30me.sh on build d).
# Steps, in order:
#   hdr    the headers WP: v29's options with COV=cov_x30.txt; REFH=<dir>: compared with the headers there (v29's WP: the units whose
#          coverage did not change must be equal)
#   x86    the x86 reference build of WP (tools/dev/x86_ref.sh) and the indoor route from boot on it (tools/dev/lean/runh.sh i, from the
#          disc folder, MGS2_DISC_LOG=1): x86/Ri/Hi.ref and Hi.dense (the hashes) and run_Hi.log (the disc table and reads)
#   pak    the route's disc pack pak/mgs2disc.pak (tools/make_pak.py over x86/Ri/run_Hi.log)
#   mix    the PC reference of the sound mix checkpoints on this route: the x86 build's objects with the voice mix split (boot/spu_split.c,
#          PS2RC's spu2_split.c, the --wrap of psp/v29me.sh's PC form), the route from boot on the pack with MGS2_SPU_ME=1:
#          x86mix/mix_ck_in.txt (index, frames, commands, CRC, B's state hash; for prof/mix_ck_x86_in_v30.txt) and the game's hashes
#   m1     the standing PS2 float-rule check (speed_notes §40) on this route: WP with every lwc1 / mtc1 site recorded (tools/dev/m1den,
#          RDFB too), build A (PSP rule) and B (MGS2_M1_PS2=1) on the pack to vb 37800 with the per-vblank VIF1 + GIF hash: m1/result.txt
#   ga pga gb gc pgbc c trace order d    as psp/v29.sh, with the route "in" in pga (ELF profile), pgc (w01a profile), trace and order
#          (its windows join the crew deck's against the w01a overlay)
#   emu    d in PPSSPP on the four routes: deck and w01a against prof/hashref_x86_{deck,w01a}_v27.txt, free against REF_FREE, in (the
#          delivered cfg dl_draft/mgs2.cfg) against REF_IN
# Container inputs (ephemeral): v24/d (genf.txt, layouts), psp/vp22, the packs psp5 and v25f/pak, the pads, the disc folder
# /home/claude/mgs2/disc (MOVIE.DAT there is a sparse stand-in of the right size: the table's sector numbers depend on it).
S=$1; V=${2:-/home/claude/v30}; MG=/mnt/project-files/mgs2; T=$MG/tools
WPW=${WPW:-wpd_all6}; REF_FREE=${REF_FREE:-$MG/prof/hashref_x86_free_v28.txt}; REF_IN=${REF_IN:-$MG/prof/hashref_x86_in_v30.txt}
TITLE="MGS2 timing harness v30"
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
STAGES="init r_title r_tnk0 n_title d00t d01t w00a w01a"
mkdir -p $V; cd $V || exit 1
case $S in
hdr)
  gzip -dc $MG/recomp/widths/$WPW.json.gz > $V/$WPW.json || exit 1
  RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_CONDCLEAN=1 RC_WPW=$V/$WPW.json MM_BASE=1 J=${J:-3} \
    COV=$MG/recomp/cov/cov_x30.txt sh $T/mk_headers.sh $V/WP > hdr.out 2>&1
  [ -z "$REFH" ] || for f in $REFH/gen_*.h $REFH/stages.h; do cmp $f $V/WP/$(basename $f) >> hdr.out 2>&1 && echo "$(basename $f) equal to $REFH" >> hdr.out; done ;;
x86)
  sh $T/dev/x86_ref.sh $V/WP $V/x86 > x86_build.out 2>&1; grep -q '^link 0 ' x86/cc.status || exit 1
  mkdir -p x86/Ri/memcard0 && cp x86/boot x86/Ri/ && sh $T/dev/lean/runh.sh $V/x86/Ri Hi i MGS2_DISC_LOG=1 ;;
pak)
  mkdir -p pak && python3 $T/make_pak.py x86/Ri/run_Hi.log /home/claude/mgs2/disc pak/mgs2disc.pak > pak.out 2>&1 && md5sum pak/mgs2disc.pak >> pak.out ;;
mix)
  mkdir -p x86mix/R/memcard0 && cd x86mix || exit 1
  CF="-O1 -g0 -w -I$MG/runtime -I$MG/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0"; AU=$MG/third_party/ps2rc/audio
  gcc $CF -I$MG/psp -I$MG/third_party/ps2rc/ps2rc/iop/rt -I$AU -c $MG/boot/spu_split.c -o spu_split.o > cc.log 2>&1 || exit 1
  gcc $CF -I$AU -Dspu2_bind_ops=__real_spu2_bind_ops -Dspu2_render48=__real_spu2_render48 -c $AU/spu2_split.c -o spu2_split.o >> cc.log 2>&1 || exit 1
  X=$V/x86; gcc -o R/boot $X/boot_rt.o $X/gen.o $X/gen_dispatch.o $X/gen_s_*.o $X/iop_main.o $X/iop_rt.o $X/iop_rt_libs.o $X/iop_rt_ucontext.o \
    $X/jsifman.o $X/cdbios.o $X/spu2_lite.o spu_split.o spu2_split.o -Wl,--wrap=spu2_bind_ops,--wrap=spu2_render48,--wrap=iop_rt_create -lm > link.log 2>&1 || exit 1
  sh $T/dev/lean/runh.sh $V/x86mix/R Hm i MGS2_DISC=$V/pak/mgs2disc.pak MGS2_SPU_ME=1
  grep -a '^mix: ck' R/run_Hm.log | awk '{print $3, $5, $7, $9, $11}' > mix_ck_in.txt
  echo "checkpoints $(wc -l < mix_ck_in.txt), eq $(grep -ac '^mix: ck.* eq' R/run_Hm.log); hashes against Hi: $(cmp R/Hm.ref ../x86/Ri/Hi.ref > /dev/null && echo equal || echo DIFFER), dense $(cmp R/Hm.dense ../x86/Ri/Hi.dense > /dev/null && echo equal || echo DIFFER)" > mix.out ;;
m1)
  mkdir -p m1/W m1/rt m1/A/memcard0 && cd m1 || exit 1
  cp $MG/runtime/guest.h rt/guest.h && patch rt/guest.h < $T/dev/m1den/guest_m1.patch > patch.log 2>&1 || exit 1
  b=0; for h in $V/WP/gen_*.h; do u=$(basename $h .h); u=${u#gen_}; u=${u#s_}
    python3 $T/dev/m1den/rewrite.py $h W/$(basename $h) $u $b || exit 1; b=$((b + $(wc -l < W/$(basename $h).sites))); done
  for f in boot_rt.o gen_dispatch.o iop_main.o iop_rt.o iop_rt_libs.o iop_rt_ucontext.o jsifman.o cdbios.o spu2_lite.o; do cp $V/x86/$f A/ || exit 1; done
  M1W=$V/m1/W M1RT=$V/m1/rt M1C=$T/dev/m1den/m1den.c sh $T/dev/m1den/build.sh $V/m1/A; grep -q '^link 0' A/cc.status || exit 1
  mkdir -p B/memcard0 && cp A/boot B/
  sh $T/dev/m1den/m1run2.sh $V/m1/A mA i MGS2_PKT_HASH=1:37801 &
  sh $T/dev/m1den/m1run2.sh $V/m1/B mB i MGS2_PKT_HASH=1:37801 MGS2_M1_PS2=1 &
  wait
  python3 - > result.txt <<'PY'
def rd(p, k):
    d = {}
    for l in open(p):
        f = l.split()
        if f: d[int(f[0])] = f[1:k]
    return d
def pk(p):
    return {int(l.split()[2]): l.split()[3] for l in open(p) if l.startswith('pkt: vb')}
R = '/home/claude/v30/x86/Ri/'
for a, b, nm, k in (('A/mA.ref', R + 'Hi.ref', 'A against the reference, every 100 (ee iop vfb)', 4), ('A/mA.dense', R + 'Hi.dense', 'A against the reference, dense (ee vfb vuh)', 4),
                    ('B/mB.ref', 'A/mA.ref', 'B against A, every 100 (ee iop; vfb apart)', 3), ('B/mB.dense', 'A/mA.dense', 'B against A, dense (ee; vuh)', 2)):
    x, y = rd(a, k), rd(b, k); c = sorted(set(x) & set(y)); n = sum(1 for v in c if x[v] != y[v])
    print('%s: %d of %d differ (%d / %d lines)' % (nm, n, len(c), len(x), len(y)))
x, y = rd('B/mB.dense', 5), rd('A/mA.dense', 5); c = [v for v in sorted(set(x) & set(y)) if len(x[v]) > 2 and len(y[v]) > 2]
print('vuh B against A: %d of %d differ' % (sum(1 for v in c if x[v][2] != y[v][2]), len(c)))
pa, pb = pk('A/run_mA.log'), pk('B/run_mB.log'); c = sorted(set(pa) & set(pb))
print('VIF1 + GIF data per vblank, B against A: %d of %d vblanks differ (A %d, B %d lines)' % (sum(1 for v in c if pa[v] != pb[v]), len(c), len(pa), len(pb)))
PY
  grep -h '^m1: mode\|^m1: rule' A/run_mA.log B/run_mB.log >> result.txt ;;
ga)
  mkdir -p ga && cp /home/claude/v24/d/genf.txt ga/ && sed -i 's|$| -DGEN_HB_SP|' ga/genf.txt
  RTXX="-Wno-error=coverage-mismatch -DVP_VFPU" VP=/home/claude/psp/vp22 J=4 sh $T/dev/mk_psp.sh $V/ga $V/WP PGO=gen PGO_ELF=1 PGO_STAGE= LAYOUT=1 \
    STAGES="$STAGES" PSP_EBOOT_TITLE="$TITLE pgo elf" > ga_build.out 2>&1 ;;
pga)
  sh $T/dev/pgo_run.sh $V/ga $V/pga deck w01a free in > pga.out 2>&1
  mkdir -p pga_ms0 && mv /home/claude/ms_home/.ppsspp/pgo/boot_rt.gcda /home/claude/ms_home/.ppsspp/pgo/gen.gcda /home/claude/ms_home/.ppsspp/pgo/gen_dispatch.gcda pga_ms0/ ;;
gb)
  mkdir -p keep && cp -a ga/gen_s_w00a.o ga/gen_s_w01a.o keep/
  W=$V/WP sh $T/dev/pgo_stagegen.sh $V/ga $V/gb w00a gen.o gen_dispatch.o boot_rt.o main_psp.o gen_s_w00a.o > gb_build.out 2>&1 ;;
gc)
  rm -rf gcpre && cp -a gb gcpre && cp -a keep/gen_s_w00a.o gcpre/gen_s_w00a.o
  W=$V/WP sh $T/dev/pgo_stagegen.sh $V/gcpre $V/gc w01a main_psp.o gen_s_w01a.o > gc_build.out 2>&1 ;;
pgbc)
  sh $T/dev/pgo_run.sh $V/gb $V/pgb deck free; mkdir -p pgb_ms0; mv /home/claude/ms_home/.ppsspp/pgo/*.gcda pgb_ms0/
  sh $T/dev/pgo_run.sh $V/gc $V/pgc w01a in; mkdir -p pgc_ms0; mv /home/claude/ms_home/.ppsspp/pgo/*.gcda pgc_ms0/ ;;
c)
  rm -rf c && cp -a ga c && cd c && rm -f gen.o gen_dispatch.o boot_rt.o main_psp.o gen_s_w00a.o gen_s_w01a.o *.gcda
  cp ../pga_ms0/gen.gcda ../pga_ms0/gen_dispatch.gcda ../pga_ms0/boot_rt.gcda ../pgb_ms0/gen_s_w00a.gcda ../pgc_ms0/gen_s_w01a.gcda .
  cp /home/claude/v24/d/layout_elf.ld /home/claude/v24/d/layout_w00a.ld /home/claude/v24/d/layout_w01a.ld .
  cd $V && W=$V/WP VP=/home/claude/psp/vp22 TITLE="$TITLE" sh $T/dev/rebuild_use.sh $V/c > c_build.out 2>&1 ;;
trace)
  P="prof 24001 24006\nprof 24151 24156\nprof 24301 24306\nprof 24451 24456\nprof 24601 24606\nprof 24751 24756\nprof 24901 24906\nprof 25051 25056"
  PF="prof 30801 30806\nprof 31301 31306\nprof 32201 32206\nprof 32951 32956\nprof 33401 33406\nprof 33851 33856\nprof 34801 34806\nprof 35801 35806"
  # the indoor route's parts (play/w01a_route.md): stand, walk, walk (aft corridor), crawl, fire, first person, sneak; alert (test window)
  PI="prof 32001 32006\nprof 32601 32606\nprof 33151 33156\nprof 34301 34306\nprof 34751 34756\nprof 35201 35206\nprof 36001 36006\nprof 37201 37206"
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t30d deck $V/lay "$P" 8 &
  IT=1 VB=25060 sh $T/dev/trace_run.sh $V/c t30w w01a $V/lay "$P" 8 &
  IT=1 VB=35810 MAXS=5000 sh $T/dev/trace_run.sh $V/c t30f free $V/lay "$PF" 8 &
  IT=1 VB=37210 MAXS=5000 sh $T/dev/trace_run.sh $V/c t30i in $V/lay "$PI" 8 &
  wait ;;
order)
  [ -x $V/layout_sim ] || gcc -O2 -pthread -o $V/layout_sim $T/layout_sim.c -lm || exit 1
  cd lay || exit 1
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w00a.elf ../c w00a > funcs_deck.txt
  python3 $T/layout_funcs.py ../c/MGS2T.elf ../c/ovl_w01a.elf ../c w01a > funcs_w01a.txt
  python3 -c "
import numpy as np
def wins(p):
    a = np.fromfile(p, dtype='<u4').reshape(-1, 2); m = list(np.nonzero((a[:, 0] == 0xffffffff) & (a[:, 1] == 0))[0]) + [len(a)]
    assert m[0] == 0 and len(m) == 9; return [a[m[k]:m[k + 1]] for k in range(8)]
d, f = wins('t30d.it'), wins('t30f.it')
np.concatenate(d[:7] + f[:7] + [d[7], f[7]]).tofile('t_df.it')
w, i = wins('t30w.it'), wins('t30i.it')
np.concatenate(w[:7] + i[:7] + [w[7], i[7]]).tofile('t_wi.it')"
  B1=$(printf %08x $((0x$(psp-nm ../c/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  for s in 1 2; do
    $V/layout_sim t_df.it funcs_deck.txt -and t_wi.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -opt 40000 -j 2 \
      -seed $s -out order_s$s.txt > ls_s$s.out 2>&1 &
  done
  wait ;;
d)
  rm -rf d && cp -a c d
  W=$V/WP VP=/home/claude/psp/vp22 TITLE="$TITLE" sh $T/dev/relink_order.sh $V/lay/order_s${SEED:-1}.txt $V/d > d_build.out 2>&1
  cd lay && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w00a.elf ../d w00a > funcs_deck_d.txt && python3 $T/layout_funcs.py ../d/MGS2T.elf ../d/ovl_w01a.elf ../d w01a > funcs_w01a_d.txt
  B1=$(printf %08x $((0x$(psp-nm ../d/MGS2T.elf | awk '$3 == "ovl_arena" { print $1 }') + 0x34)))
  $V/layout_sim t_df.it funcs_deck.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_deck_d.txt > new_dd.log 2>&1
  $V/layout_sim t_wi.it funcs_w01a.txt -wl 2 -wd 1 -test 2 -base0 0880403c -base1 $B1 -new funcs_w01a_d.txt > new_dw.log 2>&1 ;;
emu)
  GG=/home/claude/ms_home/.ppsspp/PSP/GAME; mkdir -p emu; export PPSSPP_VFPU_RN=1
  F=$(psp-nm d/MGS2T.elf | awk '$3=="g_emuprof"{print $1}')
  for r in deck w01a free in; do
    G=$GG/MGS2V30_$r; mkdir -p $G/memcard0; rm -f $G/log.txt $G/steps.csv $G/split.* $G/battery.txt
    cp d/EBOOT.PBP d/ovl_*.bin $G/; cp $GG/MGS2V5/SLES_503.83 $G/
    if [ $r = free ]; then cp /home/claude/v25f/dl/pad.txt $G/; ln -sf /home/claude/v25f/pak/mgs2disc.pak $G/mgs2disc.pak
      cp /home/claude/v26/dl_draft/mgs2.cfg $G/mgs2.cfg; echo 'prof 36495 36496' >> $G/mgs2.cfg; cp $REF_FREE $G/hashref.txt
    elif [ $r = in ]; then cp $MG/play/pad_w01af.txt $G/pad.txt; ln -sf $V/pak/mgs2disc.pak $G/mgs2disc.pak
      cp $V/dl_draft/mgs2.cfg $G/mgs2.cfg; echo 'prof 37795 37796' >> $G/mgs2.cfg; cp $REF_IN $G/hashref.txt
    else cp $GG/MGS2V5/pad.txt $G/; ln -sf /home/claude/psp5/mgs2disc.pak $G/mgs2disc.pak
      cp $MG/prof/emu_v24/mgs2_$r.cfg $G/mgs2.cfg; echo 'prof 25195 25196' >> $G/mgs2.cfg; cp $MG/prof/hashref_x86_${r}_v27.txt $G/hashref.txt; fi
    (sh $T/dev/prof_run.sh $G 0x$F $V/emu/x_$r ${MAXS:-9000} > emu/run_$r.out 2>&1; cp $G/log.txt emu/log_$r.txt; cp $G/steps.csv emu/steps_$r.csv
     ls $G/split.* > emu/splitfiles_$r.txt 2>/dev/null; echo done > emu/done_$r) &
  done
  wait ;;
*) echo "v30.sh: step hdr | x86 | pak | mix | m1 | ga | pga | gb | gc | pgbc | c | trace | order | d | emu"; exit 1 ;;
esac
