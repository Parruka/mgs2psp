# Speed levers on the three biggest main-CPU costs (rows A, D, E), H1 excluded

Study of 2026-11-23. Nothing went to the PSP and no PSP build was packaged. The work copies are in the container (scratchpad/top3); the small
patches and tools are in `proto/` and `tools/` next to this file. The dated notes are in `state.md`.

**Source profile:** gl39 play form, `build_gl39/model/ka39.1`. That is turn t56, K0 digest off, vb 29,610, 80 vblanks, which is one
PSP step. **Cost model:** PSP ms = (1.65 instr + 68 I-miss + 86 D-miss) / 333,000 a step.

**Marks:**
- [C] means read in code or measured.
- [C model] is from the cost model.
- [C static] means counted in disassembly and priced with the model.
- [I] is inferred, with its basis given.
- UNKNOWN has no basis yet.

**Reading the PSP column:** the main CPU's model-to-PSP factor is about 1.0. Earlier changes that only removed ALU work got about 0.7 of
their model gain on the PSP (v29). Single estimates in this project have missed by 2x before, so read every [I] as a range that can
itself be off by 2x.

**The rule for every lever below:** state hashes stay equal on x86, the emulator and the PSP. Under the lead's decision of 2026-11-18
(E1 accepted), the reference is today's VFPU-rounded results, not the PS2's. Levers that are not exact are in their own list (§7).

## 1. Where the rows' time sits [C model, ka39.1]

**Generated code: 37.11 ms a step.**
- 4.225 M instructions (21.1 ms), 34.6 k I-misses (7.1 ms) and 35.3 k D-misses (9.1 ms).
- About 1.05 M guest instructions, so 4.0 PSP instructions a guest instruction (v26: 5.9).

| Row | Generated code (ms) | Instr / I-miss / D-miss | Biggest classes |
|---|---|---|---|
| A, characters and game systems (17.8) | 17.80 | 9.29 / 5.29 / 3.23 | memory 5.85, call protocol 4.06, VU0 3.26, branches 2.70, ALU 1.90 |
| D, PS2 drawing lists (16.7) | 14.61 | 8.96 / 1.04 / 4.61 | memory 6.92, VU0 2.80, branches 2.21, call protocol 1.57 |
| E, VU0 micro programs as C (8.7) | runtime code | | box cull 4.14, slerp 1.62, call path 0.82 + vu_run part, vif0_mscal rehash 0.24, vup_43da1f40 0.48 |

**Classes across all generated code:**

| Class | ms | Detail |
|---|---|---|
| VU0 macro work | 8.1 | macro ops 3.18 at 8.4 PSP instr an op; lqc2/sqc2 3.45 at 6.9; inlined libvu0 leaves 1.30; moves 0.21 |
| Call protocol | 6.1 | call 2.18 at 16.7 instr a guest call, 13.5 k calls a step; entry loads 1.83; return 1.39; exit stores 0.73 |
| Guest memory | 13.8 | about 7.7 of it D-misses |
| Loop polls | 1.88 | 45 k a step, 6.7 instr each |
| Unaligned load/store | 1.01 | |

**Executed-code census** (tools/census2.py, MGS2T.elf + ovl_w00a.elf):
- 114 k checked guest accesses a step. srl 28 + beql alone is 222 k instructions, 1.1 ms of instruction time.
- Making high words (sra 31 + sw) is only 41 k instructions, 0.2 ms. That is too small to be a lever.

**I-cache:**
- Generated code touched 14,112 lines (882 KB).
- 34.5 k misses a step: 8.0 k on lines that miss about once a step, and 26.4 k on 4.9 k lines that miss more often.
- The misses are capacity-bound: shrinking hot text is the lever. Placement retrain (gl39 M7) gained nothing.
- The model shares one cache between main and ME code in K0 windows, so these figures may overstate the main CPU's misses (UNKNOWN by how
  much).

## 2. The gl39 drop reasons for M1, M2, M4 and M6

