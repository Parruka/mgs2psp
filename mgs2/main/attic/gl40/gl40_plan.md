# gl40 plan (2026-11-23, planning agents 1 and 2; PLAN ONLY: nothing built for the PSP, nothing packaged)

Marks: [C] measured or read in code, [C model] PspProf cost model, [C twin] the x86 twin, [I] inferred (basis named), UNKNOWN.
State log: attic/gl40/state.md (blocks 1-4). Scratch (container only): scratchpad gl40/ (sides.py, callers.py, mod40x.sh, rl40y.sh)
and gl40b/ (ann.py, fnsum.py, the instrumented twin copy tw/, src/xs_stat.inc, xpstat_*.txt).

## 0. Summary

| Item | Finding | Lever | Expected PSP gain [I] | gl40? |
|---|---|---|---|---|
| 1 copy plan | The "unknown ~8 ms" of drawing-turn logic is mostly the main CPU's copy plan: 5.58 model ms a step + ~0.7 more hidden in me_memcpy (its sort's copies) | XP: join each range with any of the last 8 (not only the last) + a merge sort that swaps buffers instead of copying back. Same extents by construction; twin: equal in all 198,799 plans | 1.7-3.4 ms a rain step (model -1.7..-2.6 x 1.0-1.3) | YES, behind MGS2_XP_CYCLE with on/off turns and a check mode |
| 2 H1 | Proof unfinished: 98 reader keys + 71 store lanes; the reference (hd_study 7.1) says the cut passes' outputs have no game reader except EVM's world copy and PluginActor's flag/patch world | Drop EVM from the cut, check PluginActor, classify the keys by reader, re-run on today's twin | 3-5.5 rain step (gl17 model, scaled) | NO (not proven; state hash may move) |
| 3 placement | Main-drawing turns before 24,000 slower since gl38: code order (I +0.6..+0.95 model) and data placement (D +0.4..+1.2) | Retrain with guard windows before the deck that bind (code part); a small pad search after the link (data part: the heap at gl36's offset alone, c3, helps 2 windows and hurts 1) | 0.5-1.2 ms in those turns | YES (part of the retrain gl40 needs anyway) |
| 4 M5 | gl39's M5 sat in runtime/kernel.h; the PSP compiles rt/kernel.h. The "+17.7 KB" was the header's path, not M5 | M5 into rt/kernel.h as a noinline k_take_m5 + a log counter | ~0.1 (model 0.11): below one turn's noise | YES (shown by symbol + counter; time UNKNOWN) |
| 5 other | No other exact main-CPU lever >= 1 PSP ms found for gl40. Fused walk (XP-b) ~1.0-1.4 model: gl41. Dice cut ~3 PSP: later build. Lamps ~0.3, not hash-exact: no | - | - | NO |
| 6 schedule | 74 turns as gl39, XP on/off pairs in the flat rain wait, a plan probe in every turn | MGS2_XP_CYCLE, MGS2_PROBE10 | K0 play form 78.1 -> 74.7-76.4 | - |

## Units (a conflict with the brief)
The brief says model script figures in "ms a step" are ms a picture and must be halved. Checked [C]: score39.py and sides.py divide a
window's cost by its 80 VBLANKS (= 80 game steps; 40 pictures). mq_frame runs 0.5 times a step in the profile, and gl39's K7 turn is
68.28 model against 67.06 on the PSP (t57). So these figures are already per PSP step and are NOT halved anywhere in this plan. The
halving applies only to a script that divides by pictures; none was found among those used here.

## 1. The copy plan (item 1)

### 1a. Where the time goes [C model, build_gl39/model/ka39.1 = t56 K0 digest-off at 29,610, ms a PSP step]
- Run by the main CPU at each recorded transfer's end (xlive_chk_end -> mq_chk_end -> mq_plan -> xm_plan), outside every timed part:
  xc_merge 2.572 (instr 2.185, D 0.376; 14.6 calls, 7.3 of them sorting, 40 passes, ~22,300 element moves), xm_plan 1.976 (the
  tag walk: 3,446 tags, 992 D-misses on tag reads, + extents and arena), xc_add 1.035 (6,282 calls), = 5.58; mp_mark 0.343 besides.
- Hidden in "shared": xc_merge copies its scratch back after every pass with me_memcpy (40 calls a step; 22,300 x 8 B = 178 KB;
  me_memcpy's loop is 11 instructions per 16 B) = ~0.61 ms of instructions + some D-misses: 0.65-0.9 [I, from the loop's code and
  the byte count]. So the plan costs the main CPU about 6.2-6.5 model ms a step.
- Against the PSP: K0 - K7 logic +10.72 PSP [C gl39] vs model main drawing work 7.8 + 0.7 = 8.5: ratio ~1.26.

### 1b. What the plan's ranges look like [C twin; instrumented copy of gl39's twin, MGS2_NAT_LW=2 runs the PSP's xm_plan on every
lane transfer; 3 runs, each 685/685 hashes = hashref_gl38]
| per step (100-vb window / 100) | 29,600 rain wait | 30,900 walk | 21,100 | 16,100 |
|---|---|---|---|---|
| plans | 6.0 | 6.0 | 7.0 | 9.5 |
| xc_add calls | 6,441 | 5,805 | 7,567 | 4,730 |
| rb ranges into the merge (largest plan) | 2,175 (1,308) | 1,857 (1,638) | 2,556 (6,950) | 1,084 (1,104) |
| rb extents out (the union) | 362 | 352 | 531 | 61 |
| bottom-up sort moves | 24,876 | 20,873 | 31,403 | 12,162 |
| ranges left if each joins any of the last 2 / 4 / 8 / 16 / 32 | 1,635 / 1,152 / 764 / 639 / 528 | - / 998 / 639 / 542 / 481 | 2,430 / 1,731 / 1,091 / 897 / 757 | 1,083 / 961 / 481 / 117 / 98 |
| sort moves after the K-8 join (natural merge) | 6,655 | 5,619 | 10,460 | 3,679 |
| exact duplicates dropped by a hash (n left, moves) | 1,324, 14,234 | 1,254, 13,499 | 1,940, 23,024 | 846, 8,985 |
| 1 KB page-hash join (n left, moves, probes) | 678, 6,850, 5,043 | 560, 5,293, 4,631 | 914, 9,942, 5,511 | 312, 2,918, 2,458 |
- ra (tags + margin) is small (37 ranges -> 22 extents a step in the rain); the natural merge of raw ranges saves only 14 % (runs ~2.3
  long). No plan failed and no range array filled on the whole route (failed 0; rb's largest 6,950 vs cap 16,384).
- The plan's output depends only on the UNION of the ranges: xc_add and xc_merge join ranges that meet or overlap (no gap rule), and
  the merge sorts then joins; so any change that keeps the union gives the same extents bit for bit. Checked [C twin]: a natural merge
  of the raw ranges (run lw2), and the exact gl40 candidate (join with any of the last 8, then sort and join: run k8c), each gave
  extents EQUAL to xm_plan's in all 198,799 plans of the route.

### 1c. Move the plan to the ME? Not chosen
- The tag walk and the line marks (mp_mark into mp_bm) must be done at the transfer's end, before the game's next guarded store; the
  marks are made from the plan's extents. With the plan on the ME the main CPU would have to mark from the raw ranges (~2,400 a step
  instead of ~380 extents: mp_mark x3-6) or wait for the ME.
