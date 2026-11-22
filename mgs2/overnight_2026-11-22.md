# MGS2 port: what happened overnight (2026-11-22, times UTC = Lisbon)

## What I need from you
- When the PC and the PSP are on, say **"psplink up"**. I copy gl36 to `Builds\MGS2T_gl36`, make `LOGS\gl36`, check the PSP is free, replace gl35 on the stick, and launch. Same steps as gl35.

## gl36 is ready (a measurement build)
- **Exact everywhere I can test** [C]: the PC twin in 20 setups (685/685 hashes), the emulator (62/62 hashes, 916/916 picture lists), 51 of 52 pictures equal to gl35's (the other is the usual noise frame).
- **Safe** [C], checked by an independent review: the same system calls as gl35, the clock stays 333 MHz, no new hardware writes, nothing touches the flash.
- **What's new:**
  - The cut I found overnight (below): drawing that can never show is left out before it costs anything. It takes turns on and off, so the PSP measures it.
  - The ship re-use idea from yesterday, also in turns. It helps in the rain wait and costs a little in the walk.
  - Two memory fixes. gl35 had started with less than 16 KB to spare, and gl36 first failed to start on the emulator because of it. Both fixes are exact.
  - A new speed profile.
- **Expected** [C model]: with both cuts on, about 4.4-5.3 ms a step faster than gl35 in the rain. The PSP usually shows half to two thirds of the model's gain, so perhaps 2-3.5 ms [I]. One known cost: in the turns with both cuts off, the new profile is about 2 ms slower than gl35 in the rain, while the deck and the opening are faster. I'll fix that in the next profile.
- Short run as before (it stops after the deck, ~25-30 min). USB power with the plugin on is best.

## Your rain question ("make the rain cheaper?")
- The study found one option that looked free: switching off the **falling splashes** (A). An independent check then proved it is **not invisible**. In the opening cutscene those splashes are the spray mist over Snake, and the PS2 shows it too. Switching them off would darken 68% of that picture. So I did not use it.
- On the deck, though, those splashes are always above the top of the screen. The PS2 throws them away too, but only after paying for them. So I built an **exact cut** instead: a cheap test proves that a splash (or any effect strip) cannot reach the screen, and skips it before the work. Nothing you see changes. It passed a proof read twice and 28 million fuzzed test cases with 0 errors. That cut is what's in gl36.
- Other options, each one visible, if we ever need more: fewer floor splashes and fewer puddle rings (~5-8 ms in the model). I'd show you pictures before using any of them.

## Your "separate dice for the rain" idea: a correction
- Last night I said the exact form keeps the hashes. **That was wrong.** The state hash includes the rain's own memory, so it needs a new reference and a proof of its own (days of work). It would also save main-CPU time only, and the ME is the limit today. So: a good idea for later (the logic phase), not now.

## The camera "rolling" you pointed out
- You were right that it is how the game sells the ship's motion. It is a slow up-and-down **bob** of the camera (two mixed waves), switched on by the deck script, and the game logic reads it. So I keep it exactly as the PS2 does it. A drawing-side trick could save 0.5-2 ms later, but I'll decide that after gl36's numbers. Design: `mgs2/main/attic/roll/roll_design.md`.

## Files
- Rain study and its correction: `mgs2/rain_options_2026-11-21.md` (the "Verification" section at the top).
- The exact cut: `mgs2/main/attic/cull/cull_v3.md`.
- gl36: `mgs2/main/attic/gl36/README.txt` and `report.txt`; build log entry in `mgs2/psp_harness.md` ("Build gl36").
