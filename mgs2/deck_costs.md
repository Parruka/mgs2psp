# The rainy deck: what costs the most, and what we can cut

2026-11-03, task #160. Your question (16:32): "what cost more ? what options we have to cut/downgrade?"
[C] measured, [I] inferred. ms = milliseconds per 50 Hz game step. The port shows every other step, so 25 fps = 20 ms a step and 20 fps = 25 ms a step (roadmap).

## Short answer

- **A step on the rainy aft deck costs 144 ms on your PSP (gl16)** [C]. Half is the game's own code (logic 42, its draw pipeline 29), half is our picture work (GS walk 31, picture 39). Today's effects plan for gl17 adds about 13 ms more [I].
- **The largest single things** [C model]: the people's picture (Snake and the guards: 20 ms in the model, about 17 on your PSP [I]); the weather (logic 9.8, picture 6.7 today and 10.1 more with the new effects); the ship's meshes (picture 9.8, 15.0 while Snake waits); the guards' logic 6.5, Snake's 5.4, the ropes 4.4, the lamps 3.5.
- **The draw pipeline and the GS walk are mostly shared work**, paid per object and per byte for everything on screen: VU0 programs 7.0, per-object matrices and tests 6.2, unpacking the data 11.3, texture and palette uploads 7.5 (model). The effects' own depth sort is 5.1 of the pipeline.
- **Of today's effects plan (+16.4 ms in the model: picture 14.0, walk 2.4), 10.1 is the rain's own effects**: its streaks 4.6 (rain.c) and sprites 5.5 that are the rain's by their owners' file names and because they run only where it rains [I]. The lamps' glows are only 2.3.
- **For 25 fps, the deck needs content cut as well** [I]: after all the exact work on the roadmap, the deck's logic alone would still be about 30 ms a step. The weather is the one big part of the logic that is not gameplay.
- **My recommendation:** for gl17, draw the new effects without the rain's (lamps' glows and smoke yes, the rain's sprites and streaks no, drop sheets whole): about +4 ms in the model instead of +16.4, about 3 on your PSP instead of 13 [I]. **Update 18:14:** you chose no streaks (17:38); the sprites' filter is built and measured: +5.0 ms in the model over gl16 instead of +10.3 with them, about 4 on your PSP instead of 8 [I] (stagec_notes 4.13); pictures in `SHOTS\c4_effects\rain_sprites`; **you chose to leave them out (card, 18:19)**. Then study the game-side rain cut on the PC (rain off, or fewer drops), measured in full and with pictures, so you choose with numbers.

## 1. A step on your PSP, by part [C]

gl16 run 1 (`prof/hw_gl16/run1`), everything on, the three 100-step windows that hold the model windows of section 2. gl16 does not draw the new effects (C4) yet.

| Window | Step | Game logic | PS2 draw pipeline | IOP + sound | GS walk | Picture |
|---|---|---|---|---|---|---|
| Deck, Snake waiting (vb 29,801-29,900) | 149.1 | 41.4 | 28.7 | 2.4 | 32.8 | 43.9 |
| Aft deck in the rain (vb 30,801-30,900) | 144.4 | 41.8 | 29.5 | 2.5 | 31.3 | 39.4 |
| Inside (vb 33,401-33,500) | 90.8 | 20.4 | 13.1 | 2.0 | 22.2 | 33.1 |

- Today's effects plan for gl17 (the new effects on, the rain's streaks at 1/2, sheets at 1/2) adds about 13 ms on the rainy deck [I: the model's +14.0 picture and +2.4 walk, scaled by each part's PSP / model ratio].

## 2. What costs the most inside each part

From a new model build that names the work as it runs (section 6). Against your PSP in the rainy window, the model reads the logic 2% high, the draw pipeline 11%, our picture 20% and our walk 38% (the model build has no PGO). So the PSP figures given for our code are the model's scaled by those ratios [I].

### 2.1 The game's logic, by actor [C model; the rainy window is 42.7 ms in the model, 41.8 on your PSP]

