#!/bin/sh
# mkpsnbs.sh (task #165, body_sph): mkpsph3.sh's bucket model build /home/claude/sbg0m/d with its w00a unit rebuilt in place from the
# H3 copy of v31's w00a header (/home/claude/sbg0/wpl, now with body_sph's hook) and the private nat/ (line_sph's G0 / G0H, body_sph's
# nat_bs_part behind OPT2_NBS 16384).  MGS2_OPT2: ffffbfdf G0H and H3, ffffffdf G0H, H3 and NBS.  Never shipped.
D=/home/claude/sbg0m/d; MG=/mnt/project-files/mgs2
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
cd $D || exit 1; grep -c "sbg0/nat" genf.txt
XOBJS="xl_psp.o ge_xlive.o ge_xlate.o ge_gsfront.o ge_gsup2.o ge_texconv.o ge_texcache.o ge_gelive.o ge_gebatch.o ge_xconv.o ge_vu1conv.o ge_gelist.o ge_fxthin.o ge_fxbk.o gl1_glue.o v26me_bat.o exc3b.o exc3b_s.o ps2rc_kcall.o" \
  MAINX="-DMGS2_XL -DMGS2_GL -DPOOL_KB=512" XLIBS="-Wl,--wrap=host_vb_begin,--wrap=mgs2_exit,--wrap=sceKernelExitGame,--wrap=mgs2_main -lpspgu -lpspsystemctrl_user" \
  RTX18="-DMGS2_XLIVE -DK_HSTACK=49152 -DIOP_HSTACK=24576" W=/home/claude/sbg0/wpl VP=/home/claude/psp/vp22 \
  TITLE="MGS2 harness rain G0 H3 NBS buckets" sh $MG/tools/dev/rebuild_use.sh "$D" gen_s_w00a.o > relink_nbs.out 2>&1
tail -n 3 relink_nbs.out; grep -o 'psp-gcc [^&]*-o gen_s_w00a.o' make.log | grep -c "sbg0/wpl"; ls -l EBOOT.PBP && md5sum EBOOT.PBP
psp-nm MGS2T.elf | grep -E ' (g_emuactor|g_emuprof)$'
