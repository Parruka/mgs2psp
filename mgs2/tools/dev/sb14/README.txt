sb13 + sb14 (task #144, Stage C step 3: the rooms' meshes, P1214a0 / df0; stagec_notes.md "Step 3").

Tree /home/claude/sb14 (ephemeral) = /home/claude/sb12 (gl15's tree, tools/dev/sb12/sb12.diff) + sb14.diff (checked 2026-11-02 23:37, with 3w:
applied to sb12's ge/ boot/ psp/ runtime/ it gives sb14's files byte for byte).  It holds both steps:
- sb13 (the census, x86 only): ge/c3model.c / .h, called from ge/xconv.c xc_call (c3m_call: reads only) and boot/xlive_chk.c.
  MGS2_C3_MODEL=1 the census lines ("c3 census", "c3 keys", "c3 streams", "c3 lru", "c3 depth", "c3 window"); MGS2_C3_WATCH=1 the
  write watch (the stream pages write-protected, each store sorted: A positions, B colours, C UVs, or a page neighbour); MGS2_C3_FRAMES=1
  the per-frame lines.  Results: census_c3.txt (the three routes, run c3), watch_d.txt / watch_i.txt / watch_c.txt (run w, by writer).
- sb14 (three exact trims, on by default; g_opt2 bits in runtime/host_time.h):
  3v OPT2_C3V 64: a lane's df0 call in two passes (ge/vu1conv.c vc_df0_pos: header, transform, positions, box, the tests; vc_df0_att:
     UVs, colours, flags, only for a call the cut keeps; vc_df0_flag: one flag from stream B), ge/xconv.c xc_call.
  3z OPT2_C3Z 128: a df0 call's depth range by its positions' box when the box lies inside the range kept for its projection
     (ge/xconv.c xc_df0_zin, xc_zr_df0d; the box from vc_df0_pos or from xc_df0_off's loop).
  3w OPT2_C3W 256 (with 3v): both passes write the call's vertices straight into the frame's room after its GerXform, where xc_df0
     would copy them (ge/xconv.c xc_call: Dvp; xc_df0 skips the copy when they are there already).
  MGS2_C3_CHK=1 (x86 only): every two-pass call against vc_df0_rec (count, transform, vertices, flags, box) and every depth range by
  the box against the vertex loop on a copy; the "df0 C3" line counts them ("checked", "different").
  psp/main_psp.c: the log's settings 28 "mesh 2-pass off", 29 "mesh box depth off", 30 "mesh trims off", 31 "mesh
  frame write off".

Scripts: mk.sh <dir> the x86 twin (x1); chk14.sh <i|d|c> <tag> [env] one route from boot; c3x.sh (sb13's census runs, three routes in
parallel); c4x.sh <tag> [env] sb14's three routes in parallel; c4runs.sh the checks t0 (trims off, MGS2_OPT2=ffffff1f), t1 (on, with
MGS2_C3_CHK=1), t2 (on), t3 (on, with MGS2_SB_CHK=1); c4cmp.py <a.log> <b.log> [ref.log] every line equal but the settings' own, the
hash lines against a reference (sb12/x2's out_*t15.log); mkpsp.sh <new dir> the PSP model build (no PGO: /home/claude/sb14m/d, EBOOT
8537e496); models_c3.sh the model windows (deck 30,810-30,890 and inside 33,410-33,490: trims off, 3v alone, both on).
3w: c4x3.sh <tag> [env] as c4x.sh on the twin x3 (with 3w); c4runs3.sh its checks t4 (all on, MGS2_C3_CHK=1) and t5 (all on);
models_c3w.sh the model windows on /home/claude/sb14m2/d (with 3w, EBOOT cfb5eed4): 3w off (MGS2_OPT2=fffffedf) and all on.
Game data passes through all of it: personal use only.
