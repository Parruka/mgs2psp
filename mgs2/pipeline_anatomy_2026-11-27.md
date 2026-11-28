# MGS2 PSP: where the game's own draw pipeline spends the main CPU (gl43), and what can leave it for gl45

Study of 2026-11-27 / 28, thread "MGS2 port". Reading and analysis only: no PSP build, nothing delivered.
STATUS: FINISHED (v1). Scratch, scripts and the profile files: `/home/claude/main/study_pipe45` (container, ephemeral).

Marks: **[C]** measured on the PSP (source named). **[C model]** measured in the profiling emulator with the PSP cost model
(1.65 x instructions + 68 x I-cache misses + 86 x D-cache misses, / 333,000 = ms). **[C twin]**, **[C b_proof]** proven earlier on the
x86 twin (note named). **[I]** an estimate, with its basis; never a result. **UNKNOWN** with how to find out.
Units: **every figure is ms a game step** (1/50 s of game time; a picture every 2 steps), never ms a picture.

## 0. Short answer

1. The PSP's "draw pipeline" figure is the time inside the game's call 0x11da90 (boot.c `rt_dg_call`, timing region HOST_DG). It is
   **24.70 ms a rain step and 13.31 to 13.71 indoors [C]** (gl43, gl42in). The model run made for this study counts only that call
   and gives **25.15 in the rain and 13.89 / 13.06 indoors [C model]**, so the model and the PSP agree within 0.5 ms here.
