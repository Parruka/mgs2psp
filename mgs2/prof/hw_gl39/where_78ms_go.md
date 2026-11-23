# Where a rain-wait step's ~78 ms go (gl39, play form), 2026-11-23

Asked by Ffm 14:52 UTC ("i want to know where the 79ms goes"). Main CPU only: it is the limit in the rain wait (the ME is busy
70.6 ms of the step and waits for it) [C PSP, gl39_psp_read.md].

**PSP [C]:** play form (digests off) 78.1 / 78.2 / 84.4 ms a step in three turns (mean 80.2) = logic 45.2 + drawing pipeline 27.0 +
GS walk 4.7 + rest ~3.4 (sound's own timer, frame start and end). With nothing drawn (K7): 67.1 = 34.4 + 25.8 + 4.0.

**Model split [C model]** of the same window (build_gl39/model/ka39.1 = turn t56, K0 digest off, vb 29,610; ms a PSP step;
grouped by scratch cat79.py from prof39.py with gl39's ELF and sides.py): **76.7 ms**, against 78.1 measured (ratio ~1.02).
Of the 73 ms outside the copy plan: instructions 46.2, I-cache misses 10.4, D-cache misses 16.3 (about a third is cache waits).

| # | What | ms | Biggest parts | PSP timer (approx.) |
|---|---|---|---|---|
| A | Characters and game systems (motion, collision/traps, maths, actor system, scripts, unnamed game code, character stage files) | 17.8 | libmt 3.41, libhzx 3.20, unnamed 2.71, sdk 2.55, libgv 1.05, game 0.92 | logic |
| B | Rain, splashes, smoke, glints (game code + our exact natives) | 3.9 | rain.c 1.04, nat_bs_part 0.59, d_splash_floor 0.48, nat_ls_prim 0.45, ripple 0.32, nat_ls_flush 0.31 | logic |
| C | Other deck scenery: reflections, sky, ropes, lamps | 2.0 | mirror.c 0.73, sky_column 0.44, ropemain2 0.31 | logic |
| D | Building the PS2 drawing lists, game code (libdg passes) | 16.7 | screen 4.68, chanl 1.79, nat_dbc0 (sort keys) 1.56, chain2 1.45, chain 1.36, patch 1.32, pos 1.15, prim2 1.09 | pipe |
| E | PS2 vector-unit (VU0) programs run as C | 8.7 | box cull 4.14, slerp (animation) 1.62, dispatch 0.82 | pipe + logic |
| F | Emulating the PS2's list transfers (DMA/VIF walk) | 7.7 | io_dma_start 6.08, io_flush 0.55, gif_qw (main share) ~0.44 | GS walk + pipe |
| G | Memory copies and checked stores | 5.7 | rt_qcopy_nov 3.04, rt_store 1.13, memcpy 1.03 | pipe + logic |
| H | Planning the Media Engine's copies (main CPU) | 6.3 | xc_merge 2.57, xm_plan 1.98, xc_add 1.04, me_memcpy share ~0.7 [I] | logic |
| I | Handing pictures to the Media Engine (queue, line marks) | 1.6 | mp_mark 0.34, mg_lines 0.29, mq_service 0.43 | logic |
| J | Sound and the IOP | 2.2 | render_block 0.56 | own |
| K | Dispatch, kernel calls, interrupts | 2.3 | k_take_interrupts 0.61 | logic |
| L | Other runtime | 1.8 | many under 0.15 | mixed |

Notes: prof39.py merges functions by name, so its gif_qw 2.95 includes the ME translator's static gif_qw (~2.51, moved out of F as
sides.py does). The PSP timer column is from gl39_plan.md §2 and is approximate (VU0 programs, copies and libdg/pos run under both).

**Levers on these rows [I, single estimates]:** H: gl40's XP (-1.7 to -3.4 PSP). D/E/F/G on unshown steps: H1 (-3 to -5.5),
needs its exactness proof first (gl40_plan.md §2). B: the keep-the-dice cut (-3; -5 with visible losses; hd_study §1.4). A: exact
game logic, flat (no function over 1.2 ms), no single target. Cache misses across all rows: placement (gl40, mostly outside the deck).
