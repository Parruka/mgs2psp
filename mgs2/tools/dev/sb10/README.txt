sb10: Stage C step 1, the codec faces (P07) exact on today's CPU path: T1 the texture memo's front, T2 the P07 calls held to their
transfer's end (stagec_notes.md "Step 1", task #133).  2026-11-02.
Container tree /home/claude/sb10 = sb9's tree (gl13's code); pristine copies of sb9's files in ge.orig, boot.orig, psp.orig,
runtime.orig, and of T1's state in ge.t1, boot.t1, psp.t1, runtime.t1.  sb10.diff (diff -ru against the .orig copies) holds all of it;
t2.diff (against the .t1 copies) is T2 alone.  To rebuild the tree in a new container: copy sb9's tree (tools/dev/sb9's README), then
patch -p0 < sb10.diff in its root (checked: both diffs rebuild today's files byte for byte); the scripts here go in the tree's root.  Not in the shared code yet: the next PSP build (after gl13's run is read)
carries it.

What it is (all exact; switches in g_opt2, all on by default):
- OPT2_TX_FRONT 4 (T1; MGS2_OPT2=fffffffb turns it off; x86 MGS2_TX_FRONT_CHK=1 makes "on" the check mode), ge/texcache.c tk_ufront,
  uf_put and tk_get_um's front: 32 entries (16 sets x 2 ways) keyed by TEX0 & 0x3ffffffff, CLAMP and TEXA, valid while the map's
  generation (transfers + local transfers + drawn + failed transfers) is the one the entry was made in.  A hit goes straight to the T
  entry's check with ug_get's and T's search's counters and use stamps as they would make them.  Filled after the memo's own path only.
- OPT2_C7 8 (T2; MGS2_OPT2=fffffff7 turns it off), ge/xconv.c hold7_test, hold7, place7, sv7_rebuild, xc_hold / xc_hold_pass /
  xc_held_run's P07 branches; ge/xlate.c xl_vgen, xl_vsrc, xl_vsrc_gen (lo_touch, lo_src, up_last; hooks in ud_new, ud_eager, unpack,
  call()'s template copies, xl_flush, xl_reset); ge/gelive.c gl_call (lane 4 by its own switch, the log's room), GlFrame stepc7,
  held7_n, hflush_log; psp/xl_psp.c and boot/xlive_chk.c set stepc7 from OPT2_C7 and print "P07 held (sb10 T2)".  up_last has
  division-free forms for CL = WL and WL 1 or 2 (the same values: checked for every num 1..256, CL, WL 0..255).
  A held call's record keeps, for each of the 14 slots of VU addresses 0..63 it reads, the game's data that a straight copy last put
  there (while the slot's generation matches) or a 64-byte copy in a log of 128 slots (one per slot and generation, emptied after the
  run); T + 0..3 and T + 0xfe, 0xff in the record.  At the run it is placed in the scratch memory Sv with T at 64 (a slot only when Sv
  does not hold that data already; the cache of what Sv holds is cleared at each run's start and by any P06 run), and xc_lit converts
  it there.  vc_p07_rec's -8 rebuilds the vertices in Sv (T + 0x100 + 7 i: A..D sign-extended, F's two vectors byte by byte).
  x86 checks: MGS2_C7_CHK=1 (each held call keeps the 60 quadwords call() decoded and compares them with the placed memory),
  MGS2_SB_CHK=1 (the streams' results against the rebuilt VU memory's), MGS2_SB_POISON=1 (Sv poisoned before each placement and the
  vertex area before each rebuild), MGS2_SB_FORCE=1 (every held call through the rebuild).
- psp/main_psp.c (private): cfg 24 "texture front off", 25 "face hold off" in the log's per-setting means and steps.csv.

Results: stagec_notes.md "Step 1" (x86 checks and the model).

Scripts: mk.sh <dir> the x86 twin; chk10.sh <i|d|c> <tag> [env] one route from boot (x1); codec_x86.sh the codec route; t1runs.sh
T1's sizing runs (x1 built with -DT1_SIM: ge/gelive.c's would-be second memo, t1sim.py's patch); t1chk.sh T1's checks; t2chk.sh T2's
checks; t1cmp.py <new log> <reference log> the frame, content, hash and vuh lines and the other lines that differ; mkpsp.sh <new dir>
the PSP model build (no PGO; HT the title); model.sh one window; models10.sh T1's windows on sb10m (EBOOT fbd45812), models_t2.sh T2's
on sb10m2 (EBOOT e1c979e3); pgo_gl.sh sb9's with T = sb10.  Sizing: stepc7_sim.c, stepc7_cfg.py, stepc7_trace.sh (T2's trace replay,
from tools/dev/sb2's stepc_sim); p7s.py (the output memo's measurement from xconv.c's P07_STAT lines, a dropped x86 probe).  Compare
the model with tools/dev/sb6 fitcmp.py.  Results kept here: t1_model.txt, t2_model.txt (the model windows), t2_checks.txt (the x86
checks), uplast_chk.c (up_last against the division forms for every input).
