---
name: mgs2-halfrate-study
description: MGS2 half-rate logic study (2026-11-07 halfrate_study.md; 2026-11-23 rate-site map from the reference): no time step, per-frame budgets with/without the ME, why half-rate gains nothing until the picture shrinks, H1-H4 options
metadata:
  type: project
  modified: 2026-11-23T20:45:00.000Z
---

- Thread "Study half-rate logic for 25 fps" (Ffm 2026-11-07 15:33 "Yes let's study it"; brainstorm "25 fps at any cost"). Study only, no main-tree change, no PSP build.
- UNITS: a shown frame = 2 PAL steps. Rainy aft deck 144.4 ms is per STEP -> 289 ms a frame (3.5 fps); 25 fps = 40 ms a frame = 20 ms a step (gap ~7x). The coordinator's chat said 144 "a frame" and 3.6x: wrong, corrected to Ffm 15:3x.
- CODE [C]: Act1 0x11d9d0 waits per the wait word 0x18db74 (= min vblanks per step; cutscenes set it), stores elapsed vblanks at 0x18db6c; only reader GM_ExecProc 0x1058e0 (play time). Total vblank count 0x18db64 read by stream control (0x118498/0x118790) and DG DMA funcs only. GV_ExecActorSystem 0x13f140: 12 priority lists, act(actor) with no time arg. Deck: 106 actor kinds, 392 actors.
- RESULT [I]: with the ME, today's picture side (~141 ms a deck frame + copy) is the limit, so half-rate logic adds nothing now. End state: full-rate deck 11.5-16 fps (game 46-64% speed), half-rate ~21-25 (GE ~40 ms a deck frame caps). Inside full-rate already ~19-25. Interpolation not needed for 25 fps.
- Steps.csv split [C]: picture + walk of a shown frame run in the NEXT step; reduced pipeline on unshown steps saves only ~1.2 ms inside (13.1 vs 14.3).
- RECOMMENDED: no true half-rate now; H1 = much larger exact reduced pipeline on unshown steps (est. 6-10 ms a deck step), H2 = cosmetic actors every other step (E4/G3/G4, Ffm OK), revisit H3 (deck doubled-step) when ME deck picture < ~70 ms a frame.
- x86 DEMO 2026-11-07: MGS2_POKE=30000:18db74:2 -> 50 pipeline calls per 100 vb, play time real rate, hashes differ from 30,100 (study §2).
- 2026-11-07 Ffm 'Where do we start?': H1 first (later found not hash-exact, folded into the renderer 2026-11-23).
- Related: [[mgs2-deck-costs]], [[mgs2-me-handoff]], [[mgs2-pipeline-clock]], [[mgs2-outlook]].
- Logic timing and the PCSX2 replay test (from 2026-11-07 21:03): [[mgs2-pcsx2-replay]].