2. **About 22 of the 25 rain ms run the same on every step, shown or not [I from the code's conditions + C model].** Only the two
   native stubs (ship, people: about 2.6 ms a step on average, so about 5.2 ms on a shown step) run on shown steps only, and only the
   ship's packet builder is skipped on unshown steps.
3. By who reads the output (section 3): **STAYS 14.8 rain / 8.6 indoors** (game code reads the result, or the function leaks VU0
   rounding into values game code reads); **MOVE 9.8 / 4.75** (only the picture side reads it); **DROP 0.6 / 0.6** (nobody reads
   it in our build) [C model for the costs; the split by reader rests on the earlier proofs named in section 3].
4. **5.1 rain / 3.0 indoors is not the game's maths at all** (4.6 / 2.6 of it inside STAYS): it is our runtime's way of doing the game's
   scratchpad DMA transfers (about 440 a rain step): 1.8 for the copies and 3.3 for register writes, transfer start and the
   interrupt poll [C model]. A fast path there changes no game state, so it needs no reader proof.
5. **Proposal for gl45, in this order** (section 5): (1) the scratchpad-DMA fast path: about **-2.1 rain / -1.35 indoors [I]**,
   strict hash, same reference; (2) the people path of K-N2: the old builder returns at once on unshown steps and the reuse
   check gets cheaper: about **-1.3 / -1.3 [I]**; (3) the first unshown-step leaf stubs under the E5 standard, chosen by an x86
   screening with the switch that already exists (`MGS2_LITE_FN`): constant blocks and the sea first: up to **-2.9 / -0.9 [I]**.
   Together about **-6.3 rain / -3.6 indoors [I]**, none of it measured.
6. **The pipeline alone cannot bring the rain to 25 ms a step.** If everything movable left the main CPU and the fast path
   worked as hoped, about 13 ms of pipeline would still stay in the rain and 7.6 indoors [I], beside 38.4 / 17.4 of logic [C].
7. **Rain and indoors are not limited by the same processor today.** Rain K-N2B: step 69.70 = main 69.32 + 0.38 waiting for the
   picture; ME busy 55.25 [C]. A main-CPU ms saved there is a step ms saved, for about the first 14. Indoors K-N2: step 47.93 =
   main 41.43 + 6.50 waiting for the picture; ME busy 45.15 [C]. There the main CPU already waits, so the indoor figures of these
   cuts turn into frame rate only together with fewer chain walks on the ME (39.13 of its 45.15 [C]); nothing in this proposal adds
   work to the ME.

## 1. What was measured, and how

- **The PSP's figure.** `rt_dg_call` (boot/boot.c) wraps the game's 0x11da90 in HOST_DG and sets `g_zt` for the whole call (the
  pipeline takes no game time). The exact walk of a DMA send (HOST_GS), the copy plan and the ME's work are other regions and are
  not in the 24.7. PSP, rain wait K-N2B: main 69.32 = logic 38.37 + pipeline 24.70 + about 6 other; ME busy 55.25 [C,
  prof/hw_gl43/gl43_psp_read.md]. Indoors: K7 13.31, K-N2 13.71, K-A 12.98 [C, prof/hw_gl42in/gl42in_psp_read.md].
- **The study's run [C model].** One run of the profiling emulator (PspProf), gl43's delivered program (EBOOT md5 318caa49),
  the training route, with the profiler told to count **only while `g_zt` is set** (PSPPROF_BUCKET on that word): so the files
  hold the pipeline call and nothing else, with the caches modelled over everything. 98 of 98 hash lines equal gl43's reference,
  so the run is the real game state. Eight windows of 80 steps:

  | Window (vblanks) | Turn | Scene | Pipeline, model ms a step | = instr + I-miss + D-miss |
  |---|---|---|---|---|
  | 29,710-29,790 | t57 K-N2B | rain wait | 25.21 | 17.79 + 0.80 + 6.63 |
  | 29,910-29,990 | t59 K-N2B | rain wait | 25.09 | |
  | 30,510-30,590 | t65 K7B | rain wait | 23.84 | |
  | 32,110-32,190 | t81 K-N2 | indoors | 13.44 | 9.19 + 0.67 + 3.58 |
  | 32,710-32,790 | t87 K-N2 | indoors | 13.89 | |
  | 33,910-33,990 | t99 K-N2 | indoors | 14.33 | |
  | 34,010-34,090 | t100 K7 | indoors | 13.06 | 8.86 + 0.69 + 3.51 |
  | 35,110-35,190 | t111 K-N2 | indoors, lighter scene | 10.20 | |

  Inside the call the copy plan is 0.00, the ME's code 0.00 and the exact walk 0.02 ms: the split of the PSP's regions is as
  the code says [C model].
- **Against the PSP:** rain K-N2B 25.15 model (mean of two windows) / 24.70 PSP; indoors K-N2 13.89 (mean of three) / 13.71;
  K7 13.06 / 13.31. For this part of the step the model needs no correction factor. The PSP figures are means over whole turns,
  the model's are single 80-step windows, so a difference of 0.5 ms means nothing.
- **Shares of the rain's 25.15:** instructions 17.74 (71%), D-cache misses 6.62 (26%), I-cache misses 0.80 (3%). The D-misses by the
  kind of data (window t57): the game's objects and models in its heap 4.38 (66% of them), the scratchpad 1.13, the host's heap
  0.59, program data 0.26, host data 0.23, the packet buffers 0.06 [C model].
- **Every step or shown steps only.** One profile window cannot split shown from unshown steps (one counting word a run). From the
  code [C source]: the native stubs run only in full calls (`g_nat_cfg && g_zt && !g_dg_lite` in boot/nat_stub.h, the same test
  with `g_np6_cfg` in boot/np6_stub.h); on a reduced call only 0x1214a0 returns at once (boot/dg_lite.h); so on an unshown step
  the people's old builder 0x12e8b8 runs in full, and everything else runs on every step. With the model's costs: shown-only
  code about 2.6 ms a step on average (5.2 on a shown step), unshown-only code 0.6 (1.2 on an unshown step), the rest about 22.0
  on every step. So a shown rain step is about 27.2 and an unshown one about 23.1 **[I]**. UNKNOWN as a measurement; how to find
  out: the PSP's own per-step log (alternate steps of a K-N2B turn; the gl43 log files are not in the container), or a second
  model run counting on `g_dg_lite`.

## 2. Table 1: the pipeline's cost by function or small group (own cost)

Own cost = what the function's own instructions and misses cost; a helper is its own row. Rain = mean of t57 and t59 (K-N2B);
indoors = mean of t81, t87, t99 (K-N2); K7 = t100. Calls = entries a step in the rain. "When": E every step, S shown steps
only, U unshown steps only [C source for the conditions]. All figures [C model], ms a step.

| # | Function or group | Rain | = instr + I + D | % | run % | Calls | Indoors | In K7 | When | What it does, in plain words |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Box test program (VU0 micro program 96936c0f / 8baafbc3 entry 0, run in its VFPU form) | 3.91 | 3.86 + 0.03 + 0.01 | 15.5 | 15.5 | 1,005 | 1.02 | 1.09 | E | Takes a box's 8 corners through the clip matrix and says in / out / cut. About 770 instructions a test. In the rain 554 tests a step come from the object pass and 452 from the sea |
| 2 | Scratchpad DMA: the copies (`rt_qcopy_nov`, all called from `io_dma_start`) | 1.80 | 0.59 + 0.00 + 1.20 | 7.2 | 22.7 | 1,542 | 0.91 | 1.04 | E | The game sends parts of its objects to the scratchpad and back by DMA; this is the copying. Two thirds is cache misses on the objects (first touch in the step) |
| 3 | OnePieceObjs 0x11fa58 | 1.69 | 1.12 + 0.07 + 0.50 | 6.7 | 29.4 | 166 | 0.59 | 0.61 | E | Rigid objects: each part's world matrix and clip matrix, then the part's box test |
| 4 | Scratchpad DMA: register writes (`rt_store`, its `memcpy`, `rt_st32`, `rt_mm`) | 1.65 | 1.49 + 0.03 + 0.13 | 6.5 | 35.9 | 2,743 | 1.04 | 1.11 | E | Each transfer is 7 writes and 1 read of DMA registers; every write goes through the general "store outside RAM" path (about 100 instructions, and 30 more to act on it) |
| 5 | Scratchpad DMA: start and end of a transfer (`io_dma_start` without the exact walk, `io_flush`, `k_poll*`, `k_take_m5`) | 1.64 | 1.31 + 0.08 + 0.24 | 6.5 | 42.4 | 438 | 1.02 | 1.10 | E | Decoding the transfer (interleave mode), and the look for pending interrupts after each one |
| 6 | Pass loops (0x1226c8, 0x122608, 0x12f130, 0x12f070, 0x122138, 0x121060, 0x11e5e0) | 1.23 | 0.47 + 0.09 + 0.67 | 4.9 | 47.3 | - | 0.84 | 0.86 | E | The walks over objects and meshes that call the functions below; half is cache misses on the object lists |
| 7 | StoreMatrixObjs 0x120860 (+ SetupLowObjs 0x120f00) | 1.20 | 0.69 + 0.03 + 0.48 | 4.8 | 52.1 | 21 | 1.02 | 1.19 | E | Writes for every visible mesh the 384-byte block the mesh program needs: matrices, lights, fog |
| 8 | People reuse check (`n6_csum`, `nc_rd*`; ours) | 1.14 | 0.82 + 0.00 + 0.32 | 4.5 | 56.7 | 22 | 1.13 | 0.00 | S | Our native people path: before it reuses a mesh's cooked vertices it hashes all the mesh's source streams again (some 160 to 200 KB a shown step, by its instruction and miss counts) |
| 9 | Ship stub (`rt_nat_p04`, `nge_*`; ours) | 1.07 | 0.81 + 0.01 + 0.25 | 4.2 | 60.9 | 74 | 0.55 | 0.39 | S | Our native ship path: per mesh group the check and the GE list words |
| 10 | Sea builder 0x13bf78 | 0.96 | 0.62 + 0.03 + 0.31 | 3.8 | 64.7 | 2 | 0.00 | 0.00 | E | The sea's curved patches: test each patch (row 14), build its packets |
| 11 | Prim sort keys (`nat_dbc0`, ours, + 0x12dbc0) | 0.86 | 0.58 + 0.00 + 0.28 | 3.4 | 68.2 | 262 | 0.69 | 0.67 | E | A depth key for every effect prim (splashes, glows, smoke) |
| 12 | DG_SetObjsMatrix 0x120538 | 0.71 | 0.48 + 0.03 + 0.21 | 2.8 | 71.0 | 217 | 0.41 | 0.41 | E | Per object: world matrix, clip x world, the object's box test |
| 13 | SortChainObjs 0x121c30 | 0.68 | 0.44 + 0.01 + 0.23 | 2.7 | 73.7 | 1 | 0.40 | 0.39 | E | Sends the sorted (see-through) meshes and prims in depth order |
| 14 | BoundCheck 0x11f968 and the sea's 0x13bd00 | 0.66 | 0.55 + 0.00 + 0.10 | 2.6 | 76.3 | 998 | 0.19 | 0.20 | E | The game's wrappers around row 1 (0.40 objects, 0.26 sea) |
| 15 | Stream restore 0x12d688 (+ 0x12d770) | 0.65 | 0.29 + 0.00 + 0.35 | 2.6 | 78.9 | 217 | 0.36 | 0.36 | E | Per object, puts back the vertex colour streams after temporary lights |
| 16 | DG_Prim2Chanl 0x12dc90 | 0.62 | 0.33 + 0.01 + 0.27 | 2.4 | 81.3 | 1 | 0.21 | 0.20 | E | Per prim: bounds, key, matrix |
| 17 | People builder, the original 0x12e8b8 | 0.59 | 0.43 + 0.01 + 0.14 | 2.3 | 83.7 | 22 | 0.60 | 1.02 | U in K-N2, E in K7 | The old packet builder for people. In a K-N2 turn it runs on unshown steps only, and its packets are never sent |
| 18 | ScreenObjs 0x11fde8 | 0.52 | 0.31 + 0.06 + 0.15 | 2.1 | 85.8 | 42 | 0.56 | 0.57 | E | Skeletons: per bone the clip matrix and the box test |
| 19 | DG_WritePrimPacks 0x12d910 | 0.44 | 0.34 + 0.01 + 0.09 | 1.7 | 87.5 | 262 | 0.22 | 0.22 | E | The packets of the effect prims |
| 20 | Plug-in hooks (0x135338, 0x13a310, 0x1394a0, 0x139180, 0x1352a8, 0x13d540) | 0.42 | 0.18 + 0.08 + 0.17 | 1.7 | 89.2 | - | 0.28 | 0.28 | E | The stage's extra draw hooks, the sea's among them |
| 21 | People stub `rt_nat_p06` (ours) | 0.40 | 0.27 + 0.00 + 0.12 | 1.6 | 90.8 | 22 | 0.40 | 0.00 | S | Our native people path: per mesh the look-up and the GE list words |
| 22 | VU0 call path (`vu_run_shared`) | 0.40 | 0.38 + 0.00 + 0.01 | 1.6 | 92.4 | 1,517 | 0.19 | 0.19 | E | Finds and enters the micro program of rows 1, 23, 24 |
| 23 | Depth test (program entry 2 + CheckSortZ 0x11fa10) | 0.36 | 0.35 + 0.00 + 0.00 | 1.4 | 93.8 | 314 | 0.23 | 0.22 | E | Depth of a part's centre, for the sort |
| 24 | Bone program (entry 6) | 0.34 | 0.32 + 0.02 + 0.00 | 1.4 | 95.2 | 206 | 0.34 | 0.35 | E | The per-bone matrix work of row 18 |
| 25 | Sort lists (0x121a90, 0x12d810) | 0.34 | 0.14 + 0.00 + 0.20 | 1.4 | 96.5 | - | 0.12 | 0.13 | E | The depth lists and the prims' radix sort |
| 26 | Float min / max helpers (called from rows 10 and 7) | 0.26 | 0.25 + 0.01 + 0.00 | 1.0 | 97.6 | 2,192 | 0.06 | 0.07 | E | |
| 27 | ME copy guard `mg_lines` (ours) | 0.18 | 0.16 + 0.00 + 0.02 | 0.7 | 98.3 | - | 0.11 | 0.00 | E in drawn turns | Saves a line of game RAM for the ME's walk before the runtime overwrites it |
| | All other (frame code, EVM, packet helpers, dispatch) | 0.43 | 0.14 + 0.16 + 0.14 | 1.7 | 100 | | 0.40 | 0.39 | | |
| | **Total** | **25.15** | 17.74 + 0.80 + 6.62 | | | | **13.89** | **13.06** | | |

Rows 1 to 18 are 85.8% of the rain's pipeline, rows 1 to 22 are 92.4%. The rain K7B window (t65, 23.84) has the same rows within 0.1 ms but for rows 8 and 21 (0), row 9 (0.55) and row 17 (1.17).

Things the table shows that were not known before:

- **K-N2's people path costs the main CPU more than the builder it replaced.** K-N2 pays the reuse check 1.14 + the stub 0.40 on
  shown steps and the old builder 0.59 on unshown steps = 2.13; a K7 turn pays only the old builder on every step, 1.02 to 1.17
  [C model]. The PSP agrees in sign: indoors K-N2 13.71 against K-A 12.98 [C].
- **The sea is 3.5 ms of every rain step** once its 452 box tests are counted with it (section 2a), for at most 3.8% of the
  deck's pixels [C stagec, native_ge_renderer_design.md 2.5].
- **The scratchpad DMA mechanism is 5.1 ms of every rain step** (rows 2, 4, 5) and 3.0 indoors.

### 2a. Table 2: the same costs by job (helpers handed to their callers)

Windows t57 (rain, 25.21) and t87 (indoors, 13.89). The runtime's store helpers, the small vector helpers and the VU0 call path
are handed to the game function that called them, by its share of the calls **[split: I]**; the box test program is split by the
number of tests; the DMA transfers stay a row of their own because the profile has no caller for them (by the number of
transfers they belong to OnePieceObjs 168 a step, StoreMatrixObjs 126, ScreenObjs 42, DG_SetObjsMatrix 42, others about 60).

| Job | Rain | Indoors | Made of |
|---|---|---|---|
| A. Transform pass: OnePieceObjs, ScreenObjs, DG_SetObjsMatrix, the pass loop | 4.40 | 2.53 | 2.41 + 0.83 + 0.90 + 0.27 in the rain; about 1.0 / 0.6 of it is their DMA register writes |
| B. Box and depth tests of objects, parts, bones (554 + 306 a rain step) | 3.29 | 1.64 | program 2.17, wrapper 0.75, depth test 0.35 |
| C. Scratchpad DMA transfers: copies, start and end, interrupt poll | 3.38 | 1.94 | copies 1.80; the rest 1.58 |
| D. The sea: builder, its wrapper, its 452 box tests | 3.48 | 0.00 | 1.19 + 0.55 + 1.74 |
| E. People, native path (stub + reuse check) | 1.42 | 1.43 | shown steps |
| F. People, the old builder | 0.60 | 0.60 | unshown steps |
| G. Constant blocks: StoreMatrixObjs, SetupLowObjs | 1.77 | 1.62 | 1.69 + 0.08 |
| H. Ship, native path (stub, checks, list words) | 1.29 | 0.70 | shown steps |
| I. Sorted pass: sort lists, SortChainObjs, DG_WritePrimPacks, its loop | 1.69 | 0.88 | 0.35 + 0.71 + 0.44 + 0.20 |
| J. Prim pass: DG_Prim2Chanl, sort keys | 1.48 | 0.88 | 0.62 + 0.86 |
| K. Lights: stream restore | 0.66 | 0.38 | |
| L. Loops, plug-in hooks, frame code, EVM, packet helpers | 1.58 | 1.20 | people loops 0.31, opaque loop 0.46, hooks 0.46, frame 0.18, rest |
| M. ME copy guard | 0.18 | 0.12 | |
| **Total** | **25.22** | **13.92** | |

## 3. Who reads each job's output, and the route

Reader classes: **(a)** game code outside the pipeline reads it, so it must stay or be reproduced bit for bit; **(b)** only the
picture side reads it (the ME's walk of the packets, or our native stubs); **(c)** nobody reads it in our build.
Evidence used, all earlier work and none of it repeated here:
- the October write/read trace [C twin, prof/dgt_keep_writers_vb21000_25200.txt, dgt_report_v7]: of 473,245 words the pipeline
  wrote on the deck route, game code read 1,609: the part world matrices (written by 0x11fde8 and 0x11fa58), 478 words of
  0x12dc90, the default light of 0x128d90, 5 words of 0x12d688, the frame code's chain descriptors, and stack / scratchpad
  leftovers;
- the VU0 rounding test [C b_proof, b_proof_notes.md]: the box test programs, OnePieceObjs, CalcSortZ, DG_Prim2Chanl, the sea's
  ChainObj 0x13bf78 and PluginActor put rounding into values game code reads; DG_SetObjsMatrix, StoreMatrixObjs and the shadow
  chain do not;
- the H1 round [C twin, h1_notes.md, attic/h1_2026-11-23]: cutting whole passes on unshown steps keeps all pictures but moves
  values game code reads (VU0 registers, scratchpad, shared memory left over: classes ORDER / LEFTOVER / GARBAGE-IN, list R1-R8);
  never cut 0x12dc90 (5 frames break) or 0x121060 (1,993 frames break);
- V3a [C twin, native_ge_renderer_design.md 1.1]: with 0x1214a0 and 0x12e8b8 stubbed on EVERY step the read hash is equal over
  the rain window and the talk;
- the design's keep / stub table (native_ge_renderer_design.md 1.1) and its proof standards (5.1: "strict" and "E5 reader").

Checked in gl43's code for this note [C source]: which calls are full and which reduced, and that only 0x1214a0 returns at once
in a reduced call (boot.c `rt_dg_call`, boot/dg_lite.h); that both native stubs run in full calls only and write host memory
only (boot/nat_stub.h, boot/np6_stub.h: "the game's state is read"); that a stubbed people call must not be handed back to the
old builder on the routes, and that this is counted (np6_stub.h, the memo count); that the reuse check only reads game RAM
(boot/nge_stub.h `n6_csum`, `n6_same`); and how a scratchpad transfer ends (rt/ee_io.h: registers past the data, status bit, the
interrupt raised). The reader lists themselves (which game function loads which word) are the earlier twin runs' and were not
run again.

Routes: **STAYS**; **DROP** (an exact skip: the output has no reader); **MOVE** (the Vita seam: the main CPU only finishes
world and bone matrices and hands a small snapshot over; the other processor does the rest; nothing is written back; on an
unshown step the moved work is simply not done). "Frees" = main-CPU ms a step, from table 2 [C model for the cost; the saving
itself is **[I]** until built]. "ME" = what the move would add to the ME a step **[I: the same job in plain float code without
the VU0 exactness, shown steps only, so a quarter to a half of the main CPU's cost]**.

| Job | Reader | Route | Frees rain / indoors | Adds to the ME | Main risk |
|---|---|---|---|---|---|
| A. Transform pass | (a): part and object worlds are read; OnePieceObjs leaks rounding | STAYS | 0 (but see C) | - | - |
| B. Object / part / bone box tests | (a): leftovers (VF10, VU0 memory) reach game logic [C b_proof] | STAYS | 0. A cheaper exact form is UNKNOWN (section 6) | - | - |
| C. Scratchpad DMA transfers | (a) by construction: they are how A and G move their data | STAYS, **runtime fast path** | 2.1 / 1.35 [I] (cut 1) | 0 | none for the game state if the registers and the scratchpad end the same; the hash test settles it |
| D. The sea | (b) for the packets; **open**: the patch's world, flags and LOD are read by game code (design 1.1), and 0x13bf78 leaks rounding and flips flag bit 22 of 0x1b4818 (read by w00a 0x331160, masked: likely LEFTOVER, R5) | MOVE under E5 (design step 4); DROP on unshown steps first | 3.48 / 0 on all steps; 1.74 / 0 on unshown steps only | 0.5 to 1.0 in the rain | whether the flags / LOD game code reads are written inside 0x13bf78: UNKNOWN |
| E. People, native path | (b): host-only, reads game RAM, writes none | MOVE, or make the check cheaper in place | 1.42 / 1.43 (the check alone 1.14 / 1.13) | 1.0 to 1.4 | picture only: a mesh whose vertices the game rewrote in place is shown stale until the check sees it |
| F. People, old builder on unshown steps (K-N2) | (c): its packets are never sent in a K-N2 turn; V3a had it stubbed on every step, read hash equal in the rain | **DROP** | 0.60 / 0.60 | 0 | a shown step that falls back to the old builder after skipped calls (the builder has words that outlive a call: 5,256 on the route to vb 23,300 [C twin, speed_notes 33]); the strict hash needs its own reference |
| G. Constant blocks | (b): the mesh programs and our stubs read them; passes the rounding test [C b_proof]; its scratchpad leftovers are the R-list kind | MOVE under E5; DROP on unshown steps first | 1.77 / 1.62 on all steps; 0.9 / 0.8 on unshown steps, + 0.5 / 0.3 of C | 0.4 to 0.6 | blocks that outlive a call (3,295 words on the route [C twin, speed_notes 33]); scratchpad leftovers |
| H. Ship, native path | (b): host-only | MOVE (list writing on the ME) | 1.29 / 0.70 | 1.0 to 1.3 | none for the game; the GE list must come out in the same order |
| I. Sorted pass | (b) for the packets; the game reads its view data and a flag (V2); 0x12d910's scratchpad words are read by w00a 0x3bebf0 (R5 LEFTOVER) | MOVE under E5 (design steps 3 and 7) | 1.69 / 0.88; on unshown steps 0.85 / 0.44 | 0.4 to 0.8 | the leftovers; the depth order must be the game's |
| J. Prim pass | (a): leaks rounding; a later shown packet reads its data [C twin H1] | STAYS | 0 | - | - |
| K. Stream restore | (a): read by the next call [C design 1.1] | STAYS | 0 | - | - |
| L. Loops, hooks, frame code | (a): order, page flags (R1), chain descriptors | STAYS | 0 | - | - |
| M. ME copy guard | (b) | goes when no chain is walked any more | 0.18 / 0.12 | 0 | - |

**The split in ms a step [C model for the costs; reader classes as above]:**

| | Rain (25.2) | Indoors (13.9) |
|---|---|---|
| STAYS (A, B, C, J, K, L) | 14.8 | 8.6 |
| - of it the runtime's DMA mechanism, which cut 1 shrinks (5.1 / 3.0 in all, 0.5 / 0.4 of that inside G) | 4.6 (cut 1: -2.1 [I]) | 2.6 (cut 1: -1.35 [I]) |
| MOVE (D, E, G, H, I, M) | 9.8 | 4.75 |
| - of it skippable on unshown steps before anything is moved (half of D, G, I) | 3.5, + 0.5 of C | 1.25, + 0.3 of C |
| DROP (F) | 0.6 | 0.6 |

What the ME can take: in the rain it is busy 55.25 against the main CPU's 69.32 [C], about 14 ms of room, and the moved jobs
(D, E, G, H, I) would add about 3.3 to 5.1 **[I]**, less whatever chain walks they end. Indoors the ME is already busy 45.15
against the main CPU's 41.43, and the main CPU waits 6.50 a step for the picture [C]: there a move pays only if it takes more
walk off the ME than it adds. The fast path and the unshown-step skips cost the ME nothing, but indoors they only pay once the
ME is no longer the limit.

## 4. Why the fast path comes first: what the scratchpad DMA costs [C model, C source]

The four functions of the screen pass move their data through the scratchpad with the DMA channels, the PS2's fast way to copy.
For every batch the translated code writes 7 DMA registers (status, control, address, count, scratchpad address, the interleave
word, start) and reads 1. On our runtime (boot/boot.c `rt_store`, rt/ee_io.h `mm_map`, `io_flush`, `io_dma_start`):

| Step of one transfer | Calls a rain step | ms a rain step | Indoors |
|---|---|---|---|
| each register write: `rt_st32` -> `rt_store` -> `memcpy` into the register page, then `io_flush` decodes the address again | 2,746 | 1.52 + 0.41 | 0.96 + 0.26 |
| the register read `rt_mm` | 438 | 0.13 | 0.08 |
| `io_dma_start`: reading the channel, the interleave loop's set-up | 438 | 0.81 | 0.50 |
| the look for pending interrupts after it (`k_poll_arm`, `k_poll_rest`, `k_take_m5`, `rt_poll_slow`) | 350 | 0.40 | 0.26 |
| the copies themselves (`rt_qcopy_nov`, 3 quadwords a part) | 1,543 | 1.80 | 0.91 |
| **Sum** | | **5.07** | **2.97** |

So the bookkeeping around the copies is 3.27 ms a rain step and 2.06 indoors, about 7.5 microseconds (2,500 cycles) a transfer,
for a transfer that takes no game time anyway (the pipeline runs with the clock stopped, OPT_ZT).

## 5. Proposal for gl45: the first cuts, in order

Every saving is **[I]** and none is reached. Single estimates on this project have been wrong by a factor of 2; only the PSP settles
a speed. One change per measurement: each cut is its own switch and its own turn kind.

### Cut 1. A fast path for the scratchpad DMA (runtime only). About -2.1 rain / -1.35 indoors [I]

- **What.** In the runtime, not in translated code: a store to the DMA register page writes the register directly (no general
  path, no `memcpy`, no second decode); a start on the two scratchpad channels does the interleaved copy at once and leaves the
  registers, the status bits and the scratchpad exactly as today's path does; the interrupt look runs only if the start changed
  a status bit that is enabled.
- **Basis of the figure.** Of the 3.27 / 2.06 ms of bookkeeping (section 4) I count 0.73 of `rt_store`'s 0.98, all of its
  `memcpy` 0.42, 0.31 of `io_flush`, about 0.40 of `io_dma_start`'s 0.81 and 0.26 of the poll: 2.1 in the rain; indoors the same
  shares scaled by the number of writes (1,761 against 2,746): 1.35. The copies (1.80 / 0.91) stay. Honest range 1 to 3 / 0.7 to 2.
- **Exactness.** Nothing the game can read changes: same register values, same scratchpad, same order of copies. No reader proof
  is needed. **Proof:** the strict hash against gl43's own reference on x86, the emulator and the PSP (it must not move); the
  three-way IOP / picture digests.
- **Risk.** A register sequence somewhere else in the game that relies on a side effect of the general path (a transfer started
  with the channel half set up; the chain modes). Keep the fast path to the exact case seen here (the two scratchpad channels,
  normal and interleave mode) and send everything else down today's path. The runtime's code layout changes, so the profile and
  the order are refreshed with it.
- **Cost to build.** Small: one runtime file pair, no generated code, no new reference.

### Cut 2. The people path of K-N2. About -1.3 rain / -1.3 indoors [I]

- **2a. The old builder returns at once on unshown steps** (as the ship's 0x1214a0 already does): -0.60 / -0.60
  [C model for what it costs today]. In a K-N2 turn its packets are never sent. **Proof:** the method of speed_notes 33 on the
  x86 twin (every shown step's packets and list words equal, masked game state equal, IOP and frame digests equal), the read
  hash of V3a's kind over the route, and its own strict reference. **Risk:** a shown step where the stub hands a mesh back to
  the old builder after skipped calls. Hand-backs are counted already, and "none on the routes" is already the stub's own
  requirement (boot/np6_stub.h); the same count over the route with the skip on is the check. If it is zero the risk is only in
  scenes not yet walked.
- **2b. The reuse check gets cheaper:** -0.7 of its 1.14 / 1.13 [I], by checking the cheap parts (descriptor, bases, descriptor
  sum) on every picture as today and the full content hash for a quarter of the meshes a picture in turn; or -1.1 by moving the
  hash to the ME in the rain. The check reads game RAM and writes only our own tables, so the game state is exact whatever it
  does. **Risk: picture only**: a mesh whose vertices the game rewrites in place (seen once, in the opening, vb 8,755
  [C nge_stub.h]) could be shown stale for up to 4 pictures. Whether that ever happens on w00a / w01a is UNKNOWN; how to find
  out: the count of "data changed" re-cooks in the people line over the route (x86 twin or the PSP log).
- **2c. The check's timing probe off** (`MGS2_NGE_HPROBE=0`): two clock calls a check, 88 a shown rain step; not in the model
  (a system call); about -0.1 to -0.2 on the PSP [I].
- **Cost to build.** Small (boot/np6_stub.h, boot/nge_stub.h; no generated code). 2a needs a new reference, 2b and 2c do not.

### Cut 3. The first unshown-step skips of picture-only work, under the E5 standard. Up to -2.9 rain / -0.9 indoors [I]

- **What.** On an unshown step, return at once from the leaf functions whose only output is packets nobody sends:
  StoreMatrixObjs 0x120860 (after cut 1: -0.95 rain / -0.8 indoors, half of its own cost and half of its 126 transfers a
  step), the sea builder 0x13bf78 with its exit state (-1.74 / 0, its 452 box tests go with it), DG_WritePrimPacks 0x12d910
  with its return value (-0.22 / -0.11). This is the unshown-step half of the design's steps 3, 4 and of job G, and it costs the ME nothing.
- **How to choose, cheaply.** The x86 dev switch for exactly this exists (`MGS2_LITE_FN=<address>[,...]`, boot/dg_lite.h with
  DG_LITE_DYN). One twin run a function, alone: are all shown steps' packets equal, and which read sites differ. Take into gl45
  only the functions whose differing reads all classify as ORDER / LEFTOVER (E5), or none.
- **Risk.** This is the class the H1 round left unproven for whole passes: leftovers in the scratchpad and VU0 that later code
  loads. Leaf functions leave fewer of them than passes, and StoreMatrixObjs passes the rounding test, but that is a reason to
  test, not a result. The sea has the open question of section 3 (flags and LOD). If a function fails, it waits for the
  renderer's own E5 step.
- **Not in this cut:** 0x121c30 and the sort lists (the game reads view data and a flag there), 0x12dc90 and 0x121060 (they break
  frames [C twin H1]).

### What the three cuts would leave [I]

| | Rain | Indoors |
|---|---|---|
| Pipeline today [C] | 24.70 | 13.31 to 13.71 |
| After cut 1 | about 22.6 | about 12.2 |
| After cuts 1 and 2 | about 21.3 | about 10.9 |
| After cuts 1, 2 and 3, if every function of cut 3 passes | about 18.4 | about 10.0 |
| Floor if every MOVE job also left the main CPU on shown steps | about 13.1 | about 7.6 |

A step must be 25 ms for 20 fps; the rain's logic alone is 38.4 [C]. So these cuts are worth about 6 of the rain's 69 ms and
about 3.5 of the 41 indoors; they do not reach the goal by themselves. In the rain they should show as step time one for one
(the main CPU is the limit, section 0 point 7). Indoors the drawn step is 47.93 with the main CPU waiting 6.50 for the picture
[C], so there the same cuts only lengthen the wait until the ME's walks come down.

### After gl45 (not proposed now)

- The Vita seam on shown steps for D, E, G, H, I: frees up to another 6 rain / 3 indoors on the main CPU and adds 3.3 to 5.1
  to the ME [I]; fine in the rain, indoors only together with fewer chain walks.
- A cheaper exact box test (job B and the sea's share, 3.9 ms of program a rain step): see UNKNOWN 3.

## 6. UNKNOWN, and how to find out

1. **The shown / unshown split per function as a measurement.** Section 1 has it from the code's conditions only. Find out: a
   model run counting on `g_dg_lite`, or the PSP's per-step log of a K-N2B turn (alternate steps).
2. **Who starts each scratchpad transfer.** The profile has no caller for `io_dma_start` (it is entered from `io_flush`); the
   split in section 2a is by the register read counts. Good enough for cut 1, which treats them all alike.
3. **Whether the box test has a cheaper exact form.** The VFPU form is already the gl41 one (P1, P3) and still takes about 770
   instructions a test because it follows the micro program step by step, flags included. b_proof_notes.md ("Limits") names the
   idea: compute the result fast and the leftovers (VF10, VU0 memory) only when game code reads them. Gain UNKNOWN, up to about
   2.5 of the rain's 3.9 [I, weak]. Find out: a design pass on which leftovers are read where (the read log on the VU0
   registers after each call site), then a twin.
4. **The sea's flags and LOD** (section 3, D): are the fields game code reads written inside 0x13bf78 or by its caller? Find out:
   the twin run of cut 3 with `MGS2_LITE_FN=13bf78` and the read log on the patch object's fields.
5. **Hand-backs and in-place rewrites on the people path** (cut 2): the counters exist in the people line; nobody has read them
   over w00a / w01a for this question. The gl43 PSP log files are not in the container (OWED from the lead's side).
6. **The real saving of cut 1.** [I] until the fast path is written and run in the model (one bucket run like this study's,
   10 minutes) and then on the PSP.
7. **Scenes not profiled:** only the rain wait (w00a) and three indoor places of w01a. The w01a function names are by address
   only (no name list for that overlay); none of its stage code shows in the pipeline (under 0.02 ms).

## 7. Files

- `/home/claude/main/study_pipe45/` (container): `run_pp45.sh` (the run), `cfg/mgs2_pp45.cfg`, `model/pp45*` (8 windows: per-pc
  counts, call edges, D-miss lines), `pipe.py`, `groups.py`, `final.py`, `table1.py`, `callers.py`, `dh.py` (the tables of this
  note), `model/log_pp45.txt` (98/98 hash lines).
- Read for this note: gl43's source tree `/home/claude/t43` (boot/boot.c, boot/dg_lite.h, boot/nat_stub.h, boot/np6_stub.h,
  boot/nge_stub.h, rt/ee_io.h, psp/xl_psp.c, runtime/vu0.h, runtime/host_time.h); native_ge_renderer_design.md 1.1, 2.4-2.5, 4.1,
  5.1-5.2; h1_notes.md; b_proof_notes.md; speed_notes.md 33; vita_port_study.md; prof/hw_gl43/gl43_psp_read.md;
  prof/hw_gl42in/gl42in_psp_read.md.
