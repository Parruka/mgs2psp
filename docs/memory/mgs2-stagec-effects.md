---
name: mgs2-stagec-effects
description: MGS2 Stage C step 4 effects (OPT2_C4/C4X): checks, cost, owners, rain streak/sheet options, the rain sprites filter, heap fix, Ffm's picks and the open card, gl17 tree
metadata:
  type: project
  modified: 2026-11-03T18:20:00.000Z
---

- STEP 4 BUILT 2026-11-03 12:00 (tasks #152/#153; sb15 = sb14 + tools/dev/sb15/sb15.diff; stagec_notes.md 4.5-4.12): OPT2_C4 512 draws P04+3740 sprites, +3a38 quads, +2b60 streaks from PS2 clip positions; OPT2_C4X 1024 sends +25f0/2870 strips exact (24-bit GE matrices up to 4.6 px off in the opening). Both OFF by default (OPT2_DEFAULT fffff9df; cfg 32 C4, 33 C4X, 34 both). OPT2_DEFAULT = ~bits: every new bit must be left out of it.
- Checks: all off = gl16; both on: hashes equal.
- Owners (census corrected 12:46, notes 4.9): streaks = the rain (rain.c, rain_slow.c); sprites/quads = glows, smoke etc. (body_sph.c, fade_lgt.c, bonbori, c_light, chim_smo.c...); no-block streak calls in the opening (owner unknown) are never thinned.
- COST (notes 4.12, model, aft deck rain vb 30,810-30,890): plan of 12:46 (streaks 1/2 by strips, MGS2_GL_STREAKS N) +15.2; strip thinning saves ~0.3 only; whole-call thinning MGS2_GL_STREAK_CALLS N (patch_calls.py, by block (src-lo)/0x1290 % N) exact; no streaks -4.6. Sheets whole +0.26 ms CPU; gl16 GE wait 0.00 on the deck.
- Shots SHOTS\c4_effects\streaks_half, rain_options. FFM CHOSE "NO STREAKS" (card, 17:38): gl17 cfg MGS2_GL_STREAKS=0, MGS2_GL_RAIN=1 (sheets whole), no MGS2_GL_STREAK_CALLS, splash 1/2.
- RAIN SPRITES FILTER (task #161, notes 4.13): cfg MGS2_GL_RAIN_SPRITES=0 leaves out sprite calls in blocks of body_sph (+0x78), charaspread (+0xa8), rain_gas_pers (+0x184), line_sph (0x50-byte table from +0xa0) (tools/dev/sb17/patch_rainspr*.py); exact (hashes = gl16 on x86 and master); saves 5.26 model (~4.3 PSP [I]) on the rainy aft deck: gl16 165.43, Ffm's choice 175.68, + filter 170.42. Looks: spray on clothes (24,784), deck/opening haze, drops on Snake inside vb 31,663-32,197 (charaspread). Shots SHOTS\c4_effects\rain_sprites (48 PNG checked, README 554edc1a). FFM CHOSE "LEAVE THEM OUT" (card 18:19): gl17 takes MGS2_GL_RAIN_SPRITES=0.
- HEAP FIX (task #159 done, notes 4.14, tools/dev/sb17/patch_trim159.py): malloc_trim(0) after a texture-table growth at the heap top; master s17t159 = s17r exactly, break peak 0b849000 at vb 23,675 (88 KB lower, = gl16's moment); gl17 PSP headroom ~360 KB [I].
- gl17 (task #156) after gl16ep's run: tree /home/claude/sb17 (sb15 + me14 local-copy fix + patch_none + patch_calls + patch_rainspr + patch_trim159), cfg 34 / 0 in turns + the rain per the cards, PGO with them in training. Costs by owner, options: [[mgs2-deck-costs]].
- Masters /home/claude/msm/PSP/GAME/MGS2M_{x4a,s4a,s17h,s17t,s17n,s17c3w,s17r,s17n2,s17r2,s17t159}; tools mcmp.sh <tag> <ref tag>, pairshots.py, c4master.sh; shots /home/claude/c4shots. Related: [[mgs2-stagec]], [[mgs2-renderer-work]].
