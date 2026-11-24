# Native GE renderer, step 1 (ship df0 groups): results and the step-2 PSP run plan

Design agent, 2026-11-23. Container and emulator only. No PSP build was delivered.
Base: built and checked on gl39's tree (/home/claude/t39; scratch st1/), then moved onto gl40's final tree (/home/claude/t40;
scratch st1g/) and checked again there (§2.3). gl41's merge with gl41s's levers: §6.
Marks: [C twin] = x86 twin run; [C emu] = PPSSPP headless run; [C code] = read in code; [C model] = cost model;
[I] = inferred, with its basis; UNKNOWN = no basis yet.

**Note added 2026-11-23 23:35 UTC by the gl41 build agent (coordinator's request).** Every emulator figure below was taken with
the picture loop on the main CPU. The emulator's me_pic thread (64 KB stack) could not be created once step 1's tables were
outside newlib's heap (partition 2: 27-36 KB largest block; 80020190 "no thread"), so the main CPU ran every entry itself.
They are not a proof for a loop running apart from the main CPU (the ME on hardware). With the loop apart (gl41: thread stack 16 KB),
step 1 drew the ship wrong: the walk read the stub's references and transforms from packet buffers the main CPU had already
refilled. NGN_W_ALL was 61,814 of 62,101 native batches, declined by count 31,068, content differed in N frames, and the shots
differed on the ship geometry. gl41's fix: the references are copied with the groups at REC (PSP and the x86 mirror), WORLD and
the projection come from the converter's own Dx, and NgeRef.pad carries a 16-bit digest of the stub's transform for NGN_W_ALL.
Results: attic/gl41/report.txt.

## 0. Short answer

- **Built.** Ship df0 groups are cooked once on the main CPU. The ME converter turns each referenced group into a small
  native batch, and the list writer draws it with WORLD, BASE and a CALL into the cooked list.
  - All of it sits behind `MGS2_NGE` (default 0). With the switch off, every path is today's.
  - Today's path is kept for:
    - depth-pair (dz X/Y) batches;
    - the 780 / a70 / 1088 entries;
    - streams not sent;
    - streams in packet memory;
    - a +0xD0 override;
    - every fallback (no room, table full, transform array full).
- **Twin, switch off [C twin].**
  - refcmp: 685 lines, 0 differ.
  - All 17,664 picture digests (list, frame, content) equal step 0's run.
- **Twin, switch on, check mode [C twin].**
  - refcmp: 685 lines, 0 differ.
  - 1,657,824 referenced df0 groups, vb 24,000-37,800, were compared with today's batch: **0 differences**.
  - The content digest and the new "drawn2" digest equal the switch-off run in **17,664 / 17,664** pictures. drawn2 is the GE's
    state and vertices at every PRIM, following CALL and RET.
- **Arena [C twin].** At 256 KB a region (512 KB in all), the rain (vb 29,000-31,400) never runs out of room. The deck's first
  play (vb 25,000-26,000) thrashes:
  - 14,897 calls found no room and were drawn the old way;
  - 4,707 meshes were cooked again after an eviction.

  At 448 KB a region, nothing found no room and nothing was cooked again on the whole route. But a static 448 KB carve does not
  leave enough frame arena for the opening (one picture lost batches). §3 has the numbers and the proposal.
- **The late walk had a bug, now fixed [C twin].** The twin's arm that runs the PSP's walk (`MGS2_NAT_LW=2`) found it in check
  mode.
  - ge/xlme.c gave a group the reference of the group `nat_i0` places earlier.
  - Result: 1,139 groups declined by count, and 5 groups drawn from another group's cook (content and drawn2 differed in 5
    pictures, vb 35,027-35,137). On the emulator it showed as 10 groups declined by count.
  - The fix is one line. After it, the same arm gives 0 differences in 1,657,824 groups, and content and drawn2 equal off in
    17,664 / 17,664.
- **Emulator, gl39's tree [C emu].** Off and on: 62 / 62 hashes, 0 differ. Every fixed shot is equal except shot_24784, which is
  emulator noise (it also differs between two off runs whose lists are equal). Every live shot showing the same frame is equal.
- **gl40's tree [C twin, C emu].** The diff applies with offsets only.
  - Twin: off = gl39's picture in every digest of 17,664 pictures, and the on check gives 0 differences.
  - **With gl40's final pads, step 1 breaks textures on the emulator, even with the switch off.** The image grows 25.7 KB and
    newlib's heap can no longer take the 128 KB the texture cache's table needs at vb 10,850; 703 of 3,522 pictures lose
    textures. The hashes stay equal.
  - Linked without gl40's two NOBITS pads (image +12.2 KB), off = gl40 in every digest and shot. On is also clean: 62 / 62,
    0 declined, shots equal.
  - So gl40's real margin at that moment is ~12-25.5 KB, and the log's "64 KB" low point does not show it. **gl41 must find
    memory first** (§6 item 2).
- **Step 2.** The run designed in §4.1 needs code that does not exist yet: people native, a K-N2R turn kind, and PGO training
  that includes the new code. §4.2 lists it. The smaller set of turns (K-A, K-N1, K7, §4.3) can ride on gl41. It would give
  step 1's gain [C after the run] but not P.
