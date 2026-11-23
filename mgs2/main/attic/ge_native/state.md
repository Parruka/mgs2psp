# Native GE renderer design: state (design agent, 2026-11-23; newest block first)

Task (lead, 2026-11-23): design a real PSP renderer (models cooked at stage load into GE-ready data, GE hardware transform,
lighting, skinning; game logic exact; ME freed for the sound mix). Output: /mnt/project-files/mgs2/native_ge_renderer_design.md.
Plan and container experiments only. Scratch: /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/gen/
No file in /home/claude/main, /home/claude/t39, build_gl* or the other agents' scratch (h1, gl41s) is changed.

## 2026-11-23 22:50 UTC: STEP 2a, the people's records (0x12e8b8 stub), checked on the twin
- Code (scratch st2/t = st1g/t + step 2): ge/natp06.h (NatP6 72 B: key, constant block, 3 materials, 6 streams, header, MSCAL;
  nat_p06_group: the record to lane 2's xl_lane_p06, else word by word), boot/np6_stub.h (P12e8b8 and P12e790 wrapped; the builder's
  loop in C, with its material memo kept by the stub and cleared by the template set-up's wrapper; arms 0/1/2/3/4 as the ship's;
  MGS2_NP6, else MGS2_NGE_CYCLE digit 2 -> arm 3), boot/nat_x86.c (twin walk: people beside the ship, merged by key), nge_stub.h
  (cycle digits 0-2). Twin gen.o: P12e8b8 / P12e790 weakened with _orig (both reached only through relocated calls [C objdump]).
- Twin, refcmp vs hashref_gl38 [C twin]: arm 1 (builder + compare) 0 differ: 2,163,292 records = their packets word for word, 0
  differing, 0 P06 packets without a record; most groups in a picture 494 before vb 24,000, 893 after (vb 36,772): cap 1,024.
  Arm 2 (records drawn, packets stepped over) 0 differ; list/frame/content/drawn/drawn2 = arm 1 in 17,664/17,664.
  Arm 3 (stubbed, records drawn): every picture digest = arm 1 in 17,664/17,664; 0 fallbacks; state hash differs from vb 6,300
  (623/685: packets not written, as expected): needs its own hashref + the read proof (next).

## 2026-11-23 22:00 UTC: STEP 2 (people native) started
- Coordinator: step 1 accepted. gl41 (built by the gl40 build agent) = gl40 + t40_step1.diff + gl41s P1/P3/probes + heap fix (a)(b)(c),
  turns K-A, K-N1, K7, arena 256 KB static. Step 2 here, container + emulator only, on gl40 + step 1 (scratch st2/): 0x12e8b8's
  records, C2 cooked per model, per-frame pass for blended vertices, its own check mode, V3a read proof; K-N2R turn kind and
  MGS2_NGE_CYCLE digit 2, each with its twin hashref; carve after the opening sized for ship + people (grown once the opening is
  over, guarded like the evictions). No PSP package (the checkpoint build comes after gl41's log). nice -n 19, one heavy job.

## 2026-11-23 21:52 UTC: STEP 1 done on gl39's and gl40's trees; results in step1_results.md; hand-back
- Late-walk bug found and fixed [C twin]: ge/xlme.c passed the walk's own index to nge_walk, but lw_nat's g starts at the pending
  groups (nat_i0), so a group took another group's reference. Twin arm MGS2_NAT_LW=2 (the PSP's walk, gl40's copy plan): 1,139
  declined by count, 5 groups drawn from another group's cook (content/drawn2 differ in 5 pictures, vb 35,027-35,137); on the
  emulator the 10 count declines. Fix: nge_walk(b, x->nat_i0[b] + i). After it (lw3): refcmp 685/0, 1,657,824 groups 0 differences,
  0 declined, content and drawn2 = off in 17,664/17,664, XP check 223,280 plans 0 differing. Both diffs carry the fix.
- gl40 tree (st1g) [C twin]: off = gl39-tree off in every digest of 17,664 pictures; on + check 0 differences; refcmp 0 differ each.
- gl40 tree emulator [C emu]: with gl40's final pads (EBOOT 83c78800 / 4423bdea) step 1 breaks textures even with the switch off:
  image +25.7 KB, newlib can no longer take the 128 KB for the ME picture heap's 64 -> 128 KB texture-table resize at vb 10,850;
  1,588,640 textures refused, 703 of 3,522 pictures differ; hashes still 62/62. Linked without ME_PRE/END_PAD (image +12.2 KB):
  off (g0np) = gl40's own EBOOT in every list/frame digest 24,000-31,400 and every shot but shot_24784; on (g1np, 0d5c1e75, with
  the fix) 62/62, 0 declined, W_ALL 0, fill = twin, shots equal to off. So gl40's real margin at that resize is ~12-25.5 KB; the
  log's 64 KB low point (vb 23,677) does not show it. gl41 (step 1 + gl41s's +22-33 KB) must find memory first (results §6 item 2).
