---
name: mgs2-hw-history
description: MGS2 PSP timing history build by build (v5-v30, gl2m-gl17): ms per step on the deck tutorial, crew deck, deck free play, indoors and codec calls, with what each build changed and how to read the turns
metadata:
  type: project
  modified: 2026-11-02T17:09:15.988Z
---

Details: mgs2/psp_harness.md (every build and run), speed_notes.md. Model: PSP ms ~ (1.65 instr + 68 I-miss + 86 D-miss) / 333k (renderer_notes 11.25); [[mgs2-speed-work]].
- Before the picture: deck tutorial v5 170.4 -> v23 70.00 (sound mix +7.53); crew deck v10 41.6 -> v18 27.97; deck free play v25f 92.42 -> v29 78.25; indoors (vb 31,801-37,800) 37.18 ME off, 37.57 ME mixing (v30me6c).
- Picture builds, all on, indoors / waiting deck: gl3 148.8 / 212.3; gl7 127.0 / 202.1; gl8 109.2 / 176.1 (PGO + layout); gl9 104.0 / 169.3 (lanes 1+2); gl10 96.3 / 160.2; gl11 95.9 / 153.6 (lanes 3+4); gl12 95.4 / 153.4; gl13 91.9 / 150.5 (step 0 texture pages; memo saves 3.8 calls / 5.3 deck / 5.7 inside); gl16 93.4 / 149.4; gl17 effects off 91.6 / 148.1.
- gl14 (2026-11-02; T1 + T2 exact, C1 in turns) vs gl13 at the same windows: calls 65.07 -> 58.72 / 64.11 -> 56.45 (-9.8 / -11.9%; model -5.8%), waiting deck 151.37 -> 150.02 (-0.9%; the model's +1.5% D-cache placement loss not seen), inside 92.05 -> 92.22 (model +1.1%). Turn medians: T2 saves 5.4-5.6 in the calls, T1 1.8 calls / 2.0 inside, C1 -2.0 / -2.6 in the calls and nothing elsewhere (no P07 there).
- gl15 (2026-11-02; C1 on by default, C2 in turns) vs gl14 all-on at the same windows: calls 59.89 -> 56.29 / 56.45 -> 51.92 (-6.0 / -8.0%), deck waiting 149.96 -> 150.41 (+0.3%), inside 95.79 -> 96.39 (+0.6%, noise); run 45.4 min; C2 turns +0.7/+0.8 deck, +0.79 inside (noise): no saving.
- gl16 (2026-11-03; step 3 mesh trims on, off in turns) vs gl15 all-on: deck waiting 149.89 -> 149.43, inside 93.76 -> 93.37, calls flat; trims off +1.76 deck / +2.00 inside (model 1.5 / 2.1): the model was right.
- gl17 (2026-11-03, run 2 2026-11-08; C4 effects in turns, rain per Ffm): effects on +4.4 deck / +0.5 inside (model 4.1 / 0.2); everything else -1.3..-1.4 vs gl16.
- Reading: the PSP gives 0.3-0.8 of the model's all-on change (gl14's calls 1.7-2x, the exception); off turns cost more than modeled, so judge by all-on gains; window noise inside about +-2 ms, calls under 0.5 ms, so use medians of turns.
- Run lengths (indoor route, exact to the end): gl7 59.7 min, gl8 53.4, gl9 52.1, gl10 49.6, gl11 48.1, gl12 47.2, gl13 46.2, gl14 45.9, gl15 45.4, gl14mp 46.7 (ME probe turns), gl16 44.9, gl16ep 45.5, gl17 44.7 (run 1 stopped at vb 33,044 on the battery alone). Lowest heap headroom: gl9 936 KB, gl10 612, gl12 580, gl13 404, gl14 404, gl14mp 340, gl15 388, gl16 388, gl16ep 324, gl17 444.
- Codec calls: story calls ~49-51 ms a step before the faces were drawn; gl5 132.2, gl6 108.75 with faces; gl14 56-59.
- Facts: D-miss 172 ns load / 317 store, I-miss 218 ns; VFPU rounds RN; PSP FPU keeps 0 + denormal (v16 fix). GE alone: deck frame 38.3-38.9 ms, 41.8-45.1 with the CPU busy ([[mgs2-ge-hardware]]).
- Power: test runs on the OSTENT or charger only ([[mgs2-switch-offs]]).
