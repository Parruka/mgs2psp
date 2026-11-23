# gl40 on the PSP: the read (2026-11-23, lead)

Build: gl40 (measurement build; package /home/claude/main/build_gl40/deliver, EBOOT 530f8082, hashref c9ddb593, README
84064098; plan main/attic/gl40/gl40_plan.md). PC: Builds\MGS2T_gl40, logs LOGS\gl40 (run 1) and LOGS\gl40\r2 (run 2, 59 files,
each md5-checked against the stick). Reader: rd40pc.py (e1356bfd), run on the PC; full run 2 output kept on the PC only.
Figures are ms a PSP step (a vblank); fps = 500 / ms a step. [C] = read from the PSP log.

## 1. Exactness [C]
- Run 1 (plugin OFF, battery): the PSP went off at about 26 min, log ends at vb 28,400; no crash record (the crash catcher's
  writer thread stops too: the power-cut signature). Its 56 hash lines are equal to the reference's first 56. Battery 88 % at
  the stop: not a flat battery.
- Run 2 (plugin ON, same folder on the stick, launched 22:44:33 UTC, ~30 min to "Press X to exit"): hashes 62/62 EQUAL
  (66499cdb584a), digests 155/155 EQUAL (6582f0728a36 / dz 531a3f4659b8), schedule check none, XP check 1,200 plans compared,
  DIFFERING 0; F1 planned runs with a problem 0; natcull check 0; native step-1 dropped 0; ME picture loop dropped 0.
  6 of run 2's shots equal run 1's: they are the blank K7/K8 frames (md5 7bb81a3d), not left-overs.
- Power: "external power 0, charging 0" in BOTH runs (run 2: 84 % -> 75 % in 1,791 s). The USB cable did not power the PSP with
  the plugin on either; the only run-1/run-2 difference was the plugin. Cause of run 1's stop: UNKNOWN (one more unexplained
  battery-power stop; see mgs2-builds-gl29-gl36 for the earlier ones).

## 2. The copy plan lever (XP) in the rain wait [C]
Same stretch of the wait (t56-t63, digests off, M5 on), ms a step:

| form | turns | step | main logic | ME busy | plan (walk / sort+join / rest) | sort moves a step |
|---|---|---|---|---|---|---|
| XP0 gl39's plan | t56, t61 | 78.47 | 44.50 | 70.07 | 5.83 (2.49 / 3.13 / 0.21) | 22,850 |
| XP1 ping-pong | t59 | 76.78 | 43.53 | 68.88 | 4.88 (2.47 / 2.20 / 0.22) | 22,700 (no copy-back) |
| XP3 ping-pong + look-back 8 | t57, t60, t63 | 76.44 | 42.74 | 69.68 | 4.01 (3.15 / 0.65 / 0.21) | 7,150 |

- XP3 - XP0: -2.0 a step in the same stretch (-2.49 over all n4 - n3, which mixes in t66/t67's heavier scene) = 6.38 -> 6.54 fps.
  The look-back join cuts the sort 3.13 -> 0.65 but costs +0.66 in the walk. XP3 is gl41's form. XP1 alone gives about 2/3.
- XP7 (XP3 + the check) costs +6-7 ms in mq_chk_end in its two turns: check only, never in play.
- The rain-walk XP3-XP0 pair (+0.83) is not a lever result: ME busy differs by +6.2 between those turns (the walk's scenes vary).
- What gl40 was built to find: gl39's ~8 ms UNKNOWN in "drawing's cost on the main CPU" is mostly the copy plan. Waits inside the
  transfer's end are 0.000 in every turn; mq_chk_end (the plan + its frame) is 6.3-8.5 ms a step with XP0, 4.3-5.3 with XP3 (rain turns).
  Drawing's cost on the main CPU, K0 XP0 - K7 in the wait: +13.55 (logic +11.10, pipe +1.42, gsw +0.69); first turns +14.89.

## 3. Other levers and probes [C]
- M5 (the kernel's interrupt walk, MGS2_OPT2 bit 18): OFF - on = K7 rain +1.10, K8 deck +0.36, K0 deck XP3 -0.20 (n 1 each):
  0 to ~1 ms. Keep on.
- Coupling (steady deck, K8 load - K7): arithmetic -0.02, cached reads +0.26, uncached writes +0.08 on the main CPU's logic;
  the main waits (gew) +15 for the ME's 25 ms load. K8 no load - K7: rain +1.00, deck -0.11. The ME's own memory traffic does not
  slow the main CPU in a way that matters; gl39's +2.4 for cached reads did not repeat.
- Digests on - off: +4.5 a step (gew +5.7, ME lists +16.9): measurement cost only.
- Main-drawing windows before vb 24,000: 55.51 (gl39 56.63, gl36 55.56; 20,000-24,000 84.46): gl38's regression is gone.
- Codec / steady deck K0: XP0 40.28, XP3 40.72 (+0.43, inside the ME-bound wait: gew +0.7); ME busy 40.3 = the step: the deck is
  ME-bound (12.4 fps at half speed). Stage program off (K5) - K0: -14.4 (ME -15.0).

## 4. Where the rain wait stands after gl40 (XP3, t57/t60/t63 means) [C]
- Step 76.44 = 6.54 fps at 26 % game speed. MAIN-CPU-BOUND: main logic 42.74 (game logic alone, K7: 34.5), pipe 26.3, GS walk 4.4,
  picture/wait 0.4; the ME is busy 69.7 and idles ~7.
- Main-side drawing left: the plan 4.0 (its walk 3.15 = ~6,430 range adds a step at ~0.49 us each), the rest ~4.
- So ME-side savings do not shorten the rain step until the main side drops below the ME: every rain projection must take
  max(main, ME), and the renderer checkpoint P (from K-N2R) must too.

## 5. Problem counters [C]
nat none; groups left untaken by turn (24,000: 524, 25,000: 423, 29,000: 2,037) = the emulator's; ME load entries not posted 197
(the loop behind; the load turns ran 25 ms a step as asked); XP plans failed for the arena 388 of 100,604 (they take the old path);
end of run: malloc arena 8,897 KB in use, 2,608 KB in one block still free.

## 6. What changes
- gl41 keeps XP3 on (no change to its plan). Next main-side lever: the plan's walk (3.15 ms), if the copy plan survives the renderer.
- Delivery note: the PC's permission check blocks `psprun.py remove`; `update <old> <new>` replaces the last build instead.
