---
name: mgs2-renderer-work
description: MGS2 GE renderer status (to 2026-10-31 evening): live picture builds gl1-gl10, Stage B lanes, PGO + layout, trims, step c, thinned rain, picture gaps
metadata:
  type: project
  modified: 2026-10-31T16:42:28.701Z
---

- GE numbers [[mgs2-ge-hardware]]; Stage A x86 [[mgs2-renderer-stage-a]]; PSP stops [[mgs2-switch-offs]].
- Notes: mgs2/renderer_notes.md §11 (11.13 live frame, 11.14/11.16 P06, 11.17-11.19 codec, 11.20-11.30 Stage B, trims, step c, rain); psp_harness.md "Build glN"; roadmap.md.
- STAGE B (11.20): lanes 1 (P1214a0) and 2 (P06) exact on 3 routes. Direct reads (11.22) PARKED. Every code change needs a PGO + layout refresh (pgo_gl.sh).
- GL9 (11.26): delivered, run 1 passed (exact, 52.1 min).
- TRIMS (11.28, sb3, tools/dev/sb3/): 5 exact trims, g_opt 0x800000..0x4000000.
- STEP C (11.29, tree /home/claude/sb4 = sb3 + tools/dev/sb4/sb4.diff): P06 calls held to the transfer's end (XcHeld 1,024 B records, OPT_XL_STEPC 0x8000000), exact on 3 routes; GL_HOLD 256 is fastest: keep.
- THINNED RAIN (11.30, task #114): cfg MGS2_GL_RAIN=N / MGS2_GL_SPLASH=N draw 1 strip in N of the camera drop sheets and the foot splash (ge/fxthin.c); picture only, exact; no step time saved today. Ffm chose 1/2 (16:58: "if we need more performance we go 1/3"); the rain streaks (unconverted line strips) come in at the same share. Shots: Ffm's SHOTS\rain_and_splash.
- GL10 (11.31, psp_harness "Build gl10"): /home/claude/gl10 (EBOOT 45000224), work dir /home/claude/sbg10 (pgo_gl.sh <step> <dir> [seed|k]); layout_order.py lists ge_fxthin.o. Turns 1000:100:ffffffff,f7ffffff,ffffffff,f87fffff (both offs show as cfg 4; split by vblank). RUN 1 PASSED (files 19:21, prof/hw_gl10/run1, shots /home/claude/hw_gl10/shots): exact, 49.6 min; inside 96.34 (-7.3%), waiting deck 160.16 (-5.4%); step c deck 6.0 / inside 7.5 ms, trims deck 4.6; lowest heap headroom 612 KB (gl9 936): budget memory in the next lanes. Reader tools/dev/sb4/g10win.py.
- GL11 READY (11.34, psp_harness 'Build gl11'): lanes 3 (effects) + 4 (codec faces) with PGO + layout; /home/claude/gl11, work dir sbg11; exact on x86 turns, 1.20.4, master; model deck 147.6 (-4.2%), inside 94.1 (-3.1%), codec 89.5 (-9.3%) vs gl10.
- GAPS: #84 stale text page + row-220 line; blend 0x6a, radar map, grey viewfinder (#68); opening cutscene black; rain streaks not drawn.
- CHECK PICTURES ON MASTER: master + tools/ppsspp_master_{vfpu_rn,fpu_fs}.diff = the PSP's shots byte for byte.
- RULES: PspProf runs need PPSSPP_VFPU_RN=1; a PspProf window is not exact; shots before a window shift it ~0.2 ms (compare shot runs with shot runs); MGS2_GL_SHOT max 16.
- Not taken (visual, Ffm's call): faces at 15 fps, finer depth; later (§11.7): 4:3 or stretch, trail, pacing, 4 MB eDRAM. GE skinning/lighting for P07 is now C1: built 2026-11-02 behind a switch (off by default), waits for Ffm's OK on shots ([[mgs2-stagec]]).