| Lever (gl39 plan size) | gl39's reason | Holds today? |
|---|---|---|
| M1, box cull by hand (-1.8) | Scalar hand work saves at most about 90 instr, about 0.45 ms; deeper cuts need vscl or masked vmov forms not proven on hardware. | **Mostly holds.** An exact cut on the VFPU side needs no new hardware fact: P3 (§5), -51 instr a call, -0.26 [C static]. The call path is a separate lever (E1 below). The -1.8 cannot be reached exactly with what we know today. |
| M2, VU0 dispatch cache (-0.4) | vuc_dispatch is generated, so a per-site cache needs the recompiler re-run. | **Wrong.** A runtime-only fast path in runtime/vu0.h plus a table made by script does it (P1, §5). The size was also understated: the whole call path is about 132 instr a call, not only the dispatch. P1: -1.0 [C static]. |
| M4, natives DG_SetObjsMatrix + StoreMatrixObjs (-0.8) | The native's VFPU form rounds to nearest; the owner's "Everything exact" (2026-11-08 17:46). | **Stale.** The lead accepted E1 on 2026-11-18 and the VFPU stays (DEVIATIONS E1). The native must equal today's generated VFPU form, which means the same VFPU ops in the same order; nat_dbc0 is the precedent. Still worth about -0.6 to -0.8 [I]. Note: DEVIATIONS.md still lists the 11-08 choice under the lead's decision. If the owner overrules the lead, the same natives can be written in scalar round-toward-zero. |
| M6, sort keys side by side (-0.4) | nat_dbc0's misses are sequential reads of the game's vertices, so they are inherent. | **Holds for a layout change.** Sequential misses are the textbook case for prefetch: `cache 0x1e` one or two lines ahead (D6). Whether the Allegrex fill blocks is UNKNOWN, and PspProf counts fills as free "pref", so the model cannot settle it. A hardware probe comes first. |

## 3. Ranked levers per row (exact ones only)

Days are working days for one agent, including the x86 compare harness and the twin and emulator hashes. They do not include the PSP run.

### Row E: VU0 micro programs as C (8.7 ms)