| Group | Deck waiting | Aft deck, rain | Inside | The largest (aft deck, rain) |
|---|---|---|---|---|
| Weather: rain, drops, splashes, sea, wind | 11.3 | 9.8 | 0.5 | line_sph 2.37, rain 1.42, body_sph 1.34, wave2 (the sea [I]) 1.31, charaspread 1.09, d_splash_fall 1.01, d_splash_camera (the drop sheets) 0.42 |
| Guards | 6.1 | 6.5 | 2.1 | watcher 6.35 |
| Snake | 5.0 | 5.4 | 4.6 | raiden 3.60, pl_arm 1.83 |
| Ropes | 4.6 | 4.4 | 3.5 | ropemain 3.96, ropemain2 0.46 |
| Lamps | 1.1 | 3.5 | 0.1 | fade_lgt 2.93, c_light 0.46 |
| Doors | 1.3 | 1.3 | 1.0 | wt_door |
| Trail | 0.5 | 0.5 | 0 | afterimage |
| Everything else | 11.2 | 11.3 | 6.4 | putmodel 1.12, gvd_Act 1.02, put_vanime 0.90, sling 0.62, radar 0.59, item_box 0.55, bgmfader 0.48, sprite_dma 0.48, the end-of-frame wait 0.48 |
| **Total** | 41.2 | 42.7 | 18.1 | |

### 2.2 Our picture, by kind and owner [C model; aft deck in the rain, effects as today's plan]

| What | Model | PSP [I] | The largest |
|---|---|---|---|
| The people (Snake and the guards, P06, all passes) | 20.1 | 16.7 | 188 calls, 4,500 vertices a step (stagec_notes 2.2) |
| The ship's meshes (df0) | 9.8 | 8.1 | 15.0 in the model while Snake waits |
| The frame's own work | 7.0 | 5.8 | the GE list 2.8, texture uploads 1.2, timing 1.2, compares (memcmp) 0.8, the calls' wrapper 0.7 |
| Effects gl16 draws already: the weather | 6.7 | 5.6 | d_splash_fall 5.72, d_splash_camera (sheets at 1/2) 0.82, ft_splsh 0.11 |
| Effects gl16 draws already: the rest | 4.9 | 4.1 | afterimage 1.40, blood_wl 1.27, sky_column 0.53, sling 0.34, fog_wall 0.28, set_tex 0.25 |
| New effects (C4): the rain's [I: the sprites' owners, see below] | 10.1 | 8.4 | streaks (rain.c, at 1/2 by strips) 4.63; sprites charaspread 2.27, line_sph 1.59, body_sph 1.44, rain_gas_pers 0.16 |
| New effects (C4): the lamps' glows | 2.3 | 1.9 | fade_lgt 1.35, bonbori_red / yw 0.67, c_light 0.28 |
| New effects (C4): the rest | 0.6 | 0.5 | item_box 0.22, chim_smo (smoke) 0.17, set_t_dr 0.12 |

- With the new effects off this window's picture is 47.4 in the model (39.4 on your PSP); today's plan adds 14.0 to it and 2.4 to the walk (decoding the new calls).
- Stagec_notes 4.12 called the sprites "glows, smoke, haze" from the census of whole routes. On the rainy deck, most of them are the rain's own [I]. The streaks' owners are certain (rain.c, rain_slow.c). The four sprite owners are the rain's by inference: their file names, the random seed they share with rain.c (speed_notes §15), and that they run only on the deck in the rain, none of them inside. Pictures with them left out (the next step) will show what they draw.
- Inside, the picture is 39.4 in the model: the people 16.4, the ship 14.0, the frame 5.8, the effects 3.1.

### 2.3 The draw pipeline and the GS walk, by job [C model; aft deck in the rain, effects on]

| PS2 draw pipeline (32.8 in the model, 29.5 on your PSP) | Model | | GS walk (45.3 in the model, 31.3 on your PSP) | Model |
|---|---|---|---|---|
| VU0 micro programs it runs | 7.0 | | Unpacking the VIF data | 11.3 |
| DMA kicks, copies, stores | 6.6 | | The DMA chain walk | 7.7 |
| Per-object matrices and screen tests | 6.2 | | Texture and palette uploads | 7.5 |
| The effects' depth sort (the sort pass) | 5.1 | | GS packets, quadword by quadword | 4.8 |
| The ship's meshes (chain, patch) | 3.8 | | GS register front end | 4.1 |
| The people (chain2) | 1.8 | | The translator's calls | 4.0 |
| Interrupts, polls, the rest | 2.1 | | The lanes: ship 1.0, people 1.0, effects 1.1 | 3.0 |
| | | | Timing hooks, the rest | 2.9 |

