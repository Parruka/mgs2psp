# MGS2 PSP port: deviations from the PS2 game

Every way the port differs from the PAL PS2 game (SLES-50383), chosen or not yet fixed, with its impact. Started 2026-11-07 from Ffm's charter (21:03: "Log every visual or gameplay compromise in DEVIATIONS.md with its impact"); kept by the MGS2 port thread. No compromise ships without its entry here.

Marks: [C] measured or read in the code; [C model] measured on the PSP cost model in the emulator, not on the PSP; [I] inferred, with its basis named; UNKNOWN, with how we will find out. Only PSP runs settle speed.

What "the PS2 game" means here: your disc's own code, recompiled and run on the PC (the x86 twin, state hashes every 100 vblanks), and the PS2's own drawing commands drawn by our reference renderer (gsref) or by the PPSSPP master build. No real PS2 has been measured; section E says what that leaves open.

The build described: **gl18** (2026-11-08), on your PSP since 17:45, its run not read yet. The next, gl18me (ready 17:55), keeps every picture and hash of gl18's with the rain's seven skips always on (E5) and adds no deviation: the Media Engine makes the same picture (me_step4_notes.md 4e). Its picture is gl17's with the effects off in both turns (B4), and its two turns differ only in the exact skips (E5), so it differs from the PS2 only as this file says [C: x86 twin, 17,664 frames equal; psp_harness.md "Build gl18"]. gl17's two runs on your PSP are read (psp_harness.md "Build gl17").

## A. Picture cuts you chose (in gl17 and gl18; drawing side only)

Your rule for every cut of this kind (cards 2026-11-07 21:17, "Cut cosmetic effects only", and 2026-11-08 00:09, "Drawing-side cuts only"): a weather or effect cut changes only what we draw. The game's effect actors still run every step as on PS2, because they share each stage's one random seed with the guards, so every state hash stays equal. A1-A3 are of this kind; their game-side skips (E5) are exact.