| # | Lever | Model ms | PSP ms | Days | Proof | Risk |
|---|---|---|---|---|---|---|
| E1 | **P1, VU0 call fast path**: 16-entry (image hash, entry) to (VFPU form, C form) cache in vu_run_shared. A miss or a dirty image takes today's path, which refills the entry. | **-1.0** [C static: about 132 to about 34 instr a call, x 1,977 calls; instruction part only] | -0.7 to -1.0 [I]. I-cache effect of programs leaving vuc_dispatch (+8.2 KB text): UNKNOWN sign | 1.5 (prototype exists) | Hashes; table checked equal to vuc_dispatch's cases by script; A/B switch inside one run | low |
| E2 | **P3, drop the box cull's 8 per-corner NaN/Inf checks** (the out_ check stays) | **-0.26** [C static: -51 instr a call, x 1,011] | -0.2 to -0.3; the VFPU compare-branch stall saved is UNKNOWN and may make it larger | 0.5 (generator rule or patch step) | Proof sketch (§5) + hashes + forced NaN/Inf test (VPC on x86, VFPU in the emulator) | low |
| E3 | Slerp: native SetQuatSlerp (S_w00a_P3bebf0) fused with program 33268338. Scalar round-toward-zero + flush (as today's C form), same op order, lanes in FPU registers, not in VF memory. | -0.6 to -0.9 [I: 628 instr a call, 144 of them lwc1/swc1 of VF lanes, plus the call path; hand count] | -0.4 to -0.8 | 3-4 | Per-call x86 compare of VF/VI/ACC/Q/flags, as with NAT_DBC0_CHECK + hashes | medium |
| E4 | Fused native BoundCheck (P11f968, P13bd00) + box cull: same VFPU ops, inputs straight from guest memory into VFPU registers, outputs written as today | -0.3 to -0.5 on top of E1 + E2 [I: BoundCheck 119 instr a call, half of it VF staging] | -0.2 to -0.5 | 4-5 | Same as E3 + NaN test | medium |
| E5 | Box cull trims that need hardware facts: mfv/mtv pairs to vmov (about -23 instr); skip VI loads and stores the program does not write (about -12 to -30, needs a proof that VI stays 16-bit); clip flags by integer compares instead of vcmp + vsync + mfvc | -0.1 to -0.3 [I] | UNKNOWN (stall share) | 2 + probes | Hardware probe (vmov bit copy of NaN and denormal payloads) + hashes | medium |
| E6 | vif0_mscal: cheaper image identification after an MPG change (compare with the last image, or hash only the uploaded range) | -0.1 to -0.15 [I: 0.24 today] | about -0.1 | 0.5-1 | Hashes; the image is selected the same way | low |
| E7 | vup_43da1f40 (0.48 ms, 15 calls, I-miss heavy): smaller form | -0.1 to -0.2 [I] | UNKNOWN | 2 | VU_CHECK compare | low-medium |

### Row D: PS2 drawing lists (16.7 ms; generated part 14.61)

| # | Lever | Model ms | PSP ms | Days | Proof | Risk |
|---|---|---|---|---|---|---|
| D1 | **M4 natives**: DG_SetObjsMatrix (217 calls x 443 instr) + StoreMatrixObjs (21 x 6,450); VFPU same ops (E1 reference) | -0.6 to -0.8 [I: their instruction part is 1.15 ms; their 0.63 ms of D-misses stays] | -0.4 to -0.8 | 4-5 | Per-call x86 compare + hashes | medium |
| D2 | Extend D1 to ScreenObjs (42 x 1,485, 0.22 of it VF staging) and the DG_ScreenChanl loop | -0.2 to -0.3 [I] | -0.15 to -0.3 | +2 | Same | medium |
| D3 | OnePieceObjs native (1.68 ms: 0.56 unaligned copies, 0.35 inlined MulMatrix, packet set-up into the PS2 scratchpad) | -0.5 to -0.8 [I: instruction part 1.12] | -0.4 to -0.7 | 5-6 | Same + scratchpad image compare | medium-high |
| D4 | Packet builders ChainObj chain2 (1.15), ChainObj patch (0.96), SortChainObjs (0.92): natives. Check first how far the native renderer's work already covers them. | -0.3 to -0.6 [I] | similar | 5+ | Same | medium-high |
| D5 | Store-miss avoidance: `cache 0x18` (create dirty exclusive) before whole-line packet writes | -0.3 to -0.8 [I: store 64/128 class has 1.44 ms of misses in all generated code; the whole-line share is UNKNOWN] | UNKNOWN | 3 + proof per site | Proof per site that the line is fully written before any read and is not read by the ME or DMA in between; hashes | **high** (a wrong site corrupts memory) |
| D6 | Prefetch (`cache 0x1e`): nat_dbc0 (2,413 sequential misses a step = 0.62 model ms) and object-header walks (DG_ScreenChanl, TmpLightPreshade) | up to -0.5 (nat_dbc0) + -0.2 to -0.5 [I] | UNKNOWN until the fill probe | 1 + probe | Exact (a hint only); gain needs the hardware probe | low for exactness; gain unknown |

### Row A: characters and game systems (17.8 ms)

| # | Lever | Model ms | PSP ms | Days | Proof | Risk |
|---|---|---|---|---|---|---|
| A1 | **libvu0 leaf natives** (rt_idiom or native pattern): RotMatrixX/Y/Z + _sceVu0ecossin (pool 1.40; ecossin runs at 12.5 PSP instr a guest instr), sceVu0MulMatrix (0.47, 289 calls), MT_QuatToMat (0.42); VFPU same ops, one check at the end | -0.9 to -1.3 [I: pool 2.29; VU0-heavy leaves shrink 2-3x when the VF staging and the per-group checks go, as nat_dbc0 did] | -0.6 to -1.2 | 4-6 | Per-call x86 compare + hashes | medium |
| A2 | MT_QuatNormalize (706 calls x 88 instr, FPU): inline at its two callers (generator) | -0.1 [I] | -0.1 | 1 | Hashes | low |
| A3 | HZX_SwitchTrapQuick+11f8 (1.34 ms; already 1.9 PSP instr a guest instr; 0.55 D pointer walk): prefetch of the next node only | -0.1 to -0.3 [I] | UNKNOWN (probe) | 1 + probe | Exact (a hint) | low |
| A4 | MT_ActMotion, MT_ActMotion2, HZX_LevelHazardCheck (I-miss heavy, 15 calls a step): no lever of their own; only the generator-wide cuts in hot text help | | | | | |

## 4. Generator-wide levers (all rows) and the I/D-cache levers

| # | Lever | Model ms | PSP ms | Days | Proof | Risk |
|---|---|---|---|---|---|---|
| G1 | **Lean step 4**: VU0 macro groups keep VF in VFPU registers across groups inside a function; no lv.q/sv.q staging per group; checks merged with a defined restart point | -2.0 to -3.0 [I: pool 8.1 ms; hand count on MulMatrix 49 to 32 instr an iteration, -35%] | -1.4 to -2.5 | 10-15 | VU_CHECK-style compare + hashes; designing the fallback is the hard part | medium-high |
| G2 | **Call protocol** (lean steps 5/6: hot leaf inlining, register arguments) | -1.5 to -2.5 [I: pool 6.1 ms] | -1.0 to -2.0 | 10-15 | Hashes | medium |
| G3 | k_left in a fixed register (as g_mem is in $23) | -0.3 to -0.6 [I: 45 k polls a step x about 3 instr, minus register pressure] | -0.2 to -0.5 | 2 | Exact by construction + hashes | low-medium (every object, the runtime and ME code built with it) |
| G4 | Drop address checks proven redundant (same unchanged base, offsets that cannot leave the region) | -0.3 to -0.6 [I: census 114 k checks a step, 1.1 ms in srl + beql] | -0.2 to -0.5 | 4-5 | Generator rule proof + hashes + a test at region ends | medium |
| G5 | Fuse ldl/ldr and sdl/sdr pairs on the same word | -0.2 to -0.35 [I: pool 1.01 ms at 3.3 PSP instr a guest instr] | -0.15 to -0.3 | 1-2 | Hashes | low |
| I | I-cache: no lever of its own. G1-G5 and the natives shrink hot text. | -1.0 to -1.8 of the 7.07 ms [I: if hot text falls 15-25%, misses fall about in proportion]; on top of the instruction gains | UNKNOWN | | Model run of the build | |
| DS | Guest scratchpad misses (7.2 k a step, main + ME, about 1.6 ms): put the hot part of the guest scratchpad in the PSP's own 16 KB scratchpad. R[], VF and VIr already use about 2 KB of it. | UNKNOWN (the main CPU's share is not split out) | UNKNOWN | design | Hashes | high (the address path changes) |
| x | Dropped: hi-word stores (0.2 ms pool); placement retrain (M7, 0 gain); hot/cold split and -Os (not taken before, memory) | | | | | |

