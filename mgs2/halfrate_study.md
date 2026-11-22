# Half-rate logic for 25 fps: study

2026-11-07, thread "Study half-rate logic for 25 fps". Ffm asked (15:23-15:33) what we would do to reach 25 fps "at any cost", and agreed to study the coordinator's first idea: run the game's logic at half rate, draw every step, and add the ME renderer.
A study only: no change to the main tree or deliveries, no PSP build. [C] = measured or read in the code; [I] = inferred.

## Short answer

- **Units first.** The PAL game steps 50 times a second and the port shows every other step, so a shown frame covers **two** steps. The rainy aft deck's 144.4 ms is one step [C], so a shown frame costs 289 ms there (3.5 fps). 25 fps needs **40 ms per shown frame = 20 ms per step**. The gap is about 7x, not the 3.6x the chat said.
- **The game has no time step** [C]. Act1 (0x11d9d0) measures the vblanks since the last step and stores them at 0x18db6c, and the only reader in the ELF is the play-time counter (0x1058e0). Every actor (106 kinds on the deck) moves, animates, counts its timers and runs its AI in whole steps. So "logic at half rate" means one of two things:
  1. **25 steps a second, unchanged code:** the whole game plays at half speed. The game can already do this (the wait word 0x18db74 sets the vblanks per step, which the in-engine cutscenes use), but it is what we have today when the PSP is slow.
  2. **25 steps a second, each one twice as long:** every speed, timer, animation rate, AI counter and script wait doubled. That means rewriting the game's timing, actor by actor. It is not a switch.
- **Half-rate logic gains nothing until the picture shrinks** [I]. Today a shown frame has 143 ms of game code (2 steps) and 141 ms of picture work. With the picture on the ME the frame is the slower of the two, so halving the game code still leaves 141-155 ms (about 6.5 fps). Half-rate only pays once the picture side is under the game side.
- **At the end state it is the lever that decides the rainy deck** [I]. Once the planned exact work is done (game code about 28-40 ms a step, the rebuilt renderer on the ME), full-rate logic caps the rainy deck at about 11.5-16 fps, with the game at 46-64% speed. Half-rate logic would put it at about 21-28 fps, where the GE (about 40 ms a deck frame) becomes the limit. Inside the ship, full-rate logic already reaches about 19-25 fps there, so half-rate matters for heavy areas.
- **Interpolation is not needed for 25 fps.** At 25 steps a second and 25 fps every shown frame is a fresh step. Interpolation only helps above 25 fps, and it would cost a whole extra picture each step, on the side that is already the bottleneck.
- **Recommendation:** don't start the rewrite now. First do the two "half-rate" pieces that need no gameplay change, or only a cosmetic one: (a) cut the unshown step's draw pipeline much further, which is exact and can be checked with our existing tools; (b) update cosmetic actors every other step (ropes, lamps, leftover weather), with your OK. Then finish the ME and Stage C. Revisit true half-rate when the picture side of a deck frame is under about 70 ms. By then the numbers will show whether the deck still needs it.

## 1. Where the game steps, and what is tied to the step [C unless marked]

**The loop.** Each step runs the actor system once: GV_ExecActorSystem (0x13f140) walks 12 priority lists and calls each actor's act function (actor +4) with only the actor as its argument, so no time value is passed. A pause mask (0x1e6c38) skips whole lists. The draw side is two actors itself: Act1 (0x11d9d0) starts the frame and Act2 (0x11da90 = DG_EndFrame) ends it (the call our zero-time clock wraps, speed_notes §32).

**Act1, the frame start (disassembled from SLES_503.83):**
- If the wait word 0x18db74 is 0, it calls WaitAllDone (0x11d720). Otherwise it calls WaitVSync (0x11d7f8) with that value, which loops until the vsync counter 0x18db60 reaches it. So **0x18db74 is the minimum number of vblanks a step lasts**. Stage code writes it (170 sites in 108 stages, elf_notes.md); in-engine cutscenes set it, so they run at 25 steps a second on the PAL PS2 (web_research.md).
- Then it takes the total vblank count 0x18db64, stores the difference from the last step at 0x18db6c (at least 1), and resets 0x18db60.
- **Readers of the elapsed count 0x18db6c:** one, in GM_ExecProc at 0x1058e0: `play_time += elapsed` (skipped when a flag 0x4000 is set). Nothing else in the ELF or the 25 stages checked earlier (speed_notes §16) scales by it.
- **Readers of the total vblank count 0x18db64:** the stream control (0x118498, 0x118790, near GM_StreamReset / GM_VoxStream, in a function the names file puts under GM_PutEneFind; the stream sync [I]) and the DMA functions DG_DmaStart, DG_DmaSkip and DG_DmaCheckEnd. These follow real time; they don't move the game.

**What counts steps** (by the code above and the names in our function-name lists; each class is [I] until checked function by function):

