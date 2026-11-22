# MGS2 PSP port: what the rain costs in gl35, and how to make it cheaper

Study of 2026-11-21/22 (UTC). It answers the owner's question of 21:47 ("is possible to downgrade the rain to make it more cheaper?") and the "separate dice" questions of 21:50-21:54. The build studied is gl35 as delivered: drop sheets whole (`MGS2_GL_RAIN=1`), foot splash at 1/2 (`MGS2_GL_SPLASH=2`), no streaks, no rain sprites, the seven exact skips E5 (`MGS2_OPT2=fffff9df`), `MGS2_HALF_CYCLE=b`.

Nothing here ran on the PSP, and no build was delivered. Every PSP saving below stays [C model] or [I] until a PSP run measures it, with the option switched on and off in turns.

Marks:
- [C]: measured or read in code.
- [C model]: the port's cost model, ms = (1.65 × instr + 68 × I-miss + 86 × D-miss) / 333,000.
- [C emu]: the emulator's own timers (PPSSPP 1.20.4, emulated ms).
- [C twin]: the x86 twin.
- [I]: inferred, with the basis named.
- UNKNOWN.

Windows:
- Rain wait: the ME turn vb 29,810-29,890, plus its main turn 29,910-29,990.
- Rain walk: the ME turn 31,010-31,090, plus its main turn 31,110-31,190.
- "A step" is one game step (one vblank of game time), so ms a step is as in psp_harness.md.

## Verification (2026-11-22 02:56 UTC, workflow wf_b0a8315c-5cb, 4 checkers)

Read this first: it corrects section 1.
- Option A is exact (62/62 = hashref, also without MGS2_GL_RAIN_COUNT) but NOT invisible. Every study run had MGS2_GL_RAIN_COUNT=1, which overflowed fxthin's 16-entry range table (fxthin.h:43, fxthin.c:166): the "A" run dropped 81,972 falling-splash calls, A as it would ship drops 118,932, so the opening and the d01t deck were never really tested, and ana/conv.py skipped every picture before vb 29,500 [C emu].
- In the opening (shot_10516) A removes a screen-wide spray mist: 89,357 of 130,560 pixels darker, at most 22/255 [C emu, 2 runs]. The PS2 draws the same mist (twin GS capture through tools/gsref.c: 182,497 of 262,144 pixels brighter with it, max 22) [C twin + gsref]. Our renderer draws it as the PS2 does: not a renderer bug.
- On the aft deck both the PS2's VU1 and our converter cull these calls: every vertex lies above the top clip plane in 900 of 916 frames; xc_fpos returns 0 after the full cost. They graze the top edge at vb 30,800-30,812 (PS2: at most 14 pixels by 2 levels). live_30812 differs by 1 pixel (the study's "0.00%" was a rounding).
- Saving: model 5.2-5.3 ms a step of ME time in the rain wait and walk [C model]; PSP about 4.7 (range 3.5-5.8) [I, chain-6 PSP/emulator ratio 1.92]; main turns also gain 4.5-5.1 [C model]. The study's "~4x overrate" was mostly a picture/step slip: recent levers 1.5-2.2x, keeps ~1x.
- The main CPU waits only ~8.5 ms a step in the rain wait (gl35), not 14: B and D sit at that limit, H is past it. The walk's limit is UNKNOWN.
- Draw-side options B-H were all pictured with COUNT=1 on the aft deck only: their whole-route visibility is UNKNOWN.
- Lead decision 03:05: no owner switch for the falling splashes. Instead an EXACT early cull: a cheap conservative test that a call draws nothing, before the costly conversion, with every state byte left as the full path leaves it. Study and prototype: mgs2/main/attic/cull/ (workflow wf_c8b09d58-7ce), for the next build.

## 1. Summary

**Today.**
- The ME is the limit in the rain at about 87 ms a step [C, gl35 run 1].
- The rain's drawing costs the ME 11.7 ms a step in the wait and 6.5 in the walk [C model], plus 0.8 / 0.1 of record copies.
- The emulator puts the rain at 60% of the sorted-prims chain in the wait and 46% in the walk [C emu]. On the PSP that chain takes 18.0 ms a step in the wait, so the rain is about 10.9 ms of it [I].
- On the main CPU, the rain's logic is 6.9 / 4.3 ms a step and its share of the draw pipeline about 2.7 / 2.1 [C model]. The main CPU is not the limit today: its logic is 43 ms a step.