**Overlaps:** G1 takes a third to a half of A1, D1 and E4. G2 takes part of A2. Do not add the rows of §3 and §4.

## 5. What was prototyped and measured (container only, compile-only)

**Tools** (`tools/`):
- **pg2.py**: prof_guest with gl39's model, the #line map of the w00a header and guest counts for each function.
- **fnann.py**: disassembly of a function with counts for each PC.
- **calls.py**: call edges for each step.
- **census2.py**: the executed-code census.
- **dtop.py, dmix.py, dh.py**: D-miss PCs and regions.

**Build check:** gen.o and gen_s_w00a.o were rebuilt with -g from the build's own commands; text grew 0.16-0.2%.

**P1, VU0 call fast path** (`proto/vu0_p1.diff`, `proto/vuc_fns.h`):
- runtime/vu0.h, copied, with the fast path. vuc_fns.h is made by script from vuc_dispatch's cases.
- **Baseline:** boot_rt.o built with the use build's command and profile is byte-equal to build_gl39/u/boot_rt.o [C].
- **Variant:**
  - vu_run_shared is 0xe8 bytes.
  - vuc_dispatch shrinks from 0x4b8c to 0x564 bytes. GCC had inlined programs into it and saved 9 GPRs + 12 FPRs on every call.
  - Text +8.2 KB.
  - Hot path: about 34 instr (VFPU form) or about 27 (C form, a tail jump), against about 132 today [C static]. Today's 132 is
    vu_run_shared 38.6 + vuc_entry 19.2 + vuc_dispatch 75 [C model, per call].
- **Why it is exact:** it calls the same functions, does the same FPU-mode test (vp_fast_env) and uses the same VPF wrapper on a VFPU
  fallback. A dirty image takes the old path. Not run.

**P3, box cull per-corner checks** (`proto/p3_boxcull_checks.py`):
- In a copy of gen_vu.h, the 8 `vcmp.q ES` + `goto vfail_` blocks are blanked; line numbers are kept.
- **Why it is exact:**
  - After each corner's check, the register is only multiplied (vmul, z/w masked) until the out_ check of the same register.
  - So a non-finite lane stays non-finite (NaN x anything = NaN, Inf x 0 = NaN, Inf x finite = Inf).
  - The out_ check therefore still takes vfail_, which has written nothing, and the C form reruns from the entry state.
  - The same holds on x86 (VPC path).
- **Control:** the unmodified copy at a scratch path. Both lose gen_vu.h's PGO profile, because GCC's line checksum includes the file
  name.
- **Result** [C static]: vupv_96936c0f_0 goes from 882 to 831 instructions (-51). The function is straight-line, so all 51 are on the
  executed path.
