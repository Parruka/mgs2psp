# gl39 plan: the main CPU in the rain wait and walk (plan only, nothing built)

Written 2026-11-22 after gl38's PSP run 1 (`prof/hw_gl38/gl38_psp_read.md`). Marks: [C] measured or read in code, [C PSP] from
gl38's PSP run, [C model] PspProf (1.65 instr + 68 I-miss + 86 D-miss) / 333,000, ms a PSP step, [I] inferred (basis named),
UNKNOWN. All work was done in the container; no picture left it. Evidence: `/home/claude/main/build_gl39/` (container, temporary);
the tools are copied to `attic/gl39/tools/`.

## 0. Three facts that change the plan

1. **Model factor for the main CPU is ~1.0, not 0.8.** The K7 turn (nothing drawn, main alone) is 70.3 [C model] against 69.6
   [C PSP]. The 0.77-0.88 factor holds for ME work. Gains below are model figures; expect x0.8 to x1.0 on the PSP.
2. **Main-only gains are capped by the ME.** Digest off, the rain wait needs main ~75.9 (73.2 own + 2.7 IOP/sound/rest) and ME
   ~70.8 [I from C PSP]. Every main ms under ~71 buys nothing until the ME falls too. gl39's reachable gain in the rain wait is
   about 5 ms a step; more needs an ME lever in the same build (F1, section 6) or the coupling (section 4).
3. **I was wrong in the gl38 report:** PspProf does have a D-miss trace by data address (`PSPPROF_DHIST=64`). I used it here
   (section 5).

## 1. Chain 10 in the steady deck (26,200-28,700)

- **The scene is the codec (radio) screen**, not a deck view: two face windows, the frequency panel, the subtitle box [C: emulator
  pictures K0 against K5 at 13 vblanks, 26,248 .. 28,648; `picdiff_c10.txt`].
