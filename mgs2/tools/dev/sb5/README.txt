sb5: sb4 (tools/dev/sb4: gl10's code, i.e. lanes 1 and 2, the exact trims, step c and the thinned rain and splash) + Stage B lane 4, the
codec faces (P07; renderer_notes 11.32, task #117), 2026-10-31.  Not in the shared code yet.  Container tree /home/claude/sb5 =
/home/claude/sb4 with sb5.diff applied (diff -uN per file over the 10 files it changes; apply it with patch -p1 inside a copy of sb4's tree,
then copy the scripts here over sb4's).  The checks and the model are in renderer_notes 11.32 "Built and checked".

Lane 4 (OPT_XL_LANE4 0x10000000, on by default; MGS2_OPT=efffffff turns it off; x86: MGS2_SB_LANE bit 3, 11 = lanes 1, 2 and 4, 3 =
gl10's lanes 1 and 2): where the TTE words of a VIF1 tag are STCYCL 7 / 1, the walk (runtime/ee_io.h lane_p07 in vif1_chain_fast) checks
a P07 slot as the game's builder makes it (re/p07_builder.md 3.4: 24 quadwords, 20 tags: the streams A..E, V4-16 at T + 0x100..0x104,
and F, V4-8 unsigned at T + 0x105 with STCYCL 7 / 2; the CNT of 4 with T + 0..3; the three materials; NOP + FLUSHE / NOP; the eight
bones at abs 0x20 + 4 i; NOP + MSCAL; a part not sent is a CNT of 0 with its STCYCL and a NOP; no IRQ bit, the slot in one run of RAM or
scratchpad) and hands it whole to the translator (ge/xlate.c xl_lane_p07: every UNPACK registered for the lazy decode as xl_vif1_span
would, the same St counters and census; one P07 call with XlRec lane 4), slot after slot while the next tag starts one.  call() decodes
only what P07 reads besides the streams (vm_dec: T + 0..3, abs 2..11, 16..19, 24..63, T + 0xfe, 0xff); the converter reads the vertices
from the game's data (ge/vu1conv.c vc_p07_rec: p7_loop's REC form, the same arithmetic in the same order as vc_p07, so the same float
bits); any input it does not expect returns -8 and the call goes to vc_p07 on the decoded VU memory (ge/xconv.c xc_lit).  After the
slots the walk's state is the generic walk's (STCYCL 1 / 1, no data left, the last command the MSCAL; x86: the command counters).

Files: runtime/ee_io.h (lane_p07, its dispatch in vif1_chain_fast, x86 lane7_miss), runtime/host_time.h (the bit), ge/xlate.h, xlate.c
(xl_lane_p07, vm_dec, call()'s lane 4 branch, xl_lane_groups[4]), ge/vu1conv.h, vu1conv.c (P7Rec, p7_loop's REC form, p07_main's -8
returns, vc_p07_rec, vc_p07_uv2_rec, p06_uv2's stride), ge/xconv.h, xconv.c (xc_rec7_calls / vm / n / diff, xc_lit's lane 4 branch,
the UV2 line, XC_P7CHK's sync), boot/xlive_chk.c (MGS2_SB_LANE mask 0xb, the mode line, the "sb lane 4:" totals line; MGS2_SB_CHK=1
covers lane 4: each lane P07 call also through vc_p07 on the decoded VU memory, every output compared), psp/xl_psp.c (OPT_XL_LANE4 ->
lane bit 3).

Scripts (T = /home/claude/sb5): mk.sh <dir> the x86 twin; mkpsp.sh <new dir> the PSP model build (gl7's build with this tree's objects,
no PGO for the picture code; sb5's: /home/claude/sb5m/d, EBOOT md5 aac5add5..); model.sh <tag> <A> <B> in|codec [cfg lines] one PspProf
window of it (gl3.sh's model step, GL3=/home/claude/sb5m; lane 4 off: the cfg line MGS2_OPT=efffffff); pgo_gl.sh sb4's PGO + layout
script with T = sb5's tree (not run for sb5).  chk_x86.sh <i | d | c> <prefix>: from boot, three runs side by side on the twin x1, lane 4
off (MGS2_SB_LANE=3), on (11) and on with MGS2_SB_CHK=1, rain and splash 1/2 as gl10: the indoor route to vb 37,800, the deck tutorial
to 25,200 or the codec route to 40,000; chk2_x86.sh the same with MGS2_HASH_DENSE=15700:16010 (the PSP cfgs' dense hash and vuh lines).
dig_x86.sh, dig2_x86.sh <prefix> <from> <to>: the per-call digest files (ge/xldig.c) off and on, indoor route, then cmp; digc_x86.sh,
digc2_x86.sh: the same on the codec route.  codec_x86.sh the codec route; probe_x86.sh MGS2_SB_PROBE's listing of one vblank's VIF1 tags
(dev).  Dev tools (python3 -I): census5.py <logs> (the census's P07 calls, lane 4's slots and its share, calls from the streams and from
the VU memory, the STCYCL 7 / 1 tags not taken by cause, MGS2_SB_CHK's counts); cmp4.py <on log> <off log> <reference log> (as sb4's);
href5.py <hashref> <logs> (the hash lines against prof/hashref_x86_*_h2.txt, the vuh lines counted); fitcmp.py <dump off> <dump on>
<steps> <build dir> [functions] (two PspProf dumps of one build with 11.25's fitted weights: the total, each part and the named
functions, off -> on).