| | Option | What the player sees (pictures in the container) | ME saving, ms a step, wait / walk [C model] | Main-CPU saving [C model] | Exact (hashes = c9ddb593)? | Recommended? |
|---|---|---|---|---|---|---|
| A | Falling splashes off (`MGS2_GL_RAIN_OUT=16`) | WRONG (see Verification): the opening loses a real PS2 spray mist (vb ~10,516, 68% of pixels darker, up to 22/255); on the aft deck a few pixels at the top edge | 5.2 / 5.2 (PSP about 4.7 [I]) | 0 in ME turns; 4.5-5.1 in main turns [C model] | Yes, 62 of 62 [C emu] | **No as a switch**: replaced by an exact early cull of calls that draw nothing (study attic/cull/) |
| B | A, plus floor splashes and puddle ripples at 1/2 (`MGS2_GL_RAIN_OUT=16`, `MGS2_GL_RAIN_CALLS=5:2,6:2`) | Fewer white specks bouncing on the deck plates and fewer rings in the puddles, in the wait and the start of the walk (22 of 50 pictures, 0.2-25% of the pixels) | 7.8 / 5.2 | 0 | Yes, 62 of 62 [C emu] | The other second step: it keeps every effect at half, but changes more of the picture |
| C | As B with floor splashes and ripples at 1/4 | Sparser still (the 1/4 run: 22 of 50 pictures, 0.3-30% of the pixels) | about 9.0 / 5.1 [I: the 1/4 run's 7.65 / 3.77 plus the falling splashes' last quarter] | 0 | Yes [C emu, 1/4 run] | Only if B is not enough |
| D | A, plus puddle ripples off (`MGS2_GL_RAIN_OUT=80`) | No rings in the puddles, in the wait and the start of the walk (the ripples-off run: 16 of 50 pictures, 0.4-4.3% of the pixels) | about 8.1 / 5.2 [I: sum of the two runs; sums held within 0.2 in every combined run] | 0 | Yes [C emu, each run] | Yes, as the second step, if more is wanted: my pick over B (more saved in the wait, far fewer pixels changed); the owner sees both first |
| E | Floor splashes off (`MGS2_GL_RAIN_OUT=32`) | The deck plates look dry: 22 of 50 pictures, up to 35% of the pixels | 2.3 / 0 | 0 | Yes | No: the main rain cue on the floor |
| F | Drop sheets (the slanting rain lines) at 1/2 (`MGS2_GL_RAIN=2`) or off (`MGS2_GL_RAIN_OUT=1`) | Thinner or no rain in the air: 47 of 50 pictures, up to 70% / 85% of the pixels | 0.4 / 0.4, off 1.1 / 1.1 | 0 | Yes | No: the main rain look, little saved |
| G | Raiden's foot splash at 1/4 (`MGS2_GL_SPLASH=4`) or off | A smaller or no splash at his feet in the walk (up to 1.5% / 2.5% of a picture) | 0 / 0 | 0 | Yes | No: saves nothing |
| H | All rain drawing off (`MGS2_GL_RAIN_OUT=243`) | The rain is gone (47 of 50 pictures, up to 87%) | 11.7 / 6.5, plus 0.8 / 0.1 of record copies | 0 | Yes | No as a setting; yes as the bound in the PSP test |
| I | G1: the rain actors not run on the aft deck | The rain is gone, and play changes: the twin takes a new code path at vb 37,398 | 13.1 / 7.9, plus 2.7 / 1.4 of copies | 9.6 / 6.4 | No | No |
| J | G2: fewer drops in the game | Thinner rain, and play changes as in I | as the draw-side 1/2 options [I] | about 4.8 / 3.2 at 1/2 [I] | No | No |
| K | A separate dice and clock for the rain (DICE + NP), then fewer or no drops in the game (all but charaspread.c) | The rain thinner or gone; on the aft deck the guards and lamps roll other numbers than on the PS2; after the deck the twin plays on identically | about as I: 13 / 8, plus copies [I] | 6.8 / 4.1 of logic, plus most of the draw pipeline's 2.7 / 2.1 [C model, I] | No: a one-time change on the aft deck; new hash reference | Not now |
| L | The exact form: the rain keeps taking its full draws and clock, a private dice draws the thinned rain | As K, but the guards' dice and the pace stay the PS2's | as the draw-side options of the same thinning | less than K, by the shadows' cost | Not by hash (the rain's own memory differs): new reference plus a masked-hash proof | Not now: several days, and it saves only on the main CPU |

The ME column is [C model]. On the PSP, expect between a quarter of that figure and the full figure [I]:
- The model overrated ME savings about 4x in gl34 and gl35, but those were cache and copy levers.
- When whole calls were left out (gl17's effects), the PSP measured 4.4 ms against the model's 4.1 [C, psp_harness gl17].
- The ME can lose about 14 ms a step before the main CPU becomes the limit, so every draw-side option fits under that cap. That cap is about 87 ms minus the main CPU's 13.5 ms of waits a step (gl33's log), or about 73 ms of main-CPU work [I].