| What | Tied to | What a doubled step would need |
|---|---|---|
| Movement and physics (GM_ActControl, the HZD collision libs) | one move per step | doubled speeds, and collision checks over the longer move, or things pass through thin walls |
| Animation (motion players in the actors, libmt slerps) | one frame advance per step | a doubled advance, with blends redone |
| Camera smoothing (libgv/near: 51 GV_Near* functions, e.g. GV_NearTime, GV_NearSpeed) | time constants in steps | every caller's constant halved |
| AI and actor timers (watcher, raiden, wt_door, ...) | counters in steps | every counter halved, or AI reacts twice as slowly |
| Scripts (GCL) waits | steps [I] | halved waits |
| Effects (rain, splashes, sea, ropes, lamps) | one update per step | doubled updates, or they run at half speed |
| Pad | read once per step | nothing: a press lasts more than 40 ms |
| Sound and music | the IOP, on its own clock | nothing; the game's sound requests come one step later at most |
| Voice and demo streams | the vblank counter (above) | nothing: real time |
| In-engine cutscenes | already 25 steps a second (wait word) | nothing: one step per shown frame already |
| Play time | elapsed vblanks | nothing |

**How big the rewrite is.** The deck alone runs 106 actor kinds (392 actors at vb 24,980; `prof/actor_list_vb24980.txt`) plus the shared libraries above. The whole game is about 13,500 game functions (roadmap). Our exactness method (state hashes equal on x86, emulator and PSP) can't check it, because the game's state differs from the first doubled step on; every area would be checked by eye and by play.

**Cutscenes, a free win already counted.** In-engine cutscenes cost one step per shown frame, half of gameplay. psp_harness.md logs the wait word in steps.csv, so cutscene steps can be timed apart.

## 2. The prototype on x86