- shot_24784 is emulator noise: gl40's own EBOOT and g0np differ there by <= 2 at 225 px with equal digests.
- gl41s merge [C]: no file in common; their P1, P3, probes apply on gl40 + step 1 with no failed hunk. The conflict is the build
  (one PGO training, one code order and pads) and the heap.
- Shared: step1_results.md here; step1/ refreshed (both diffs with the fix, scripts_gl40/rl_np.sh, chain3.sh). Shots stay in the
  container (st1/emu_*, st1g/emu_*). No PSP build delivered.

## 2026-11-23 20:17 UTC: STEP 1 emulator done on gl39's tree; step 1 moved onto gl40's tree (st1g), its checks running
- Emulator (PPSSPP headless, st1/pk, EBOOTs 327b5a32 / 900ac1ac, emulator only) [C emu]: off n0 and on n1, n1b (256 KB): 62/62
  hashes, 0 differ each. Fixed shots 15/16 pixel-equal to off; shot_24784 differs by 1 in one channel at 50-75 scattered pixels,
  and also between the two on runs (52 px): cause UNKNOWN, test on gl40 under way (digests over 24,000-31,400, a third on run).
  Live shots showing the same frame: all equal. Stub-time WORLD vs the converter's (new count W_ALL): 0 on emulator and twin.
  Fill lines on the emulator = the twin's, window by window. Converter declined 10 groups by count on the emulator (0 on x86).
- Memory [C emu]: nge tables 65,024 B + 2 x 47,104 B from the system's memory outside newlib's heap (after: largest 36 KB, total
  60 KB); newlib heap low point 64 KB as gl39 (the first build put them in the heap and stopped at boot; fixed).