- **Leftover hardware question:** VFPU IEEE behaviour for Inf x 0, to be confirmed by the NaN test in the emulator and by one PSP probe.

**Not done:** no twin or emulator hash runs and no model runs of P1 or P3. Copying a twin tree would have pressed on the shared disk
(6.7 GB free). Their proof is the next builder's first step.

## 6. Native-candidate verdicts, top 25 of rows A and D [C model, ka39.1]

ms = instr / I / D; calls a step; instr a call.

| # | Row | Function | ms (i / I / D) | Calls x instr | Main classes | Verdict |
|---|---|---|---|---|---|---|
| 1 | D | OnePieceObjs P11fa58 | 1.68 (1.12 / 0.07 / 0.48) | 166 x 1,361 | unaligned 0.56, inlined MulMatrix 0.35 | **native** (D3), after D1; G5 takes part |
| 2 | A | HZX_SwitchTrapQuick+11f8 P14b008 | 1.34 (0.75 / 0.04 / 0.55) | 27 x 5,608 | loads 0.63, branches 0.51 | no (already 1.9 PSP instr a guest instr); prefetch only (A3) |
| 3 | D | ChainObj chain2 P12e8b8 | 1.15 (0.86 / 0.03 / 0.26) | 44 x 3,949 | store 64/128 0.41 | maybe (D4); check against the native renderer's people work |
| 4 | D | StoreMatrixObjs P120860 | 1.12 (0.67 / 0.02 / 0.42) | 21 x 6,450 | VF staging 0.34 | **native** (D1, M4) |
| 5 | D | ChainObj patch P13bf78 | 0.96 (0.63 / 0.03 / 0.31) | 2 x 63,123 | loads 0.43 | maybe (D4); prefetch |
| 6 | D | SortChainObjs P121c30 | 0.92 (0.61 / 0.01 / 0.31) | 1 x 122,213 | store 64/128 0.44 | store-miss lever (D5) before a native |
| 7 | D | DG_SetObjsMatrix P120538 | 0.71 (0.48 / 0.02 / 0.21) | 217 x 443 | inlined leaf 0.23 | **native** (D1, M4) |
| 8 | D | DG_Prim2Chanl P12dc90 | 0.69 (0.37 / 0.01 / 0.30) | loop | inlined leaf 0.18, branches 0.18 | no; nat_dbc0 already has its inner loop |
| 9 | A | _sceVu0ecossin P166090 | 0.66 (0.52 / 0.12 / 0.02) | 258 x 405 | branches 0.39, VU0 0.23 | **native** with RotMatrixX/Y/Z (A1) |
| 10 | D | DG_TmpLightPreshadeChanl+128 P12d688 | 0.62 (0.28 / 0.00 / 0.35) | 217 x 257 | branches 0.37 | no; prefetch only |
| 11 | D | DG_WritePrimPacks P12d910 | 0.58 (0.47 / 0.01 / 0.10) | 362 x 264 | stores 0.17 | no (2.3 PSP instr a guest instr); D5 |
| 12 | D | ScreenObjs P11fde8 | 0.52 (0.31 / 0.07 / 0.14) | 42 x 1,485 | VF staging 0.22 | with D1 (D2) |
| 13 | A | MT_ActMotion S_w00a_P3beee8 | 0.48 (0.17 / 0.18 / 0.14) | 15 x 2,235 | I-miss | no; text shrink only |
| 14 | A | sceVu0MulMatrix P165d78 | 0.47 (0.37 / 0.04 / 0.06) | 289 x 260 | VF staging 0.21, VU0 0.14 | **native** (A1), or G1 |
| 15 | A | MT_ActMotion2 S_w00a_P3bf860 | 0.47 (0.19 / 0.10 / 0.18) | 15 x 2,563 | unaligned 0.15 | no; G5 |
| 16 | A | MT_QuatToMat P11c828 | 0.42 (0.28 / 0.07 / 0.07) | 241 x 237 | VU0 0.26 | **native** (A1) |
| 17 | D | BoundCheck screen P11f968 | 0.41 (0.33 / 0.01 / 0.07) | 559 x 119 | VF staging 0.23 | fused with box cull (E4) |
| 18 | D | DG_PutVector P125f18 | 0.39 (0.31 / 0.04 / 0.04) | 440 x 144 | VF staging 0.17 | native with A1 (-0.15 to -0.2 [I]) |
| 19 | A | CalcObjsCollisionWithInvM S_w00a_P3b89e0 | 0.37 (0.27 / 0.07 / 0.04) | 9 x 5,988 | VU0 0.23 | no; G1 |
| 20 | A | l2dGetParts+5d0 S_w00a_P3a2280 | 0.33 (0.19 / 0.00 / 0.14) | 14 x 2,681 | stores 0.17 | no |
| 21 | D | CalcSortZ P12dbc0 | 0.33 (0.28 / 0.00 / 0.05) | 362 x 154 | | no (nat_dbc0 in place) |
| 22 | A | MT_QuatNormalize P11cd68 | 0.33 (0.31 / 0.02 / 0.01) | 706 x 88 | FPU 0.22, call overhead | inline (A2) |
| 23 | A | HZX_LevelHazardCheck P14d860 | 0.30 (0.09 / 0.17 / 0.04) | 15 x 1,175 | I-miss | no |
| 24 | D | DG_ScreenChanl P121060 | 0.30 (0.10 / 0.01 / 0.19) | loop | loads 0.12 | with D1/D2; prefetch |
| 25 | D | DG_SetTmpSpotLight+178 P126f58 | 0.29 (0.12 / 0.07 / 0.09) | 7 x 3,551 | FPU loads 0.11 | no |