- Names of the pipeline's functions are the reference's leads (our function-name lists): libdg/screen, prim2, chain, chain2, patch.
- Only the depth sort (5.1) and the three lanes (3.0) belong to one kind. The rest grows with the number of objects and bytes on screen, so every cut of content saves some of it, but this build cannot say how much per kind [I].

## 3. What it means for 25 fps on the rainy deck [I]

- The roadmap's exact work moves or removes most of our picture work: the picture on the Media Engine (Next 1), native drawing that replaces the PS2 draw pipeline and the walk on shown steps (Now, Stage C).
- What it cannot remove is the game's logic: 41.8 ms here. The logic work on the roadmap (native forms of the weather and the ropes, 5-8 ms; render-only actors skipping the unshown step, about 4) would leave about 30 ms (speed_notes §48 and §22; roadmap "What bounds the rainy deck").
- That is above both 20 ms (25 fps) and 25 ms (20 fps at 80% game speed). So the rainy deck needs at least 10 ms less logic a step for 25 fps, and at least 5 for 20 fps, beyond the exact work: "at least" because the drawing needs some of the step too.
- **2026-11-08 00:09, your card "Drawing-side cuts only":** the weather and effect logic can no longer be cut on the game side (the effect actors share the stage's random seed with the guards). So that logic saving has to come from exact work (4.1 E3, E4 only where every state hash stays equal, H1, and the logic work), and G1-G7 below are out.
- The weather is 9.8 ms of logic here (8.2 without the sea and the wind), the ropes 4.4, the lamps 3.5. The guards and Snake are gameplay.
- The GE needs about 40 ms for a deck frame (roadmap), and 25 fps allows 40. The new effects add GE fill that the model does not see; gl17's GE wait will show it.

## 4. The options

### 4.1 Exact (the game and the picture unchanged), already on the roadmap, for scale

| | Option | Saves (rainy deck) | Where it stands |
|---|---|---|---|
| E1 | The picture on the Media Engine | the picture part (39 ms, about 51 with today's effects [I]) off the main CPU [I] | Next 1; the eDRAM probe gl16ep is in your folder, waiting for its run |
| E2 | Native drawing per kind, then the game's packet builders skipped on shown steps | 38-57 ms a step on the deck in the model for the study's five steps, about 7 of it already in (gl13's texture lookups 5.3, gl16's trims 1.8 on your PSP) [I: native_draw_study.md §3] | Now, Stage C (steps 4-6) |
| E3 | Native forms of the weather and the ropes | logic 5-8 [I, speed_notes §48] | Next 2 |
| E4 | Render-only actors skip the unshown step, the random seed carried on | logic about 4 on average [I, speed_notes §22] | Next 2 |

### 4.2 Picture only (the game stays exact; only what the PSP draws changes)

| | Option | Saves | What you would see | Where it stands |
|---|---|---|---|---|
| P1 | The rain's streaks left out | 4.6 model [C] (4.8 in stagec_notes 4.12), about 4 PSP [I] | no streaks, as gl16 today | built; **you chose it for gl17 (card, 17:38)** |
| P2 | The rain's drop sprites left out (charaspread, line_sph, body_sph, rain_gas_pers) | **5.3 model measured** [C] (their conversions 2.8, decode 1.3, lists, batches, textures, copies 1.3; the filter's scan +0.1), about 4.3 PSP [I] | no spray on clothes, no haze on the deck and in the opening, no drops on Snake inside after the door; as gl16 today | **built and checked exact** (stagec_notes 4.13; cfg `MGS2_GL_RAIN_SPRITES=0`); pictures in `SHOTS\c4_effects\rain_sprites`; **you chose it for gl17 (card, 18:19)** |
| P3 | The lamps' glows left out (fade_lgt, bonbori, c_light) | 2.3 model, about 1.9 PSP [I] | lamps without glow, as gl16 | the same filter |
| P4 | The falling splashes (d_splash_fall) at 1 call in 2 | about 2.9 model [I: half of its 5.7] | fewer splashes | new: calls 1 in N, as the streaks' |
| P5 | The trail (afterimage) not drawn | 1.4 model, about 1.2 PSP [I] | no trail (you OKed removing it if too heavy) | new, small |
| P6 | The people's passes 2 and 3 left out | not measured (the people are 20.1 model in all) | less shine on Snake and the guards | needs a measurement |

### 4.3 Game side (the game itself changes, the same way on the PC and your PSP; new hash references)

Out since your card of 2026-11-08 00:09 ("Drawing-side cuts only"), except G0, which is exact. Kept here for the record.

| | Option | Saves | What you would see | What it needs |
|---|---|---|---|---|
| G0 | The rain's four sprite actors (line_sph, body_sph, charaspread, rain_gas_pers), which gl17 no longer draws, skipped exactly: stubs that keep the seed, the game clock and what other code reads | about 5-6 ms a step on your PSP [I]; destroying them (the upper bound) measured 8.4 model, about 7.6 PSP [I] (`rain_study.md` §4) | nothing: the same game and picture as gl17 | **your pick (card, 2026-11-03 22:03). On the PC (`rain_study.md` §6-§7): line_sph's work skipped and all four's prims hidden, exact; 4.99 model ms a step less on the rainy deck, about 4.3 on your PSP [I]; body_sph's and charaspread's own work next.** Destroying them instead plays differently after the deck (§2) |
| G1 | The rain off on the deck (rain, drops, splashes; the sea and wind kept) | logic 8.2 and picture 6.7 [C model], the rain's new effects if drawn (10.1), and part of the 5.1 ms depth sort and of the walk [I] | no rain | a PC study: how to switch the rain actors off the game's own way, and what else reads them (they share the random seed with item_box and the smoke, speed_notes §15) |
| G2 | Fewer drops and splashes (1/2 or 1/3 of each rain actor) | about that share of G1 [I] | lighter rain | the same study |
| G3 | The ropes moved every other step | about 2.2 [I] | the ropes swing in coarser steps | a PC study |
| G4 | The lamps (fade_lgt) updated every other step | about 1.5 [I] | the lamps fade in coarser steps | a PC study |
| G5 | The trail removed in the game | logic 0.5, picture 1.4, part of the pipeline [I] | no trail | OKed in principle |
| G6 | A shorter draw distance | not measured (the ship: picture 9.8-15.0, pipeline 3.8) | far parts of the ship pop in | a PC study |
| G7 | A simpler or calmer sea (wave2 [I]) | logic up to 1.3 [C model]; its drawing is not split out in this build | the sea moves less | a PC study |

Pacing (no content cut) stays as the roadmap has it (Next 4: a frame skip near the budget, or 20 fps at 80% game speed), your OK.

## 5. My recommendation

1. **For gl17, the new effects without the rain's** (P1 + P2, with the drop sheets whole as you asked at 13:27): the lamps' glows, the smoke and the small effects drawn as the PS2 does, the rain as gl16 shows it now. About +4 ms in the model over gl16 instead of +16.4, about 3 on your PSP instead of 13 [I] (the sheets whole are 0.26 of it, plus GE fill the model does not see), so most of what step 4 adds stays affordable. It needs a small filter by owner (x86 first, exact checks, then pictures for you before gl17 ships). Your open card on the streaks still applies; with this plan "No streaks" fits.
   **Where it stands (18:14):** you chose "No streaks" (17:38). The filter by owner is built and exact (every state hash equal on the PC and in the emulator; it leaves out every call of the four and no other's). Measured in the model on the rainy aft deck (sb17m3, vb 30,810-30,890): gl16 165.43 ms; your choice 175.68 (+10.25); your choice with the rain's sprites left out 170.42 (+4.99); about +8 and +4 on your PSP [I]. Pictures: `SHOTS\c4_effects\rain_sprites` (README there). **You chose "Leave them out" (18:19): gl17 draws the new effects without the rain's streaks and sprites, the drop sheets whole.**
2. **Then the game-side rain study (G1, G2)** on the PC: switch the rain actors off or thin them the game's own way, measure the full saving (logic, pipeline, walk, picture), and make pictures. It is the largest content lever on the deck and the only one big enough for 25 fps there [I].
   **Where it stands (20:30, `rain_study.md`):** switching rain actors off the game's own way changes the game after the deck, because they share the stage's random seed and the game clock (§2: on our route a guard is hit inside). The four sprite actors gl17 no longer draws cost at most 8.4 ms a step in the model, about 7.6 on your PSP [I]; skipping their work exactly (G0) would save about 5-6 [I] with the same game and picture. Your call on the card: G0 (recommended), off in play builds only, or leave it. **(22:25) You chose G0 (card, 22:03). line_sph is done on the PC (`rain_study.md` §6): exact, and 3.27 model ms a step less on the rainy aft deck, about 2.9 on your PSP [I]; body_sph, charaspread and rain_gas_pers come next.** **(23:25) The other three's prims are hidden exactly too (H3, `rain_study.md` §7): with line_sph's G0H, 4.99 model ms a step less, about 4.3 on your PSP [I], and nearly all of the kill's pipeline, walk and picture saving; what is left is logic. Next: body_sph's and charaspread's own work.**
3. **Exact work stays first** (E1-E4): it is where most of the 144 ms goes. The picture on the ME (gl16ep's run) comes next on the roadmap.

## 6. How it was measured

- **Build** `/home/claude/sb17km/d` (EBOOT 13cf3b0b, ephemeral) = the gl17 tree sb17 with three hooks, no PGO (`tools/dev/sb17k/`: `sb17k.diff`, `fxbk.c`, `fxbk.h`, `mkpsp17k.sh`). PspProf (PPSSPP 1.20.4 with our patches, PPSSPP_VFPU_RN=1) adds each instruction's cost to the bucket named by the word `g_emuactor` (PSPPROF_BUCKET):
  - the act function of the actor the scheduler runs (GEN_ACTOR_PROF, boot/dispatch.h);
  - a timed region (IOP, GS walk, draw pipeline, picture: psp/main_psp.c HOST_BUCKET);
  - a converter call, by its entry and owner (ge/gelive.c FX_BUCKET).
- **Owners.** The owner of an effect call is the actor whose own block points at the allocator block holding the call's data, as in the x86 census (stagec_notes 4.9). Actors are named by their file name at the end of the run.
- **The look-up's own cost** (19.4 ms a step with the effects on) has its own bucket and is left out of every figure. The effects-off total, 168.66, is within 0.7 ms of the same run without hooks (4.12's sb17m, 167.97).
- **Windows** (indoor route, 80 steps each):
  - deck waiting, vb 29,810-29,890, effects on;
  - aft deck in the rain, vb 30,810-30,890, on and off;
  - inside, vb 33,410-33,490, on.
  - Both with the drop sheets and the splash at feet at 1/2, as gl16 (since gl10). "On" is today's plan: OPT2 ffffffdf with the streaks at 1/2 by strips. "Off" is OPT2 fffff9df: what gl16 draws.
- **Per-function profiles** of the draw pipeline, the GS walk and the frame's work come from PSPPROF_BUCKET_ONLY (the rainy window, effects on), named with our function-name lists.
- **ms a step** = (1.647 instructions + 68.2 I-misses + 85.9 D-misses) / 333,000 / 80. The PSP [I] figures are model x the part's PSP / model ratio in the rainy window: logic 0.98, draw pipeline 0.90, walk 0.73, picture 0.83.
- **Readers:** `tools/dev/sb17k/bkread.py` (parts, actors, converters), `bkgroup.py` (the groups above), `fnsplit.py` (functions and groups), `jobs.py` (section 2.3's jobs). Runs: `models17k.sh`, `models17f.sh`; outputs `/home/claude/sb17km/model` (ephemeral).