- Twin on3 (W_ALL + ring build) [C twin]: refcmp 0 differ; check mode 0 differences of every kind; content and drawn2 = off.
- gl40 final (coordinator): step 1's diff applied to a copy of t40 (offsets only), in st1g/t; twin (build_gl40/x19), PSP build
  (gl40's u, gcda, ord/final and its pf pads), then twin off/on and emulator g0/g1/g1b. gl41s's patches touch only runtime/ and
  rt/ee_io.h: no file in common with step 1.

## 2026-11-23 19:10 UTC: STEP 1 twin checks done (st1/bld, run.sh tags off / on)
- Switch off [C twin]: refcmp vs hashref_gl38 685 lines, 0 differ; list, frame and content digests equal to step 0's run for all
  17,664 pictures (step 0's harness drew today's picture).
- Switch on, check mode MGS2_NGE_CHK=1 [C twin]: refcmp 685 lines, 0 differ; 1,657,824 referenced df0 groups compared with today's
  batch (count, vertices, strips, decoded list words, WORLD/pa/pb, box, first drawn vertex, transform words): 0 differences;
  1,067,139 native batches drawn, 590,685 cut. Content digest equal to off for 17,664 / 17,664 pictures; "drawn2" (the GE's state
  + vertices at each PRIM, following CALL/RET: 1,071,929 CALLs followed) equal to off for 17,664 / 17,664. List/frame digests
  differ in 4,687 pictures, as expected (CALL words).
- Fill at 256 KB a region (512 KB) [C twin]: rain 29,000-31,400 0 no room, 0 re-cooks; deck's first play 25,000-26,000 thrashes:
  14,897 calls no room (today's path), 4,707 re-cooks after eviction, peak 512 KB. Frame data with the switch on: deck max 535 KB,
  rain 525 KB, after 31,401 758 KB (off: 616 / 612 / 865); opening 1,114 KB (cap at 256: 1,176). A 448 KB run is under way.
- Bug found and fixed before the full runs: the converter's stream test wanted kind 2 for stream C (it is kind 4, V2-16).

## 2026-11-23 18:50 UTC: STEP 1 (ship, df0 groups) under way: code written, x86 twin building
- Coordinator: step 0 accepted; step 1 = container + emulator only, no PSP delivery; base gl39's tree (gl40 is being built by
  another agent: build_gl40, attic/gl40 not touched; heavy jobs nice -n 19).
- Scratch st1/t (t39 copy + step 1), switch MGS2_NGE (default 0; off = today's path in every place): ge/nge.h (shared refs, counts),
  boot/nge_stub.h (stub: cook on a miss, reuse check each use, WORLD at stub time, evicting arena in each frame memory's arena tail,
  default 256 KB a region, per-picture fill line "nge f"), xconv.c (native batch for a referenced df0 group; check mode MGS2_NGE_CHK
  on x86), gelist.c (WORLD when changed, BASE/CALL into the cooked list; WORLD/VTYPE/BASE memos cleared after the CALL), gelive.c
  (digests native-aware), xlive_chk.c (cooked regions mapped; "drawn2" = state + vertices at each PRIM following CALL/RET).
  Today's path kept for: dz pair batches, 780/a70/1088 entries, streams not sent, streams in packet memory, +0xD0 override.
- Old cook of a changed mesh is kept (zombie entry) until 8 pipeline calls and, on the PSP, 3 GE finishes have passed.

## 2026-11-23 18:14 UTC: STEP 0 done; results in step0_results.md, sources in cooker/
- Run 2 (nge2, refcmp 0 differ): the 74,082 df0 conversions with no walked group are all before vb 24,000 (tanker; stub not armed).
  Stub groups by entry: df0 1,926,185; 780 203,499; a70 84,971; 1088 18,996. blk (SPR 0xc54) never mesh+0xFC (a separate block,
  e.g. 0x01b2bff0); 0 compared groups lacked the stub-time transform. Cooked bytes per picture (w00a max/mean): drawn 860,536 / 198,010;
  queue pre-lit per object + lit per model 1,065,764 max; whole w00a drawn-at-least-once 2,010,260 B.
- Verdict [C twin]: every df0 ship group vb 24,000-37,800 identical (1,658,466 compared, 0 differences); exceptions stay on today's
  path: dz X arm 31,483 batches, non-df0 entries, 67 no-stream groups, 275 packet-area groups. 0x12cd78: 0 calls.
- Scratch kept for a replacement: st0/nge, st0/t, st0/bld (boot, nge_nge1/2.txt), st0/mk.sh, st0/run.sh.

## 2026-11-23 18:07 UTC: harness run 1 (st0/bld, tag nge1) done
- [C twin] refcmp vs hashref_gl38: 685 lines, 0 differ (the harness moves no state). 209 s.
- [C twin] 1,658,466 P04+df0 group conversions compared with their cooked groups (vb 24,000-37,800): 0 differences of any kind
  (positions, UVs, colours, alpha, ADC strips, the 12 WORLD floats and pa/pb, the cut, neg-UV). 0 stale; 2,032 cooks, 710 of them
  re-cooks after a mesh's pointers/descriptors changed (nge_mesh_same); 0x12cd78 called 0 times on the route.
- Not covered: 74,082 df0 conversions with no walked group (being classified in run 2), 67 groups with a stream not sent, 307,466
  non-df0 ship groups (lit 780 / a70 / 1088), 31,483 ship batches drawn by the dz X arm (MGS2_GL_DZ pairs).
- Run 2 (nge2) adds: no-group classification, blk vs mesh+0xFC, entry histogram, cooked bytes pre-lit / lit / lit per model.

## 2026-11-23 18:00 UTC: cooker written; harness twin building
- Cooker [C, compiles]: scratch st0/nge/ nge_fmt.h, nge_cook.h/.c (one mesh: pre-lit groups byte for byte as today's df0 converter's
  VcVert, vc_strips' runs, one list per group VTYPE BASE (VADDR) PRIM.. RET; lit groups in a 20 B / 24 B GE-lighting form), nge_list.h/.c
  (per-mesh WORLD from the constant block exactly as vc_xform; BASE CALL BASE per group). gcc -Wall -Wextra clean; psp-gcc (BASEF flags)
  clean, nge_cook.o text 4,944 B.
- Harness: st0/nge/nge_twin.c (MGS2_NGE=1, reads only) + hooks under -DNGE_TWIN in scratch copies st0/t (nat_stub.h: stub hook and a
  P12cd78 wrapper; nat_x86.c: the walked group; xconv.c: df0 compare + xc_nge_off; gelist.c: dz arms). Build st0/mk.sh -> st0/bld/boot
  (build19 gen.o, P1214a0 and P12cd78 weakened); run st0/run.sh <tag> (h1/tr.sh settings + MGS2_NGE=1, refcmp vs hashref_gl38).
- EVM builder [C gen_elf.h]: 0x137730 calls 0x136d18 at 0x138464 then 0x136f38 at 0x13846c; 0x136f38 writes REF tags with UNPACK
  0x6d008100 (V4-16 -> T+0x100), V4-32 / V4-8 unpacks and MSCAL 0x14000000|entry into SPR packet buffers 0x700003f0 + 384 k: the face
  mesh packet builder [I: from its words]. 
- Next: run the harness to vb 37,800; read the "nge:" lines; write step0_results.md.

## 2026-11-23 17:45 UTC: vb 25,944 / 25,974 classified (READ_LOG run); resumed after the usage-limit pause
- Runs [C twin]: scratch st0/rl.sh = native/xt/rh.sh on a copy of its binary (md5 96750356, gl28 game settings), base (rlb) and
  V3a (rlv: MGS2_LITE_ALL=0 MGS2_LITE_FN=1214a0,12e8b8), both to vb 25,976 with MGS2_READ_LOG=25944:25944,25974:25974.
  rh_stat: the read hash differs in the same 2 vblanks of the deck's first play (and the opening's 3,785), read counts equal everywhere.
- rl_lanes.py / rl_cmp.py: 599,919 reads paired in the 2 vblanks; 0 reads only one run makes; 918 reads see other bytes, ALL in the w lane,
  ALL by one reader S_d01t_P329668 (offsets +377 / +477 / +56b / +65c), ALL last written by S_d01t_P299e90 in both runs, all in the
  scratchpad 0x70000c00-0x70000fff. Base w = VIF codes of the builder's group template (0x6d808101, 0x6c408100) or floats; V3a w =
  other VIF codes / floats (0x6d408101, 0x3f800000, 0). VERDICT: LEFTOVER (H1's R5 lists d01t P329668: quadword copies, w lane).
  No later read differs (read hash equal in every other deck vblank), so nothing acts on it. Same pair and reader as gl30n's report.
- Next: the cooker sources and the twin harness (equivalence of list words), then the census.

## 2026-11-23 16:30 UTC (clock of `date -u`): STEP 0 agent started (container only)
- Brief (lead): step 0 of design §5.2: (1) cooker in C for w00a ship meshes (x86 twin + psp-gcc), (2) list-word equivalence of the
  native ship path with today's P04 batches over the route on the x86 twin, (3) open items: READ_LOG for vb 25,944 / 25,974; census of
  all w00a models and of 0x12cd78 colour rewrites; EVM builder address, (4) cooked memory of the deck vs budget.
- Outputs: this folder: step0_results.md, cooker/ (sources). Scratch: scratchpad/st0/ (container; RAM images and model data stay there).
- Read: design (all), this file, native_step0/1 reports, H1 state 16:30 (R1-R8). Nothing in /home/claude/main or t39 is changed.

## 2026-11-23 17:25 UTC: design written (native_ge_renderer_design.md); hand-back

- Design at /mnt/project-files/mgs2/native_ge_renderer_design.md (460 lines; md5 73ab66be...). Sections: 0 short answer, 1 seam
  (what keeps running, data sources, list assembly with an ME patch table), 2 per kind, 3 memory, 4 costs + checkpoint turns
  (K-A, K-N1, K-N2, K-N2R, K7) and a proposed decision rule, 5 build order with proof standard per step (strict: steps 1-2;
  E5: 3-7), 6 risks + probes P1-P5.
- Key estimates [I]: after step 2 64-72 ms a step (~7.4 fps); full deck set 54-63 (~8.6 fps); ceiling with free drawing 52-55;
  with the rain deletes and logic levers 39-54 (~11 fps). The literal checkpoint (ship + people < 50 ms) fails even if the plan
  succeeds: the lead should restate it before step 2's run.
- Open for whoever continues: classify V3a's read differences at vb 25,944 / 25,974 (READ_LOG run); step 0 census of w00a's whole
  model set and of 0x12cd78 colour rewrites; EVM builder address inside 0x137730.
- Temp files: none left in /mnt/project-files. Scratch gen/ (container) holds the tools and the RAM image (32 MB).

## 2026-11-23 17:05 UTC: V3a read-hash result; cooked sizes; lead's H1 note taken in

- V3a (stubs 0x1214a0 + 0x12e8b8, gl28 read-hash twin, route i to vb 31,400; gen/out_v3a.log vs native/xt/out_base.log) [C twin]:
  read counts equal in all 31,399 vblanks; read hash equal in the whole rain window (vb 29,300-31,399, 2,099 vb), the talk and
  "between"; differs in 2 vblanks of the deck's first play (25,944 and 25,974: not yet classified) and 3,785 of the opening's
  demos (old V3a: w-lane leftovers). IOP hash and vfb equal at all 314 checkpoints; strict ee hash differs at 272 of 314 (from
  vb 4,300 on: packet bytes no longer written). So: ship + people stubs keep every game read equal in the rain, but need their own
  hashref (strict hash of the stub build, twin = emulator = PSP), as the H1 agent found for any libdg write change.
- Cooked VB sizes from ram.29900 (gen/cooksize.py) [C dump + format model]: whole queue 1,044 KB (P04 pre-lit per object 640 KB,
  40,013 v in 37 objects, ship object 30,017 v; P04 lit per model 102 KB; P06 per model 302 KB); drawn set 343 KB.
- Today's C2 computes d per frame for blended P06 vertices from the frame's bones (ge/vu1conv.c p6_c2loop) [C code]: cooking once
  is not identical for 2,188 drawn blended vertices; the design keeps a per-frame d pass for them.
- Lead note 16:40 read (h1_notes.md "2026-11-23 16:40", R1-R8 in h1 state 16:30): H1 not built; renderer steps keep the strict
  hash where the leaf seam allows, else the E5 reader standard with a new hashref, named per step. Taken into the design.
- Next: write native_ge_renderer_design.md.

## 2026-11-23 16:40 UTC: deck census from a RAM image; profile split; V3a read-hash run started
- RAM image: copy of the H1 agent's base.29900 (twin "base", rain deck) in scratch gen/ram.29900; scripts gen/census.py, gen/drawn.py,
  gen/chains.py (read only). [C dump] channel 0 at vb 29,900 draws: P04 18 objects / 146 meshes / 587 groups / 11,303 vertices
  (pre-lit 116 meshes 10,447 v; lit 30 meshes 856 v, 284 of them 2-matrix blend); P06 2 objects (44 meshes) / 282 groups / 6,816 v
  (2,188 blended; passes 2+3 on 3,916 v); 161 materials; 15 models; no stream override (+0xD0) this vblank; no sorted mesh drawn.
  Matches the old count runs (147 calls of 0x1214a0, 44 of 0x12e8b8 a picture). Queue: 217 objects, 79 models, 58,198 vertices,
  2,783 groups in all (ship model 30,017 v); prims queued 287; EVM 1; texture pages 32.
- Chains in that (reduced) step's packet buffer [C dump]: people chain 2,924 tags / 44k REF qw; sorted prims chain 2,197 / 56k;
  stage program chain 1,332 / 60k; sea 441 / 3.7k; ship chain only page uploads (reduced step).
- Model split, gl39 rain K0 (ka39.1) and gl38 K7 (k38b.6) [C model]: K7 main 70.3 model = 67.1 PSP; io_dma_start 6.25, box cull
  vupv_96936c0f_0 4.04, qcopy 3.20, OnePieceObjs 1.68, StoreMatrixObjs 1.20, people builder 12e8b8 1.14, sea builder 13bf78 0.95,
  SortChainObjs 0.93, rt_nat_p04 (ship stub) 0.53, WritePrimPacks 0.59. Output gen/prof_ka39_1.txt.
- FIRST FINDING [I]: ship + people natively drawn take ~8-12 ms a step off the main CPU (78 -> ~64-70), NOT 28: the ship is already a
  stub + records since gl31 (its main-CPU part is gone), and K7 (nothing drawn at all) is 67.1 [C]. The <50 ms checkpoint after two
  kinds is not reachable by this estimate; design will propose a measurable bound kind instead (see design doc section 4).
- Running: V3a read-hash proof (old gl28-settings twin scratchpad/native/xt, stubs 1214a0+12e8b8 every step) to vb 31,400, output
  gen/out_v3a.log; compare with native/xt/out_base.log by native/xt/rh_stat.py.

## 2026-11-23 16:05 UTC: started
- Read: method skill, memory index + native-renderer/status/rules/working-facts/ge-hardware/renderer-work/stagec/stagec-effects/
  c2-people/deck-marks/visible-choices/h1/me-audio/builds-gl29-gl36/outlook, native_renderer_design.md (Phase 2, ME converter
  design), hd_study §1-7, b_proof_notes.md, our reference notes, where_78ms_go.md, gl39_psp_read.md, H1 agent state.md (15:30).
- Key inputs so far: gl39 rain wait play form 78.1 = logic 45.2 + pipe 27.0 + gsw 4.7 + ~3.4 [C]; nothing drawn (K7) 67.1 =
  34.4 + 25.8 + 4.0 [C]. H1 agent: the state hash covers ALL EE RAM + scratchpad, no mask: any change in what libdg writes
  (packets, chain descriptors, scratchpad) moves the hash -> each stub set needs its own hashref + a reader proof vs the old path.
Next: current GE path code (t39/ge: natp04.h, natsnap.h, xlme.c, xconv.c, gelist.c), native_step0/1 reports, re/ notes on
mesh packets and dg_objects, the psp-developer skill (GE skinning/lighting/matrix facts), then deck census on the twin.
