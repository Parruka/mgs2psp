---
name: mgs2-h1
description: H1 (task #168), cutting the draw pipeline on unshown steps exactly: cut list, proof method, model results, why it is on hold
metadata:
  type: project
  modified: 2026-11-08T06:32:45.355Z
---

H1 = more of the draw pipeline returns at entry on reduced (unshown) calls; exact = same pictures, iop, time, and game code reads and stores the same. Notes: mgs2/h1_notes.md (2026-11-08).

**Cut list (MGS2_LITE_FN, hex):** 1214a0 (today's default), 133cc0,1324f0 (shadow + spot), 138ac8,138ae0 (EVM), 130390,1303a8 (people), 13d540 (sea hook), 122138 (sort chain) = "six-pass cut". Do NOT cut 12dc90 (Prim2 sort: a later shown packet reads its data) or 121060 (transform: 1,993 frames).

**Proof method (trace twins, /home/claude/sbh1/boot/boot.c):** MGS2_READ_HASH=4 (rh places read, rv value sums per reader lane), MGS2_WRITE_HASH=1 (wh stores), MGS2_RV_VB / MGS2_READ_LOG to find first differences; runs sh sbh1/runth1.sh <build> <tag> 37800 [env].

**Result:** pictures, iop, time and rh equal on route i, but rv differs on 98 reader keys and stores on 71 store-site lanes (VU0 registers, scratchpad and shared-memory leftovers: d00t P323c38, d01t P329668, w00a P3b7c88; P272fd0/P276b38/P3bebf0/P3aaef8 each unshown step). Not proven exact: by DEVIATIONS E5's standard each differing value must be a leftover no code acts on; that sorting was not finished (UNKNOWN).

**Model (PSP bucket builds sbg0m/d vs sbh1m/d, rainy deck 30,810-30,890):** whole step instr -5.10, D -1.00, I +2.27 (placement, redone by a layout search): upper bound ~-6.1 model / ~-5.5 PSP [I]; inside -2.7 model. Reduced calls 15.23 -> 12.44 model ms; after H1 they still hold ~8.9 instr+D (box cull 96936c0f_0 1.14, qcopy 0.86, OnePieceObjs 0.78, StoreMatrixObjs 0.74, sort keys 0.84). GS walk of unshown lists 4.1 -> 2.0.

**Status 2026-11-08: ON HOLD.** gl18 (rain exact skips) and the ME picture first; then finish the sorting (MGS2_READ_LOG in d01t, w00a, w01a) or build the form that leaves the leftovers as the full call does (cost UNKNOWN) with the box-cull speed-up of b_proof_notes.md. Related: [[mgs2-halfrate-study]], [[mgs2-game-random]].

- 2026-11-23 PROOF ROUND (h1_notes.md "16:40", attic/h1_2026-11-23/state.md R1-R8): whole-pass cut c7 (EVM kept) moves the strict hash at 603/684 points (every pass alone moves it); reader proof: only ORDER / LEFTOVER / GARBAGE-IN (d01t P21a798, P335100) reads differ; ~3.5 model ms [I]. LEAD DECISION: not built on the old path; the list is the GE renderer's seam input; E5 + new hashref per renderer step where the strict hash can't hold.
