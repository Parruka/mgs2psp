---
name: mgs2-e1
description: E1 "everything exact" (all VU0 math toward zero): NOT SHIPPED, E1 accepted under the 2026-11-18 lead; the gl18e1 build and its checks kept for the record
metadata:
  type: project
---
- **2026-11-18 LEAD DECISION: E1 ACCEPTED, the VFPU stays (about 9 ms a deck step saved); gl18e1 not shipped; #178 closed** (DEVIATIONS.md E1, plan_deck_25fps.md §2). The build lost its container tree; the delivery files are on the PC (PROJECT\backup_2026-11-16\mgs2_thread\files\home\gl18e1). Lines below are history.
- Ffm's card 2026-11-08 17:46 "Everything exact" (#178): all VU0 math rounds toward zero as on the PS2 (macro groups, micro programs, our hand forms). One change per measurement: ships after gl18me's run is read; DEVIATIONS E1 is resolved when it ships with hashes = the twin's. Then gl19 with B4 (C4 effects) on.
- Build /home/claude/e1/d2 (EBOOT 34839d48): VU_RZ_ALL (vu_vgrp = F only, FCR31 0x01000001; boot_rt.o without VP_VFPU; natives as C forms with DAZ on loads). ELF text 4,105,400 (gl18me 4,247,304); heap break peak 136 KB lower.
- Delivery /home/claude/gl18e1 (made by /home/claude/e1/mk_gl18e1.sh from /home/claude/e1/dl): EBOOT 53a693ee (title "MGS2 harness gl18e1 exact vector math"), cfg 8eee8d94 (settings = gl18me's), hashref 879e5b75 (= prof/hashref_x86_in_e1.txt), README bb2a9176; overlays d00t a50a328b, d01t 8a5e83e4, init 67233d5e, n_title bb3382f7, r_title a310a295, r_tnk0 143eadde, w00a f2a6da1e, w01a f9f5df34. Ship like gl18me: Builds\MGS2T_gl18e1, SLES from Sources, pak and pad from MGS2T_gl18, empty memcard0, 15 md5s.
- Checks [C]: 1.20.4 run e1b 434/434 hashes, 311/311 vuh, 17,664/17,664 frames = the RZ twin out_r18rzd; headroom 2,524 KB (PSP ~970 KB [I]). Master MGS2M_gl18e1: hashes equal; shots vs gl18me's master: 5 equal (8760, 15761, 24244, 31663, 35027), 5 small (8-827 px), 33,613 25% at edges (sub-pixel camera move), 5 rain shots 64-90% (drops in other places, mostly 1-8 levels).
- Cost [I]: +8.6-9.7 ms a step on the rainy deck, ~+4 inside; the real figure is UNKNOWN until the PSP run.
- Still to write (#178): psp_harness.md "## Build gl18e1" (after "## Build gl18me"), DEVIATIONS.md E1 status (lines 87-92), NOTES.md item 7.