Next in line: RotMatrixX/Y/Z at 0.25 each (A1), BoundCheck patch P13bd00 0.26 (452 calls, E4), WriteJointData 0.27, HZX_AddDynamicTrap
0.25 (268 calls), GV_ExecActorSystem 0.23. The full list is in the container at scratchpad/top3/top_rowsAD.txt.

**What the natives must keep:**
- Exact VFPU op order.
- One NaN/Inf check at the end, with the translation as the fallback.
- Virtual time and the poll points (NAT_POLL_SOON, as nat_ls_prim does).
- An x86 per-call compare switch (as NAT_DBC0_CHECK).

## 7. Not exact (they change hashes, the game's logic, or need the owner)

| Lever | Model ms [I] | Why not exact |
|---|---|---|
| Box cull with vtfm4/vdot (fused dot products) | about -2.5 (of 4.14) | rounding of the fused sums differs from separate vmul + vadd |
| No address checks | -2 to -3 (text -32.7%, speed_notes) | a non-RAM access would go wrong |
| Half-rate motion (MT_* family pool about 3.8 ms) | about -1.9 | game logic; the owner's rule |
| Slerp on the VFPU (round to nearest) | about -1.0 | today's slerp is round-toward-zero C; the hashes would move |
| Drop all VFPU NaN/Inf checks | -0.8 to -1.2 | no fallback when a value is not finite |
| Coarser loop polls | about -0.9 | virtual time changes |
| ecossin via vsin/vcos | about -0.5 | different values |

## 8. Recommended order for the two builds after gl40

**gl41: exact, small, runtime-side, plus hardware probes** (start after gl40's log is read).
1. E1 (P1, the VU0 call fast path), with a switch that alternates inside one run.
2. E2 (P3, the box cull per-corner checks), as a generator rule (vupipe) or a patch step on gen_vu.h.
3. E6 (vif0_mscal rehash).
4. Measurement-only probes, each off by default:
   - (a) a timed sequential walk with and without `cache 0x1e` a line or two ahead, to show whether the fill blocks;
   - (b) VFPU vmov and Inf x 0 behaviour on hardware;
   - (c) a timed loop of the box cull on recorded inputs, to see its stall share.

Expected: -1.3 to -1.4 model ms [C static], -0.9 to -1.3 PSP [I].

**gl42: natives in the existing pattern, and small generator items.**
1. A1: RotMatrixX/Y/Z + ecossin first, then MulMatrix, QuatToMat, DG_PutVector.
2. D1, the M4 natives.
3. G3 (k_left in a register) and G5 (unaligned pairs).
4. D6 prefetch in nat_dbc0, only if probe (a) shows a fill that does not block.

Expected: [I] -2.0 to -3.3 model ms, about -1.4 to -2.6 PSP. This can be off by 2x.

**After that:** G1 (lean step 4) and G2 (call protocol) are the largest exact levers (-2 to -3 and -1.5 to -2.5 model [I]), at 2-3 weeks
each. Then decide whether E3, E4 and D3 are still worth doing on top of G1, and look again at D4 and D5 with the renderer work in view.

Each lever goes behind its own switch and is compared with its twin. One change per measurement still applies: a build may carry
several levers only if the harness alternates each one inside the run.