**A1. No rain streaks.**
- What: the thin falling streaks of rain.c and rain_slow.c are not drawn (cfg `MGS2_GL_STREAKS=0`).
- Your call: card 2026-11-03 17:38, "No streaks".
- Picture: the streaks are missing wherever it rains (the opening's bridge, the decks); at the rain shots 1-3% of the pixels differ by more than 8 levels from the streaks drawn (stagec_notes.md 4.12; your `SHOTS\c4_effects\rain_options`) [C]. gl16 and earlier never drew them (no converter), so gl17 looks as gl16 did here.
- Speed: drawing them would cost 5.1 ms a step on the rainy aft deck [C model] (stagec_notes.md 4.12). Their game-side work is skipped exactly in gl18 (E5).
- Undo: unset `MGS2_GL_STREAKS`, and turn H3R and G0R (E5) off.

**A2. The rain's own sprites left out.**
- What: the sprite calls of four rain actors (body_sph, charaspread, line_sph, rain_gas_pers) are left out whole (cfg `MGS2_GL_RAIN_SPRITES=0`).
- Your call: card 2026-11-03 18:19, "Leave them out".
- Picture: missing are a fine grey spray of drops over Snake's and the guards' clothes (strongest close to the camera), a faint speckle and haze on the far deck and in the opening, and small drops on Snake for about 10 s after the door inside. At vb 24,784, 37% of the pixels differ by more than 8 levels (stagec_notes.md 4.13; `SHOTS\c4_effects\rain_sprites`) [C]. gl16 and earlier did not draw them either (C1).
- Speed: 5.26 ms a step on the rainy aft deck [C model], about 4.3 on the PSP [I: the model's figure times deck_costs.md §2's model-to-PSP ratios].
- Undo: unset `MGS2_GL_RAIN_SPRITES`, and turn G0H, H3, NBS and G0C (E5) off.

**A3. The foot splash at half.**
- What: when Snake walks on the wet deck, 1 strip in 2 of the splash is drawn, the same strips every frame (cfg `MGS2_GL_SPLASH=2`).
- Your call: card 2026-10-31 16:55, "Half", confirmed 16:58 ("lets stay 1/2 if we need more perfomance we go 1/3"). Your reason (15:04): too much for the PSP's screen.
- Picture: half of the splash's drops (renderer_notes.md §11.30; `SHOTS\rain_and_splash`).
- Speed: no CPU saving (178.8 against 178.9 ms a step [C model]). The GE draws fewer pixels, which saves step time only when the CPU waits for the GE; gl16 never waited for it on the deck [C, prof/hw_gl16/run1].
- Undo: unset `MGS2_GL_SPLASH`.

**A4. The close camera's drop sheets: undone.** They were at half in gl10-gl16 and are whole again since gl17 (card 2026-11-03 17:38, `MGS2_GL_RAIN=1`). No deviation now.

## B. Picture differences of the method (your OKs, or waiting for your call)

**B1. The picture is stretched to the whole 480 x 272 screen.**
- What: the PS2's frame (0-512 across and down in its own coordinates) is scaled by 0.9375 across and 0.53125 down onto the whole screen [C: ge/xconv.c lines 62-64 and 123-125].
- Impact: the game's 4:3 picture fills the PSP's 16:9 screen, so everything looks about 32% wider than on a TV [C: (480 / 272) / (4 / 3) = 1.32]. Thin lines and letter edges lose detail in the squeeze (renderer_notes.md §11.19).
- Your call: not made yet. The faithful form, 4:3 in 362 x 272 with side bars, is renderer_notes.md §11.7's recommended default; not built. To decide with pictures of both.

**B2. The motion trail lasts twice as long.**
- What: the game blends the other frame buffer over each new frame at 50% (its FIX 0x40). The PSP draws only the shown frames, so that blend takes the frame shown two steps before [C: ge/gelist.c line 104 uses the game's FIX; renderer_notes.md §9 "The trail at 25 fps"].
- Impact: each shown frame is 0.5 new + 0.5 the last shown, where the PS2 gives 0.5 new + 0.25 the step before + 0.25 older: the trail lasts twice as long in time [C: the blend's formula]. The trail frames also differ from the reference in their bottom 16 rows (256-271, up to 5 levels; #68; gl16 run 1) [C].
- Your call: not made yet. A 25% blend (FIX 0x20) gives the PS2's weights (recommended in §9); not built. To decide with pictures. Your standing OK (2026-10-18 / 19): the trail and blur may be removed if too heavy; not used.

**B3. The codec faces are skinned and lit by the GE (C1). On by default since gl15.**
- Your calls: the test (card 2026-11-02 12:51, "Test in gl14"); the default you left to me (17:12), and I turned it on.
- Picture: the faces differ by 1 colour level in 1-2.6% of a codec frame's pixels, a few edge pixels by more (stagec_notes.md 1.9) [C].
- Speed: 2.0-2.6 ms a step less in the calls on your PSP (gl14 run 1) [C].
- Undo: g_opt2 bit 16 off.

**B4. The effects drawn by the GE (C4 and C4X): tested in gl17, off in gl18, your call open.**
- What: lamps' glows, smoke, haze and the game's other sprites, quads and strips (the sort pass and Prim2), drawn in every other turn of gl17; off by default and in both of gl18's turns, so the default picture still lacks them (C1).
- Picture with them on: they appear; with C4X their vertices land within 0.06 px of the PS2's positions (stagec_notes.md 4.7) [C].
- On your PSP (gl17 runs 1 and 2) [C]: every effects-on shot equals the PPSSPP master's byte for byte, except 15,761's 52 pixels (C6) and the trail frames' bottom band (B2), which builds without the effects show too.
- Speed on your PSP (gl17 run 2) [C]: +4.39 ms a step on the rainy deck (3 turns: +4.4, +4.2, +4.8; the model said +4.14), so 148.1 -> about 152.5 ms a step there (3.37 -> 3.28 fps); +0.55 inside (22 turns); +0.2 or less in the calls and the opening.
- The game side: with them on, the rain's skips (E5) stay exact (x86 twins with C4 on, psp_harness.md "Build gl18") [C].
- Your call: the card of 2026-11-08 06:48, "Turn on" from gl19 (recommended: they are the game's own picture and the largest part of C1's gap, P04+3740's sprites) or "Keep off" (you chose the test, card 2026-11-03 12:46).

**B5. The people on the GE (C2): off.** Your card 2026-11-02 23:22. No deviation while off. On, the people would differ by 1 level in up to 3.8% of a frame's pixels (stagec_notes.md 2.9), and it saved no time on your PSP (gl15 run 1) [C].

**B6. The deck's shadow marks drawn with their floor piece's own corner points (arm X with subset pairing; the lead's choice, 2026-11-20). In the walk since gl29x/gl30; on the whole run from gl31.**
- What: the game draws each shadow mark (blend 0x42) as a copy of its floor piece raised by 5 model units. The PSP's GE dropped that copy in patches in its depth test: the black flickering marks on the deck. We draw the mark from the floor piece's own corner points and strip order (or, for a mark that covers part of its piece, its own strips moved onto the piece's points), so it passes the depth test wherever the floor shows [C: gl29, gl29x and gl30 runs: no hole patch left].
- Impact: each shadow sits 5 model units lower, under 1 px on screen; on the emulator the pictures change only inside the marks' own area, by a few colour levels at most [C: gl30 and gl31 G3]. Marks with no matching floor piece are drawn as before (192 of 224 in the opening part before the deck; all of w01a's after the deck, not yet in a run) [C: gl31 census].
- Undo: MGS2_GL_DZ unset (the holes come back on the PSP).

**B7. The people lit, skinned and mapped by the GE (C2, `OPT2_C2`; the lead's choice, 2026-11-24). On in every turn from gl42 (gl41 keeps it off: its turns compare with gl40's).**
- What: the people's vertices are lit and skinned the PS2's way, but the final lighting and mapping are done by the PSP's GE (mapping (b), stagec_notes.md step 2). The native renderer draws the people only in this form (renderer step 2, main/attic/ge_native/step2_results.md).
- Impact: up to 3.8 % of a picture's pixels differ from today's people path, 99.3 % of them by 1 colour level, edge pixels by up to 15; dark frames are equal [C: master emulator, indoor, 2026-11-02; shots in SHOTS\c2_people]. Game state is unchanged (hashes equal). The PSP's GE draws it exactly as the master emulator does [C: gl15 run].
- Why now: your card of 2026-11-02 23:22 said "keep it off" and retest it after the picture moved to the ME. The picture is on the ME now, and the native people need C2.
- Undo: clear bit 32 of MGS2_OPT2 (the people go back to today's path, not native).

## C. Picture gaps not fixed yet (defects, not choices)

- **C1. Effects the default picture does not draw** (C4 off): one sprite program, P04+3740 (10-17% of the deck's shown pixels; 38-41% on the aft deck in the rain, most of it the rain's own sprites, which A2 leaves out anyway); P04+780 (up to 1.9%); small turned squares (up to 1.4%); the sea (up to 3.8% on the aft deck); the radar map's lines (0.3-0.5%) [C: PS2 frames with each kind left out, stagec_notes.md 4.2]. Fix: C4 (B4), then Stage C step 5 (the sea, the 2D and HUD, the radar).
- **C2. The opening cutscene:** black on the PSP; at vb 8,774 a plain grey where the PS2 shows the scene in the rain (renderer_notes.md §11.18, stagec_notes.md 4.6) [C].
- **C3. #84:** the codec subtitle page's stale text and a faint line at row 220.
- **C4. #68:** the radar map, the viewfinder and one blend (0x6a) have no rule yet; the trail frames' bottom rows (B2).
- **C5. Textures converted after the game refilled their source:** 718,144 of 13.6 billion texels on the deck route (a 4-bit 512 x 256 texture on the title, the opening's text, the codec text page) (roadmap.md "Open picture gaps") [C].
- **C6. Frame 15,761:** 52 pixels in rows 70-74 differ from the PPSSPP master (the two emulators disagree there too) (gl16 run 1) [C].
- **C7. The ME's walk one step later (gl25, the lead's choice 2026-11-19; in the two-area ME turns only, `MGS2_ME_PIC_TWO`).** The walk turns a drawing step's packets into the GE's frame and reads textures left in game memory as it goes. From gl25 it may run until the second hook after its own (gl24: the next hook), so the main CPU runs the next drawing step meanwhile. The emulator check lchk4 (main/attic/lchk_gl24d/README.md) found that memory unchanged for two hooks on the whole route except at vb 20,692 (the talks, 18,896 bytes at 011a09f0) and vb 26,008 (the deck's start, 4 x 32 bytes at 00c04140) [C, emulator]. In those frames the PSP may draw a texture with bytes the game wrote one step later, the same kind of gap as C5, and only when the ME is still walking; no game state changes (hashes equal). Copying those two areas instead was tried (gl25b) and changed the picture against the x86 twin from vb 10,516 on, so it was dropped. Game state and the 9 shots stay as gl24: confirmed on the PSP (gl25 run 2026-11-19: 372 hashes, 9 shots byte-equal) [C]. Gain: the deck's two-area turns -16.7%, the rain's -22.7% [C].

On your PSP (gl16 run 1), 11 of the 16 shots equal the PPSSPP master byte for byte; the other five are C4 and C6 [C].

## D. Sound, pacing and input: not built yet

- **D1. No sound in the picture builds (gl1-gl18).** They run with the Media Engine off, and so mix no sound [C: psp_harness.md]. The IOP and the game's sound requests run as on the PS2 (their state is in the hashes). The ME's sound mix passed 3 exact runs in v30me6c; bringing it into the picture builds is roadmap.md Next 3 (task #129).
- **D2. No pacing.** The timing builds run each step as fast as the PSP can, and the game slows down with them (about 14% of full speed on the rainy deck today) [C: the game has no time step, logic_timing.md §1]. A playable build needs roadmap.md Next 4.
- **D3. Input.** The harness plays a recorded pad file (pad.txt); your own pad comes with the playable Tanker deck (roadmap.md Later).

## E. Game logic against a real PS2

The game makes the same decisions on the PC, both emulators and your PSP in every build (state hashes equal, every build since v13) [C]. Against a real PS2 it can differ in these ways:

- **E1. VU0 vector math rounds to nearest.** The PSP's vector unit (VFPU) does the game's VU0 vector math, with a NaN / Inf check and the exact form as its fallback. It rounds to nearest where the PS2 rounds toward zero, so results can differ in the last bit [C: speed_notes.md §22]. Your OK: card 2026-10-20 00:39.
  - **Impact: gameplay** [C: x86 twin, and the logic thread's per-frame compare with PCSX2; route i in w01a]. E1 changes the guards' random numbers. Each stage has one random seed, shared by the guards and the effect actors (NOTES.md section 4). With the PS2's rounding (dev switch `MGS2_VFPU_RZ=1`) the twin's seed stays equal to PCSX2's 190 frames longer, to vb 37,498. With ours it leaves at vb 37,305, 100 s after the route's first walk input (vb 32,300): crtrg.c (w01a 0x3551c8) steps the seed at 37,305 with the PS2's rounding and first at 37,307 with ours, and Snake's y and z differ by 1-2 ulps in that window (prof/hr2_route_i/window_37300_37520_rn_rz.txt). The guards start drawing from the seed at vb 37,498 (SIG_BasicShoot and the other guard code), so with the same pad input they make different random choices, for example when they fire. A second split from PCSX2 at vb 37,498-37,499 happens with both roundings, so it is not E1; its cause is being found (tasks #174-#175).
  - Measured [C, x86]: with every float operation of the game rounded to nearest (a stronger change), the deck route to vb 25,200 made every disc read at the same vblank and kept the same 170 actors; the player's and guards' positions differed by under 0.1 units, not growing over 1,800 steps (speed_notes.md §22, "What a different float rounding does").
  - Speed: against the exact toward-zero forms, the VFPU forms save about 9.3-10.4 model ms a rainy deck step (~8.6-9.7 PSP [I]): 2.35-3.22 in the game logic, the rest in the draw pipeline, 6.2 of it the box cull's micro programs. Inside: 4.2-4.8 model ms, 1.3-1.8 in the logic [C model: census of today's tree, 2026-11-08, task #173; 3.1 ms at v10].
  - Under the charter's "original game logic": kept, by your card of 2026-11-07 21:22 ("Keep it"). Reopened 2026-11-08 01:47 with the impact above, and superseded for the logic by your card of 2026-11-08 01:54 ("Game logic exact"): the game logic's VU0 vector math will round toward zero as on the PS2 (about +2.3-3.2 PSP ms a deck step [I]), while the draw pipeline and the box cull keep the VFPU forms, provided a PC proof shows their VU0 results never reach the game state (task #176; if it fails, the choice goes back to you). **The proof failed** (2026-11-08 05:30, `b_proof_notes.md`) [C: x86 twin, route i]: the box cull's three micro programs and most draw functions put their rounding into values game code stores or reads, among them GM_ActControl's x, level heights and the block bounds. Only seven drawing functions pass (the shadow chain, DG_SetObjsMatrix, StoreMatrixObjs), and keeping only them on the VFPU saves about 0.3-0.4 PSP ms of the full cost [I]. So the choice is back with you, on the logic thread's card of 2026-11-08 05:31 ("Make all vector math exact at about 9 ms a deck step?"): Everything exact (recommended; about +8.6-9.7 PSP ms a rainy deck step, about +4 inside [I]), Keep as now, or Partly exact (the logic exact, about +2.3-3.2, the drawing math still leaking into game values). You answered at 17:46 (next line). gl18, gl18me and the earlier builds have E1 as described above.
  - **LEAD DECISION 2026-11-18 (Ffm gave the lead 20:10): E1 ACCEPTED, the VFPU stays.** "Everything exact" would cost about 9 PSP ms a rainy deck step [I], about half of the 20 ms a step full speed needs; the difference is the last bit of vector math and changes only which random choices the guards make later, not how the game plays. gl18e1 is not shipped; task #178 closed; the PCSX2 comparison (#177, #183) leaves the critical path. `plan_deck_25fps.md` §2.
  - **SUPERSEDED by the lead decision of 2026-11-18 above (history only; the VFPU stays, and the reference for every exact lever is today's VFPU-rounded twin).** Your choice 2026-11-08 17:46: "Everything exact" (the logic thread's card of 05:31): all VU0 vector math will round toward zero as on the PS2, the macro-op groups and the box cull's micro programs included (about +8.6-9.7 PSP ms a rainy deck step, about +4 inside [I]). It goes into its own build after gl18me, checked against the all-toward-zero PC twin (`MGS2_VFPU_RZ=1`) with new hash references (task #178). **E1 is resolved when that build ships with hashes equal to the twin's.** A possible exact speed-up for later: the box cull's micro program on the VFPU with its exact leftovers computed only when game code reads them (up to about 4.7 PSP ms a deck step [I]; UNKNOWN until designed and proven, `b_proof_notes.md`).
- **E2. FPU denormals follow the PSP's rule** (denormal operands used exactly), where the PS2 treats them as zero. Your card 2026-10-23 12:49, "Keep PSP rule".
  - Impact: none found. A PC build with the PS2's rule equals ours in everything the game keeps or sends, on the two routes checked (speed_notes.md §40) [C].
  - Standing check: every new route also runs with the PS2's rule on the PC.
- **E3. Time.** The port gives the game a PS2 that is never late.
  - The virtual clock adds 64 cycles at each loop back-edge of the game's code, not the PS2's real cycle count [C: boot.c IRQ_POLLF; speed_notes.md §3].
  - The draw pipeline takes no game time (your "A", 2026-10-21).
  - On our routes every step sees one vblank, two in the cutscenes that ask for it [C: logic_timing.md §1].
  - The game has no time step, so the frame rate sets its speed (logic_timing.md) [C]. On a real PS2 a step that runs late adds 2 to the elapsed word, and the play time counts it.
  - Impact against a real PS2: UNKNOWN (the play time where a PS2 runs late; anything the game paces inside a step). How we find out: the PCSX2 replay test in logic_timing.md §3, which needs a BIOS dumped from your PS2 and your OK for a session on your PC.
- **E4. The PS2's hardware is our model.** What the game sees of the graphics chip's registers and interrupts, of the DMA and of the IOP (disc, pad, sound requests) comes from our models, checked against our own reference only [C]. Against a real PS2: UNKNOWN, the same test as E3.
- **E5. Exact skips (not deviations).** They leave out work whose result nothing uses:
  - the reduced draw pipeline on unshown steps (since v21);
  - the rain's unseen work, G0H, H3, NBS and G0C (your card 2026-11-03 22:03, "Exact skip"), and H3R and G0R: all in gl18, taking turns (H3 and H3R alone in one turn, all seven bits in the other; psp_harness.md "Build gl18");
  - H1 (on hold: not exact as built, h1_notes.md).

  Proof for each: every load the game makes is at the same address in every vblank, with the same bytes except leftovers no code acts on, and the virtual time, the sound state and every picture digest are equal (speed_notes.md §34; rain_study.md §6-§10) [C]. The RAM differs only in the draw pipeline's buffers and the skipped actors' own words.
  - The rain ones are exact only while A1 and A2 stand: they skip what feeds only the streaks and the rain's sprites. Bringing either back means turning its skips off.

## F. Proposed, not adopted (each would be a deviation; your OK first)

**Your rule** (cards 2026-11-07 21:17, "Cut cosmetic effects only", and 2026-11-08 00:09, "Drawing-side cuts only"; logic_timing.md §2):
- Where the original logic cannot reach 25 fps, weather and effects may be lightened in what we draw only: drawing-side filters and exact skips, which keep every state hash equal.
- The effect actors run in the game as on PS2. They share each stage's random seed with the guards, so running one less often would change the guards' later choices (NOTES.md section 4).
- Each cut comes with pictures and numbers, gets your OK first, and gets its entry here.
- Gameplay logic (AI, physics, timers, input) stays original. Half-rate logic is out.

- **H2:** cosmetic actors (weather, effects) stepped every other step (halfrate_study.md §5): out (your card, 2026-11-08 00:09; it changes the guards' random numbers).
- **A':** a picture skip driven by time, showing 1 step in 3 when late (logic_timing.md §2). Display only; not asked.
- **Half-rate logic with scaled constants** (logic_timing.md §2, B): out (your card, 21:17).
- **The deck's content cuts** (deck_costs.md §4.3): the rain off or fewer drops the game's own way, ropes, lamps or effects every other step, a simpler sea: out (your card, 2026-11-08 00:09), since each changes the game. **Reopened under the lead (2026-11-18): see G1.** A shorter draw distance or fewer drops drawn would have to be drawing-side cuts, each with pictures and your OK.
- **Other:** 16-bit frame buffers (renderer_notes.md §11.7); the trail and blur removed (your OK if too heavy; not used).

## G. The lead's game-side cuts (Ffm 2026-11-18 20:10: "downscale what is needed for a psp port"; logged here, told to you)

- **G1. Cosmetic actors at half rate** (`MGS2_HALF`, main/ tools/patch_half.py, boot.c `rt_half_skip`; plan_deck_25fps.md Phase 1). The ropes (ropemain, ropemain2: on the deck they also move Snake's headband tails) and the sea (wave, wave2) act on even game steps only, the lamps (fade_lgt, c_light, bonbori) on odd steps only, the trail effect (afterimage) never. **Status: ON from gl26 (2026-11-19, lead decision after gl25's run): all but the sea (cut b) in every step from vb 24,000; the x86 twin with the same cut is gl26's hashref (72e0ca4c).** gl19-gl25 ran it as a test in turns (from vb 24,000: off, off, all four, all four, off, off, all but the sea, all but the sea). **Impact:** they share the stage's random sequence with the guards, so from the first cut step the guards' later random choices differ from the PS2's (the x86 twin with the same cuts stays the bug detector); on screen in the PC's shots: the headband tails move at 25 Hz (3-45 pixels on alternate frames), the lamps and the trail showed no change, the sea's cut changed the rain's drop pattern (the sea itself was not in view). **Saving [C model]:** all but the sea 3.55 ms a step on the deck waiting, 4.81 in the rain; the sea's cut costs 3.44 more in the rain (the falling splashes run 11 times as much, cause UNKNOWN), so the sea is likely left out. The PSP figures come from gl19's run. **Correction (2026-11-23, seed census on the x86 twin, mgs2/hd_study/hd_study_2026-11-23.md §1) [C twin]:** on route i in w00a and d01t the half-rate actors (ropes, lamps, bonbori, c_light) take no draws from the shared random sequence, so there the guards' random choices stay the PS2's; the ropes' wind helper draws only for a rope point inside a local wind box, which happens in w01a. The game state still differs from the PS2's (the cut actors' own memory), so gl26's hashref stands.

- **G2. The aft deck's splashes and ripples without their pictures, the logic kept to the PS2's random numbers (Ffm's rain option "B", 2026-11-22 13:03/13:05; `OPT2_B43`, MGS2_OPT2 bit 21 = 2097152; t43/boot/native_b43.{h,c}, build_gl43/tools/b43_apply.py; gl43 tests it in its own turns, attic/gl43).**
  - **What (game side):** while the bit is set, four of the deck's rain functions run as a short native form in place of their translation. These are the floor splash's update and spawn (0x318418, 0x317e88) and the falling splash's update and dead step (0x319600, 0x319078); the falling splash's spawn (0x3190c8) stays translated. Each form keeps only what other code can see:
    - the stage seed 0x1fc954, advanced by exactly the translation's draw count (the floor spawn draws 1 + 96);
    - the slots' lives and counters (word 0, word 4, the float life with its clamp at 0);
    - the floor spawn's three 16-byte copies into the splash work vectors (*(0x1f85c8/cc/d0));
    - the matrix call P125a78;
    - the game time: the translation's interrupt polls × 64 added to the clock.

    A form runs only when no interrupt is due inside it (NOW + P × 64 < k_poll_at). Otherwise, or when a guard holds (word 4 = 0, actor+76 = 0), the translation runs. The interrupts therefore land on the same game instructions as on the PS2.
  - **What (picture side):** the floor splash prims (0x318808), falling splash prims (0x319aec) and ripple rings (0x3321f4, 0x332214, 0x332494, 0x3324a4, 0x3324b4) are hidden (their +164 word gets bits 12-13). In the same turns MGS2_BOUND_CYCLE 4 (BD_RAIN) leaves out the rain's other effect calls: the far drop sheets and the foot splash. MGS2_GL_RAIN_NEAR=1 keeps the nearest of the 4 drop-sheet calls (d_splash_camera's last). The streaks are H3R's and stay as they are.
  - **Why this form:** this is the exact form (rain_options s5.3), not DICE + NP. DICE alone changes the guards' random numbers on the aft deck (rain_options s5.2). In the exact form every draw and every poll is the translation's, so the guards' draws, the route and the stage loads are the reference's. B can also switch on and off per turn.
  - **Impact [C twin, x86 twin of gl43, route i to vb 37,800]:**
    - **Seed and route.** The seed log is identical with B on in its turns (sch), B on in every turn from vb 29,000 (bon), and B never on (off). It covers every act's seeds 0x1fc954 / 0x1fbd74 / 0x1ff554 and the game time at entry, 12,699 lines (md5 b27073b998d1). It also covers 65,070 seed changes with the drawing act, including the guards (watcher.c 187, attacker.c 221; md5 99a5a9c90b85).
    - **IOP and pictures after the deck.** IOP and frame-buffer hashes are equal at all 684 points. Pictures after the deck (3,072 content and digest lines) are equal.
    - **Check mode.** In check mode every B call ran translated and was compared with the form's prediction: floor spawn 964, floor update 11,036, fall update 9,560, fall dead 45,704. 0 differed in time, seed, lives or copies.
    - **Read hash.** Every load the game makes outside the draw pipeline, hashed per vblank, is equal in 37,795 of 37,799 vblanks. The 4 others are t69 (vb 30,928/929/932/933): 16 loads of an 8-byte unaligned copy (GLDU) at scratchpad 0x70003f88. Its low word is freshly written (S_w00a_P2e59b0); its high word 0x70003f8c is a leftover from the translated splash code (0 with B). No later load sees a different value.
    - **State hash.** The EE state hash differs from vb 29,100 by design: the slots' particles, the prims' vertex and position buffers, the actors' own blocks and the packet areas. gl43's hashref is its own.
  - **Impact [C emu]:** the PSP build on the emulator gives 62/62 hashes = gl43's hashref. Its B-turn counters equal the twin's; the B digest turn (t73) has content = the twin's.
  - **What you see in a B turn:** the deck's rain has only its streaks and the nearest drop sheet. The splashes on the floor, the falling splashes, the ripple rings, the far drop sheets and the foot splash are MISSING ON PURPOSE. Right after a B turn, the splashes that started during it may stay hidden until they end [I]. On the emulator, against a run without B, the first 6 to 16 pictures after each B turn differ (up to about 30 vblanks), and one at vb 31,062; then the pictures are equal [C emu, fh1/fh0]. In the twin's t55, two turns after a B turn, the picture content is equal and only the list digests differ: leftover vertex bytes of splash slots that are not shown.
  - **Undo:** clear bit 21 of MGS2_OPT2 / the MGS2_OPT2_CYCLE words (ffdff9ff = B off). Also drop bound kind 4 from MGS2_BOUND_CYCLE and MGS2_GL_RAIN_NEAR.
  - **Status:** gl43 = a measurement build (B in turns t51, t53, t57, t59, t62, t65, t69, t72, t73). Not on by default.