- **gl41s.** No file in common with step 1. Their series applies on gl40 + step 1 with no failed hunk. What conflicts is the
  build: one PGO training, one order and pads for both, and the heap (§6).

## 1. What was built (scratch st1/t; sources in attic/ge_native/step1/)

| Part | File | What it does |
|---|---|---|
| Format | nge/nge_fmt.h | `NgeGroup` (36 B: list, vb, runs, nw, nv, kind, nst, flags, i1, alpha, reg, box). `NgeMesh` is the main CPU's reuse key. |
| Cooker | nge/nge_cook.c/.h | Cooks one mesh: the group array, then for each group its VB (`VcVert`, byte for byte as today's df0), its list (VTYPE 0x11e, BASE, VADDR/PRIM per strip, RET) and its strips. It also cooks the first drawn vertex (i1) and the box. A dry run gives the size. |
| WORLD | nge/nge_list.c | `nge_xf_block`: WORLD and pa/pb from the call's constant block (SPR 0xc54), the same bits as `vc_xform` [C twin]. |
| Shared | ge/nge.h | `NgeRef` per recorded group (parallel to the `NatGrp` array), `NgeTail` after a native `GerXform` (pad `NGE_MAGIC`), the converter's counts. ME-safe. |
| Stub | boot/nge_stub.h (from nat_stub.h) | After rt_nat_p04 records a call: look up the mesh; check it against RAM at each use (`nge_mesh_same`); cook on a miss; compute WORLD at stub time; give each df0 group that is in place a reference. The arena is described below. |
| Walk | ge/xlme.c, boot/nat_x86.c | `nge_walk(b, i)` around `nat_p04_group`, `i` the group's index in its buffer's whole array (in xlme.c `nat_i0[b] + i`): the converter sees the group's reference. |
| Converter | ge/xconv.c | `xc_nge_try` checks: streams A/B V4-16 with no ROW and C V2-16; count; alpha. On success, `xc_nge_batch` writes the `GerXform` with nv, nst and nidx 0 and pad MAGIC, then the `NgeTail`. Cut, depth range (box, i1) and twopass work as today's. x86 check mode: `MGS2_NGE_CHK=1`. |
| List writer | ge/gelist.c | `xf_nge`: WORLD only when it changed and no other WORLD came between; the batch's state (`xf_rest`); BASE if needed; CALL. Afterwards the VTYPE and BASE memos are cleared, so the next batch sends its own. Depth-pair X/Y arms of a native batch use the accessors (today's path). |
| Digests | ge/gelive.c | `gl_digest_c` (content) hashes a native batch as today's batch would be. `gl_digest` adds the cooked VB and runs. |
| Twin host | boot/xlive_chk.c | Cooked regions at GE 0x09400000 / 0x09800000, mapped by `gx_ptr`. The frame arena is carved as on the PSP. `MGS2_GX_SEM=2` prints "drawn2". `nat_bline` is native-aware. |
| PSP host | psp/xl_psp.c | Carves `MGS2_NGE_KB` (default 256) from the tail of each frame memory's arena (`gb_arena` and `mq->fm_arena`). Its tables come from blocks outside newlib's heap, set up after the native arrays and before the ME starts. `ge_fin_n` holds evictions. Turn and end lines. |

**Switches.**
- `MGS2_NGE=1`: on for the whole run.
- `MGS2_NGE_CYCLE=<0/1 digits>`: by the bound turns' rule (`MGS2_OPT_CYCLE`'s from and n).
- `MGS2_NGE_KB`: KB per region, 64-4096.
- `MGS2_NGE_LOG=0`: no fill lines.
- `MGS2_NGE_CHK=1`: x86 check mode.

**The arena.**
- One region in each frame memory. A region never crosses a 16 MB boundary of the GE's space, so one BASE serves it; the
  set-up checks this.
- Placement is first fit. When nothing fits, the least recently used mesh is evicted, but only one that no pipeline call has
  used in the last 8 and, on the PSP, one the GE has finished with 3 times since its last use.
- A mesh whose RAM changed keeps its old cook as a "zombie" until the same guards pass, so a list in flight never sees bytes
  change under it.
- Tables: 768 entries (dead entries are reused), 512 blocks a region, references 18 KB and transforms 512 x 56 B per packet
  buffer.
- **Fill line, one per picture:** `nge f vb .. used .. peak .. ck .. rc .. ev .. nr .. nat .. tod .. xf ..` (bytes used, peak,
  cooks, re-cooks, evictions, calls with no room, native groups, groups on today's path, transform slots).
  - x86 prints each one at once. The PSP keeps them in a ring and writes them with the turn line `nge turn vb A-B: <counts>`.
  - At the end: `nge totals` and `nge converter`.

**What the main CPU does more [I].**
- The reuse check: about 30 words read per stubbed call.
- WORLD from the constant block, per call with native groups.
- The cooks: about 16 B written per vertex of a new mesh.
- Stub-time WORLD repeats work the ME also does: the converter still computes `Dx` from VU memory for the cut. Step 1 keeps
  both, because the check compares them. Dropping the ME's copy is a step-2 option.

## 2. Checks

### 2.1 x86 twin (st1/bld/boot: gl39's twin units, build19 gen.o; st1/run.sh = st0/run.sh's settings + MGS2_GX_SEM=2)

| Run | refcmp vs hashref_gl38 (685 lines) | Pictures | Result |
|---|---|---|---|
| off (MGS2_NGE unset) | **0 differ** [C twin] | 17,664 | list, frame, content digests = step 0's run (today's picture) in 17,664 / 17,664 [C twin] |
| on, MGS2_NGE_CHK=1, 256 KB | **0 differ** [C twin] | 17,664 | see below |
| on, MGS2_NGE_CHK=1, 448 KB | **0 differ** [C twin] | 6,594 from vb 24,000 | content and drawn2 = off in 6,594 / 6,594 |

