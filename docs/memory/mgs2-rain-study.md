---
name: mgs2-rain-study
description: MGS2 game-side rain study (tasks #164, #165, #167): seed and game clock facts; Ffm chose the exact skip; G0H, H3, NBS, G0C, H3R, G0R all proven exact (PC + emulator), ~-8.0 model ms on the rainy deck; their g_opt2 bits and what the next build needs
metadata:
  type: project
  modified: 2026-11-06T04:56:47.737Z
---

- Notes: mgs2/rain_study.md §1-§10; deck_costs.md §4.3. Tools mgs2/tools/dev/rain/ (bkread_r.py: BKD=<model dir>/ python3 -I bkread_r.py a b) and rain/g0/README.md (sections G0, H3, NBS, G0C, H3R and G0R: diffs against each step's private-tree files, build and run scripts, proof commands).
- Each stage image has its own seed (x*0x5d588b65+1; w00a 0x1fc954); GAME CLOCK = loop iterations (K_POLL_CYCLES 64): removing game work shifts the pad, so any inexact cut changes the game after the deck.
- KILL = upper bound (aft deck rain vb 30,810-30,890, 80 steps): 8.36 model, ~7.6 PSP [I].
- FFM CHOSE "EXACT SKIP" (card 2026-11-03 22:03). Private trees /home/claude/sbg0 (x86) and /home/claude/sbg0m (PSP model build d). g_opt2 bits, off by default, all SET in gl16/gl17 cycle words (gl17 fffff9df,ffffffdf): the next build chooses its words.
  - G0 2048 + G0H 4096, line_sph (§6): native stretch + hide bits; -3.27 model.
  - H3 8192 (§7): the other three's prims hidden; G0H+H3 vs all off -4.99 model (~4.3 PSP).
  - NBS 16384 (§8): body_sph's parts native (nat_bs_part; matrix 0x1b99f0); -0.53.
  - G0C 32768 (§9, 2026-11-06): charaspread's sets native (nat_cs_sets hooked before L2d2980, back at L2d304c; births with the real P16d358 calls, seed, polls S*(7N/2-3)-1; the 7 drawing buffers left stale). NAT_CHECK 2,462 calls 0 diff; twin hashes, time, 17,664 digests equal (raw RAM differs only in the buffers); read hash (xt4, g_rh_off around the loop): one leftover in vb 23,669 (d01t P28ff50 copies records over freed packet memory), nothing reads it; emulator = twin. Act 1.08->0.19 model (-0.88, ~-0.85 PSP); walk/picture move +-0.2 run to run (repeat pair). H3 MUST STAY ON once G0C has been on in a run.
  - Together ~6.4 model / ~5.7 PSP [I] on the rainy deck. Left: line_sph 1.15, body_sph 0.94, charaspread 0.19, rain_gas_pers 0.15 (not worth it).
  - H3R 65536 (§10, 2026-11-06): rain.c's (act 0x31af28) streak prims hidden, both sets (first set's show store 0x31afd8 keeps the hide bits; h3r_second sets bit 12 on the second set before the load at 0x31af68, undone when off). EXACT ONLY WITH MGS2_GL_STREAKS=0. -1.44 model (~-1.2 PSP [I]), all pipeline + walk.
  - G0R 131072 (§10): rain.c's vertex loop L31b3c8-L31b6fc in place in RAM (g0r_call at 0x31b3c4; last entry translated; skipped polls added per entry; second set's new buffers left stale, in the x86 hash mask). NEEDS H3R; H3R MUST STAY ON once G0R has been on. NAT_CHECK 2,462 calls 0 diff; read hash 0 of 37,799 vblanks differ; emulator = twin. -0.14 (act 1.48->1.27; 1.08 of it own code; next lever a VFPU vertex loop [I]).
  - A rebuilt w00a unit compiles changed acts WITHOUT their PGO profile (CFG mismatch with the 2026-10-26 gcda; GCC silently uses -O2, -w hides the warning): rain.c's act +0.07 with both off.
  - All six ~8.0 model / ~7.0 PSP [I] on the rainy deck (each pair in its own build).
- Model words: all off ffffc7df, all four ffffffdf; G0C pair ffff7fdf/ffffffdf; H3R/G0R fffcffdf (both off), fffdffdf (H3R), ffffffdf (both). Next timing build: these in turns after gl17's and probe 2's runs are read (plan: H3R on in all turns, G0R in turns), PGO trained with them, its own x86 state references.
- Related: [[mgs2-deck-costs]], [[mgs2-stagec-effects]], [[mgs2-routes-and-hashes]].
