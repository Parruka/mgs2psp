# MGS2 PSP port: NOTES.md

The top page of the project: the charter, where we stand, the decisions, the mapped addresses, the open questions. Started 2026-11-07 from Ffm's charter; kept by the MGS2 port thread. Every subject has its own notes (the last section); this page names them and does not repeat them. Deviations from the PS2 game: `DEVIATIONS.md`.

Marks: [C] measured or read in the code; [C model] the PSP cost model in the emulator; [I] inferred, with its basis named; UNKNOWN, with how we will find out.

## 1. The charter (Ffm, 2026-11-07 21:03 and 21:04)

> You are lead engineer on a port of Metal Gear Solid 2: Sons of Liberty (PS2, [serial from the disc's SYSTEM.CNF]) to a real PSP-3000 running ARK4.
> Hard target: at least 25 fps on real hardware everywhere, including the heaviest scenes, with the original game logic.
> - If you don't know something, write UNKNOWN and say how we will find out. Never guess.
> - PPSSPP is for development only. Only real-PSP measurements count.
> - Log every visual or gameplay compromise in DEVIATIONS.md with its impact.
> - Every change ends with: build, regression/hash test against the reference, on-device fps, and what is left.
> - Keep NOTES.md current: decisions, mapped addresses, open questions.
>
> (21:04) if you want to change any rule so we success with the project, ask first

The disc: SLES-50383, PAL (SYSTEM.CNF: VMODE = PAL; boot file SLES_503.83) [C: logic_timing.md, the delivered builds].

**How we apply it:**
- **Numbers:** an [I] figure always names its basis (the cost model, a study's sums). Where there is no basis, the figure is UNKNOWN with how we find out. An [I] or [C model] figure is never reported as reached.
- **Speed:** only a PSP run's log counts (psp_harness.md "Build glN"). PPSSPP and the cost model choose and check work; they never settle speed.
- **fps:** the game steps 50 times a second and the port shows every other step, so fps = 500 / (ms a step). 25 fps = 20 ms a step at full game speed.
- **Every change:**
  1. Built behind its own switch.
  2. Checked exact on the PC and both emulators: state hashes, picture digests and lists against the reference.
  3. Timed in its own turns in the next PSP build: the harness switches it on and off every 100 vblanks in one run.
  4. Done only when that run's log is read. Its line in psp_harness.md then gives the build, the hash result, ms a step and fps, and what is left.
- **Deviations:** every visual or gameplay difference goes in DEVIATIONS.md before it ships.

## 2. Where we stand against "25 fps everywhere" (2026-11-08)

**Measured on your PSP** [C] (gl16 run 1, 2026-11-03: the last complete timing run):

| Area | ms a step | fps |
|---|---|---|
| The aft deck in the rain | 144.4 | 3.5 |
| Waiting on the deck | 149.4 | 3.3 |
| Walking on the deck | 140.7 | 3.6 |
| Inside, the rooms | 93.4 | 5.4 |
| Codec calls | 52.6-56.5 | 8.9-9.5 |
| The opening, to vb 15,600 | 46.1 | 10.8 |

The rainy deck's 144.4 ms is: game logic 41.8, the PS2 draw pipeline 29.5, IOP 2.5, the GS walk 31.3, our picture 39.4 (deck_costs.md §2) [C].

**gl17 on your PSP** (psp_harness.md "Build gl17") [C]: run 1 (2026-11-07) was exact to its stop, but the PSP froze and switched off 37 minutes in (vb 33,044-33,100, on the battery alone; cause UNKNOWN, section 5 item 11). Run 2 (2026-11-08, charging over USB) ran to the end, exact, in 44.7 minutes. With the effects off it is 0.4-1.4 ms a step faster than gl16 at the same vblanks (1.3-1.4 on the deck and inside): waiting on the deck 148.1 ms (3.4 fps), inside 91.6 (5.5 fps), calls 54.7-55.0 (9.1 fps), the opening 45.1 (11.1 fps). The effects on (C4, DEVIATIONS B4) cost 4.4 ms a deck step (model 4.1) and 0.55 inside.

**gl18** (delivered 2026-11-08 17:45, waiting for your run; psp_harness.md "Build gl18"): gl17 with the rain's exact skips in turns (DEVIATIONS E5). On the rainy deck the model puts all seven skips 4.2 ms a step under the hiding alone, and the hiding 2.2 under gl17's effects-off turns [C model]; on your PSP about 4 and 2 [I].

**gl18me** (ready 2026-11-08 17:55, ships after gl18's run is read; me_step4_notes.md 4d-5 and 4e): the first build in which the Media Engine makes the picture, taking turns of 100 vblanks with the main CPU, on gl18's game with the rain's seven skips always on. On the emulators every hash, frame and shot equals gl18's [C]. They have no ME, so the ME's own part (its start, its caches, its eDRAM, its waits) and what it saves are UNKNOWN until your run. The main thread keeps 256 KB of stack instead of 1 MB (it has used about 3 KB in every run), so the heap gains 768 KB: about 840 KB free at the lowest on your PSP [I] (gl18 about 444). **Run (2026-11-18, psp_harness.md "Build gl18me"):** exact to the end on your PSP (685 hashes, the 150 window frames equal the emulator's, headroom 864 KB), but the ME never reached its first heartbeat: its clock register (Count) was never started, so its first 5 us wait never ended. The main CPU made the whole picture, 14-19% slower than gl17 (the split's bookkeeping, no PGO). Fixed in main/ for gl19 (Count started, the wait capped).

**gl19** (delivered 2026-11-18 23:23, run read 2026-11-19 01:00; psp_harness.md "Build gl19"): gl18me with the ME's start fixed and the cosmetic actors at half rate in turns (DEVIATIONS G1). Exact on your PSP (434 hashes, 150 frames, 311 vuh; the first four shots = gl18me's). The ME made its turns' picture for the whole run with no take-back. Its turns are 11-14% quicker than the main CPU alone on the deck, inside and in the calls, but 18-25% slower in the black opening; the whole run is 1.3% longer [C]. The ME is now the slow side: the main CPU waits for it about 48 ms of a 145 ms deck step, and it waits for the GE at every frame start [I: one arena, one frame memory]. Cut b saves about 5.8 ms a deck step in the main CPU's turns [C, 2 pairs]. Next: gl20 = the fresh PGO + the ME's own timers, same turns.

**gl20** (delivered 2026-11-19 02:01, waiting for your run; psp_harness.md "Build gl20"): gl19 with fresh profiles and a new code order (the model: -18 to -19% in the main CPU's turns [C model]) and the ME's own timers (one log line per ME turn). Same cfg and turns as gl19; exact on the emulator [C].

**The current plan's end state** [I: halfrate_study.md §3, its sums on these measurements]. This assumes all the planned exact work and the picture on the Media Engine. On the rainy deck the game's own code would be about 28-40 ms a step, giving 11.5-16 fps with the game at 46-64% speed. Inside the ship it would be 19-25 fps, and in the codec calls about 25.

**H1** cuts the draw pipeline on the steps that are never shown. On the cost model an unshown step still runs 30.5 of the full step's 31.9 ms of pipeline [C model, 2026-11-07]. The six-pass cut keeps every picture, the IOP and the time on the PC, but what game code reads differs at 98 places, from leftovers in the VU0 registers, the scratchpad and shared memory, so it is not proven exact (h1_notes.md) [C]. Made exact it would be worth about 5.5 PSP ms a rainy deck step and 2.5 inside [I: the model's upper bound, 6.1]; the study's estimate was 6-10. On hold until gl18 and the picture on the ME are done (task #168).

**So:**
- On the heaviest scene measured, the plan does not reach 25 fps with the original logic.
- 25 fps there needs the game's code at about 20 ms a step, with the picture on the Media Engine. Today it is 71.3 ms (logic 41.8 + pipeline 29.5).
- Single estimates have been wrong by up to 2x before (gl12's translator cut, gl14's C1), so only the PSP runs of each step settle it.
- What closes the gap after the exact work: your rules of 21:17 and 00:09. Weather and effects may be lightened in what we draw only (drawing-side filters and exact skips, every state hash equal), each cut with pictures, numbers and your OK (DEVIATIONS.md F). The effect actors themselves run as on PS2, so any logic saving on the deck has to come from exact work.
- **The rest of the game is UNKNOWN.** No route goes past the Tanker's opening, deck, rooms and codec calls (123 stages in all). How we find out: a route into each area on the PC, then timed PSP runs. The Plant chapter's heavy scenes (bosses, crowds) may need the same choices as the deck [I].

## 3. Decisions (Ffm's words and cards; newest first)

| When (UTC) | Decision |
|---|---|
| 2026-11-08 17:46 | "Everything exact" (the logic thread's card of 05:31, "Make all vector math exact at about 9 ms a deck step?"): all of the game's VU0 vector math rounds toward zero as on the PS2, the macro-op groups and the micro programs (the box cull's among them) included (option A, `b_proof_notes.md`; about +8.6-9.7 PSP ms a rainy deck step and about +4 inside [I]). Supersedes 2026-11-08 01:54 and 2026-11-07 21:22. Its own build after gl18me (one change per measurement), checked against the all-toward-zero PC twin (`MGS2_VFPU_RZ=1`) with new hash references; DEVIATIONS E1 is resolved when that build ships with hashes equal to the twin's (task #178) |
| 2026-11-08 01:54 | "Game logic exact" (card on the logic thread, "Should the port's vector math round like the PS2 does?"): the game logic's VU0 vector math rounds toward zero as on the PS2 (the exact forms in place of the VFPU forms in the logic's macro-op groups, about +2.3-3.2 PSP ms a deck step [I]); the draw pipeline and the box cull keep the VFPU forms, provided a PC proof shows their VU0 results never reach the game state (task #176; if it fails, the choice goes back to you). Supersedes 2026-11-07 21:22 for the logic (DEVIATIONS E1). The proof failed (2026-11-08 05:30, `b_proof_notes.md`), so the logic thread's card of 2026-11-08 05:31 asked again; superseded 2026-11-08 17:46 ("Everything exact") |
| 2026-11-08 00:09 | "Drawing-side cuts only" (card on the logic thread): effect cuts in heavy scenes change only what is drawn; the effect actors run in the game as on PS2 and every state hash stays equal, because they share each stage's random seed with the guards (section 4). Game-side effect thinning and H2 are out. Narrows 21:17 |
| 2026-11-07 21:22 | "Keep it" (card): the VFPU's round-to-nearest VU0 math stays under the original-logic rule, logged as DEVIATIONS E1 (superseded for the logic 2026-11-08 01:54, in full 2026-11-08 17:46) |
| 2026-11-07 21:17 | "Cut cosmetic effects only" (card; narrowed 2026-11-08 00:09 to the drawing side): where the original logic can't reach 25 fps, weather and effect actors may be lightened, each with pictures, numbers and your OK first, logged in DEVIATIONS.md; gameplay logic (AI, physics, timers, input) stays original; half-rate logic is out (logic_timing.md §2) |
| 2026-11-07 21:03-21:04 | The charter (section 1); ask first before changing any rule |
| 2026-11-07 20:40 | "Where do we start?": H1 first (exact, no gameplay change) |
| 2026-11-07 15:33 | Study half-rate logic (halfrate_study.md) |
| 2026-11-03 22:03 | "Exact skip": the rain's unseen work skipped exactly (rain_study.md §6-§10) |
| 2026-11-03 18:19 | "Leave them out": the rain's own sprites (DEVIATIONS A2) |
| 2026-11-03 17:38 | "No streaks"; the close camera's drop sheets whole (DEVIATIONS A1, A4) |
| 2026-11-03 12:46 | Test the effects (C4, C4X) |
| 2026-11-03 10:32 | eDRAM probe turns A and B; turn C not approved |
| 2026-11-02 23:22 | Keep C2 (the people on the GE) off |
| 2026-11-02 19:12 | Test C2 in gl15 |
| 2026-11-02 17:12 | C1's default left to me: on from gl15 (DEVIATIONS B3) |
| 2026-11-02 12:51 | Test C1 in gl14 |
| 2026-11-02 00:48 | The ME picture probe after gl13's run |
| 2026-11-01 22:00 | Builds need no word from Ffm; the consents still do |
| 2026-11-01 21:47 | Goal: the full game at 25 fps, 20 at least, 30 a bonus |
| 2026-11-01 20:50 | The native renderer (Stage C) |
| 2026-10-31 18:34 | A big weather and ropes cut "if possible and need, we will see" |
| 2026-10-31 16:55 / 16:58 | The rain's drop sheets and the foot splash at half (sheets whole again since 17:38 on 11-03) |
| 2026-10-31 13:41 | I switch the effort level myself |
| 2026-10-28 14:07 | Heavy effects may be cut, "but always let me know the options first" |
| 2026-10-27 00:33 | "All of Sony's" ME start: 0xBC100050 = 7 and the VME registers |
| 2026-10-26 20:43 | "Build it" (the resting ME): writes 0xBC100044, 0xBC300008, 0xBC300000 |
| 2026-10-24 11:45 | Downgrades allowed in principle, each with numbers, pictures and the best alternative, OK first |
| 2026-10-23 18:28 | The sound mix on the Media Engine (card A) |
| 2026-10-23 12:49 | Keep the PSP's FPU denormal rule (DEVIATIONS E2) |
| 2026-10-21 01:22 | No rush: "lets be perfectionists" |
| 2026-10-21 00:14 | Clock "A": the draw pipeline takes no game time (DEVIATIONS E3) |
| 2026-10-20 14:14 | A new PSP build only after the last run's log is read |
| 2026-10-20 00:39 | VU0 math on the VFPU, last bit may differ (DEVIATIONS E1) |
| 2026-10-19 20:16 | Always suggest the best option |
| 2026-10-18 / 19 | The motion trail and blur may be removed if too heavy |
| 2026-10-18 23:31 | Never above 333 MHz; stock 333 MHz approved |
| 2026-10-17 | The port: your PAL disc to your PSP-3000 on ARK-4, personal use only |

Standing rules besides these:
- Never write flash0 / NAND or power and clock registers.
- New register writes, a new kernel module, hardware counters, and any change to gameplay or the picture need your OK first.
- Test runs only on the OSTENT or the charger.
- Game-derived files are personal use only.

## 4. Mapped addresses (main ELF unless a stage is named; detail in `re/`)

| Address | What | Source |
|---|---|---|
| 0x18db60 / 64 / 68 / 6c / 74 | vsync count this step / total / total at the last step / elapsed vblanks (read only at 0x1058e0, the play time) / wait word (minimum vblanks a step; cutscenes 2) | logic_timing.md §1 |
| 0x11d930, 0x11d9d0, 0x11d7f8 | vsync handler, Act1 (the step), WaitVSync | logic_timing.md |
| 0x18dba0-0x18dbab | Act1's frame meters (Timer 1), read by nothing else | speed_notes.md §31 |
| 0x13f140 | GV_ExecActorSystem: 12 priority lists, act(actor) with no time argument; pause mask 0x1e6c38 | logic_timing.md, halfrate_study.md §1 |
| 0x1b9d94 + 0x64 n | actor list heads (tail at head + 0x30); actor +0 next, +4 act function, +0x28 file name | renderer_notes.md §11.30 |
| 0x106d70, 0x106f64 | GM_ActControl; its movement add (once a step, unscaled) | logic_timing.md |
| 0x142020, 0x142060, 0x18f4d0 | GV_NearSpeed, GV_NearTime, its 1/t table | logic_timing.md |
| 0x11da90 -> 0x11ee98 | DG_ActLast: the PS2 draw pipeline's call each step | speed_notes.md §31-§34 |
| 0x18dbc0 | the pipeline's pass table A (18 passes: screen, hooks, transform 0x121060, people 0x130390, opaque 0x1226c8, shadow 0x133cc0, spot 0x1324f0, Prim2 sort 0x12dc90, sort chain 0x122138) | re/pipeline_hot.md |
| 0x1214a0 | the rooms' mesh packet builder (reduced on unshown steps since v21) | speed_notes.md §31-§34 |
| 0x12e8b8, 0x12d910 | the people's packets (P06); the sort pass's effects (returns the packet pointer) | re/pipeline_hot.md, re/mesh_packets.md |
| 0x13bf78, 0x13bd00 | the sea (452 VU0 box tests a step on the deck) | re/pipeline_hot.md |
| w00a 0x1fc954, w01a 0x1fbd74 | each stage's one random seed (inlined step x = x*0x5d588b65+1; w01a 0x3a4328 sets it). The guards and the effect actors step the same word: on route i the deck's effects stepped it about 2.7 million times, the guards' helpers 60 | prof/hr2_route_i/seed_users_route_i.txt (2026-11-07) |
| w01a 0x2b4238 (w00a 0x2b9478) | SIG_BasicShoot, a guard's shot test (actor "attacker.c"): steps the seed every 4, 2 or 1 steps and fires (ENE_ShootBullet, w01a 0x251ed8) when bits 8-9 of the new value are 0 | same, HD names |
| w00a 0x2d16b0, w01a 0x2d09c8 | the in-game clock's act: +92 year, +96 month, +100 day, +104 hour, +108 minute, +112 second, +116 the vblank its second started; on route i only it reads them | same |
| 0x1cc8b8 | the console clock copy: written at boot and at stage loads; the clock re-syncs when its bytes change | same |
| w00a 0x3334e0-0x333774 | line_sph's prim stretch (G0) | rain_study.md §6 |
| w00a 0x342878, 0x329f60 | d_splash_camera's act (the drop sheets), ft_splsh's act (the foot splash) | renderer_notes.md §11.30 |
| 0x1f1c00 | the stage image's 32-byte header | renderer_notes.md §11.30 |

- More: re/dg_objects.md (DG objects), re/mesh_packets.md, re/p07_builder.md (the codec faces), re/vu1_programs.md, elf_notes.md, stage_notes.md.
- Function names: our function-name lists, version 2 (leads, each checked against behaviour).
- **Warning:** decomp_names_sles.md's actor-system addresses are wrong.

## 5. Open questions (UNKNOWN, and how we find out)

1. **25 fps on the rainy deck with the original logic.** Our estimates say no (section 2). How we find out: each exact step on your PSP, in this order: gl18 (the rain's exact skips; on your PSP since 2026-11-08 17:45), the picture on the ME (gl18me, ready; me_step4_notes.md), the vector math made exact (your choice of 2026-11-08 17:46; it costs about +9 ms a rainy deck step [I]), H1 (on hold, h1_notes.md). Then each drawing-side cut, by your rules of 21:17 and 00:09 (DEVIATIONS F).
2. **Can the Media Engine draw the picture?** Its eDRAM is settled: probe 2 (2026-11-08) reached all 4 MB through the kernel segments with today's register writes, every word kept, and the ME copying into it cost the main CPU 0.6-1.6% of a step; probe 1's stop was the user segment (me_edram_probe_notes.md) [C]. The picture: step 4 (me_step4_notes.md). On the emulator the hand-off is exact on the whole indoor route with the copy taken at its deadline and the guards' log laid over it (4b-2, 2026-11-08) [C]. On your PSP: UNKNOWN until gl18me (ready 2026-11-08 17:55, 4e) runs there after gl18's run is read.
3. **The rest of the game's speed.** No route reaches it (section 2).
4. **Does the whole game's code fit?** -O2 code for every stage does not fit next to the 32 MB of PS2 RAM. The plan is hot code per stage plus an exact interpreter; the interpreter's speed and each stage's working set are UNKNOWN (task #20: a PC probe).
5. **Do the stage code's AI, animation or input read time?** The ELF does not [C, logic_timing.md]. For the stage code: a PC census of every reader of the time and pad words on the indoor route (logic_timing.md §4), on the MGS2 port thread's list after H1's census. Stages the route does not reach stay UNKNOWN.
6. **Does the port's logic match a real PS2?** See DEVIATIONS E1-E4. How we find out: the PCSX2 replay test (logic_timing.md §3); it needs a BIOS from your PS2 and your OK for a session on your PC.
7. ~~Does the VFPU's rounding (DEVIATIONS E1) fit "original game logic"?~~ Settled by your card of 2026-11-08 17:46, "Everything exact": all VU0 math will round toward zero as on the PS2 (DEVIATIONS E1). Left: the build (task #178, after gl18me), its hashes against the all-toward-zero PC twin, and its cost on your PSP (about +8.6-9.7 ms a rainy deck step and +4 inside [I]; UNKNOWN until its run). Before: kept 2026-11-07 21:22; reopened 2026-11-08 for the guards' random numbers; 01:54 "Game logic exact" failed its PC proof (05:30, `b_proof_notes.md`).
8. ~~The GE's time on the deck with the effects on.~~ Settled for the route measured: gl17 run 1's GE wait is 0.00 ms a step in 291 of 330 windows and at most 0.33 after the title, effects on and off [C], so the GE finishes inside the CPU's step.
9. **The sound mix and the picture both on the ME:** whether both fit is UNKNOWN until the ME picture runs (task #129).
10. **Picture gaps:** #84's cause (stale text page), #68 (no rule yet), #145 (title and menu frame digests differ between the PC and the emulators; the lists are equal).
11. **Why the PSP stops in the rooms stretch on the battery alone.** gl17 run 1 froze and switched off between vb 33,044 and 33,100 (the picture frozen several seconds, then off: a hang first, Ffm 21:40); gl11's run 1 stopped between 33,500 and 33,545 (black at once). Both exact to the stop, no record, the whole system stopped (the battery thread too). On the battery alone 2 of 7 runs through this stretch stopped there (probe 2's passed it, 2026-11-08); charging over USB 0 of 6 (gl17 run 2 passed) [C]. In gl17's run the USB cable was in (Ffm 21:44), but the PSP reported no external power and no charging all run (97% to 86%) [C]; Ffm is sure it is not the battery (21:50). The emulator with every warning logged ran the whole route without a stop or a warning, its hashes, pad and 15 of 16 shots equal to the earlier runs [C]. Power or a hang: UNKNOWN. How we find out: every run logs its power lines; gl17 run 2 (charging from 35 s in) did not stop, so the battery is not ruled out. If a stop comes back: a build that records more around the stop (every 10 vblanks from 32,500, no OK needed; an on-screen trail needs your OK).

12. ~~May an effect cut change what the effect actors do in the game, or only what we draw?~~ Settled: only what we draw (your card, 2026-11-08 00:09, "Drawing-side cuts only"). The question was: The guards draw their random numbers from the same word as the splashes, smoke and particles (section 4), so running an effect actor less often changes the guards' later choices, e.g. when they shoot. Every cut so far changes only the drawing and keeps the guards original. How we find out: your answer on the logic thread's card (2026-11-07). Until then game-side effect thinning and H2 are on hold.

## 6. Where things are (in /mnt/project-files/mgs2/)

- **The plan:** roadmap.md (Where we are, Now, Next, Later); roadmap_history.md.
- **Every PSP build and run:** psp_harness.md ("Build glN": what changed, checks, cfg, the run's numbers).
- **Speed:**
  - speed_notes.md (the code generator, the clock, exact speed-ups, the cost model);
  - deck_costs.md (the rainy deck by part, actor and effect; the cut options);
  - halfrate_study.md and logic_timing.md (time and the step);
  - rain_study.md (the rain's exact skips);
  - h1_notes.md (H1, on hold).
- **The picture:**
  - renderer_notes.md (Stages A and B, §11);
  - stagec_notes.md (Stage C, steps 0-4);
  - native_draw_study.md;
  - codec_speed.md.
- **The Media Engine:**
  - me_picture_study.md, me_handoff_design.md;
  - me_probe_notes.md, me_edram_probe_notes.md;
  - me_step1_notes.md, me_step2_notes.md, me_step4_notes.md;
  - sound_notes.md, me_startup_study.md, me_switchoff_plan.md.
- **Other:**
  - the audits: audit_2026-10-23.md, audit_v30.md;
  - the studies: android_port_study.md, ark5_study.md;
  - code size and stages: stage_notes.md.