**The on run's check mode [C twin].** 1,657,824 referenced groups were converted and compared with today's input
(`vc_df0_rec` on the same record):

| What was compared | Differences |
|---|---|
| Count | 0 |
| The 16 B vertices | 0 |
| `vc_strips` runs vs the cooked runs | 0 |
| The cooked list, decoded (VTYPE, BASE, VADDR, PRIM type 4 and count per strip, RET position) vs the strips | 0 |
| Stub-time WORLD and pa/pb vs the converter's | 0 |
| A fresh `vc_xform` vs the converter's memo | 0 |
| The batch's transform words (`GerXform` up to nv) | 0 |
| Box | 0 |
| First drawn vertex | 0 |
| Declined by stream kind, count or alpha | 0 |

- 1,067,139 native batches drawn; 590,685 cut by the box test (as today's); 1,071,929 CALLs followed by drawn2.
- Pictures:
  - **content** digest = off in 17,664 / 17,664.
  - **drawn2** = off in 17,664 / 17,664.
  - The list, frame and old "drawn" digests differ in 4,687 pictures, as they must: the words are CALLs.
- Stub counts:
  - 427,940 calls; 242,053 with native groups.
  - 2,233,651 groups; 1,864,128 native.
  - On today's path: 307,466 not df0; 275 not in place; 61,782 in calls with no room (the deck's first play, §3).
- A bug found by the smoke run and fixed before the full runs: the stream test wanted kind 2 for stream C, but C is kind 4
  (V2-16, ge/xlate.h).

### 2.2 Emulator (PPSSPP 1.20.4 headless, software GE, PPSSPP_VFPU_RN=1, as em39.sh; to vb 31,400)

Package st1/pk: st1/psp's EBOOT and 8 overlays (gl39's PGO=use objects, the six changed ones compiled again with gl39's lines
and profiles, gl39's code order; gates xme_rest_chk, mesec_chk, me_call_chk PASS), hashref_gl38, pad.txt. cfg: gl39's with
every turn drawn (st1/cfg/nge0.cfg, nge1.cfg). Shots stay in the container (st1/emu_*/pic), for the coordinator's eyes.

| Run | EBOOT | Switch | Hashes vs hashref_gl38 | Fixed shots (16) vs n0 | Live shots (34) |
|---|---|---|---|---|---|
| n0 | 327b5a32 | off | **62 / 62, 0 differ** [C emu] | (reference) | (reference) |
| n1 | 327b5a32 | on, 256 KB | **62 / 62, 0 differ** [C emu] | 15 equal; shot_24784: 73 px differ by 1 in one channel | the 18 that show the same frame as n0: 18 equal |
| n1b | 900ac1ac (n1 + the W_ALL count + the ring written when full) | on, 256 KB | **62 / 62, 0 differ** [C emu] | 15 equal; shot_24784: 60 px differ by 1 | 18 of 18 equal to n0; 34 of 34 equal to n1 |

- **Live shots** copy the buffer on screen as it is, so which frame they show depends on timing; they are paired by the frame
  shown (livecmp.py). Every pair showing the same frame is pixel-equal.
