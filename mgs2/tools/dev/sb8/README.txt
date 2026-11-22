sb8: sb6 (tools/dev/sb6: gl11's code, i.e. lanes 1 to 4, the trims, step c, the thinned rain and splash, the lanes following g_opt per
transfer on x86 as on the PSP) + the lanes' bookkeeping made cheaper (task #121, renderer_notes 11.35), 2026-11-01.  Not in the shared code
yet.  Container tree /home/claude/sb8 = /home/claude/sb6 with tools/dev/sb8/sb8.diff applied (diff -u over the 5 files it changes:
ge/xlate.c, ge/xlate.h, psp/xl_psp.c, boot/xlive_chk.c, runtime/host_time.h; in a copy of sb6's tree: sed 's|^--- sb6/|--- a/|;
s|^+++ sb8/|+++ b/|' sb8.diff | patch -p1), then the scripts here over sb6's.  Exact on x86 and in the model: renderer_notes 11.35.

Two switches, both on by default, both exact (the same VU memory, descriptors and own[] entries), for the PSP's timing turns:
- OPT_XL_EAGER 0x40000000 (MGS2_OPT=bfffffff turns it off; x86 MGS2_XL_EAGER=0): ge/xlate.c ud_eager.  The lanes' constant blocks (lane
  1's and lane 2's block and materials, s < 0, and their CNT headers; lane 4's k >= 6; lane 3's k >= 2) are copied into VU memory as they
  arrive when they take the one form the census found for all of them (V4-32, no mask, MODE 0, CL = WL, not wrapping), and their quadwords
  owned by VM again (own 0).  Any other form, or off: ud_new, as gl11 (the lazy decode).  The streams always wait, as before.
- OPT_XL_MARK 0x80000000 (MGS2_OPT=7fffffff; x86 MGS2_XL_MARK=0): ge/xlate.c ud_new's own[] marking.  WL 1 strides whose last
  quadword stays below 1024 are marked four entries a turn, and WL 2 strides with CL > 2 two entries a turn, without the wrap test;
  the same descriptor and own[] entries.  Anything else, or off: as gl11's ud_new.  A first form that moved ud_new's usual case into a
  leaf function (ud_lean, PSP model build sb8n) cost 8 instructions more a call and was dropped (renderer_notes 11.35).

Scripts: mk.sh <dir> (the x86 twin), chk8.sh <i|d|c> <tag> [env] (one route from boot, lanes 1-4, rain and splash 1/2), mkcov.sh <dir>
(mk.sh with line counts in ge/xlate.c), mkpsp.sh <new dir> (the PSP model build, no PGO), model.sh <tag> <a> <b> <in|codec> [cfg]
(GL3=/home/claude/sb8b by default: the final form's PSP model build), models8b.sh (the 8 model windows of 11.35), models8n.sh (the
dropped leaf form's, on sb8n), models8.sh (the first version's, on sb8m).  The checks use sb6's dig_x86.sh / digc_x86.sh and cmp6.py,
and tools/dev/sb6/turnchk.py.

sb6: sb5 (tools/dev/sb5: gl10's code, i.e. lanes 1 and 2, the exact trims, step c and the thinned rain and splash, + lane 4, the codec
faces) + Stage B lane 3, the effects (the sort pass's prims, P12d910; renderer_notes 11.33, task #118), 2026-10-31.  Not in the shared
code yet.  Container tree /home/claude/sb6 = /home/claude/sb5 with sb6.diff applied (diff -uN per file over the 10 files it changes; apply
it with patch -p1 inside a copy of sb5's tree, then copy the scripts here over sb5's).  The checks and the model are in renderer_notes
11.33 "Built and checked".  gl11 is to carry lanes 3 and 4 (Ffm, 2026-10-31 21:11).

Lane 3 (OPT_XL_LANE3 0x20000000, on by default; MGS2_OPT=dfffffff turns it off; x86: MGS2_SB_LANE bit 2, 15 = lanes 1 to 4, 11 = sb5's
lanes 1, 2 and 4): where the TTE words of a VIF1 tag are STCYCL cl 3, 6 or 12 / wl 1 and UNPACK V4-32 to T + 0x100, the walk
(runtime/ee_io.h lane_fx in vif1_chain_fast) checks a prim as the sort pass's builder writes it (re/mesh_packets.md 7, corrected: 10
quadwords, 6 tags at quadwords 0, 1, 2, 3, 8 and 9: REF the positions, V4-32 n at T + 0x100; REF the attributes, V4-16 signed 2 n at
T + 0x101 with STCYCL 3 / 2, or 12 / 4 after cl 12; REF 8 the owner's block, V4-32 at T + 0xe8; CNT 4 the matrix, V4-32 at T + 0x10, its
data inline; REF 3, V4-32 at T + 0x14..0x16 (not 0x13); the last four with STCYCL 1 / 1; CNT 0 with NOP and MSCAL; each REF's UNPACK
takes exactly its data; no IRQ bit; the prim in one run of RAM or scratchpad) and hands it whole to the translator (ge/xlate.c
xl_lane_fx: every UNPACK registered for the lazy decode as xl_vif1_span would, the same St counters and census; one call with XlRec lane
3), prim after prim while the next tag starts one.  call() decodes T + 0x10..0x15 and abs 2..5 for the entries with a converter (+25f0,
+2870); xc_fpos reads the positions and the attributes from the game's data (ge/vu1conv.c vc_fpos_rec: vc_fpos's arithmetic on the
streams, V4-16 signed plus ROW where the UNPACK was MODE 1; the clipper's input and the colour's w through fx_attr) when both were sent
with at least the header's count of vertices in the cl 3 layout, else (and on -8) vc_fpos on the decoded VU memory (xl_vm_sync).  The
thinning's XlCall.src is the positions' guest address, as the generic walk sets it.  After the prims the walk's state is the generic
walk's (STCYCL 1 / 1, no data left, the last command the MSCAL; x86: the command counters).

