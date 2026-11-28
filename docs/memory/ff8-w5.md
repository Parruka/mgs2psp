---
name: ff8-w5
description: FF8 dev after p6 (2026-11-18 05:00-): world map w4 (GE wrapped-palette limit) + w5 (flat span, faster index scan, fast fill/copy), all checked; sources src_w gpu13.c + ge6.c; model figures; r2 layout in progress
metadata:
  type: project
  modified: 2026-11-18T05:55:56.867Z
---

Follows [[ff8-p6]] (p6 READY, not yet delivered; no build ships before p6's log is read). Container work dir psp/src_w, layout psp/lay_r2, analysis tools psp/lay_o1 (wsim.c, osim.c).

**w4 (ge6.c)** [C emulator]: 179k of 354k world map primitives a window were not queued for the GE: the 8-bit palettes at x 832/896/960 row 193 wrap past x 1023 into the picture buffer's row 193, so the drawing area 0,0-319,223 "met a palette in use". Now a GE palette slot counts only lim = 1024 - x entries while every primitive using it reads only those (need = exact side's texel scan max + 1, global gpu_host_pal_n set in gpu10+ lz_pal8; 256 when unknown); a primitive needing more is not queued and makes the slot whole (dirty). cfg "env GEPLIM 0" = old way. p4 session: out = p4's, card 0d48a931, VSUM 49/49, every [ge] check line and differ sample = p6's; queued 175k -> 351k.

**w5 (gpu11-13.c)**, each with a check build comparing old vs new on the 5 replays (route, p1, p3, fx1, p4 session; all exact): flat untextured span in draw_tri_core mode 2 (the strip's 3 grey see-through quads, 12,288 px a frame; 75,597 records compared, 0 differ; cfg env LZFLAT 0 = old); lz_idx_range word-at-a-time int min/max (353,253 scans, 0 differ); fill rows two px a store and VRAM copy rows by memmove when no x wrap and no mask bits (24,769 fills, 329,357 copies, 0 differ; env CPSLOW 1 = old). Check variants gpu11c/12c/13c.c.

**Model [C model] p4 session, l6q layout (SHIFT 0x1a000 needed now):** world map w3d 85.76 / 78.05 -> w4a 74.95 / 67.74 -> w5 67.00 / 61.60 (p6 with r1 layout: 80.06 / 73.37); other windows +-0.5. Profiles run_dk/w4a_prof, w5_prof.

**r2 layout (06:40):** lopt on tr3_all.bin 1.112 M -> 0.916 M misses; lay_r2/r2.x, w5 objects (lay_r2/vy_w5: r_gpu 87bab9a8, ge 764802e7, gte 7c2e4641), build lay_r2/r2_bin EBOOT c14926b1 (g_emuprof 0a09fbc0), 740 units where planned, five replays exact; profile run_dk/r2_prof.
**w6-w11 (src_w gpu14-19.c, 2026-11-18 06:00-08:00, all exact: five replays + [vsum] 49 = p6 eager, check variants 0 differ):** w6 palette zero scan by words; w7 VRAM write log (kept scans revalidated by logged boxes, env LZWLOG 0); w8 bigger scan caches (64 / 256, log 128); w9 cover map off per region NOT KEPT (fight 39.7 -> 46.4); w10 cover map marked inside tri_time's walk (lz_cov_pend; check gpu18c 3.9 M triangles 0 differ); w11 whole-region fill owed (env LZQFILL 0). Current source gpu19.c (b_w11, l6q). Layouts: r3 = w8 + layout (lay_r3/r3_bin d56ff045: WM 61.38 / 55.60, title 64.0, rooms 20.2 / 22.7, fights 37.2 / 42.3 [C model]); r4 for w11 in progress (lay_r4, trace5_bin, lopt5). Notes draft scratchpad wm/notes_w6.md (not yet in stage5_notes).

**Findings for later:** world map overlay game code = 441 KB of lines touched in 6 frames (PS1 module 181 KB; a PS1 call site = ~22 PSP instructions); per-function sections in genasm would allow placing game code (exe + overlays) by trace. Runtime lines touched a world map frame ~135 KB: capacity, not placement, is the wall; FA-LRU bound already beaten.