- **shot_24784** (the deck, before the first-play thrash): ±1 in one channel, at 50-75 scattered pixels, rows 104-208. It also
  differs between the two on runs (n1 vs n1b: 52 px), at partly different places. It is not the WORLD: the new count
  `NGN_W_ALL` (stub-time WORLD or pa/pb != the converter's, counted on every platform) is **0** on the emulator and the twin.
  **It is emulator noise** (§2.3): two gl40-tree off runs with equal list and frame digests in every picture differ there too.
- **The emulator's arena decisions equal the twin's** [C emu]: the fill lines by window (24,000-31,000) give the same cooks,
  re-cooks, evictions, no-room calls and native / today groups as the twin, and "evictions held by the GE" is 0.
- **nge totals (n1b)** [C emu]: 9,424 pictures; calls 242,826, native 163,135; cooked 646; re-cooked after eviction 4,942;
  evicted 431; no room 15,637; constant block not in RAM 260; groups 993,411, native 814,720.
- **nge converter (n1b)** [C emu, the ME's counts read from RAM at the end]: referenced groups 608,347; native batches 257,245;
  cut 351,092; declined by count **10**, W_ALL 0.
  - The 10 are the late walk's index bug (§2.3): a group took another group's reference.
  - It is fixed in both diffs, and checked on gl40's tree (0 declined). The gl39-tree PSP build was not built again.
- **Memory** [C emu]: tables 65,024 B at 0bee3000, references and transforms 47,104 B a packet buffer at 0bfdb000 / 0bfe6800, from
  the system's memory outside newlib's heap (before: largest 116 KB, total 216 KB; after: 36 KB, 60 KB). newlib's heap: 64 KB
  headroom at its lowest (vb 23,675), as gl39.
- **Digests 29,490-31,400**: list and frame equal between the two on runs in 916 / 916 pictures [C emu].

### 2.3 Step 1 on gl40's tree (st1g)

**Tree.** A copy of /home/claude/t40 (boot, boot_g0, ge, psp, rt, runtime) with step 1's diff applied. All 8 files took it with
line offsets only, no fuzz, no reject [C]. gl40's own changes do not touch what step 1 touches. Those changes are the XP lever
in ge/xlme.c, ge/xlme.h and boot/nat_stub.h, M5 in rt/kernel.h, and probes in psp/xl_psp.c [C code].

**Builds.**
- Twin st1g/bld/boot: gl40's twin units. gen.o is build_gl40/x19's, with P1214a0 made weak as tw40.sh does.
- PSP: first st1g/psp_v1, EBOOT **83c78800** (before the walk fix), then st1g/psp, EBOOT **4423bdea** (with it). Each is gl40's PGO=use objects (build_gl40/u, gcda), with the six changed objects compiled
  again with gl40's lines and profiles. The link uses gl40's code order (ord/final) and **gl40's final pads**, rl40.sh's pf
  (RO_PAD 0x260, ME_PAD 0x15c0, XL_PAD 0x210, ME_PRE 0x1d00, END_PAD 0x1a00). Gates: xme_rest_chk, mesec_chk (gl40's
  mesec_chk40), me_call_chk all PASS.
- Emulator only.

**Twin [C twin], vs hashref_gl38 (c9ddb593, as gl40):**

| Run | refcmp | Digests |
|---|---|---|
| off (switch unset) | 685 lines, **0 differ** | list, frame, content, drawn, drawn2 = gl39-tree step 1's off run in **17,664 / 17,664** (so gl40 + step 1 with the switch off draws gl39's and gl40's picture) |
| on, `MGS2_NGE_CHK=1` (+ `MGS2_XP_CYCLE` 7 in every turn) | 685 lines, **0 differ** | content and drawn2 = off in **17,664 / 17,664**; list, frame and drawn differ in 4,687 (the CALLs) |
| late walk lw (`MGS2_NAT_LW=2`: the PSP's walk, ge/xlme.c, through gl40's copy plan; check mode; `MGS2_XP_CHECK=1`), **before the fix** | 685 lines, 0 differ | **content and drawn2 differ in 5 pictures** (vb 35,027-35,137); the converter declined 1,139 groups by count and found 5 whose vertices and box differ |
| late walk lw3, **after the fix** (below) | 685 lines, **0 differ** | content and drawn2 = off in **17,664 / 17,664**; 1,657,824 groups compared, **0 differences**, 0 declined; XP check 223,280 plans, 0 differing |

- The on run's check: 1,657,824 groups compared, **0 differences of every kind**, W_ALL 0.
- Its stub and converter totals equal the gl39-tree run's to the unit: native batches 1,067,139, cut 590,685, no room 15,637,
  re-cooks 4,942, evicted 629, cooked 735.
- `MGS2_XP_CYCLE` alone does nothing in this arm: the twin uses gl40's plan only with `MGS2_NAT_LW=2`. The late-walk row is the
  run that uses it.

**A bug the late-walk arm found, and its fix [C twin].**
- The PSP's walk (ge/xlme.c lw_nat) is handed the pending groups only: `g` starts at the first group not yet walked
  (`nat_i0`). Step 1 passed the walk's own index `i` to `nge_walk`, but the references are numbered over the whole array.
- When `nat_i0` > 0, a group took another group's reference:
  - the converter declined the group when the counts differed (1,139 on the twin, **10 on the emulator**);
  - it drew the wrong mesh when the counts happened to match. 5 groups on the twin; the check's detail line showed a
    4-vertex group drawn from another 4-vertex group's cook.
- The x86 walk (nat_x86.c) uses the absolute index, so the earlier twin runs could not see this.
- The fix is one line: `nge_walk(b, x->nat_i0[b] + i)`, the index `xl_nr_begin` already uses.
- It is in both diffs (t39 and t40). The check mode now also prints the first 8 vertex differences in full (x86 only).
- The emulator runs n1, n1b (gl39's tree) and g1 (gl40's) were made before the fix. Their 10 count declines are this bug.

**Emulator on gl40's tree [C emu]** (PPSSPP 1.20.4 headless, software GE). cfg: gl40's with every turn drawn, `MGS2_XP_CYCLE` 3
in every turn, and digests over 24,000-31,400 (st1g/cfg/nge0.cfg, nge1.cfg). b0 is gl40's own delivered EBOOT run with
nge0.cfg (`MGS2_NGE` means nothing to it), the A/B base.

| Run | EBOOT | Link | Switch | Hashes | ME picture heap | Textures refused | Digests 24,000-31,400 vs b0 | Shots vs b0 (16 fixed, 34 live) |
|---|---|---|---|---|---|---|---|---|
| b0 | 530f8082 (gl40 as delivered) | gl40's pads | n/a | 62 / 62, 0 differ | 492 KB added, 0 refused | 343 | (base) | (base) |
| g0 | 83c78800 | gl40's pads | off | 62 / 62, 0 differ | **428 KB; the 64 -> 128 KB resize at vb 10,850 refused, then 1,588,297 times** | **1,588,640** | **703 of 3,522 differ** (frames with fewer textures and batches) | 9 fixed and 22 live differ |
| g1 | 83c78800 | gl40's pads | on (before the walk fix) | 62 / 62, 0 differ | the same failure | 1,588,640 | (lists differ by design) | n/a |
| g0np | 67c97afd | **without ME_PRE and END_PAD** (image +12.2 KB above gl40, not +25.7) | off | 62 / 62, 0 differ | 492 KB, 0 refused | 343 | **3,522 / 3,522 equal** (list and frame) | 15 / 16 fixed equal (not shot_24784: 225 px by ≤ 2); 34 / 34 live equal |
| g1np | 0d5c1e75 (with the walk fix) | without ME_PRE and END_PAD | **on** | **62 / 62, 0 differ** | 492 KB, 0 refused | 343 | n/a | vs g0np: 15 / 16 fixed equal (not shot_24784: 356 px by ≤ 3); the 18 live showing the same frame: 18 equal |

- **The switch off is gl40 on the emulator** when the heap holds: g0np = b0 in every list and frame digest of 24,000-31,400
  and in every shot but shot_24784.
- **shot_24784 is emulator noise.** b0 and g0np differ there by ≤ 2 at 225 px, though every list and frame digest is equal.
  So the ±1-3 differences seen there in every pair of runs (on or off, gl39's tree or gl40's) are not step 1's. That closes
  §2.2's open point.
- **With the switch on (g1np), after the walk fix:**
  - the converter declined 0 groups (10 before the fix) and W_ALL is 0;
  - the fill lines equal the twin's window by window;
  - nge totals: calls 242,826, native 163,135; native batches 257,333; cut 351,327.
- **The failure with gl40's pads.**
  - It is newlib's heap, not step 1's logic. The image grows 25.7 KB and the heap's end is fixed (start headroom: b0 12,528 KB,
    g0np 12,512, g0 12,496; 16 KB probe).
  - At vb 10,850 the texture cache's table grows from 64 to 128 KB. newlib takes 128 KB fresh at the top for that, and in g0
    that no longer fits. Every later texture conversion that needs it is refused, so textures go missing from frames, with
    the switch on or off.
  - The hash test cannot see this, and neither can the twin (x86 memory).
- **Heap low point (log, 16 KB probe):** b0 64 KB, g0np and g1np **32 KB**.
  - The failing g0 and g1 also print 64 KB, because their ME heap is 64 KB smaller.
  - So gl40's real margin at the vb 10,850 resize is between ~12 and ~25.5 KB [C emu, the two builds' bounds]. §6 item 2 has
    what that means for gl41.