- The arena's room decisions (spill, drain, early copy `me_vb >= mp_dl && !mp_spr(e)`) are taken in mq_chk_end on the plan's size.
  Moving them is a protocol change of the picture loop; net ~2.2 model ms [I] after the extra marking, for the most risk.
- The ME has ~7.5-9.6 ms idle a step in the play-form rain turn (ME busy 70.6 of 78.1-80.2 [C gl39]) but is the limit on the steady deck
  (41.5 ME-limited [C gl39]).

### 1d. Lever XP (two parts, each with its own bit, one per-turn switch)
- XP bit 1 (ping-pong): xc_merge alternates r and t between passes and copies once at the end only when the result lies in t (or
  returns the buffer). The same merges in the same order: output bit-equal. Saves the per-pass me_memcpy: ~0.6-0.8 model [I].
- XP bit 2 (look-back join): in xc_add, when the new range does not meet the last entry, test the previous 7 (K = 8 in all, stop at
  the first that meets, same meet-or-overlap rule) and widen that entry. Union kept by construction. rb 2,175 -> 764 ranges a rain
  step, sort moves 24,876 -> ~6,700 [C twin], the largest plan ~460 ranges [I, scaled] (7 KB with its scratch: fits the D-cache). Extra
  tests ~9,600 a step x ~7 instructions = ~0.3 model [I]. (K = 8 over K = 16/32: the gain flattens (639 / 528) while the misses' scans
  double; the page hash leaves a similar n (678) for more code; exact-duplicate hashing leaves 1,324.) With fewer entries the
  mid-way merge at a full array (cap 16,384) comes later; the final union is the same, and no array filled on the route anyway.