- **K5 removes the right-hand face (Snake's portrait).** In all 13 pairs 12,000-12,200 pixels differ (9.2-9.3% of the screen),
  all in the box x 331-435, y 14-134; the window's frame and its noise stay, the face is gone. Nothing else changes [C].
- **What feeds it** [C twin census, `tw/cen_cen.txt`]: chain 10 = 6,608 calls a 100-vblank turn (~132 a picture) of the game's
  skinned-face program P07, built by ChainEvmObj 0x136f38 under _DG_ChainEvmChanl 0x137730 (`re/p07_builder.md`: a face is ~137
  groups, ~3,100 vertices, up to 4 bones a vertex, 1-3 texture passes). Chain 3 = 8,109 P07 calls = the other face (Otacon) [I:
  same kind, the left window]; chain 5 = 193 calls (a 4-group part); chain 4 = 628 P06 calls. In the rain, place 10 is the stage
  program: chain numbers are places in the frame's list, as gl38's report warned.
- **Cost**: -15.1 step, -15.6 ME [C PSP]; -17.6 [C model] = ME 16.6 + main 1.0. The ME's 16.6 is flat: record walk 1.36, copies
  1.26, list batch 1.21, vertex conversion 1.80 (vc_p07_c1b + c1s), lights 0.74, held-call machinery 1.2, texture and CLUT per call
  1.25, the rest in pieces under 0.5. It is per-call work (~240 us model a call), not GE fill.
- **Cheaper forms:**

| Form | Exact? | Gain, ms a PSP step | Note |
|---|---|---|---|
| Half resolution | - | ~0 [I] | The face is 105 x 121 pixels; the cost is ME work per call. |
| Fewer passes (drop passes 2/3 of 179 groups) | no, visible | UNKNOWN | The profile does not split passes. Not proposed. |
| Each face every other picture (its list kept and redrawn between) | no: face motion at half rate | ~-7.8 a face, ~-15.6 both [I: half of K5's C PSP] -> ~26 ms, 19 fps | Owner's OK needed (visual). Card candidate. |
| A. Converted-vertex cache (the model's streams are static data) | yes (same list words) | -3 to -4 model = -2.4 to -3.2 PSP a face [I] | ME work, not gl39's subject. |
| B. Unchanged-face memo (inputs equal the picture before: keep the list) | yes | share of such pictures x 15.6: UNKNOWN | A twin census answers it (step 0.4). |

- The codec is ME-limited (ME 41.5, main own 22.1 [C PSP]): gl39's main-CPU levers do not move it. A and B belong to the next
  ME build; "every other picture" is the only quick lever and it is the owner's choice.
- Side observation [C]: two emulator runs with the same kinds differed in 341 pixels by 0.4 levels at 24,784 (first deck scene).
  Emulator shots there are not repeatable to the pixel; picture proofs should use the digests.
- The 130 pictures are in `/home/claude/main/build_gl38/emu_c10k0/pic` and `emu_c10k5/pic` (container only), ready for a card.

## 2. (a) Where the main CPU's time goes

Rain wait, K7 turn 30,500 (main alone): **70.3 [C model] = instructions 44.5 + I-misses 10.3 + D-misses 15.4**. Walk K7
(30,900): 67.8 = 42.8 + 10.3 + 14.7, same order of items. PSP: logic 35.5, pipe 26.6, GS walk ~4.2 [C PSP].

| Group (rain wait K7) | ms | instr | I-miss | D-miss | PSP timer |
|---|---|---|---|---|---|
| VU0 micro programs (translated) | 8.9 | 7.9 | 1.0 | 0.05 | pipe + logic |
| io_dma_start: exact VIF1/GIF walk + scratchpad DMA | 6.25 | 5.1 | 0.25 | 0.9 | GS walk + pipe |
| libdg/screen (OnePieceObjs 1.66, StoreMatrixObjs 1.20, DG_SetObjsMatrix 0.77, ScreenObjs 0.56, BoundCheck 0.40) | 4.8 | 3.1 | 0.3 | 1.4 | pipe |
| other libdg (chanl 1.76, chain2 1.45, chain 1.37, patch 1.31, pos 1.15, prim2 1.11, pshade 0.65, vanime 0.43, light 0.30) | 9.5 | 5.8 | 0.7 | 3.0 | pipe |
| native pieces nat_* (nat_dbc0 1.58, nat_bs_part 0.61, rt_nat_p04 0.53) | 3.6 | 2.5 | 0.1 | 1.0 | pipe |
| rt_qcopy_nov (quadword copies) | 3.2 | 1.0 | 0.0 | 2.2 | pipe |
| libmt (motion) | 3.4 | 2.0 | 0.9 | 0.5 | logic |
| libhzx (collision, traps) | 3.4 | 1.5 | 1.0 | 0.8 | logic |
| sdk (libvu0 maths: ecossin 0.74, MulMatrix 0.46) | 2.7 | 1.9 | 0.7 | 0.1 | both |
| unnamed game code | 2.75 | 1.4 | 0.95 | 0.4 | logic |
| dispatch, kernel calls, interrupts (k_take_interrupts 0.61) | 2.4 | 1.4 | 0.4 | 0.5 | logic |
| sound + IOP | 1.8 | 1.05 | 0.3 | 0.5 | own timer |
| rt_store 1.15, memcpy 1.04 | 2.2 | 1.6 | 0.2 | 0.4 | both |
| stage actors (rain.c 1.04, mirror 0.72, splash floor 0.57, sky_column 0.43, ripple 0.32, ropemain2 0.28) and the rest | ~15 | | | | logic |

The profile is flat: the largest single function is 9% (io_dma_start), the largest game function 2.4% (OnePieceObjs).
Codec K7 (main alone 20.2 [C model], 22.1 [C PSP]): io_dma_start 4.4, the face builder 3.3 (ChainEvmObj 2.4), stage code 1.7.

### Levers (each behind its own switch; gains are [I] single estimates, which have been wrong by 2x here before)

| # | Lever | Today [C model] | Kind | Gain, model ms a step | Exactness proof |
|---|---|---|---|---|---|
| M1 | **Box cull by hand.** vupv_96936c0f_0: 1,003 calls x 805 instr. Of the 805, 384 are scalar clip/flag bookkeeping and 110 VFPU prefixes; 221 are the float ops. Keep the float ops and their order (its results reach game code, `b_proof_notes.md`); rewrite the clip tests on vcmp masks. Target <= 450 instr. | 4.04 | hand-tuned VFPU | -1.8 (range -1.0 to -2.4) | VU_CHECK on the twin: every call's whole VU0 state against the interpreter, 0 differ; MGS2_VFPU_STAT on the PSP; hashes. |
| M2 | **VU0 dispatch.** vuc_dispatch: 2,024 calls x 74 instr. Cache the program per call site, keep one hash compare. | 0.82 | hand-tuned | -0.4 | VU_CHECK; hashes. |
| M3 | **The exact walk in io_dma_start.** One loop (+9f0..+b20) is 518k instr a step = 2.57 ms and runs the same when nothing is drawn. Find what it still must produce when a lane already took the run (VU1 memory nobody reads?) and skip that. | 6.25 (codec 4.4) | native C | up to -2.5 (upper bound; feasibility UNKNOWN) | Twin: poison what is skipped after each run; 685 hash lines and all digests equal in both modes. Step 0.2 decides. |
| M4 | **Two pipeline functions natively.** DG_SetObjsMatrix (216 calls x 443 instr) and StoreMatrixObjs (22 x 6,790): the only two hot ones whose VFPU-rounded form passed the b-proof (gMb, gMc). OnePieceObjs, CalcSortZ, DG_Prim2Chanl failed it and stay. | 1.97 | native C + VFPU | -0.8 | The b-proof's four sums (rh, wh, wv, rv) equal in modes 1 and 2; hashes; digests in digest-on turns. |
| M5 | **Small runtime paths.** k_take_interrupts: 445 calls x 212 instr, one loop 6,600 instr a step: leave early when nothing is pending. rt_store: 3,014 calls x 70: inline the common case. | 1.76 | hand-tuned | -0.6 | Hashes (state-exact by construction; the interrupt order is unchanged). |
| M6 | **Sort keys side by side.** nat_dbc0: one load (+e0) takes 2,444 D-misses a step; HZX trap walk (0x14b008 +1e4) 1,876. Keep the keys in a small array for nat_dbc0 (ours); the HZX walk is game data and stays. | 0.65 D | data layout | -0.4 | Hashes; digests (sort order feeds the picture). |
| M7 | **Code placement for the play configuration.** Train and score the order search on digest-off K0 and on K7 windows; today's training turns include ones the play build never runs (the main CPU drawing) and no digest-off or K7 turn. Also needed so that M1-M6 do not lose their gain to moved code (`h1_notes.md`: +2.3 I-miss after natives). | 10.3 I | I-cache placement | -1.0 to -1.5 | No code change: byte-equal functions, hashes. |
| M8 | **Static data placement** (section 5). | +1.84 over gl36 in main-drawing turns, +0.62 in ME turns | D-cache placement | -0.5 on the ME side of play turns | Hashes; DHIST guard. |

Sum, M1-M7: **-5 to -7 model ms a step [I]**; with section 0.2's cap, about -5 on the PSP's rain wait.
Not levers: rt_qcopy_nov already claims its lines (`cache 0x18`, v18): its 2.2 are source misses. The game's logic (libmt, libhzx,
actors: ~25 ms in pieces under 1.2) has no single target; thinning actors is not exact and not proposed.

## 3. (b) A digest-off turn kind

- **What:** a new key `MGS2_DIG_CYCLE=<digits>`, one digit a turn like MGS2_BOUND_CYCLE: 0 = digests as MGS2_GL_DIG says, 1 = no
  frame digest for frames whose last vblank is in this turn. Three places test the range today (`psp/xl_psp.c` 432 and 2789,
  `boot/xlive_chk.c` 294/1435): each gets "and the turn's digit is 0". It is orthogonal to the bound kind, so K0 and K7 can both
  run digest-off.
- **Exactness:** state hashes do not see the digest. The twin and the emulator read the same key, so the "digests by kind"
  compare just has fewer lines; the reader must print how many were expected. The digest-off frames have no picture proof of
  their own, so **every (kind, lever arm) pair must also appear in at least one digest-on turn.**
- **Schedule, rain wait 29,500-30,600 (12 turns):** 4 x K0 digest-off levers on; 4 x K0 digest-off levers off (the twin, via
  MGS2_OPT2_CYCLE); 2 x K0 digest-on levers on (picture proof, and the link to gl38's 84.99); 1 x K7 digest-off; 1 x K7 + ME
  load (section 4). Walk (6 turns): K0 digest-off on / off, K0 digest-on, K7, 2 spare for the largest lever alone (M1).
  With n 4 and a 1.8 turn-to-turn spread, a 5 ms gain is clear; single levers under 2 ms are not, and are costed by the model.
- The steady deck already runs without digests: its figures are play figures.

## 4. (c) The coupling: main logic +7.3 when the ME works

What the data say:
- "logic" is a remainder (the step minus the timed parts, `psp/main_psp.c`), so anything untimed lands in it.
- [C model] The main CPU's own code differs by only +2.25 ms between K0 and K7: gif_qw +2.57 (that is in the GS walk timer),
  hand-over code +1.4 (mp_mark, mq_wait, mg_lines, mq_service), game code -0.6. So **at most ~1.4 of the 7.3 is code the model
  sees.** My first guess (hand-over copies on the main CPU) is ruled out.
- [C PSP] pipe is not slowed (26.2 against 25.6 / 26.6), and the effect is not additive: K1 -0.6, K2 -1.5, K4 +0.1, but K6
  -6.4 and K7 -7.3. It goes away only when the ME (and the GE) have almost nothing to do.
- [C PSP] the codec has +4.8 on a logic of 6.8 alone. A bus slowed by the ME cannot add 70% to code with few misses [I]: this
  speaks against plain bus contention and for a fixed cost per service call or interrupt.

| Cause | For | Against |
|---|---|---|
| H1 Kernel time in the queue service: mq_service runs at every transfer hook and wait (212 a step in K0, 13 in K7 [C model]); while the GE holds a list each call polls sceGeDrawSync(1); time reads rise from 82 to 592 a step. The emulator charges none of it. | Non-additive like the GE's busy time; in logic, not pipe. | Most calls sit in the timed wait [I: 200 of the 212 come with a wait round]. |
| H2 Bus contention: the ME's fills and its uncached list writes delay the main CPU's ~110k line fills a step (7.3 ms = 66 ns a fill). | Simple. | Pipe has many misses and is not slowed; the codec's +70%. |
| H3 Interrupts on the main CPU caused by the picture (GE finish/signal, ME requests). | Fixed cost per event, lands in logic. | No count yet. |
| H4 The 12 guarded game functions (rt_g0_pre) and the write filter. | Only on while records are open. | 0.3 [C model]. |
| H5 The digest's reads of every list. | The read's suggestion. | The steady deck has no digest and has +4.8. |

How gl39 times it (all read-only, exact):
1. **Split the remainder.** Count and time, per turn: mq_service calls made outside the waits, sceGeDrawSync polls, heap serves,
   interrupt entries by source, and guarded stores. Two time reads per event, only in probe turns (the probe has its own digit).
2. **K7 + ME load** (new kind): nothing is drawn and the ME runs a loop on its own buffer, in three flavours by turn: arithmetic
   only; cached reads over 8 MB; uncached writes like list writes. Main logic against plain K7 gives H2 directly.
3. **Digest-off K0 against digest-on K0** (section 3) gives H5.
4. **Model check** of each fix candidate before the PSP.

What each answer leads to: H1/H3 -> service the queue once a picture and at waits, not at every hook (exact, up to ~-6 [I]);
H2 -> cached list writes with one write-back, a later build; H5 -> nothing to fix in play. Until measured the gain is UNKNOWN
and is not in section 7's figure.

## 5. (d) gl38's retrain guard failure

Measured now: `PSPPROF_DHIST=64`, gl36's and gl38's builds, same cfg, window 29,910-29,990 (the main CPU draws)
[C model, `dh_main_turn.txt`]:
- **+9,100 D-misses a step = +2.35 ms: static data +7,140 (+1.84), dynamic +1,970 (+0.51).** The game's RAM (g_mem, 58.7k) is
  unchanged (-254). It is all in the picture code's own statics.
- Two causes: (1) **gl38's new counters and timer tables**, each on a line of its own and written in hot paths: xm_lw3_miss
  +567, ts_t +408, xl_nf_n +282, xl_nr_r +277, ud_n +252, xme_x +147 (all 0 in gl36); (2) **old hot tables that now collide**
  after .data/.bss moved: St +1,847, glf +725, F +720, R.8/R.0/R.9 +1,400, vc_km +435, C2m +421, v3.7 +366, vpv_clipt +322
  (VM -1,025, EC -316).
- In the ME turn 29,810-29,890 the same effect is +2,410 static = +0.62 ms.

Judgement:
- **Keeping gl36's placement is not possible** (the code and data changed) and not needed.
- **The failed turns are ones a play build never runs** (the main CPU drawing). In play it costs the ME ~0.5 ms [I]. The PSP
  agrees: K0 84.99 against gl36's 85.1 [C PSP].
- **Fix in gl39 (cheap, exact):** (1) the picture code's counters into one 64-byte-aligned block per CPU; (2) the hot tables
  named above into one section in a fixed order, so later builds do not move them against each other; (3) a new guard line: static
  D-misses a step by DHIST no higher than gl36's + 2% in the ME turn and in the K7 turn. Expected: back to gl36's level, -0.5 ms
  on the ME side in play [I], -1.8 in main-drawing turns [C model as the size of the loss].
- The guard itself should change: judge on digest-off K0 (both CPUs) and K7 (main alone) against gl38's own windows; keep the
  main-drawing turns as information only.

## 6. (e) F1 default-on?

- [C PSP] re-use 0: step -1.6 (inside the 1.8 spread), ME -1.8, the register stage 8.6 -> 5.9 us a ship group. [C model] -2.2,
  x0.8 = -1.8: the model and the PSP agree in sign and size. Re-use 1: +1.5 PSP, +1.7 model = noise in both.
- **Recommendation: on by default with re-use 0, keeping its switch.** Exact (digests equal by kind), and it is the one ME lever
  ready; with the digest off the ME sits within ~5 ms of the main CPU, where -1.8 on the ME decides how much of the main gain
  shows. Keep 2 turns with F1 off in the rain wait for a second reading. Re-use 1 stays off.
- It is a second change next to the main levers, but in its own turns, so the one-change rule holds.

## 7. (f) Expected rain-wait step

| State | Main needs | ME needs | Step | Mark |
|---|---|---|---|---|
| gl38, digest on | 75.9 | 84.4 | 84.99 | [C PSP] |
| gl38, digest off | 75.9 | ~70.8 | ~76 | [I]: K1 left the ME at 60 and the step at 75.8 [C PSP] |
| gl39: M1-M7 (-5 to -7 model) | 69-71 | ~70.8 | 71-73 | [I] |
| gl39 + F1 + data fix | 69-71 | ~68.5 | 70-72 | [I] |
| if the coupling is H1/H3 and removed | 63-66 | ~68.5 | 68-70 (ME-limited again) | [I], speculative |

So gl39's honest target is **~76 -> 70-72 ms a step in the rain wait (6.6 -> ~7 fps at a picture every 2 steps)**, walk similar
(main alone 66.7 [C PSP]). Out of reach by exact main-CPU work: anything under ~65. The game's own logic is 35.5 alone and flat,
the pipeline's exact parts (OnePieceObjs, the sort, the box cull's float ops) must stay, and under ~70 the ME is the limit again
(people chain 19.6, ship 16.9, chain 6 15.2 [C PSP]).

## 8. Order of work for gl39 (when the build is asked for)

Step 0, container only, no PSP: (0.1) DHIST and per-function profile of digest-off K0 and K7 on gl38's code as the baseline;
(0.2) the walk census for M3; (0.3) VU_CHECK harness ready for M1/M2; (0.4) the face census for section 1 B.
Then one lever a build step, each with its twin turn, gates as gl38's (twin 685 lines, emulator 62 + digests by kind, VU_CHECK,
b-proof sums for M4, the new guard), retrain last (M7 + M8), package, model table, then the PSP run.
Rules as before: one change a measurement, every lever its own switch, a failed proof is reported as failed.

## 9. Open points for the coordinator

1. The staged gl38 log / `rd38pc_out.txt`: per-turn main-drawing steps before 24,000 against gl36's (settles section 5 on the PSP)
   and the mq_service / flip counters by turn (first look at H1).
2. Whether a codec card (each face every other picture, section 1) should go to the owner now or after the exact forms A/B.
3. The 12-turn schedule of section 3: it gives up per-lever PSP numbers for one clear all-on / all-off pair. Say if single
   levers matter more.
4. M3 touches the exact walk: if its census shows the walk feeds anything beyond VU1 memory, it is dropped (-2.5 from the sum).