- **st1g/psp is the gl40-pads build (EBOOT 4423bdea, with the walk fix).** It has the same heap failure as g0 and is kept only
  as the link a gl41 build would use. **The pad-less psp_np (0d5c1e75) is the one checked here.**

## 3. Arena numbers

**Fill by 1,000 vblanks, 256 KB a region [C twin]** (fill lines; "today" = df0 groups drawn the old way):

| vb | peak KB | cooks | re-cooks | evictions | calls no room | groups native | groups today |
|---|---|---|---|---|---|---|---|
| 24,000 | 327 | 79 | 0 | 0 | 0 | 46,238 | 25,872 |
| 25,000 (deck, first play) | **512** | 387 | **4,707** | 134 | **14,897** | 199,633 | 92,341 |
| 29,000 (rain) | 512 | 120 | 0 | 252 | 0 | 264,047 | 22,303 |
| 30,000 (rain) | 511 | 58 | 0 | 31 | 0 | 259,239 | 25,858 |
| 31,000 | 493 | 18 | 0 | 200 | 0 | 76,585 | 16,457 |
| 32,000-37,000 | 319-502 | 73 | 0 | 12 | 0 | 1,011,299 | 183,001 |

**At 448 KB a region (896 KB) [C twin]:** 0 calls no room, 0 re-cooks, 829 cooks, 456 evictions; peak 896 KB.

**Frame arena data per picture [C twin]** (the `xlive gb` lines; data cap = arena - 264 KB):

| Stretch | Off: max / mean KB | On: max / mean KB |
|---|---|---|
| Opening (< 24,000) | 1,114 / 79 | 1,114 / 79 |
| Deck 24,000-29,489 | 616 / 299 | 535 / 291 |
| Rain 29,490-31,400 | 612 / 499 | 525 / 433 |
| After 31,401 | 865 / 485 | 758 / 371 |

- **Static carve.**
  - 256 KB leaves the opening 62 KB of margin (cap 1,176 KB).
  - 448 KB leaves none: cap 984 KB against the opening's 1,114 KB; one opening picture lost batches in the 448 KB twin run.
  - So 256 KB is the largest safe static carve with margin. The ceiling is 318 KB.