- Switch: MGS2_XP_CYCLE, one digit a turn (0 = gl39's plan, 1 = ping-pong, 3 = both, 7 = both + check). Read into a global at the
  turn change like MGS2_DIG_CYCLE; in xc_add the test sits on the miss path only (gl39's path unchanged when 0).
- Check mode (digit 7, and MGS2_XP_CHECK=1 for whole runs on the twin and the emulator): the plan also runs gl39's way on a copy of
  the ranges (the ranges are kept as recorded) and the two extent lists are compared; a difference is logged with vb, channel and
  counts and counted per turn ("XP check: plans n, differing n"). Gate: 0 differing on the twin's and the emulator's whole route.
- The game state hash cannot see XP (extents decide what the ME copies, not game state): XP's proofs are the extent check and the
  frame digests (digest-on turns with XP on).

### 1e. Expected gain [I]
- Model: merge 2.57 -> ~0.65 (moves -73 %, D-misses mostly gone), its copies 0.65-0.9 -> ~0.05, xc_add +0.3: -1.7 to -2.6 model ms a
  rain step [I: scaled by the twin's move counts and the model's per-function costs; not run on the model].
- PSP: x 1.0-1.3 (the K0-K7 logic gap 10.72 PSP vs 8.5 model) = 1.7-3.4 ms a rain step [I]; ping-pong alone 0.6-1.0. Single
  estimates have missed by 2x before. The step stays main-limited after it (78.1 - 3.4 = 74.7 > ME busy 70.6), so a gain shows in
  full in the play form; in digest-on turns (ME-limited, 84.6) it does not show.
- Walk (30,900) similar; 21,100 more (its sort is the largest); the codec deck K0 is ME-limited: the probe shows the plan's µs there.

### 1f. Risk
- Wrong extents would leave stale bytes in a picture (hash blind): covered by the check mode on the twin and the emulator (every plan
  of the route) and by the digest-on turns of the PSP run with XP on. Small code: xc_add +~60 B, xc_merge +~40 B.
- XP changes when the ME can copy early only through the plan's time, not through its result.

### 1g. Later (not gl40)
- XP-b, a fused walk: vif1_chain_fast (rt/ee_io.h) already walks the same chain with the same tag rules just before xlive_chk_end;
  recording ranges there saves xc_walk's second pass (instructions + the 992 tag-read D-misses): ~1.0-1.4 model [I]. It touches the
  runtime's hot DMA path and needs xc_walk's late-walk rules proven equal there (the XP check mode does that). gl41.
- xc_add inlined into xc_tag / xc_run / xc_nat (6,441 calls): ~0.2 model [I]; it would apply to both arms, so it cannot be timed by
  turns. Later, with XP-b.

## 2. H1 (item 2)
- Today [C model, ka39.1, all steps]: the H1 functions cost P12e8b8 1.145, P121c30 0.92, ChainObj P13bf78 0.963, P12d910 0.578,
  P13bd00 0.261; the box cull 4.14. Half of each ~= gl17's unshown-step figures, so gl17's H1 model result still applies: -3.84
  whole step with the I-miss rise, -6.1 without it (h1_notes.md) -> 3-5.5 PSP ms a rain step [I].
- Left of the proof (h1_notes.md): the six-pass cut kept all 17,664 pictures, IOP, time and the addresses game code reads, but the
  VALUES read differ on 98 reader keys and the stores on 71 store-site lanes (VU0 registers, scratchpad, shared memory leftovers;
  d00t P323c38, d01t P329668, w00a P3b7c88 read scratchpad leftovers; d00t P272fd0, d01t P276b38, w00a P3bebf0 = SetQuatSlerp,
  w01a P3aaef8 differ at every unshown step).
- What the reference says (hd_study_2026-11-23.md section 7.1, HD build, strong evidence not proof): the only drawing-pass result game
  code reads is obj->world, made by the screen pass (0x121060, NOT in the cut). No game reader for screen matrices, prim2 screen,
  sort lists, packets, fog, matrix_addr, the Prim2 table; bound_mode only in a debug viewer and compiled-out code; shadow and spot
  write no object fields; the sort chain sorts on sort_z (written by the screen pass). Exceptions: the EVM pass copies world from
  its root each call and the codec face code copies that world into another object [I]; PluginActor set a texture-list flag on the
  PS2 and the curved-patch path wrote the patch's world, flags and LOD.
- Work to finish (2-4 days [I]):
  1. Take EVM (0x138ac8, 0x138ae0) out of the cut, or keep its world copy; read our PluginActor (0x13d540) for the texture-list
     flag and patch world and find their readers in our code (grep the generated units for the field offsets).
  2. Re-run the cut on today's twin (gl39 code; the pipeline and the native groups changed since gl17): pictures, IOP, time, reader
     addresses, and the state hash lines (whether the hash moves: UNKNOWN; the cut passes' buffers are in hashed RAM [I]).
  3. Classify each differing reader key by its reader function: an obj->world reader that differs is a real fault; the rest must be
     leftovers no code acts on (log the loads with MGS2_READ_LOG in d01t, w00a, w01a and follow each to a store or a branch). The
     four keys that differ at every unshown step first (SetQuatSlerp reading VU0 lanes it did not write is the likely kind [I]).
  4. If the state hash moves, the proof standard is the reader proof of speed_notes section 34 / DEVIATIONS E5, not the lead's hash
     rule: that is the owner's call before H1 ships.
- Questions the reference could still settle (Remote Control is stopped this round; for a later one): (a) does libmt SetQuatSlerp,
  or the code at w00a 0x3b7c88 (near GM_StreamCopyIpuImage), read VU0 registers or scratchpad words it did not write, and are those
  lanes used? (b) which VU0 registers and scratchpad words does each cut pass leave at return? (c) who reads the texture-list flag
  that PluginActor sets on the PS2? (d) does the EVM pass write anything besides the world copy that the codec reads?
- gl40: NO. Not proven exact; a turn with H1 on could move the state hash for the rest of the run and void the run's hash check.
  Candidate for gl41/42 once 1-4 are done.

## 3. The placement regression outside the deck (item 3)
PSP [C, rd39pc.py on the gl36/gl38/gl39 logs]: main-drawing turns before 24,000: 55.56 / 56.31 / 56.63; 20,000-24,000 84.87 / 86.45 /
86.94. ME-drawing turns: 55.52 / 55.64 / 55.82 (no regression).

Model, the same build relinked, gl38's guard cfg (mgs2_m37_rc0: main-drawing turns in odd hundreds), ms a PSP step [C model]:
| window | gl36 (x36) | gl39 (x39) | gl39 - gl36 (I / D) | c1 | c2 | c3 (heap at gl36's offset) | c3 - gl36 |
|---|---|---|---|---|---|---|---|
| 16,110 | 59.24 | 61.04 | +1.80 (I +0.61 / D +1.17) | 60.36 | 60.71 | 60.47 | +1.23 |
| 21,110 | 58.81 | 60.25 | +1.44 (I +0.96 / D +0.41) | 60.40 | 60.70 | 60.60 | +1.79 |
| 23,110 | 24.27 | 24.41 | +0.14 | 24.42 | 24.44 | 24.40 | +0.13 |
| 27,510 | 60.61 | 62.18 | +1.57 (I +0.59 / D +0.87) | 61.96 | 62.05 | 61.65 | +1.04 |
c1 = ME_PAD 0x12c0, RO_PAD 0x1800; c2 = ME_PAD 0x1040, RO_PAD 0x1580; c3 = gl39's pads + END_PAD 0x1a00 (exp40/end_pad.ld, a NOBITS
pad after .me_bss): heap block 0x0b239900, gl36's offset in an 8 KB way (0x1900). Each run 55/55 hash lines equal; instructions
within 0.1 of gl39's in every window. The emulator places the heap block where the PSP does (gl36: 0x0b23b900 in both logs) [C].
- Why gl36 was better: (1) CODE ORDER: gl36's order was searched for gl36's code; gl38 could not reuse it (109 hot functions changed
  size, overlay base +24 KB) and its search weighted the rain windows x4 with a guard against gl36 that could not bind (harness notes,
  gl38 "Retrain"); I-misses +0.6..+0.95 model in the main-drawing windows. (2) DATA: the heap block moved from 0x0b23b900 (gl36:
  0x1900 in an 8 KB way) to 0x0b237f00 (gl39: 0x1f00) as the image shrank, and .rodata from 0x0ea0 to 0x0080 (gl39's RO_PAD put it
  at gl38's offset, not gl36's); DHIST 16,110: dynamic +2,959 D-misses a step (heap +1,478, the stack area 0x0bf... +1,130), static
  +1,546 (St, F, glf, C1m, H, lw_mem, g_io_ee, g_io_seen) [C model]. gl39's pads (c0) and two other pad sets (c1, c2) do not repair it.
- The heap at gl36's offset (c3) is not the fix by itself: -0.57 and -0.53 in two windows, +0.35 in 21,110 [C model]. What lands where
  inside the heap also follows the allocation order, so one offset helps some windows and hurts others; no pad set of four (c0-c3)
  repairs every window.
- Cheapest fix: (a) the CODE part (I +0.6..+0.96 model, 40-65 % of the regression) by gl40's order retrain, which gl40 needs anyway
  for XP, M5 and the probe: the four main-drawing windows 16,110 / 21,110 / 23,110 / 27,510 join the search's windows (rain no longer
  x4 alone) with a guard that binds: I-misses <= gl36's + 2 % in each; (b) the DATA part by a small search after the final link:
  END_PAD (exp40/end_pad.ld; program headers otherwise equal [C readelf]) and RO_PAD over 4-6 values, each relinked and modelled
  on the four windows (4 runs in parallel ~12 min), keeping the best set that is not worse than gl39 in any window and not worse
  in the rain windows; static tables compared mod 0x2000 with gl36's (rule 3), not gl38's.
- Expected [I]: (a) 0.6-0.95 model if the guard binds; (b) 0-0.5 (c3's mean over the three heavy windows: -0.25); together 0.5-1.2
  PSP ms in main-drawing turns before 24,000 (56.63 toward 55.56) [I: model deltas x ~1; the gl38/gl39 PSP-vs-model match outside
  the deck was ~1].

## 4. M5 (item 4)
- [C]: gl39's M5 block is in runtime/kernel.h (the twin's); the PSP's boot_rt.o includes rt/kernel.h (-I$M/rt before -I$M/runtime),
  so MGS2_OPT2 bit 18 did nothing on the PSP. Side compiles (exp40/m5, gl40b/m5): boot_rt.o compiled with gl39's command in another
  folder is byte-equal to build_gl39/u's (pz); with kernel.h (unchanged) found at another path it is +17.3 KB (v0), also with
  -fprofile-prefix-map (pv0): GCC's profile checksum takes the header's path, so every kernel.h function lost its profile. So the
  gl39 report's "+17.7 KB, 17 functions re-inlined" was the header path, not M5. M5 alone (va vs v0): only k_take_interrupts changes
  (352 -> 552 B); as a noinline function (vb): k_take_interrupts 96 B + .part 260 B + k_take_m5 300 B.
- Lever: vb's form in rt/kernel.h: `static __attribute__((noinline)) void k_take_m5(void)` (the set-bit walk, exact: a bit a handler
  sets above its channel is still taken in the pass, one at or below is left, as gl39's runtime/kernel.h explains) called by
  `if (g_opt2 & OPT2_M5) { k_take_m5(); return; }`, plus a counter of M5 walks printed per turn.
- Shown in the PSP ELF by: `psp-nm MGS2T.elf | grep k_take_m5` (a symbol), its disassembly (ctz = clz of the isolated bit), and the
  log's per-turn counter (0 in M5-off turns, > 0 in M5-on turns). In place the lines below it shift, which drops their stale
  profile entries: harmless because gl40 trains a new profile.
- Gain: 0.11 model ms [C model, side build, gl39]; on the PSP below one turn's spread (0.1-0.3): time UNKNOWN; it goes in as
  shown-and-counted, its time read only as a K7 pair (t58 vs t64).

## 5. Other main-CPU work in the rain wait (item 5) [C model, ka39.1 t56, ms a PSP step]
| Function | ms | Exact lever? |
|---|---|---|
| io_dma_start | 6.08 | the runtime's exact DMA and VIF parse (1.01 M instructions: ~11,700 VIF codes, ~7,000 tags a step); M3 is dead (the VU-header hash); only the fused walk (1g) found |
| box cull vupv_96936c0f_0 | 4.14 | its results reach game code (b_proof_notes); M1's exact part <= 0.45 |
| rt_qcopy_nov | 3.04 | the game's quadword copies (~610 KB a step); it already creates destination lines (cache 0x18); its 7,980 D-misses are source reads: nothing cheap |
| nat_dbc0 1.56, rt_store 1.13, memcpy 1.04, vuc_dispatch 0.82, k_take_interrupts 0.61, spu2 render_block 0.56, kernel ~0.8 | | no exact lever >= 1 PSP ms seen; the sound renderer made asynchronous would not be exact |
- Lamps (fade_lgt, the lead's point 3): our code has HZX_OnlineHazardCheck (P14fe30) with getters (HZX_GetOnlineHazard*, libhzx/onlinex
  names), so the line test's result is kept in module state other code can read [I, names]; and a skipped test leaves the lamp's
  alpha in RAM different, so the state hash moves either way. ~0.3 ms [I]: under the 1 ms bar and not hash-exact: not gl40.
- The "keep the dice" cut of cosmetic actors (hd_study section 1): ~3 PSP ms a rain step [I], 3-6 days, hashes move (its proof is a
  seed log at every guard draw, equal in both twins): a later build, not gl40 (it does not fit beside XP and the retrain, and it
  needs its own proof standard and the owner's word).
- So for gl40: XP is the only new main-CPU lever >= 1 PSP ms that is exact under the hash rule.

## 6. The gl40 schedule (item 6)
Same route and turn frame as gl39 (74 turns of 100 vblanks from 24,000; sched39.py's structure; F1 on everywhere; re-use off). New
keys: MGS2_XP_CYCLE (a digit a turn), MGS2_PROBE10=1 (the plan probe, every turn), M5 by MGS2_OPT2_CYCLE bit 18 (off at t % 16 in
{0, 1} as gl39: t0, t1, t16, t17, t32, t33, t48, t49, t64, t65). Digests on only in t55, t65, t68 (checks against the twin; the
twin's same-schedule arm runs the same XP digits). t0-t50 keep gl39's kinds and loads; in the rain wait gl39's K8 load turns
(t59-t61) become XP turns and one K8 no-load turn stays.

| turns | scene | kind / XP / digests |
|---|---|---|
| t0-t50 | as gl39 (deck, nothing drawn, steady deck, change of scene) | gl39's kinds; XP 3 in t22, t29, t32, t38, t44 (steady deck K0: ME-limited, for the probe), 0 elsewhere |
| t51 t52 t53 t54 | rain wait, first turns | K0 XP0, K0 XP3, K7, K0 XP3 |
| t55 | rain wait | K0 XP7, digests on (check + digests with XP on) |
| t56 t57 | rain wait | K0 XP0, K0 XP3 |
| t58 | rain wait | K7 (M5 on) |
| t59 t60 | rain wait | K0 XP1 (ping-pong alone), K0 XP3 |
| t61 t62 | rain wait | K0 XP0, K8 no load |
| t63 | rain wait | K0 XP3 |
| t64 | rain wait | K7, M5 off (t % 16 = 0) |
| t65 | rain wait | K0 XP0, digests on, M5 off (gl39's default: digests against the twin) |
| t66 t67 | rain wait, heavier | K0 XP3, K0 XP0 |
| t68 t69 t70 t71 t72 | rain walk | K0 XP7 digests on, K0 XP0, K0 XP3, K7, K0 XP3 |
| t73 | the walk ends | K0 XP3 |
Pairs: XP3 - XP0 = (t57 - t56), (t60 - t61), (t63 - t61), heavier (t66 - t67), walk (t70 - t69); XP1 - XP0 =
t59 - t56; M5 = t58 - t64 (K7). Placement: the main-drawing turns before 24,000 against gl36/gl38/gl39 by rd40pc.py.

The plan probe (MGS2_PROBE10=1; psp/xl_psp.c, the clock sceKernelGetSystemTimeLow as gl39's probes; ge/xlme.c gets an optional
clock pointer in XmCopy, NULL in the twin), one line a turn: "gl40 plan: plans n, us total / walk / sort+join / rest, mp_mark us,
xc_add calls, ra n, rb n into the merge, sort moves, mk n, XP check differing n; waits in mq_chk_end by reason n/us". Cost ~10 clock
reads a plan = ~60 a step (~0.05 ms [I]), the same in every turn.

Expected PSP, ms a step [I: gl39's PSP turns + section 1e]:
| turn kind (rain wait, play form, F1 on) | step | logic | plan probe us a step |
|---|---|---|---|
| K0 XP0 | 78.1 +-0.5 (gl39 78.11-78.16) | 45 | 6,200-8,500 |
| K0 XP1 | 77.1-77.5 | 44.3 | 5,500-7,500 |
| K0 XP3 | 74.7-76.4 | 42-43.5 | 3,800-5,500 |
| K0 XP7 / digests on | ~84.6 (ME-limited, the digest) | - | (check doubles it) |
| K7 M5 on / off | 67.0 / 67.1 | 34.4 | 0 |
| K8 no load | 66.7 | 34.4 | 0 |
| heavier t66 XP3 vs t67 XP0 | ~81-82.7 vs ~84.4 | | |
| main-drawing turns before 24,000 | 55.4-56.1 (gl39 56.63) | | - |
If the probe reads ~8 ms a step in XP0 turns, the plan is most of the K0-K7 logic gap (10.7); if it reads ~4, the rest of the gap is
still UNKNOWN (candidates: record copies, GE bus traffic).

## 7. Build round (steps, rough time; one build out at a time; gl39's log is read)
1. Code XP in ge/xlme.c (bits 1 and 2, check mode) + MGS2_XP_CYCLE in psp/xl_psp.c and the twin's xlive_chk.c; the plan probe; M5 into
   rt/kernel.h (k_take_m5 + counter); sched40.py. 1.5-2 h.
2. Twin: build; arms XP0 / XP3 / XP7 whole route + gl40's schedule: 685/685 hashes, XP check 0 differing, frames equal to gl39's arm.
   0.5-1 h.
3. PSP compile, PGO training (5 runs, 0 differing) and the order search with the main-drawing guard windows; then the data pad
   search (END_PAD / RO_PAD, 4-6 relinks modelled on the four windows); readelf -l against gl39 (3 LOAD segments; only MemSiz may
   move); hot tables mod 0x2000 against gl36; `psp-nm MGS2T.elf | grep k_take_m5`. 3.5-4.5 h.
4. Model (fcr emulator): rain t55-t67 windows XP0/XP3 + the four guard windows against gl36 and gl39. 1-1.5 h.
5. Emulator gates: final package 62/62 hashes and the digests = the twin's same-schedule arm; XP check 0 differing on the emulator;
   schedule check; heap headroom. 1-1.5 h.
6. Package, README, MANIFEST, rd40pc.py, delivery and md5 on the PC. 0.5-1 h.
Total ~8.5-12 h [I] (gl38/gl39 rounds took about that).

## 8. UNKNOWN (and how we find out)
- XP's PSP gain: the XP pairs in the rain wait and the walk; the probe's µs per part.
- How much of the K0-K7 logic gap (10.7 PSP) the plan explains: the probe in XP0 turns.
- M5's PSP time (below noise): only its counter and symbol are shown.
- Whether H1 moves the state hash, and its 98 keys: twin re-run and the classification (section 2).
- Whether the guard-window retrain recovers gl36's I-misses: the order search's own result in step 3.
- The data part of item 3: no pad set found yet that helps every window (c0-c3); the pad search of step 3 decides it.