Files: runtime/ee_io.h (lane_fx, its dispatch in vif1_chain_fast, x86 lane3_miss and lane3_gen), runtime/host_time.h (the bit),
ge/xlate.h, xlate.c (XlCall.lane, xl_lane_fx, call()'s lane 3 branch, xl_lane_groups[3]), ge/vu1conv.h, vu1conv.c (vc_fpos_rec_ok,
fpos_rec_loop, vc_fpos_rec), ge/xconv.h, xconv.c (xc_rec3_calls / vm / n / diff, fx_attr, xc_fpos's lane 3 path), boot/xlive_chk.c
(MGS2_SB_LANE mask 0xf, the mode line, the "sb lane 3:" totals line, the census by lane: "sb census:" lines; MGS2_SB_CHK=1 covers lane 3:
each lane 3 call's vertex inputs compared with the decoded VU memory and its outputs with vc_fpos's; and a fix, x86 only: glf.ram set at
each transfer the translator takes, so that the thinning's scan has the RAM on the first frame of a translator started late: the digest
runs from vb 30,800 stopped there before, lane 3 off and on alike), psp/xl_psp.c (OPT_XL_LANE3 -> lane bit 2).

Scripts (T = /home/claude/sb6): mk.sh <dir> the x86 twin; mkpsp.sh <new dir> the PSP model build (gl7's build with this tree's objects,
no PGO for the picture code; sb6's: /home/claude/sb6m/d, EBOOT md5 d6f888f2..); model.sh <tag> <A> <B> in|codec [cfg lines] one PspProf
window of it (gl3.sh's model step, GL3=/home/claude/sb6m; lane 3 off: the cfg line MGS2_OPT=dfffffff); run6m.sh the six windows of 11.33
and fitcmp.py by pair, one after another (sb6 ran its codec pair so); run6m2.sh the deck and inside windows' four runs side by side,
each in its own PPSSPP game folder (PspProf counts the emulated machine, so the host's load changes nothing), then fitcmp.py for d and i;
the emulator run of the whole indoor route with gl10's delivery cfg is gl3.sh's (GL3=/home/claude/sb6m TAG=s6 sh psp/gl3.sh emu hw),
and emuchk6.sh (bash) compares it with gl10's (hwg10) and sb5's (hws5): hash, vuh and picture lines, the frame digests against hwg10
and the x86 twin's indoor run (gl1_cmp.py), the shots byte for byte, the memory lines;
pgo_gl.sh sb5's PGO + layout script with T = sb6's tree (not run for sb6).  chk_x86.sh <i | d | c> <prefix>: from
boot, three runs side by side on the twin x1, lane 3 off (MGS2_SB_LANE=11), on (15) and on with MGS2_SB_CHK=1, rain and splash 1/2 as
gl10: the indoor route to vb 37,800, the deck tutorial to 25,200 or the codec route to 40,000; chk2_x86.sh the same with
MGS2_HASH_DENSE=15700:16010 (the PSP cfgs' dense hash and vuh lines).  thin_x86.sh <i | d> <prefix> <N>: lane 3 off and on with the rain
and splash at 1 in N and MGS2_THIN_CHK=1.  dig_x86.sh, dig2_x86.sh <prefix> <from> <to>: the per-call digest files (ge/xldig.c) off and
on, indoor route, then cmp; digc_x86.sh, digc2_x86.sh: the same on the codec route.  run6.sh <A | B | C | D>: the batches of 11.33's
checks (A indoor + deck, B codec + the digest files, C the thinning at 1/2, D at 1/1), each into x1/res_<batch>.txt.  codec_x86.sh the
codec route; probe_x86.sh MGS2_SB_PROBE's listing of one vblank's VIF1 tags (dev).  Dev tools (python3 -I): census6.py <logs> (per effect
type and entry the census's calls by lane, lane 3's prims, its calls from the streams and from the VU memory, the prim tags not taken by
the first failed test and those the generic walk took by why, MGS2_SB_CHK's counts for lanes 1 to 4); run6e.sh batch E, after the x86
fix: the digest files again and the indoor run with lane 3 on against batch A's; gdbdig.sh <tag> <lane mask> the digest run under gdb
(how the stop was found); prims6.py <probe log> the effect prims of an MGS2_SB_PROBE listing by tag pattern (the layout of 11.33's
results); modelchk6.py <model dir> <sb5 model dir> the model runs' hash, vuh and memory lines; cmp6.py <on log> <off log>
<reference log> (cmp4.py reading .gz logs); tl6.py <log> <logs> (the lines cmp6.py leaves out as timings: which differ, and whether only
in their numbers); href6.py <hashref> <logs> (the hash lines against prof/hashref_x86_*_h2.txt, the vuh lines counted); fitcmp.py <dump
off> <dump on> <steps> <build dir> [functions] (two PspProf dumps of one build with 11.25's fitted weights: the total, each part and the
named functions, default lane 3's, off -> on).
g12win.py <gl11 run 2 log> <gl12 run 1 log>: the gl12 PSP run's 100-vblank windows against gl11 run 2's (all-on windows one for one; copy off k%4 1, marking off k%4 3 against their all-on neighbours); psp_harness.md "Build gl12".
g12steps.py <gl11 steps.csv> <gl12 steps.csv> [from to]: the same game steps (exact runs) gl12 - gl11 by part, by gl12's setting and the step's place in its window (inside 31,801-37,800 by default).