Run by the MGS2 port thread on its v31 x86 build (game data stays in its container), 2026-11-07 15:56, indoor route i to vb 31,000 (the rainy aft deck), with a read-only per-vblank printer added. A = the reference env; B = the same plus `MGS2_POKE=30000:18db74:2` (the wait word at 2 vblanks a step, the cutscenes' own mechanism) [C]:

| | A (reference) | B (wait word 2 from vb 30,000) |
|---|---|---|
| State hashes | all 310 points equal prof/hashes_x86_in_h2.txt | equal to vb 30,000, differ from 30,100 on |
| Draw-pipeline calls (game steps) per 100 vblanks, vb 30,000-31,000 | 100 in every window | **50** in every window |
| Elapsed vblanks per step (0x18db6c) | 1 | **2** |
| Play time (save block +0x130), vb 30,000 to 31,000 | +1,000 (1 a vblank) | +999 (2 a step, same rate) |
| Wait word 0x18db74 | 0 | 2 throughout; nothing in the stage writes it back |

- So the game accepts gameplay at 25 steps a second through its own wait word, and only the play time follows real time. Everything else gets one step per two vblanks, so it plays at half speed [C for the step count and play time; I for "everything else": that each B step moves the game exactly as an A step does was not compared per step (no position of Snake was read), but section 1's code read leaves nothing else that could scale by time].
- Cost: a step is the same work as before, so per second the logic halves together with the game's speed. Nothing is gained per unit of game time. That is the case for the rewrite in section 1, not a shortcut around it.

## 3. Per shown frame: today and the end state [C inputs, I projections]

Inputs: gl16 run 1 on your PSP (`prof/hw_gl16/run1`). Rainy aft deck (vb 30,801-30,900) per step: logic 41.8, draw pipeline 29.5, IOP + sound 2.5, GS walk 31.3, picture 39.4 = 144.4 ms. Inside (vb 33,401-33,500): 20.4 / 13.1 / 2.0 / 22.2 / 33.1 = 90.8.

**Where the picture work falls** [C, steps.csv, inside, vb 33,001-37,800, cfg 0]: the walk and the picture of a shown frame run in the step after it. Full steps: 49.9 ms (walk 3.0, picture 7.0, pipeline 14.3). Next steps: 134.3 ms (walk 42.4, picture 58.1, pipeline 13.1). The reduced pipeline of the unshown step saves only about 1.2 ms there today.

**ME costs used** (me_probe_notes.md run 1, me_handoff_design.md): the main CPU loses about 1.7-1.8 ms a step to the ME's copy sharing the RAM [C, probe B]; the ME copies 6.7-13.4 ms a frame [I].

| Per shown frame (ms, fps) | Rainy aft deck | Inside |
|---|---|---|
| Today, one core: 2 steps | 289 (3.5) | 182 (5.5) |
| Half-rate logic only, one core: 1 step of game code, 2 vblanks of IOP, the same picture | 218 (4.6) | 148 (6.8) |
| ME picture, full-rate logic: main 2 x (logic + pipeline) + IOP + sharing; ME walk + picture + copy | main 149, ME 148-155: about 150-155 (6.5) | main 73, ME 117-123: about 120 (8.3) |
| ME picture + half-rate logic | main 78, ME 148-155: about 150-155 (6.5) | main 39, ME 117-123: about 120 (8.3) |

So with today's picture costs, half-rate logic adds nothing to the ME. The ME side is the limit.

**End state** (native_draw_study.md §4-§5, roadmap "What bounds the rainy deck"): game code (logic + pipeline) about 28-40 ms a step on the rainy deck after the logic work; the rebuilt renderer's CPU work 6-15 ms a step (12-30 a frame) on the deck; the GE needs about 40 ms for a deck frame (38.9-45.1 measured before the rain was halved). Inside: logic 10-14 + pipeline 6.5-9 = about 17-23 a step (the same 50-70% shares).

| End state, per shown frame | Rainy aft deck | Inside |
|---|---|---|
| ME picture, full-rate logic | main 2 x (28-40) + 5 + 2 = 63-87: **11.5-16 fps**, the game at 46-64% speed | main 2 x (17-23) + 4 + 2 = 40-52: **19-25 fps** |
| ME picture, half-rate logic | main 35-47, ME 19-43, GE about 40: **about 21-25 fps** at full game speed (28 if the GE allowed) | main 23-29, GE-bound: **25 fps** |

**What would still be left for 40 ms on the deck with half-rate logic** [I]: the game code per step must be under about 33 ms (the top of the range, 40, needs about 7 ms more cut: the rain's remaining logic, the sea), and the GE's frame must stay under 40 ms (rain beyond half, a simpler sea or a shorter draw distance if it doesn't). The picture's CPU side on the ME has room.

## 4. Interpolation

- **For 25 fps it isn't needed.** With 25 steps a second and 25 shown frames, each shown frame is a new step. Today's port already shows a fresh step every frame (every other step).
- **Where it would apply:** 30 fps (the bonus) or more on top of 25 Hz logic. Each in-between frame is a whole extra picture: on the deck that's about 12-30 ms more ME work and about 40 ms more GE time per in-between frame, on the parts that bound us already [I].
- **How it would be done, if ever:** blend each object's world matrix (and the camera) between the last two steps before drawing. The draw pipeline already keeps per-object transforms across calls (P11fa58, the bone matrices P11fde8; speed_notes §31), so the two step states exist; effects that are rebuilt every step (particles, rain) would be drawn at the newer step. It is feasible, but it isn't worth its cost on the PSP [I].

## 5. What can be done now, smallest gameplay change first

| Option | Gameplay change | Saving on the rainy deck | Check | Status |
|---|---|---|---|---|
| H1. The unshown step's draw pipeline cut further (today only P1214a0 is skipped: about 1.2 ms of 13-14 inside). On a step that is never shown, keep only the transforms and the words the game and the next call read (speed_notes §31: 1,716 game-read words, 24,473 cross-call words); skip the DMA kicks, the sort pass, the culls' packets and the people's chain | none (the next shown step and the game's reads stay the same) | the deck pipeline is 29.5 ms a step; DMA 6.6, sort 5.1, people 1.8 and part of the VU0 programs in the model are packet-only work, so about 6-10 ms a step averaged [I] | the read-hash method of speed_notes §34 already proves this kind of change on x86 | new; can start on x86 |
| H2. Cosmetic actors every other step (the roadmap's E4, G3, G4): render-only actors on the unshown step, the ropes, the lamps, the weather left after the exact G0 work | small visual (coarser motion where not doubled); E4 exact if the seed and work area are kept | E4 about 4, ropes 2.2, lamps 1.5 [I, deck_costs.md] | x86 pictures, your OK | sized earlier, not built |
| H3. True half-rate logic on the deck (doubled step for its 106 actor kinds and the shared libraries) | yes, everywhere; exactness lost | halves the game code per frame (section 3) | by eye and by play only | not recommended yet |
| H4. True half-rate logic, whole game | yes | the same, in every area | by eye and by play | a rewrite |

## 6. Recommendation

1. **Don't start true half-rate logic now.** It changes the game everywhere, can't be checked with our exact method, and with today's picture costs it buys nothing once the ME draws (section 3).
2. **Do H1 next on x86** (an exact, larger reduced pipeline on unshown steps). It is the half-rate idea applied to the draw side, with no gameplay change, worth an estimated 6-10 ms a step on the deck [I].
3. **H2 with your OK**, one actor class at a time with pictures, as the other levers were.
4. **Revisit H3 when the ME draws the deck's picture in under about 70 ms a frame.** Then the game code becomes the limit, and only half-rate logic takes the rainy deck from about 11.5-16 fps toward 25 [I]. A narrow first trial then would be the deck alone: the wait word set to 2 vblanks, and the 106 actor kinds' step constants doubled, starting with Snake's movement, the camera and the guards.

## Sources

Code: SLES_503.83 disassembled (capstone) around 0x11d7f8-0x11da90, 0x13f140, 0x1058c0; loads and stores of 0x18db60-0x18db74 found by a lui/offset scan of .text. Names: our function-name lists. Numbers: prof/hw_gl16/run1 (log.txt windows, steps.csv), deck_costs.md, native_draw_study.md §3-§5, roadmap.md, me_probe_notes.md, me_handoff_design.md, speed_notes.md §16, §31-§34, elf_notes.md, web_research.md.
