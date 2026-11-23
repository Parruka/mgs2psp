# H1: a larger reduced draw pipeline on unshown steps (task #168)

Thread "MGS2 port", 2026-11-07 / 11-08. From halfrate_study.md's H1 ("exact larger reduced pipeline on unshown steps"). Function names
from the reference list (our function-name lists), names only. Status: ON HOLD (not proven exact; gl18 and the ME picture go first).

## What H1 is

On a step whose picture is never shown (the step after a shown one), rt_dg_call runs the draw pipeline "reduced": functions on the
reduced list return at entry. Today only P1214a0 (the rooms' meshes) is on it. H1 adds more, as long as the run stays exact: the same
pictures, IOP and time, and game code outside the pipeline reads and stores the same values.

## The six-pass cut [C, x86 twin, route i to vb 37,800]

- Added to the list: 133cc0, 1324f0 (shadow, spot), 138ac8, 138ae0 (EVM), 130390, 1303a8 (people), 13d540 (PluginActor, the sea hook),
  122138 (the sort chain). Do not cut 12dc90 (DG_Prim2Chanl: 5 frames break, a later shown packet reads its data) or 121060 (the
  transform: 1,993 frames break).
- Kept: all 17,664 pictures, IOP and time at all 378 points, and the places game code reads (rh) at all 37,799 vblanks.
- Not proven: the values game code reads differ on 98 reader keys, and the stores on 71 store-site lanes. They come from what the
  pipeline leaves for later code: VU0 registers, the scratchpad and shared memory (d00t P323c38, d01t P329668, w00a P3b7c88 read
  scratchpad leftovers; d00t P272fd0, d01t P276b38, w00a P3bebf0, w01a P3aaef8 differ at every unshown step). By DEVIATIONS.md E5's
  standard each must be shown to be a leftover no code acts on, or be left as the full call leaves it. The sorting of them was not
  finished, so whether H1 is exact is UNKNOWN; how we find out: finish logging those loads (MGS2_READ_LOG) in d01t, w00a and w01a,
  or build the form that leaves the leftovers as the full call does (cost UNKNOWN until designed).

## Cost on the PSP model [C model; PSP figures I]

PSP bucket model builds (never shipped): /home/claude/sbg0m/d (gl17's picture code, gl7's game objects, the rain's G0..G0R deck unit)
and /home/claude/sbh1m/d (the same with gen.o made with the six-pass cut, tools: sbh1m/mk_h1m.sh). Windows: rainy aft deck vb
30,810-30,890, inside 33,410-33,490, 80 steps each, rain settings as gl17 (no streaks, sheets whole, no rain sprites).

| Rainy deck, ms a step | Instructions | I-misses | D-misses | Model total |
|---|---|---|---|---|
| Whole step, H1 against today (188.77 -> 184.93) | -5.10 | +2.27 | -1.00 | -3.84 |
| Of it, the reduced calls (15.23 -> 12.44) | -3.04 | +1.12 | -0.87 | -2.79 |
| Of it, the skip steps (45.05 -> 44.49; the GS walk of the unshown list 4.1 -> 2.0) | -1.93 | +1.65 | -0.28 | -0.56 |

- The I-miss rise is code placement (the eight functions lose their PGO profile and two leave .text.hot); a build's layout search
  redoes it. So H1's upper bound is about -6.1 model ms a rainy deck step (instructions and D-misses), about -5.5 PSP [I].
- Inside: whole step -2.06 instructions, -0.66 D-misses (-2.7 model ms), +0.85 I-misses; reduced calls 7.40 -> 6.67 model ms.

## What the reduced calls still run after H1 (rainy deck, instructions + D-misses, model ms a step) [C]

Reduced calls 12.44 model ms (today 15.23); instructions + D-misses 8.90 (today 12.81). H1 removed P12e8b8 (0.75), P121c30 (0.50),
ChainObj P13bf78 (0.42), P12d910 (0.31), P13bd00 (0.13), and half the box cull 96936c0f_0 (2.04 -> 1.14). What is left:

| Function | ms | Note |
|---|---|---|
| box cull 96936c0f_0 | 1.14 | its results reach game code (b_proof_notes.md) |
| rt_qcopy_nov | 0.86 | quadword copies |
| OnePieceObjs P11fa58 | 0.78 | its VU0 results reach game code |
| StoreMatrixObjs P120860 | 0.74 | |
| nat_dbc0 + CalcSortZ P12dbc0 | 0.84 | |
| rt_store | 0.49 | |
| DG_Prim2Chanl P12dc90 | 0.35 | a later shown packet reads its data |
| DG_SetObjsMatrix P120538 | 0.35 | |
| io_dma_start, io_flush | 0.57 | |
| P12d688, P11fde8, P11f968, memcpy | 1.00 | |

Inside, after H1: 4.75 (today 5.98); the box cull 0.46 and StoreMatrixObjs 0.51 stay.

## Where this leaves H1

- Worth about 5.5 PSP ms a rainy deck step [I] if made exact, 2.5 inside [I]. Beyond it, the reduced calls hold about 9 model ms on
  the deck, much of it work whose results reach the game; no bigger lever on unshown steps is known.
- Order: gl18 (the rain's exact skips, about 7 PSP ms on the rainy deck [I], proven) and the ME picture (roadmap Next 1) first; then
  H1's exact form, and the box-cull speed-up of b_proof_notes.md ("Limits") with it.

## Tools (container, ephemeral)

Scratchpad h1m/: fnlist.py (per function, one dump), fndiff.py and fncomp.py (two dumps), tot.py, bkparts.py. Model runs:
sbh1m/modelh1b.sh (whole step), modelh1s.sh (skip steps only, PSPPROF_BUCKET=skip_now), modelh1r.sh (reduced calls only,
PSPPROF_BUCKET=g_dg_lite); outputs in sbg0m/model and sbh1m/model (h1r_*, h1q_*).

## 2026-11-23 16:40 UTC: result of the H1 proof round and the lead's decision

- Proof round (attic/h1_2026-11-23/state.md; reader list R1-R8 in its block 16:30) [C twin]: with EVM kept out of the cut (c7), the
  state hash differs at 603 of 684 points; every pass alone moves it (sort chain 603, people 502, PluginActor 149, shadow+spot 561).
  Frame digests, IOP, virtual time and VU0 state at 311 dense points stay equal. Cause: cut passes leave different bytes in packet
  buffers, chain descriptors, DMA timing words, stack/scratchpad and VU0 lanes. Reader proof: 53 of 74,009 load sites and 58 of 64,847
  store sites see different bytes; every one is ORDER (same words read in another order), LEFTOVER (unused w/z lanes) or GARBAGE-IN
  (d01t P21a798 at vb 20,145 and P335100 at 26,165-26,171 compute from scratchpad slots the passes left; no game load outside the
  pipeline sees a different byte from them on route i). EVM was the cause of two of the four always-differing keys; P3bebf0 (w00a)
  and P3aaef8 (w01a) remain as lane leftovers. Saving [I]: ~3.5 model ms a step (half the c7 passes' 7.0 inclusive).
- LEAD DECISION (Ffm 15:57 "you are the boss"): H1 is NOT built on the old path. The course change of 15:58 (plan_deck_25fps.md)
  replaces the same passes' leaf work with the GE renderer, which changes the same leftovers; the R1-R8 list is the renderer's seam
  input. The proof standard for renderer steps that cannot keep the strict hash will be the E5 reader standard with a new hashref,
  decided per step with this list (GARBAGE-IN included, with the two d01t sites checked again on the step's own twin).