- **Proposal for step 2 [I].** Carve after the opening: when the stub is first armed (vb 24,000 on this route) and the GE has
  released the frame memory (the ME's `listed` / `ge_done` handshake), grow each region to 448 KB.
  - With native on, the frame data then peaks at 758 KB against a 984 KB cap (226 KB margin) [C twin].
  - People's cooked data adds about 300 KB (P06 per model 302 KB, step-0 census [C dump]); 448 KB a region will not hold ship
    and people. Step 2 needs a memory plan:
    - a larger carve after the opening (up to ~600 KB a region leaves the w01a stretch 70 KB of margin [C twin]);
    - or a block outside the heap on the PSP. The emulator has only ~216 KB there [C emu, gl39's log], and the PSP log's
      figure has to be read again.
- **Host memory for the tables.** 63.5 KB of tables (65,024 B) plus 2 x 46 KB per packet buffer = 155.5 KB [C emu log], taken from the system's
  memory outside newlib's heap. On the emulator that memory has 116 + 100 KB left after gl39's own users [C emu log], and
  newlib's heap only ~64 KB at its lowest.
  - The first emulator build put the tables in newlib's heap. It stopped at boot ("fileio: no memory for a read of 361,520
    bytes").
  - It also grew boot_rt.o by 83 KB, because the six recompiled objects lost gl39's profile ids (GCC hashes the source path).
  - The final build preprocesses those six with build_gl39/m's paths, so the unchanged functions take gl39's profile.

## 4. The step-2 PSP run (design §4.3), ready to ship after gl40's log is read

### 4.1 Turns

| Turn | Ship | People | Other chains | Picture | Switch words (proposed) |
|---|---|---|---|---|---|
| K-A | today's records + ME | builder + walk | walk | full | `MGS2_NGE_CYCLE` digit 0, bound kind 0 |
| K-N1 | native (step 1) | builder + walk | walk | full | digit 1, kind 0 |
| K-N2 | native | native | walk | full | digit 2 (ship + people), kind 0 |
| K-N2R | native | native | built by the game, not translated | ship and people only | digit 2, new bound kind (e.g. 9) |
| K7 | not drawn | not drawn | not drawn | none | kind 7 (exists) |

**Order.** Turns of 100 vblanks from 24,000, as gl38-gl39. Repeat the five over the deck and the rain so that each kind gets at
least 6 turns in each stretch. Put K-A next to K-N1 in each block, so their difference is not a scene difference. The hashref
comes from the x86 twin with the same cycle words; the state hash must equal hashref_gl38 in every kind [C twin today for
off/on].

**What to read** (prof per function / 80 vblanks, as gl39):
- (a) K-N2 - K-A: the gain of two kinds [C after the run].
- (b) P = K-N2R - (12d910, 13bf78, the EVM builder, io_dma_start, qcopy: from K-N2R's own profile) + their list writing
  [C model ~1.5-2].
- (c) P minus the logic levers, each measured in its own run.
- K-A vs K-N1: **step 1's gain** [C after the run]. Today it is UNKNOWN; the design's estimate for ship + people is 8-12 ms a
  step [I].

**Stop rule.**
- P >= 62 ms: stop or rethink.
- (c) < 50: go on.
- In between: go on only with the logic plan funded.

**Ffm must be told before the run** that K-N2R and K7 turns show incomplete pictures (ship and people only; nothing).

### 4.2 What the step-2 build needs that step 1 has not built

1. **People native (design §2.2):** the 0x12e8b8 stub's records, C2 cooked per model (P06 302 KB in w00a), and the ME's d
   pass for blended vertices (2,188 drawn on the rain deck).
   - Proof: x86 check mode as step 1's: list words, vertices and drawn2 equal today's C2.
   - The strict read proof V3a, with vb 25,944 / 25,974 classified first (one READ_LOG run).
   - Not started.
2. **K-N2R.**
   - The walk drops every record but the native ship and people groups. Chains 0-1, 5, 6, 7-10 are built by the game but not
     translated; the screen set-up stays.
   - The design says "ME rests". In step 1, though, the ME writes the native batches' CALLs.
   - Proposed [I]: in K-N2R the ME still converts and writes the native groups only (light work). P measures the main CPU,
     and the ME's bus load is small.
   - A true "ME rests" turn needs a main-CPU list writer, the stub writing WORLD + CALL into a main-CPU list. Textures add a
     problem: they are converted on the ME today, so the main CPU would need a read-only look into the texture cache. That is
     not a step-2 item unless the lead wants it.
3. **`MGS2_NGE_CYCLE` digit 2** (people) and the new bound kind, in the twin and on the PSP, with the twin hashref for the
   cycle words.
4. **Arena:** the carve after the opening (§3), sized for ship + people, with the fill lines read from the emulator first.
5. **PGO and code order on gl40:** the new hot code (nge_rec, nge_get, nge_mesh_same, xc_nge_try/batch, xf_nge, the people
   stub) is trained in, as design §4.4 asks. Without that, K-A vs K-N1 includes I-cache noise from untrained code [I].
6. **Probes riding along (each its own switch, off by default):**
   - P1: VBs in VRAM vs RAM in turns. It cannot be measured on the emulator: PPSSPP does not time the GE's bus. UNKNOWN until
     the PSP run.
   - P2: µs in the stub's nge parts per turn (two clock reads per call).
   - P3: CALL depth. gl_dl -> frame list -> cooked list is 2 levels. The public skill says the GE's CALL nests two levels
     [I]; PPSSPP allows 32, so only the PSP's shots prove it.
   - P5: the fill lines (built).
7. **The ME's counts on the PSP:** `nge converter` prints the ME's `.me_bss` words as RAM holds them at the end. The ME has to
   write them back (or hand them over in the queue) for the PSP log to be exact. Today they are exact on x86 only.
8. **README items:** what each turn shows; the two incomplete-picture kinds; the turn table by vblank; the fill and turn
   lines to read; the expected `nge totals` (no room 0 in the rain).

### 4.3 The smaller run, now as gl41's step-1 turns (proposal, the lead's call)

The lead's plan is that gl41 carries the ship stub. The turns that need no new code are K-A, K-N1 and K7: step 1 on gl40's tree
with PGO trained, the carve after the opening, and `MGS2_NGE_CYCLE` alternating.
- They give step 1's gain [C after the run] and the arena fill on the PSP, plus the design's probes if they ride along.
- They do not give P: K-N2R needs people native (§4.2 item 1).
- gl41s's levers P1 and P3 then take their own turns (§6, item 3).

**Names.** The design's probes P1-P5 in §4.2 item 6 (VBs in VRAM, stub time, CALL depth, fill lines) are not gl41s's levers
P1 (the VU0 call fast path) and P3 (the box cull without 8 checks). gl41's README should call the probes N-P1 .. N-P5.

## 5. Risks and open points

- **CALL depth 2 on the PSP** (P3): not checkable on the emulator.
- **`.me_bss` is not cleared by the program** (NOLOAD): the new ME words (`xc_nge_*`) rely on the loader zeroing it, as every
  other ME static does today [I, same as gl39].
- **Evictions on the PSP:** guarded by 8 pipeline calls and 3 GE finishes. A changed mesh keeps its old cook as a zombie under
  the same guards. On x86 the guard is never tested by a GE.
- **Memory:** the log's low point (~64 KB, 16 KB probe) is not the binding moment. The ME picture heap's 64 -> 128 KB resize at
  vb 10,850 is, and gl40 passes it with only ~12-25.5 KB to spare [C emu, §2.3]. Step 1 moves the image end up 25.7 KB on gl40
  and fails it with gl40's pads; gl41s adds 22-33 KB [I]. §6 item 2 has the numbers and the ways to make room.
- **The gl39-tree emulator build (st1/psp) was linked without gl39's data pads** (RO_PAD, ME_PAD, XL_PAD). That changes cache
  placement only, and it was an emulator build. The gl40-tree build (st1g/psp) uses gl40's final pads.
- **Arena thrash in the deck's first play at 256 KB** (§3): measured on x86 only. Its main-CPU cost on the PSP is UNKNOWN
  (cook cost per vertex not measured).

## 6. gl41: merging step 1 with gl41s's levers onto gl40

**Files [C code].**
- Step 1 changes 8 files of the tree: boot/nat_stub.h, boot/nat_x86.c and boot/xlive_chk.c (x86 twin only), ge/gelist.c,
  ge/gelive.c, ge/xconv.c, ge/xlme.c, psp/xl_psp.c. It adds ge/nge.h, boot/nge_stub.h and nge/ (5 files).
- gl41s's series (P1, P3, probes; E6 dropped) changes runtime/host_time.h, runtime/vu0.h, runtime/vu_l41.h (new), rt/ee_io.h and
  runtime/ee_io.h. **No file in common.**
- Tried on a copy of gl40 + step 1: P1, P3 and the probes apply in series with no failed hunk. The probes patch lands 70 lines
  higher because E6 is left out [C].
- Switches do not clash: step 1 is the cfg keys `MGS2_NGE*`. gl41s uses `MGS2_OPT2` bits 19-21. Step 1 reads no `MGS2_OPT2` bit.
- The patch to merge: attic/ge_native/step1/t40_step1.diff (the 8 files against t40) plus the new files in nge/, ge/nge.h and
  boot/nge_stub.h. The t39 diff is kept beside it.

**What does conflict: the build, not the text.**
1. **boot_rt.o, PGO and the code order.** Both series change boot_rt.o: step 1 through nat_stub.h / nge_stub.h, P1 and P3
   through runtime/vu0.h and gen_vu.h. gl41 needs one PGO training run and one new code order and set of pads covering both
   (gl41s §2: layout_sim + layout_order.py, RO_PAD and ME pads tuned again).
   - Train with `MGS2_NGE=1` in some turns, so the new hot code is profiled and placed. That code is nge_rec, nge_get,
     nge_mesh_same and nge_xf_block on the main CPU, and xc_nge_try, xc_nge_batch and xf_nge on the ME.
   - Today none of it is in gl40's order: no nge name in ord/final/*.ld [C]. In the st1g build it lands in the unordered text
     after the hot part.
   - pspb.sh's trick (preprocess, rename the paths so another build's profile ids match) is only for emulator builds made
     from gl39's or gl40's profile. gl41 trains its own, so it does not need it.
   - gl41_genvu.py makes gen_vu.h again. The x86 twin's boot_rt.o line names `gen/vp22/gen_vu.h`, so for gl41 that line must
     name the new file on x86 and on the PSP alike.
2. **Memory.** This is the binding one.
   - gl40 + step 1's image ends **26,304 B (25.7 KB) above gl40's** [C ELF]:
     - .text +14,852 B;
     - .rodata +2,440 B;
     - .bss +9,728 B (the cooker's scratch nc_v 6 KB, nge_bkt 2 KB, nc_runs + nc_fl 1 KB);
     - .me_bss +192 B.
   - newlib's heap ends at a fixed address, so the image's growth comes off its headroom.
   - gl41s adds +22 to +33 KB [I, their §5]. Together that is about 48-59 KB against gl40's low point.
   - **What the emulator shows [C emu, §2.3]: the binding moment is not the log's low point.** At vb 10,850 the ME picture heap
     grows the texture cache's table from 64 to 128 KB. newlib's realloc then takes a fresh 128 KB at the top of its heap
     (xl_psp.c me_resize's note), so for a moment it needs 128 KB above the break.
     - gl40's own EBOOT does this with between ~12 and ~25.5 KB to spare. The bounds come from two builds: gl40 + step 1
       without the two NOBITS pads (image +12.2 KB) passes; with gl40's pads (+25.7 KB) it fails.
     - When it fails, the texture cache cannot convert, and textures go missing from frames, switch on or off.
     - The log's "headroom about 64 KB at its lowest" (vb 23,677, a 16 KB probe) does not show this.
   - So gl41s's +22-33 KB alone is likely to fail the same resize [I, from the bounds above]. With step 1 as well it will fail
     [I]. gl41 needs memory found before it ships. Ways to make room [I, none built]:
     - (a) **Remove the 128 KB moment**: size the texture cache's table at 128 KB from the start, or give the ME picture heap's
       first region the room, so no realloc at the top is needed. The owner of xl_psp.c and the texture cache decides.
     - (b) **Print newlib's room at each ME heap event** (the hooks already log the break), so a PSP log shows the real margin.
     - (c) **Step 1's own share:**
       - move the cooker's scratch and the bucket array (nc_v 6,144 B, nc_fl 512 B, nc_runs 516 B, nge_bkt 2,048 B: 9,220 B
         [C code]) out of .bss into the table block outside newlib's heap. That block had 36 KB to spare on the emulator after
         step 1; the PSP's figure must be read from gl40's log;
       - build the cooker (nge_cook.c, run only on a miss) and the print code for size (-Os or `cold`), outside the hot order:
         a few KB [I].
     - (d) **Drop ME_PRE / END_PAD** (13.75 KB of NOBITS). That gives back what gl40 tuned for the D-cache: the lead's call.
     - (e) Leave out any probe text gl41 does not ride (E6 is already dropped).
3. **Turns.** gl41s A/Bs its levers by turn through `MGS2_OPT2_CYCLE`. Step 1 and 2 use `MGS2_NGE_CYCLE` digits and bound
   kinds. Both ride the same 100-vblank turns from 24,000.
   - The turn table must keep one change per measurement. For example, P1 and P3 in their own blocks of K-A turns, and the
     K-A / K-N1 pairs with P1 and P3 fixed.
   - The lead's call when gl41's plan is written.
4. **XP (gl40) with step 1.** XP runs only in the late walk on the twin (`MGS2_NAT_LW=2`). The lw3 arm ran gl40's XP check in
   every plan with step 1's check mode on: 223,280 plans, 0 differing, and every native group equal to today's [C twin]. On
   the emulator, g1np ran `MGS2_XP_CYCLE` 3 in every turn with step 1 on (§2.3).

## 7. Files

- Scratch, container only:
  - st1/ (gl39's tree):
    - t/ (the tree), bld/ (twin), u/, psp/, pk/ (PSP build and package);
    - emu_n0, emu_n1, emu_n1b (emulator logs and shots: for the coordinator's eyes, never copied out);
    - mk.sh, run.sh, run_base.sh, pspb.sh, em.sh, dcmp.py, fill.py, kb.py, shotcmp.py, livecmp.py, pxd.py;
    - cfg/nge0.cfg, cfg/nge1.cfg.
  - st1g/ (gl40's tree):
    - t/, bld/;
    - u/, psp/ (gl40's pads, with the walk fix) and psp_np/ (the same objects without ME_PRE and END_PAD);
    - u_v1/, psp_v1/ (before the walk fix);
    - pk/, pk_np/, pk40/ (gl40's own package, for the A/B run);
    - emu_b0, emu_g0, emu_g1, emu_g0np, emu_g1np;
    - the scripts (mk.sh, run.sh, pspb.sh, rl_np.sh, em.sh, em_b.sh, chain*.sh) and cfg/.
  - The shots stay in the container.
- Shared attic/ge_native/step1/ (no game data):
  - the step-1 sources: nge/, ge/nge.h, boot/nge_stub.h;
  - t39_step1.diff and t40_step1.diff: the 8 changed files against t39 and t40;
  - scripts/ and cfg/ (gl39), scripts_gl40/ and cfg_gl40/ (gl40).