**Recommendation.** Switch the falling splashes off (option A, `MGS2_GL_RAIN_OUT=16`). Nothing changes on the screen, the state hashes stay c9ddb593, and the model saves 5.2 ms a step on the ME, which is the limit, in both the wait and the walk (the emulator's own ME timer: 2.4 emulated ms a step, a quarter to a third of its sorted-prims chain). On the PSP expect 1.3 to 5.2 ms a step [I]. Measure it as one lever in the next gl build, switched on and off in turns inside one run, with option H (all rain drawing off) in the same run as the bound.

If more is wanted after that, the next step is visible, so the owner sees the pictures first and it goes into DEVIATIONS.md. My pick is D (ripples off as well): about 8.1 / 5.2 ms a step in all, and it changes at most 4.3% of a picture. B (floor splashes and ripples at 1/2) saves 7.8 / 5.2 and keeps every effect, but thins the floor splashes, the main rain cue on the floor, and changes up to 25% of a picture. Both stay well under the 14 ms the ME can lose before the main CPU becomes the limit.

Do not do the game-side options (I, J) or the separate dice (K, L) now. Beyond the draw-side options they save only on the main CPU, which is not the limit today. Every form of them changes the state hashes. I and J also change play.

## 2. Where the rain's time goes today
### 2.1 The Media Engine (the limit)

The ME draws the rain's owners through the sorted-prims chain (chain 6 in the 'me chains' lines). By owner, with each owner's calls left out whole before they are decoded (the study's filter `MGS2_GL_RAIN_OUT`), against the same build with everything drawn:

| Owner (what it draws) | Calls a picture, wait / walk [C emu counters] | ME, ms a step, wait / walk [C model] |
|---|---|---|
| d_splash_fall.c: falling splashes (4 actors); no visible pixel on the PSP today (section 3) | 68 / 67 | 5.24 / 5.20 |
| ripple.c: rings in the puddles | 12 / 0 (hidden in the walk) | 2.90 / 0.01 |
| d_splash_floor.c: splashes on the deck floor | 20 / 0 (hidden in the walk) | 2.29 / 0.00 |
| rain.c: the drop sheets, the slanting rain lines (drawn whole today) | 4 / 4 | 1.11 / 1.06 |
| ft_splsh.c etc.: Raiden's foot splash (drawn at 1/2 today) | 0 / 0.2 | 0.00 / 0.02 |
| d_splash_parts_slow.c: splash parts | 0 / 2.4 | (in "all") |
| **All of the above** | | **11.69 / 6.50** (+0.82 / +0.13 of record copies) |
| Already not drawn: streaks (rain.c, rain_slow.c), the rain's sprites (d_splash_camera and three more), since gl16-18 | | 0 |

The emulator's own ME timers agree on the share: chain 6 takes 9.48 emulated ms a step in the wait, of which the rain is 5.74 (60%); in the walk 6.61, of which 3.07 (46%); no other chain changes [C emu]. On the PSP, chain 6 is 18.0 ms a step in the rain wait [C, gl35 run 1]; 60% of it is 10.9 ms [I], close to the model's 11.7.

### 2.2 The main CPU

The rain actors' own logic (the profiler's per-actor buckets: everything run inside each actor's act) [C model, ms a step, wait / walk]:

| Actor | Wait | Walk |
|---|---|---|
| d_splash_floor.c | 1.64 | 0.02 |
| rain.c | 1.22 | 1.20 |
| line_sph.c (8; native G0) | 1.07 | 1.07 |
| d_splash_fall.c (4) | 1.06 | 0.65 |
| body_sph.c (native NBS) | 0.87 | 0.41 |
| d_splash_camera.c | 0.41 | 0.40 |
| ripple.c | 0.33 | 0.02 |
| charaspread.c (native G0C) | 0.15 | 0.18 |
| rain_gas_pers.c, rain_cm.c, d_splash_parts_slow.c, ft_splsh.c, d_splash_motion.c | 0.22 | 0.39 |
| **All** | **6.9** | **4.3** |

The draw pipeline's share for the rain's objects (the main CPU's libdg, DMA and copies), from the rain-off runs (all twelve actors stopped from vb 29,700 / 30,900; the model's main-side difference minus the actors' own logic and the study switch's own cost of 0.8): about 2.7 ms a step in the wait and 2.1 in the walk [C model]. The draw-side options do not touch the main CPU (all of them: -0.4 to +0.1, noise) [C model].

## 3. Draw-side options (the game is not touched)

These leave calls out on the ME after the PS2 side has built them, so the game, its RAM and its pace stay the PS2's: every option's emulator run gives every state hash of deliver/hashref.txt (c9ddb593), 62 of 62 [C emu]. They save only on the ME (and the record copies), which is the limit. The filter finds each call's owner by its data block, as the drop-sheet thinning does since gl16; "1 in N" keeps every Nth call of that owner in the frame's order.

| Option (cfg line) | ME, ms a step, wait / walk [C model] | Emulator chain 6, emulated ms a step, wait / walk [C emu] | Pictures changed, of 50 (pixel share) [C emu] | State hashes [C emu] |
|---|---|---|---|---|
| Falling splashes off (`MGS2_GL_RAIN_OUT=16`) | -5.24 / -5.20 | -2.46 / -2.40 | 0 (byte-equal) | 62 of 62 |
| Falling splashes at 1/2 (`MGS2_GL_RAIN_CALLS=4:2`) | -2.52 / -2.49 | -1.21 / -1.18 | 0 (byte-equal) | 62 of 62 |
| Floor splashes off (`MGS2_GL_RAIN_OUT=32`) | -2.29 / 0.00 | -0.94 / 0.00 | 22 (0.5-35%) | 62 of 62 |
| Ripples off (`MGS2_GL_RAIN_OUT=64`) | -2.90 / -0.01 | -1.60 / 0.00 | 16 (0.4-4.3%) | 62 of 62 |
| Falling, floor and ripples at 1/2 (`MGS2_GL_RAIN_CALLS=4:2,5:2,6:2`) | -5.05 / -2.49 | -2.49 / -1.19 | 22 (0.2-25%) | 62 of 62 |
| Falling, floor and ripples at 1/4 (`MGS2_GL_RAIN_CALLS=4:4,5:4,6:4`) | -7.65 / -3.77 | -3.74 / -1.37 | 22 (0.3-30%) | 62 of 62 |
| **Falling off, floor and ripples at 1/2** (`MGS2_GL_RAIN_OUT=16` + `MGS2_GL_RAIN_CALLS=5:2,6:2`) | -7.75 / -5.19 | -3.74 / -2.38 | 22 (0.2-25%) | 62 of 62 |
| Drop sheets at 1/2 by strips (`MGS2_GL_RAIN=2`) | -0.37 / -0.36 | -0.32 / -0.29 | 47 (5-70%) | 62 of 62 |
| Drop sheets off (`MGS2_GL_RAIN_OUT=1`) | -1.11 / -1.06 | -0.73 / -0.65 | 47 (10-85%) | 62 of 62 |
| Foot splash at 1/4 (`MGS2_GL_SPLASH=4`) | 0.0 / 0.0 | 0.0 / 0.0 | 26 (0.01-1.5%) | 62 of 62 |
| Foot splash off (`MGS2_GL_RAIN_OUT=2`) | 0.0 / 0.0 | 0.0 / 0.0 | 27 (0.02-2.5%) | 62 of 62 |
| All rain drawing off (`MGS2_GL_RAIN_OUT=243`) | -11.69 / -6.50 (record copies -0.82 / -0.13) | -5.74 / -3.07 | 47 (10-87%) | 62 of 62 |

The 50 pictures: 14 shots (vb 29,500-31,262, among them 29,700-30,300 in the wait) and 36 live captures (vb 30,512-31,387, every 25 vblanks), at the same vblanks as gl35's rain-wait and walk shots. "Pictures changed" counts pictures with any pixel different from the study build's base run at the same vblank. No option changes the main CPU's side by more than the model's noise (-0.2 to +0.1 ms a step) [C model]. The model's figures are the ME turns' windows; the emulator's chain 6 is the same windows' ME turns. The sums hold: the 1/2 and 1/4 rows are within 0.2 ms of the sums of the single owners' figures, scaled [C model].

What the player sees (pictures in `/home/claude/main/rainstudy/shots/<option>/`, README.txt there; pixel shares are against base at the same vblank):
- **Falling splashes** (d_splash_fall.c, 68 calls a picture): leaving them out changes **no pixel** of the 50 pictures of the rain wait and walk (shots and live captures, byte-equal to base) [C emu]. The ME converts and clips these calls today and nothing of them reaches the picture; why (zero-size or fully faded particles, or clipped away) is UNKNOWN, and so is whether the PS2 itself shows them, but on the PSP they cost the ME for nothing. At 1/2 likewise: all 50 pictures byte-equal [C emu]. If the PS2 does show them, the PSP already differs there today, and leaving them out changes nothing further; to find out, compare a PS2 picture of the aft deck (the owner's reference shots) with gl35's.
- **Drop sheets** (rain.c): the thin slanting rain lines in the air, the main thing that reads as rain, and part of the sparkle near the floor. Off: the air is clear (47 of 50 pictures change, 10-85% of their pixels, by about 2 of 255 on average). At 1/2 by strips (gl16's form): thinner rain, but only 0.4 ms saved, because each strip is still decoded before it is dropped.
- **Floor splashes** (d_splash_floor.c): the white specks bouncing off the deck plates around Raiden, the main rain cue on the floor. Off: the deck looks dry; 22 of 50 pictures change, by 0.5-35% of the pixels, in the wait and at the start of the walk (until about vb 30,800), none after. They are drawn until vb ~30,800 and hidden after, so they save in the wait only.
- **Ripples** (ripple.c): the rings spreading in the puddles near Raiden. Off: 16 of the 50 pictures change, by 0.4-4.3% of the pixels (about 1.5 of 255 on average), in the wait and at the start of the walk only. They are drawn until about vb 30,700 and hidden after, so they save in the wait only (2.9 ms a step in the model, 1.6 emulated ms).
- **Foot splash** (Raiden's own, at his feet in the walk): already at 1/2. At 1/4 26 pictures change by up to 1.5% of the pixels, off 27 by up to 2.5%; neither saves anything measurable (0.2 calls a picture).
- Not drawn already (no saving left): the streaks, the rain's sprites (camera splash and three more).

## 4. Game-side options

### G1: no rain on the aft deck (the rain actors not run)

Upper bound from runs with the twelve rain actors stopped from vb 29,700 (wait) / 30,900 (walk) [C model, ms a step]: ME 13.1 / 7.9 plus record copies 2.7 / 1.4; main CPU 9.6 / 6.4 (the actors' logic 6.9 / 4.3 plus their share of the draw pipeline about 2.7 / 2.1). On the ME it saves only 1.4 more than "all drawing off" (plus the copies); its gain over the draw-side options is on the main CPU, which is not the limit today.

It changes play [C twin, pad route]. With the rain stopped on the aft deck, the sound state differs from vb 29,600, 26-100% of the pictures after the deck differ, and at vb 37,398 the game takes a code path the PS2 route never took (the twin stops there). Stopping only the five splash actors already takes a new path at vb 31,319, still on the deck. Why: the rain actors draw from the shared random seed (13,500 draws a step), take 18-21% of the PS2's clock on the aft deck (44% on the deck before it, where the PS2 drops steps), and other actors talk to charaspread (section 5).

### G2: fewer drops (the game's own rain thinned)

Not run [I]: about half of G1 on the main CPU at 1/2 (≈ 4.8 / 3.2 ms a step), and on the ME the same as the draw-side 1/2 options (the ME cost is proportional to the calls, fall2 = half of falloff [C model]). It changes play as G1 does unless the rain keeps taking its full draws and clock (section 5.3), and "fewer drops" by the game's own counters also changes the rain's memory, so the hash reference changes in every form.

The change to play, in plain words: on the PS2 the guards and every other random thing on the deck take their numbers from the same dice the rain uses, and the deck runs at the pace the rain allows. Thinning the rain in the game gives the guards other numbers and the game another pace from the first rainy step: the deck can play out differently (a guard turning another way, a different alert moment), and the pad route recorded on the PS2 may no longer fit after the deck.

## 5. A separate dice for the rain (the owner's question, 21:50-21:54)

The owner's aim: the guards' dice stay exactly the PS2's while the rain gets cheaper. Two things tie the rain to the rest of the game: the shared random generator (one word at 0x1fc954, x·0x5d588b65+1) and the game clock (every loop's backward branch adds 64 cycles; an interrupt fires when the clock passes the next event). I measured both per rain actor on the x86 twin, with a counter that reads, at every act of a rain actor, the clock and the seed before and after (MGS2_RAIN_PC; the plain run with it still equals hashref_gl35, 685 of 685 hash lines [C twin]).

### 5.1 What the rain takes from the dice and the clock, per step [C twin]

Rain wait (vb 29,200-30,400) and rain walk (30,500-31,300), per step (one act of each actor per vblank there):

| Actor | Draws from the shared seed a step (wait / walk) | Draws an act: values seen | Clock a step (cycles; wait / walk) | Clock an act: range |
|---|---|---|---|---|
| line_sph.c (8 actors; native G0) | 12,928 / 12,928 | 768-5,504 (4 per vertex) | 534,784 / 534,792 | 31,552-226,944 |
| body_sph.c (native NBS) | 316 / 243 | 271-343, 25 different counts (pose) | 168,099 / 114,157 | 166,208-170,176 in the wait |
| d_splash_floor.c | 155 / 58 | 0-485 in steps of 97 (up to 5 spawns) | 145,586 / 54,405 | 127,808-183,616 |
| d_splash_fall.c (4 actors) | 24 / 7.5 | 0 or 81 (one spawn) | 137,949 / 92,125 | 12,800-191,616 |
| rain.c (drop sheets) | 0.03 / 0.03 | 0 or 1 | 134,208 / 134,209 | constant |
| charaspread.c (native G0C) | 32 / 82 | 0-128 | 84,544 / 84,545 | constant |
| d_splash_camera.c | 43 / 43 | 0-339, 4 different counts | 29,095 / 29,008 | 26,432-47,360 |
| ripple.c | 17 / 4.5 | 0-42 in steps of 3 | 4,352 / 1,212 | 64-4,352 |
| rain_gas_pers.c | 1.1 / 1.1 | 0-12 | 5,696 / 5,697 | constant |
| rain_cm.c | 0 / 0 | 0 | 4,160 / 4,161 | constant |
| ft_splsh.c, d_splash_motion.c (Raiden's own splashes) | 0 / 2.6 and 0 / 2.4 | 64 / 16 a spawn | 128-7,362 / 2,688-3,931 | |
| **All** | **13,516 / 13,371** | | **1.25 M / 1.07 M = 21% / 18% of the 5.9 M cycles of a vblank** | |

- The rain's share of the clock is large. On the steady deck before the aft deck (stage d01t, vb 24,000-24,500) the same actors take 2.6 M cycles a vblank (44%), and there the PS2 cannot keep up: the twin runs 462 game steps in those 500 vblanks with the rain's clock, 489 without it [C twin: MGS2_RAIN_NP from vb 23,600]. That is, on the PS2 the rain makes the game drop steps on the deck, and the rest of the game (guards included) follows that pace.
- line_sph takes 96% of the rain's draws; it is already done natively (G0), which computes its draw count and its clock exactly today (E5).

### 5.2 Decoupling as a one-time change: what the twin shows

Switches on the twin (x86 only; `twsrc*/`): DICE = the 18 rain seed sites in w00a and the natives' NAT_SEED use a private word (0xc0000); NP = the rain actors' acts do not advance the clock (complete only in the third twin, `twx3`: the game code's polls, the natives' NAT_ADV, the runtime's K_ADV and its own polls), from a chosen vblank. Reference: the plain twin = hashref_gl35. The twin is a reduced build: a code path the reference route never takes stops it ("missing function"), so a changed route shows at once. "Pictures" = the twin's hash of each drawn picture.

| Run | Hashes | Pictures after the deck (w01a, vb 31,500-37,800) | Route |
|---|---|---|---|
| DICE | RAM differs from vb 23,300 to the end; sound (IOP) differs 74 lines, equal again at the end | identical, every picture; also identical in d01t | same path, same stage loads, to vb 37,800 |
| DICE + NP (aft deck only, from vb 29,000; complete) | RAM differs from 29,100; sound identical throughout | identical, every picture (and every picture of the deck too) | same path to 37,800 |
| DICE + NP from vb 23,600 (incomplete gating) | | every picture differs | same path; 27 more game steps in vb 24,000-24,500 (see 5.1) |
| Rain off from 29,000 (11 actors), plain | RAM, sound (29,600-34,900) | 26-100% of the pictures differ in each 500 vblanks | new path at vb 37,398 (missing function 23a148) |
| Rain off from 29,000, DICE + NP | RAM, sound (30,800-34,900) | the same pictures as the plain rain-off run | the same new path at 37,398 |
| Splashes off from 29,000 (fall, floor, camera, parts, ripple), plain | RAM, sound from 29,300 | (stopped) | new path at vb 31,319 (missing function 345040) |
| Splashes off from 29,000, DICE + NP | RAM; sound 4 lines (30,700-...), equal at the end | identical, every picture | same path to 37,800 |
| rain.c, rain_cm, rain_gas_pers off from 29,000, DICE + NP | RAM; sound identical | identical, every picture | same path to 37,800 |
| line_sph off from 29,000, DICE + NP | RAM; sound identical | identical, every picture | same path to 37,800 |
| body_sph off from 29,000, DICE + NP | RAM; sound identical | identical, every picture | same path to 37,800 |
| line_sph, body_sph, charaspread off from 29,000, DICE + NP | RAM; sound from 35,000 | the same pictures as the whole rain off | the same new path at 37,398 |
| every rain actor but charaspread off from 29,000, DICE + NP | RAM differs from 29,100; sound 2 lines (from 30,800), equal again at the end | identical, every picture | same path to 37,800 |

What changes with DICE alone (dumps at vb 30,000 and 37,000) [C twin]: on the aft deck, the non-rain users of the shared seed get other numbers: the guards (z_man.c), the chimney smoke, the lamps' flicker, item boxes, weapon spheres and more differ in RAM at vb 30,000. The next stage sets the seed again (0x00000016 at vb 37,000 in both runs), so after the deck only heap addresses differ (allocations moved by 0x150 bytes) and the private word itself. So DICE is a one-time change confined to the aft deck: the guards there roll other numbers than on the PS2 (what the player meets on the deck can differ), and the pad route still reaches the deck's end and plays on identically.

What changes with NP alone (aft deck only): the interrupts fall elsewhere inside the steps, so RAM differs (414 words at vb 30,000 in heap packet buffers; at vb 37,000 26 words, flag bits in a drawing packet buffer [I: GIF/DMA tags]); no game step is added or lost on the aft deck (the steps there fit in a vblank), and every picture is identical. On the d01t deck, where the PS2 drops steps, NP changes the number of game steps and with it everything after. So NP is not a free change in general: it is only harmless where the PS2 was not dropping steps.

So the owner's design point holds: to keep the guards' dice and pace exactly the PS2's, the rain must keep taking its full draw count from the shared seed and adding its full clock, whatever it draws. Then the private dice only decide where the thinned drops fall.

### 5.3 The exact form, actor by actor

Design: each rain act still advances the shared seed by exactly the full rain's draw count (a jump-ahead: N steps of an LCG are one multiply-add with precomputed constants, as the G0 natives do) and adds exactly the full rain's clock (as G0C adds its polls), while the thinned rain draws from a private generator. The hashes then do NOT stay hashref c9ddb593: the state hash covers the rain actors' own memory (their slots, their DG prims), which a thinned rain changes. It needs a new reference and a proof that the rest of RAM matches, as for E5: a masked hash or the read-hash method [C: the state hash is whole-RAM, boot.c state_hash2].

| Actor | Draw count computable without the full logic? | Clock computable? | Shadow's cost a step | Read by other code [C readers trace, words a step] | Main-CPU saving if thinned to 1/2 [I] |
|---|---|---|---|---|---|
| rain.c | Yes: 0 or 1 a step, from a countdown (+0x1a4 = 16 + (draw & 31)) [C code] | Yes: constant 134,208 cycles a step (the entry loop is fixed; its vertex loop is G0R native, which adds its polls exactly) [C twin] | ~nil [I] | 59: charaspread 15, wp_chng2 11, radar 9, d_splash_camera 9 (the camera splash reads the rain's intensity, 0x1f86c8 [C code]) | ~0.5 ms of its 1.2 [I] |
| d_splash_fall (4) | Yes in principle: 81 draws a spawn; a spawn needs the slot count (+164, from a camera product, clamped and smoothed), each slot's life (float, -1.0 a step; 24 + ((draw >> 8) & 15) at birth) and a once-a-step global gate [C code] | Partly: per active slot a fixed loop, plus sinf/cosf in the spawn, whose path depends on the angle [C code]; the twin shows 12,800-191,616 cycles an act | the slot bookkeeping, small [I] | 16 (2 each: charaspread, emb_control, it_chng2, gaged) | ~0.4 of 1.06 (wait), ~0.3 of 0.65 (walk) |
| d_splash_floor | Yes in principle: 97 draws a spawn, up to 5 a step; int life 8 + (draw & 7); density from +72 [C code] | Partly: varies 127,808-183,616 an act; DG_SetPos2 / DG_RotVector per spawn | small [I] | 20, of which **sdmanage (sound) 2** | ~0.7 of 1.64 (wait); ~0 in the walk (hidden) |
| d_splash_camera | Yes: 1 + 16·k a spawn; active count = +180 × the rain intensity [C code] | Varies 26,432-47,360 | small | 49 (d_splash_floor 9, d_splash_fall 9, wp_chng2 6, radar 5) | ~0.2 of 0.41 (its sprites are already not drawn) |
| ripple | Only by running each element's radius (respawn with 3 draws past 800.0) [C code]: the shadow is most of the act | in the wait constant (4,352) | ~the act | **150** (putobj 16, rain_cm 9, ft_splsh 9, blur 9) | ~0 |
| body_sph (NBS native) | Pose-dependent normal tests: 271-343 draws; the shadow is the act's own tests [C code, twin] | ~constant 166-170 k | ~the act | 6 | small |
| line_sph (G0), charaspread (G0C) | Already computed exactly by their natives (E5) | already exact | (today's natives) | 6 / 8 | their drawing is the ME's; logic already native |
| rain_gas_pers, rain_cm | tiny | constant | nil | 40 / - | ~0.1 |
| ft_splsh, d_splash_motion | 64 / 16 draws a spawn on the shared seed at sites outside the 18 (near 0x3299c4 and 0x328fb8; 0x34381c [I by address]) | | | ft_splsh reads ripple's state | Raiden's own splashes, not the rain |

The "other readers" columns come from the gl16-era write/read trace (prof/act_readers_vb24001_24100.txt). Most of these few words are probably the shared seed and the shared scratch area [I]; two are not: the camera splash reads the rain's intensity, and the sound manager reads the floor splash. The twin runs agree: removing the splashes changes the sound state within 300 vblanks.

Memory and object lists [C code]: the splash slots are preallocated inside each actor's own block, so thinning by slot changes no allocation (killing an actor does, which is why the "rain off" twin runs are only an upper bound of the change); the DG objects stay registered and only their prims are hidden. If the thinning removes prims before the main CPU's draw pipeline (libdg), that pipeline's clock must be added back too, because it is not a rain actor.

### 5.4 With both decoupled, is "fewer drops" or "no rain" a free choice?

- **Fewer splashes or none: yes on the pad route** [C twin]: with DICE + NP on the aft deck, removing the five splash actors (falling, floor, camera, parts, ripples) leaves every picture after the deck identical, the same code path to vb 37,800, and the sound equal again by the end. Without the decoupling, the same removal takes a new path at vb 31,319.
- **The rest of the rain, but charaspread.c: yes on the pad route** [C twin]: with DICE + NP, removing rain.c (the drop sheets), rain_cm, rain_gas_pers, line_sph or body_sph leaves every picture after the deck identical, the sound identical, the same path. Removing all of these together (every rain actor but charaspread, with the five splash actors) does the same: every picture after the deck identical, the same path to vb 37,800, the sound equal again by the end [C twin, run dicenpxkillnc].
- **charaspread.c: no** [C twin, by elimination]: every run that removes it (with or without the decouplings) changes the pictures after the deck and takes a new path at vb 37,398; the run that removes every other rain actor together and keeps charaspread does not, and neither does body_sph alone, while body_sph + charaspread together do. Other actors talk to charaspread: it receives a message 3 that makes its prims shown (0x2d27f0) [C code, the H3 note in the w00a header]. Whether a sender then behaves differently when charaspread is gone, or whether stopping an actor that others still point at is the cause (an artifact of the test), is UNKNOWN. Thinning it instead of stopping it was not tried. Its drawing costs little: it is not one of the drawn owners in section 2.1, and its logic is 0.15 ms a step [C model].
- So with both decoupled, fewer drops or no drops become a free choice on this route for every rain actor tested except charaspread, which would stay as it is.
- These are one pad route on the twin. "Free" means: the same route and the same pictures; the RAM is not equal (the rain's own memory, the interrupt positions), so the hash reference changes and a masked-hash proof is still owed.
- Saving if the rain but charaspread is removed this way [C model, ms a step, wait / walk]: on the ME the same as the draw-side "all off" (11.7 / 6.5) plus about 1.4 / 1.4 of the walk's own overhead and 2.7 / 1.4 of record copies (the G1 runs); on the main CPU the actors' logic without charaspread, 6.8 / 4.1, plus most of the draw pipeline's 2.7 / 2.1 [C model, G1 runs]. The splashes alone: main 3.4 / 1.2 (fall 1.06 + floor 1.64 + camera 0.41 + ripple 0.33 in the wait; 0.65 + 0.02 + 0.40 + 0.02 + 0.07 parts in the walk). The main CPU is not the limit today (logic 43 ms; the ME is at ~87), so these main-CPU savings shorten the step only after the ME has been cut below the main CPU's busy time (about 73 ms [I]).
- Thinning by the game ("fewer drops") was not run on the twin; only removal was. Thinning keeps the actors alive, so the stop-an-actor artifact cannot occur, but each thinned actor's RAM differs, so the reference changes either way [I].
- The exact form (5.3) gives the same savings without the one-time change, at the price of a shadow per actor and a proof per actor, as E5 did for seven skips.

### 5.5 Work it would take [I]

- DICE (one-time change on the aft deck only): 18 sites + 2-3 sites in ft_splsh / d_splash_motion if they are counted as rain + the natives' NAT_SEED; a private word; new hashref; twin + emulator + PSP checks. About a day.
- NP: the gate on the game code's polls, NAT_ADV, K_ADV and the runtime's own polls while a rain act runs (as twx3), only on stages where the PS2 does not drop steps (aft deck yes, d01t no). About a day, plus the proof that no step is lost or gained.
- The exact form: per actor a shadow (counts and clock) and an exact-skip proof as E5's; rain.c and the camera splash are easy, the falling and floor splashes need the clock per path (sinf/cosf), ripple's shadow is nearly the whole act. Several days.

## 6. Method and files

**Study build** (`/home/claude/main/rainstudy`, measurement only, never delivered): gl35's objects with eight recompiled from a copy of t35 (`t/`), each with a `#line` header naming gl35's original path so GCC's profile ids match and the unchanged objects stay byte-identical (all 19 checked by disassembly); EBOOT 99091bad (text +3.7 KB on gl35). Changes, all off unless a cfg key is set:
- `ge/fxthin.c`: five more owners found by their actor's data block (d_splash_fall +0x4c, d_splash_floor +0x50, ripple +0x40 x3, d_splash_parts_slow +0x44, d_thunder_parts +0x4b0 x3); `ge/gelive.c`: a call of an owner set "out" is dropped in gl_call before it is decoded (as the rain sprites are), or kept 1 in N; counters by owner.
- `psp/xl_psp.c`: cfg `MGS2_GL_RAIN_OUT=<mask>` (1 sheets, 2 foot splash, 16 falling drops, 32 floor splashes, 64 ripples, 128 splash parts, 256 thunder parts), `MGS2_GL_RAIN_CALLS=<kind>:<N>,...`, `MGS2_GL_RAIN_COUNT=1`; a "rain kinds" line each turn.
- `boot/gen_dispatch.c` with GEN_ACTOR_PROF (the profiler's per-actor buckets); `boot/boot.c` `MGS2_KILL_ACT=<from>:<names>` (stops the named actors: changes the game; for the G1 bound only).

**Model** [C model]: PspProf (the port's profiling PPSSPP) on the study build with gl35's model cfg (`cfg_emu/mgs2_m35_p.cfg`) plus the option, the four rain windows as model35.sh; per-function costs split by processor (ME: the ge_* converter, walk and list code; main: game code, runtime, stubs; record copies listed apart), per-actor buckets for the main CPU's logic. ms a step = the window's cost / 80. The study build's own cost: ME +3.0, main +2.0 ms a step against gl35's g35p (layout and counters); every option is compared with the study build's own base, never with gl35. Note: PspProf runs give 3 hash lines (30,000, 30,500, 31,000) that differ from hashref_gl35, as gl35's own g35p does; the hash check is the emulator's.

**Emulator** [C emu]: headless PPSSPP 1.20.4, the study build with the delivered cfg, pad and hashref (+ four rain-wait shots at 29,700-30,300 and the counters), 62 hash lines each, to vb 31,400; 'me chains' gives the ME's emulated ms by chain. The base run's pictures are byte-equal to gl35's emu_e2 run but one (shot_24784: 453 pixels by at most 4 of 255, the opening, not the rain).

**Twin** [C twin]: the x86 twin rebuilt from `twsrc*/` (`twbuild*.sh`): switches MGS2_RAIN_DICE (the 18 w00a seed sites in `twgen/gen_s_w00a.h` and the natives' NAT_SEED to a private word at 0xc0000), MGS2_RAIN_NP (+ _FROM; the rain acts' polls, NAT_ADV, K_ADV and the runtime's own polls not counted: complete only in `twx3`), MGS2_RAIN_PC (each rain act's clock and seed draws, read only), MGS2_KILL_ACT. The plain twin with the counter equals hashref_gl35 (685 of 685). The twin is built only with the functions the reference route runs, so a new path stops it ("missing function").

**Not done / open:** no PSP run (every PSP saving is [I]); option D (falling splashes and ripples off together) and option C were not run as one, their figures are sums of runs [I]; the masked-hash proof that a decoupled rain leaves the rest of RAM equal; why stopping charaspread changes the route (a real dependency or a test artifact); the sinf/cosf clock paths of the falling splash; only one pad route.

**Files (container, game data stays here):** shots `/home/claude/main/rainstudy/shots/<option>/*.png` with `shots/README.txt`; logs `emu/<option>/`, `model/`, twin summaries `tw/s_*.log`; scripts `model.sh`, `em.sh`, `tw_run.sh`, `twbuild*.sh`, `ana/*.py`. Shared: only this file.
