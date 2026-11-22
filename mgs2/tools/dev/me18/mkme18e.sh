#!/bin/sh
# me18e mkme18e.sh: mkme18d.sh as it is, on me18e's ge/ (task #180 step 4d-1: ge/xme.c's cache steps through the host's hook).  From mkme18d.sh:
# mkme18d.sh (me18c mkme18c.sh with ge/xme.c, the ME's loop, and ge/xlme.c xm_lay among the entries: task #180 step 4c) from
# mkme18c.sh (me18b mkme18b.sh with ge/xlme.c xm_plan and xm_fill among the entries: task #180 step 4b-2, the copy taken at the deadline
# with the guards' log laid over it) <ge dir> <out dir>; from mkme18b.sh: (me14 mkme_psp.sh with ge/xlme.c xm_copy and xv_ram among the entries: task #180 step 4b-1) task #149: the picture files and me_rt compiled for the PSP as the ME unit will be (gl16's GF flags +
# -fno-jump-tables -DMGS2_ME_SAFE), plus -fstack-usage for the check; the compiler's memcpy/memset/memcmp calls renamed to me_rt's;
# (step 2b: and ge/xlme.c, the ME's loop: the late walk with the lanes, the switches, the callbacks, a frame's end and start)
# then the ME unit as a relocatable kept only from the ME's entry points (--gc-sections), for mecheck.py.
G=$1; O=$2
export PATH=/home/claude/pspsdk/pspdev/bin:$PATH
GF="-O2 -G0 -w -fpermissive -fwrapv -fno-strict-aliasing -ffp-contract=off -fno-math-errno -ffixed-23 -D_PSP_FW_VERSION=600 -ffunction-sections"
MEF="-fno-jump-tables -DMGS2_ME_SAFE -fstack-usage"
cd $O || exit 1
for f in xlive xlate gsfront texconv texcache gelive gebatch xconv vu1conv gelist gsup2 xlme xme me_rt; do
  psp-gcc $GF $MEF -c $G/$f.c -o $f.o || exit 1
  [ $f = me_rt ] || psp-objcopy --redefine-sym memcpy=me_memcpy --redefine-sym memset=me_memset --redefine-sym memcmp=me_memcmp $f.o || exit 1
done
E="xlv_dma xl_flush xl_vif1_span xl_lane_p04 xl_lane_p06 xl_lane_p07 xl_lane_fx gl_begin gl_end gl_call gl_pass gl_prim gl_flush gl_thin
 gl_digest gl_tex_cb gx_frame gx_depth_map tk_frame gu_ld_ident tk_ufront tk_umemo xl_lane xl_ud_eager xl_ud_mark xl_ud_v48 xl_vm_mode
 gu_area_memo gu_clut_memo tk_blk tk_flags tk_t16 xl_vm_logical xl_vm_sync xl_reset xl_uploads gu_keys gu_zero_empty gu_on_data xl_need
 xl_on_call xl_on_prim xl_on_pass xl_on_flush
 xm_init xm_install xm_objects xm_switch xm_walk xm_end xm_list xm_begin xm_ram xm_copy xm_plan xm_fill xv_ram xm_lay xme_entry xme_run
 xme_heap_ask"   # step 2b: ge/xlme.c, the ME's loop
echo "$E" | tr ' ' '\n' | grep . > entries.txt
U=""; for e in $E; do U="$U -u $e"; done
psp-ld -r --gc-sections $U -o meunit.o xlive.o xlate.o gsfront.o texconv.o texcache.o gelive.o gebatch.o xconv.o vu1conv.o gelist.o gsup2.o xlme.o xme.o me_rt.o || exit 1
psp-ld -r -o all.o xlive.o xlate.o gsfront.o texconv.o texcache.o gelive.o gebatch.o xconv.o vu1conv.o gelist.o gsup2.o xlme.o xme.o me_rt.o || exit 1
cat xlive.su xlate.su gsfront.su texconv.su texcache.su gelive.su gebatch.su xconv.su vu1conv.su gelist.su gsup2.su xlme.su xme.su me_rt.su > stack.txt
psp-size meunit.o all.o
echo "undefined in the ME unit:"; psp-nm -u meunit.o
