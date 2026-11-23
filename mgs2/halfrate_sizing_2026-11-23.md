# Half-rate game on the Tanker deck w00a: sizing (2026-11-23)

Study only: no PSP build, no change to the main tree or to other agents' folders. Work folder (container): /home/claude/hr
(state.md, tools/, tw/). Inputs: halfrate_study.md (2026-11-07), hd_study/hd_study_2026-11-23.md §1, §7.1, §8,
hd_study/rate_sites_2026-11-23.md (the site map, 4 parts), our function-name lists, prof/actor_list_vb24980.txt,
prof/actors_free_v26.txt, prof/hw_gl39/where_78ms_go.md, native_ge_renderer_design.md §0 and §4.
This note restates some values from the site map (the 60 Hz literals); it names no source beyond our own hd_study notes.

Marks: [C] read in our code or measured; [C twin] measured on the x86 twin this session; [C model] cost model;
[C map] taken from the site map (hd_study); [I] inferred, basis given; UNKNOWN. No [I] here is reached.

Units: a step = one game logic update. Today 50 steps a second, a picture every 2 steps. Half-rate game = 25 steps a second,
a picture every step, every rate-dependent constant rescaled so that the game plays at normal speed.

## 0. Short answers

1. **Konami already did this once.** Our PAL disc runs Konami's compile-time 50 Hz conversion. Checked in our own code at
   function level [C]:
   - float speeds are x6/5;
   - Raiden's gravity is x6/5: 115.2, not 96.0 and not 138.24;
   - the motion tick is 6.0, not 5.0;
   - variable counts are converted at run time by (t*5+1)/6;
   - Konami's hand-picked PAL frame numbers are compiled in.

   So a half-rate game is a second conversion of the same sites, at ratio 2 from 50 Hz. The open question of the HD study
   ("did Konami scale floats and gravity for PAL?") is settled: **yes, x6/5, gravity treated as a speed**.
2. **Deck size [I].**
   - About 270 sites listed in the map with their values.
   - About 110 effect sites [I].
   - 500-800 inline guard and player counters, of which the map gives totals only.
   - In all, roughly 900-1,200 sites for the deck, and about 4,300 for the whole game.
3. **Twin prototype [C twin].** Wait word 2 from vb 30,000 on route i, against the full-rate run at the same vblank (half-rate
   step k against full-rate step 2k), with 16 constant/read patches (motion tick, Raiden's gravity, guard walking speeds):
   - Snake's error drops from 9,481 to 995 units at worst. Snake follows the reference path with a fixed lag of 8-13 vblanks.
   - The camera's error drops from 11,062 to 1,198.
   - The guards' error is unchanged (2,100-4,800), because their wait timers are not rescaled yet.
   - Without patches, the half-rate game is the full-rate game in exact slow motion: guards and camera match the reference at
     an equal step count with 0.0 error.
4. **Biggest risks.**
   - Every-other-vblank gates: the guards' shot is gated on the vblank counter's parity [C].
   - Blends counted in whole steps [C].
   - Small counters.
   - Collision at twice the move per step: UNKNOWN.
5. **Gain [I].** After the native renderer, the deck goes from 78-108 ms a picture (9.3-12.8 fps, full-rate logic) to about
   46-67 ms a picture (15-22 fps). It gains nothing before the renderer: today the ME would cap it near 7 fps.
6. **Cost [I].**
   - Deck: about 5-9 weeks.
   - Whole game: about 4-9 months.
   - The half-rate game becomes a new exact reference (twin = emulator = PSP). Equality with the original game is lost.
7. **Vita-style skip lever, no half-rate logic [I].**
   - The 8 HALF effects save about 1.2-1.35 ms a step on gl39 numbers, but only about 0.3-0.5 once the keep-the-dice cut is in.
   - Beyond the renderer, skipping the remaining exact pass code on unshown steps is worth about 4.5-5.5 ms a step. That is the
     bigger piece, and it needs the reader-standard proof.
   - The guards' dice can stay unchanged for line_sph, rain, ripple and the 4 effects that take no numbers. charaspread must
     stay whole.

## 1. The deck's site list

Deck actors: 106 kinds and 392 actors at vb 24,980 (prof/actor_list_vb24980.txt). The guards appear later. At vb 30,400 [C twin, this session's
dump] the list holds 3 watcher.c (act 0x229120), command.c (0x2273f0), attacker.c (0x2a2b88), raiden.c (0x290ce0),
camerad.c and 10 camera.c.

Kinds (the map's short names): DT/CV = step counts (timers, AI counters); TB/BT = motion time per step (1/300 s: 5 NTSC, 6 PAL);
SV/SVF/AT2/AT3 = per-step distances and float speeds (x6/5); AT4 = accelerations (x1.44); PG = gravity; FPS = steps per second;
hand = Konami's hand-picked PAL numbers.

| Area (deck actors) | Listed sites in the map, by kind [C map] | In our binaries [C] |
|---|---|---|
| Motion, all characters (libmt in w00a: MT_InitMotion 0x3bec30 .. MT_ActSequence) | TB 6 (sequence 1, header 1, motion init/act/interp/speed 4). Face and eye animation TB about 6 more (eyeanim x4, codec faces) [I which]. One motion file keeps its own 5 | float 6.0 at MT_InitMotion 0x3bed80, MT_SetMotionInterp 0x3c09c8, MT_SetMotionSpeed 0x3c0a38 (the default when speed < 0; stored per channel at +0x3c), MT_InitSequence 0x3c2d14; no 5.0 anywhere in libmt. All 4 reached on route i. Float 6.0 in w00a: 68 constants, 38 reached (TB callers among them, not separable by value) |
| Camera (camera.c x10, camerad.c; ELF game/cam_util) | DT 4 (camera set 1, utils 3). The camera's smoothing (libgv near) is not tagged | runtime (t*5+1)/6 at 0x10df78, 0x10e06c, 0x10e798 (2 reached). The smoothing constants are UNKNOWN |
| Player: raiden.c and plugins (pl_arm, pl_global, subcam, attachments, ropes, hair) | DT 51 (headers 6, raiden 2, hang 13, routine 4, first person 3, weapons 17, fall/ladder/door/other 6); FPS 6; float speeds and gravity AT3 4; int distance AT2 2; TB 1; PG about 24 (attachments, corpses, hair, ropes). Swimming, water, elevator and NPC Snake (about 110 more) are not on the deck | gravity 115.2 at 0x291528 (reached) and 0x28f5dc |
| Guards: watcher x3, command, attacker (korekado and sigeno code, w00a 0x2221a8-0x2c0230) | defines and other non-counter sites: CV 41, AT 3, SV 22, TB 7, BT 2, FPS 1. Attacker actions: DT 27, CV 2, TB 5, BT 1. Hand-picked PAL branches: about 30 in AI files game-wide | §2 |
| GCL and demo (delay.c x2, cinema_scr, d_fade_io, d_fog_set, in-engine demos) | script delay DT 2, cinema DT 3, fades/fog DT about 2, demo daemon TB 7 + FPS 1, demo-effect PG | runtime (t*5+1)/6 in delay.c at 0x2c02d8 and 0x2c045c, both reached. The script values themselves stay 60 Hz numbers in the data |
| Game core (game daemon, alert/evasion/search meters) | about 10 | 150 in the ELF core; 853 (21 in guard code, 31 in all of w00a) and 3000 (19) in w00a guard code |
| Effects on the deck (about 35 kinds) | the splash family PG at least 10 (falling splash x8, splash parts, slow parts, blood); charaspread PG x0.4. The rest is untagged per file | not mapped |

Runtime conversions in our binaries [C]: 43 divides by 6 (ELF 3, w00a 40). 19 of them have the exact (t*5+1)/6 shape
(x4 + add, +1, div 6): ELF 3 and w00a 16. Each is one place where a variable 60 Hz count (from a script or a message) becomes
a PAL count. At ratio 2 the divisor becomes 12.

What the map does not give:
- **The ~2,500 inline counter checks are totals only** (COUNT 1,350 + DIRECT_TICK 1,191 game-wide; enemies alone 1,304 counts
  in 164 files).
  - Deck-relevant files among the largest: damage reactions 97, guard walking 63, held-up guard 41, attacker avoid 36, think
    box 34 (= 271).
  - Estimate for the deck's guards and player: 500-800 [I]. Basis: those 271, plus about 40-60 smaller deck-relevant files
    with about 5-10 counters each.
- Also missing: the per-site lines of effects (about 3.1 sites per effect file game-wide, so about 110 for the deck's
  about 35 kinds [I]); the sub-actor files; the camera's smoothing constants; per-step random rolls and parity gates
  (these are not rate macros, see §3); and files that define a local macro of the same name (about 670 occurrences
  game-wide, left out of the 3,533).

**Totals [I]:**
- Deck: about 270 listed sites with values, about 110 effect sites, 500-800 inline counters. That is about 900-1,200, plus
  the 43 runtime conversions.
- Whole game: 3,533 + about 670 (local macros) + about 120 hand-picked = **about 4,300 sites**.
- Each of the 123 stage images carries its own copy of the shared player, guard and effect code. Patches must therefore be
  applied per image, matched by function bytes, not by address [I: w00a holds the korekado/sonoyama/libmt objects itself].

## 2. Mapping into our binaries

Tools (/home/claude/hr/tools):
- imm.py indexes every immediate in the ELF .text and the w00a text (li, slti/sltiu, lui+ori pairs followed by mtc1 =
  float constants, lwc1 from data).
- fn.py lists one function's constants. sites.py runs the test below. dis.sh disassembles a slice with psp-objdump.

The PAL value Konami compiled: (t*5+1)//6 for counts, x6/5 for distances and float speeds. A float is "found" only when the
whole pair (lui + ori, then mtc1) gives the PAL bits.

**Region test [C].** 51 listed sites, 98 literal checks, each run in the code region of its subsystem: player, guards,
attacker/doors, ELF core.

| Result | Count |
|---|---|
| Found: the PAL value is present and the 60 Hz literal absent | 20 |
| Ambiguous: both present, or the value is 12 or less | 74 |
| Not found | 4 |

- Without function bounds a region test cannot isolate one site, because round numbers recur.
- In aggregate the binary is clearly the converted one. For the 41 distinct integer literals of 100 or more, the PAL values
  occur 866 times in their regions and the 60 Hz literals 198 times.
- Found includes: sleep levels 3000/6000/9000/12000; faint 1500; sleep 15000; go-sign and voice lengths 200; around blank
  225; accident delays 750/3000; M4 shot 116/151; spray 500; the watertight door 40.
- Not found: tranquilizer 1500 (neither value in the player code); high-throw frame 149 -> 124 (149 is present twice);
  first-person strafe 72.0; body 153.6.

**Function-level checks [C]** (12 sites: 10 found, 2 not found):

| Site | Ours (w00a unless ELF) | 60 Hz [C map] | PAL compiled | In our code | Reached on route i |
|---|---|---|---|---|---|
| Raiden's gravity, per step | raiden act 0x291528: the vertical speed at +0x54 -= g | 96.0 | 115.2 | 115.2 (0x42e66667 = 96 x 1.2f) | yes |
| Raiden's fall speed from a count | 0x28f5dc: vy = -n x g | 96.0 | 115.2 | 115.2 | no |
| Guard walking | ENE_ActMove 0x264930 | 12.5 x2, 25 x2, 50 x2, 60 | 15, 30, 60, 72 | 15, 15, 60, 30, 30, 72, 60 (and a 10.0 the map does not list) | yes |
| Guard walk-to-point | watcher file 0x2294c8 | 50, 8, 5 | 60, 9.6, 6 | speed = max(60 - n x 9.6, 6) | yes |
| Damage reactions (6 uses) | korekado/action 0x2539a8, 0x2552e0, 0x255578 | 64.0 | 76.8 | 76.8, 3 loads | no |
| Alert level | 0x25f688 (ENE_SetHeadMark2 area) | 16 | 19.2 | 19.2 | yes |
| Motion tick | libmt, 4 sites (§1) | 5 | 6 | 6.0 x4, no 5.0 | yes |
| Attacker motion frames (hand-picked) | 0x22ff24, 0x2b6a74 | 19/66/101 | 16/55/84 | 16/55/84 x2; no 19/66/101 cluster anywhere in w00a | no |
| GCL delay | delay.c 0x2c02d8, 0x2c045c | runtime | (t*5+1)/6 | same | yes |
| Camera interpolation times | ELF 0x10df78, 0x10e06c, 0x10e798 | runtime | (t*5+1)/6 | same (3 of the 4 listed) | 2 of 3 |
| First-person strafe | player code | 60.0 | 72.0 | 72.0 absent, 60.0 x3 present | UNKNOWN whether left unscaled |
| Body | guard code | 128.0 | 153.6 | neither | probably not in w00a [I] |

- **Float speeds and gravity: Konami scaled them x6/5 for PAL [C].** All 9 float sites checked at function level hold the
  x6/5 value.
- Raiden's gravity was treated as a speed (x1.2), not as an acceleration (x1.44).
- The x6/5 product was compiled as value x 1.2f. That gives 0x42e66667, one ulp above the nearest float to 115.2, so a
  value search for the nearest float misses it.
- The first quick scan's "96.0 in Raiden's code" is stick scaling at 0x28b490 / 0x28b580 (x96.0 with x240.0, 1/128, 1/255
  and +-400), not gravity.
- Reached = the instruction has a line in our route translation (main/gen/wpl/gen_s_w00a.h, gen/WP/gen_elf.h hold only
  reached code).

## 3. Risk kinds at ratio 2, with deck examples

| Risk | Deck example | What a fix needs |
|---|---|---|
| Collision: twice the move per step | Snake runs at least 44 units a vblank [C twin: x from -731 to -9,620 in 200 vb], so about 80-90 a step at 25 Hz. Guard walk up to 72 becomes 144. GM_ActControl (ELF 0x106d70) calls HZX_NearHazardCheck x1, HZX_OnlineHazardCheck x1, HZX_LevelHazardCheck x3 a step [C]. Whether the line check covers the whole move on every path is UNKNOWN. Deck thin parts: railings, wire, crates | twin runs into railings at double step; if needed, two control sub-steps for fast movers (+0-1 ms [I]) |
| One-step reactions and flinches | hit flinch CV 6 -> 5 (PAL) -> 2 or 3. In the 30 fps mode the reference lengthened flinches because a 1-step reaction fell on a skipped picture [C map]. At half rate every step is shown, so that reason goes [I]; reactions that must last N steps (invincibility, hit-stop) still halve | halve with a floor of 1 |
| Small integer counters | frames to spot a body part 13 -> 6 or 7 (+-8% sight time); indistinct alert 6 -> 3; counters of 1 stay 1 (twice as long in real time); a count halved to 0 under a "--t == 0" test never fires | Q = (P+1)//2 from the compiled P; check every == 0 test |
| Every-N-steps and parity gates | SIG_BasicShoot (w00a 0x2b9478) lets a guard shoot only when the vblank counter 0x18db64 is even and a 1-in-4 roll passes [C code, logic_timing]. At 2 vblanks a step the parity is the same at every step, so the guard shoots always or never. chim_smo respawns every 12th step [C notes]. Our G1 split (ropes on even passes, lamps on odd) [C] | rewrite each gate on the step count; search all reads of 0x18db64 and the and-1/3/7 masks on step counters |
| Animation blends | PlayMotion (0x3c0e10) counts blend progress in whole steps (+36 += 1 a step) next to the motion time [C]. With the time doubled and this counter not, blends last twice as long. The twin's 8-13 vb lag of Snake at the start of his run fits this [I] | halve the blend counts (MT_SetMotionInterp, SetMotion) |
| Motion interpolation | the tick drives time and root motion: Snake moves by the root curve's delta per step [C notes, rootmotion trace], so a doubled tick doubles his move per step [C twin, §4] | the tick (one constant per site), or the playback reads |
| GCL script waits | the delay command converts at run time (§2) [C]. Script polls of step counters: UNKNOWN | divisor 6 -> 12 at the 43 runtime sites |
| Random numbers | the effects' draws per second halve, so the guards' later choices differ from the 50 Hz game (the reference accepted the same at 30 fps [C map]). Per-step chances halve per second: the 1-in-4 shot roll per step becomes half as frequent | double per-step chances (1-in-4 -> 1-in-2) where they are gameplay |
| Sound | the IOP and SPU keep their own clock; voice and demo streams follow the vblank counter [C halfrate_study]. Sound requests leave at step boundaries, up to 40 ms late instead of 20 [I] | nothing |
| Play time | counts elapsed vblanks [C] | nothing |

## 4. Twin prototype [C twin]

**Set-up.** Everything is in /home/claude/hr/tw and was rebuilt from read-only copies.
- run.sh is route i with gl39's settings (main/scripts/twin.sh's environment and gl39's schedule keys).
- The reference run A1 used gl39's twin binary unchanged. Its 615 hash lines equal hashref_gl38.
- The patched unit: gen_s_w00a.h was patched by patch_hr.py and compiled as main/scripts/x86.sh does. The unpatched control
  compile equals the gl39 twin's gen_s_w00a.o byte for byte (16e6f205). The baseline relink equals gl39's twin byte for byte
  (62421fda). The patched twin is boot_hr (8c287da4).
- The switch is MGS2_POKE=30000:18db74:2: the wait word, the game's own 25 Hz mechanism.
- Every patch is guarded by "wait word == 2". So B1's hashes equal the reference up to vb 30,000 and differ from 30,100,
  which proves the patches are inert before the switch.

**Addresses [C twin, vb 30,400 actor list and RAM dumps].**
- Snake: raiden.c actor 0x142b6c0, position floats at +0x40.
- Guards: watcher.c 0x14bd790, 0x14dd0a0 and 0x14fc9b0, position at +0x40.
- Camera eye at 0x1dfa00 [I by behaviour: it follows Snake about 600 to the side, 5,750 up and 5,700 behind].
- The actors' file names were checked in every sample.

**Pad.** The pad script is per vblank and the game reads the pad once a step. So half-rate step k (at vb 30,000+2k) sees the same
pad state as full-rate step 2k: no resampling is needed. A press shorter than 2 vblanks, or an edge on an odd vblank, moves by
one vblank [C script format; I for the effect].

**M1, the minimal rescale (16 patches).**
- Motion tick: the stored speed doubled where playback reads it (PlayMotion 0x3c0e08, PlayMoveData2, PlayRootData,
  PlayFixData, PlayJointData). This covers motions started before the switch.
- Raiden's gravity: x4 (vertical speed per step x2, so the change per step x4).
- Guard walking: ENE_ActMove x7 and walk-to-point x3, each x2.
- Not in M1: the camera (ELF), guard timers, blends, GCL delay, the parity gate.

**Runs.** Dumps every 20 vb from 30,000 to 30,800 (400 half-rate steps). Snake stands to about 30,500, then runs.
Error = 3D distance in world units to the reference run at the same vblank.

| Run | Snake max | Snake at 30,800 | Camera max | Watchers max (w1 / w2 / w3) |
|---|---|---|---|---|
| B0: wait word 2, no rescale | 9,481 | 9,481 | 11,062 | 4,683 / 4,602 / 2,552 |
| B1: wait word 2 + M1 | 995 | 995 | 1,198 | 4,667 / 4,815 / 2,120 |

- **B0 is exact slow motion.** Compared at an equal step count (B at vb v against A at 30,000 + (v - 30,000)/2), the guards
  match with 0.0 error to 30,800, and the camera and Snake to 30,500 (until the pad acts).
  - So nothing in the game scales by time, as the code read said.
  - The reference is the first thing the patches must be judged against.
- **B1, Snake.** Up to 30,500 his error stays under 1.2 units. From the start of his run he follows the reference path
  8-13 vblanks late, 20-200 units off the path; matched by time, the error is about 600-1,000 units.
  - His speed matches: the error stays flat while he runs.
  - The lag sets in at the start of the run: unscaled blend and turn counters [I] (§3).
- **B1, camera.** It follows Snake: error about 1,060 during the run. Its smoothing is not rescaled yet.
- **B1, guards.** About the same as B0. Their error stays flat while both runs walk and grows at every stop: wait and think
  timers (inline counters) run twice as long in real time. M1 fixed their walking speed only.

**Next version (M2), exact plan.**
1. Find each watcher's patrol-wait and think counters: dump the watcher struct every 2 vb, look for words that count down
   by 1 a step and reset at departures, and get the writing pc with MGS2_WWATCH. Patch their reload constants to (P+1)//2.
   Done when the guards' error stays under about 100 units through 30,800.
2. Halve the blend counts at MT_SetMotionInterp/SetMotion. Done when Snake's lag is 2 vblanks or less.
3. The ELF unit for the camera, the GCL divisor (6 -> 12) and the shot parity gate: needs gen_nat.o rebuilt in the work
   folder, about 1 day [I].
4. Extend to the walk window 30,500-31,400 and the guards' alert at 36,4xx-37,8xx (w01a).

Accept a version when no actor's error exceeds about 1 step of its own movement, and the same guard decisions happen
(alert, shots) within 1-2 steps. Each run takes about 2.5 minutes (to vb 30,810) with nice 19; the dumps are deleted as
they are read (extract.py).

## 5. Effort and gain

**Sites to patch:** deck about 900-1,200 plus 43 runtime divisors; whole game about 4,300 [I, §1].

**What can be automated:**
- A constant patch table (function bytes or address, old, new) applied to the image before translation. patch_hr.py does
  this on the generated C today. Then the twin, the emulator and the PSP all build from the same patched code, and the
  half-rate game gets its own exact hash reference: our method keeps working, against the new game.
- Finding distinctive values: by search, as in §2.
- The runtime divisors and the motion tick: mechanical.
- Matching each function across the 123 stage images by its bytes: a script.

**What stays manual:**
- The inline counters: each compare has to be judged against the function's meaning.
- Parity gates and per-step chances.
- Collision.
- Each patch batch judged by trajectory error on the twin.

**Effort [I, can be off by 2x]:**

| Work | Effort |
|---|---|
| Listed deck sites with values (about 270), about 10 min each | about 6 days |
| Inline guard/player counters (500-800), about 5 min each | 5-9 days |
| Effects: rescale about 110, or leave cosmetic effects at half speed as the reference's 30 fps mode does | 2-3 days, or 0.5 |
| Runtime divisors | 1 day |
| Parity and chance gates | 1-2 days |
| Collision check and sub-steps if needed | 2-5 days, UNKNOWN until tested |
| Twin iteration (M2 onward) | 1-2 weeks |
| New hash reference and PSP build | 2-3 days |
| **Deck total** | **about 25-45 working days (5-9 weeks)** |
| **Whole game** (about 4x the sites, every stage's scripts and bosses) | **about 4-9 months** |

**Gain per picture [I, 2x].** Base: the renderer design's full set plus the logic levers, 39-54 ms a step, main CPU (§4.2 there).
- Full-rate logic: a picture costs 2 steps = 78-108 ms, so 9.3-12.8 fps.
- Half-rate logic: a picture costs 1 step, plus four corrections.

| Half-rate correction | ms a picture |
|---|---|
| Base: one step | 39-54 |
| GE list writing now every step | +2-3 |
| The half-rate cosmetic actors save nothing more: they run once a picture either way (G2's planned 3.5-5.5 + G1's ropes and lamps about 1, gl39 row C) | +4.5-6.5 |
| GE bus share per step doubles | +0-2.5 |
| Collision sub-steps if needed | +0-1 |
| **Total** | **about 46-67 (15-22 fps)** |

- The GE (28-31 ms a picture) and the ME (sound plus about 10-20) stay under it.
- 20 fps (50 ms) is reached only in the lower part of the range.
- Before the renderer, half rate buys little: gl39's ME is busy about 141 ms per picture, so a picture every step would be
  ME-bound near 7 fps against today's 6.3 [I, gl39 numbers]. This is the 2026-11-07 finding, still true.

## 6. The Vita-style skip lever on w00a (full-rate logic)

**HALF effects (the 8 the reference updates only on drawn steps).** These are rain, line_sph, charaspread, sky_prev, ripple,
blood_wl, rain_cm and blood_sp. Their logic cost in the rain wait:
- From gl39's row B [C model]: rain.c 1.04, line_sph 0.76 (nat_ls_prim 0.45 + nat_ls_flush 0.31), ripple 0.32.
- From older models [C model]: charaspread about 0.15 (rain_options); sky_prev 0.01-0.23, blood_sp 0.12, blood_wl 0.04,
  rain_cm 0.03 (actors_free_v26).
- Together about 2.3-2.7 ms a step.

Updating them on drawn steps only saves about half: **1.2-1.35 ms a step [I], about 1.6% of gl39's 78-80**.
- This overlaps the keep-the-dice cut (about -3 ms a step, already in the renderer plan's 39-54). That cut removes line_sph
  (about 1.0) and rain.c's hidden streak loop (0.6-1.0) on every step.
- After it, the HALF lever adds about **0.3-0.5 ms a step [I]**: ripple, rain.c's drawn sheets and the small ones. charaspread
  stays whole (below).
- The rain/splash row B is 3.9 ms a step in all; body_sph's particles (0.59) and d_splash_floor (0.48) are not in the HALF set.

**Draw-only work on unshown steps (H1), and what is left beyond the renderer.**
- The renderer removes the builders and the list walk outright (about 11-12 model ms a step).
- What still runs on every step for exactness is the pass code, about 13-14 model ms a step [C model, renderer §4.1]: box cull
  4.04, OnePieceObjs 1.66, sort keys 1.58, StoreMatrixObjs 1.20, the sort/screen/prim2/matrix passes 2.96, BoundCheck and the
  rest 1.36.
- The only pass output the game reads is each object's world matrices (plus the EVM world copy and the patch flag and world)
  [C reference read, hd_study §7.1].
- So on an unshown step everything but the world matrices (about 3 model ms) could be skipped: about 9-11 every other step,
  **about 4.5-5.5 ms a step [I]**.
- It needs the reader-standard proof (E5), because RAM differs.
- With half-rate logic there are no unshown steps, so this lever and the half-rate game do not add up.

**Keeping the guards' dice [C twin hd_study §1.2 unless marked].** A skipped act must still move the stage seed (w00a 0x1fc954)
by the count it would have drawn. Jumping N steps of the LCG is one multiply-add.

| Effect | Is the count known without running it? |
|---|---|
| line_sph | yes: fixed per actor, 4 per vertex (768-5,504), no own state |
| rain.c | yes, with a one-word shadow countdown: it draws once when its countdown 16 + (r & 31) runs out, and r is the value the jump itself lands on [I] |
| ripple | only with a shadow of the ring radii: it draws 3 at each respawn past radius 800 [I] |
| charaspread | no: its particle sets are driven by other actors' messages; keep it whole (stopping it changed route i [C notes]) |
| sky_prev, blood_wl, rain_cm, blood_sp | take no numbers in the measured windows: free |

- The guards act mid-step, after line_sph, chim_smo and ripple and before rain and charaspread. A skipped act's count must be
  added at its own place in the order, or anywhere before the guards for the early ones.
- On the aft deck the PS2 runs one step per vblank with idle time, so the clock cannot change the pace there [C twin notes].

**Do HALF effects move at half speed at 30 fps?**
- Yes for the general rule [C map]. In the 30 fps mode every rate helper returns its 60 Hz value, and the HALF effects simply
  update half as often.
- Only a short list was compensated: chaff life 2 per update, stage smoke timings halved, the enemy flinch lengthened, one
  texture animation, the play-time clock. None of the deck's 8 is in it.
- Whether any of the 8 carries its own inline compensation is UNKNOWN: sub-actor files were not followed, and inline counters
  have no per-site lines in the map.
- On our deck the same lever means rain falling and ripples growing at half speed. That is a visible change to log in
  DEVIATIONS.md; it is small after the rain decision B (only the lines nearest the camera stay).

## 7. Verdict

The half-rate deck is feasible.
- **Why it works.** Konami already converted every one of these sites once for PAL: our binary holds x6/5 float speeds and
  gravity, a 6.0 motion tick, run-time (t*5+1)/6 counts and their hand-picked PAL frames. The game has no time step. A
  16-patch prototype on the x86 twin already cut Snake's and the camera's trajectory error by about 90% [C twin].
- **What it costs.** The guards still drift on their unscaled timers. The deck is roughly 900-1,200 sites, mostly inline
  counters the map gives only as totals, plus parity gates, blends and an untested collision risk: about 5-9 weeks for the
  deck and 4-9 months for the game [I]. Exactness against the original game is lost; it is replaced by a new exact
  reference for the half-rate game.
- **What it gains.** About 46-67 ms a picture instead of 78-108, so 15-22 fps instead of 9.3-12.8 in the rain [I]. That gain
  exists only once the native renderer has removed the ME bottleneck. Before it, half rate is worth about +0.7 fps.
- **What to build first.**
  1. Keep building the renderer: it is the precondition.
  2. In parallel, on the x86 twin only: M2. That is the watchers' wait and think counters, the blend counts, then the camera,
     the GCL divisor and the shot parity gate. Judge each step by trajectory error. The aim is to measure how many of the
     500-800 inline counters really matter on the deck before anyone commits weeks to the full table.
  3. Decide at the renderer's checkpoint: if the main CPU per step lands near the low end (about 46 ms), the half-rate deck
     gives about 20 fps and is worth it; at the high end it gives about 15 and the Vita-style lever plus the remaining pass
     skips are the cheaper path.

## 8. M2 (2026-11-23)

All runs are in /home/claude/hr/tw, route i, switch at vb 30,000, same set-up as §4. Error is the same-vblank 3D distance to
the full-rate reference A1 (half-rate step k against full-rate step 2k), in world units, over 40 samples from 30,020 to 30,800.

**Short answers.**
- Guards: the wait timer is found and halved. Mean error falls 4-9x for w2 and w3 and halves for w1. Snake and the camera have not
  moved since M1: no M2 batch reaches what is left of their error [C twin].
- Collision: a doubled move does not pass through anything tested. Snake was forced into 3 deck contacts at 2x, 4x and 8x the
  full-rate step. He always came to rest against the same face; only where along the face changes [C twin]. No sub-steps are
  needed for walls [I].
- The 5-9 week deck estimate holds only within its own 2x margin. My central figure moves to about 6-10 weeks (§8.6) [I].

### 8.1 What M2 found and patched

**Guards (batch G, 8 patches in w00a) [C].** Found by scanning RAM dumps (A2, every 20 vb) for words that move by exactly 1 a vblank
(tools/ctr.py), then reading the code that writes them.
- The patrol wait is a halfword at think->+8 (actor+0x1870), offset 3074 (actor+0x2472). It counts down by 1 a step at 0x22bb84 and
  0x22bfac.
- It is loaded from the route node (+2432) when the next node is chosen and converted at run time by (5t+1)/6 at 0x22a70c..0x22a72c.
  Patch: 6 -> 12.
- The default wait 150 at 0x22a7d0, 0x22a810 and 0x22a83c becomes 75.
- A second loader at 0x3cc69c stores the route value with no conversion. Patch: halve it.
- A state counter at think+824 counts up by 1 a step. Its thresholds become 13, 5 and 26: slti 25 at 0x22b26c, slti 9 and 51 at
  0x22b3b4/b8.

**Fix-up at the switch.** w2's and w3's waits had been loaded before vb 30,000, so they kept full-rate counts. That is an artefact of
switching mid-run: a real half-rate game converts from the start. At 30,001 four pokes halve the live values:
- w2: 350 -> 175;
- w3: 250 -> 125;
- three countdowns in an unnamed actor (act 0x2dfeb8): 219 / 599 / 839.

**Other batches.**
- **C, GCL delays (2 patches) [C].** The delay act takes 1 off +64 each step (0x2c026c). Its run-time (5t+1)/6 at 0x2c02dc and
  0x2c0448 becomes /12.
- **B, blends in libmt (2 patches) [C].** MT_SetMotionInterp stores the length n (+20) and the rate 6/n (+28). MT_ActMotion takes
  the tick 6 off n each step (0x3bf610). Patches: 6 -> 12 at both places, which covers every motion blend, Raiden's and the
  guards'.
  - Raiden's motion-event checks need no patch. PL_CheckMotionTime (0x294ff0) is a crossing latch (>=), so it is safe at double
    speed.
- **L, libgv/near (22 functions in the ELF unit) [C].** The per-step smoothing and speed helpers become wrappers that, when the
  wait word is 2, run the function twice: NearExp2..16 as int, F, P, VF and PV, NearSpeed and NearSpeedF.
  - Reached call sites: about 40 in w00a and about 57 in the ELF.
  - The unpatched control compile, objcopied as tw39.sh does, equals the twin's gen_nat.o byte for byte.
  - Not wrapped: NearTime* (the caller's own step counts set the time), NearRange* (clamps) and NearPhase.
- **K, camera interpolation times (2 patches, ELF) [C].** cam_util's (5t+1)/6 at 0x10df7c and 0x10e054 becomes /12.

### 8.2 Error table, worst / mean [C twin]

| Run | Patches | Snake | w1 | w2 | w3 | Camera |
|---|---|---|---|---|---|---|
| B0 | wait word 2 only | 9,481 / 2,078 | 4,683 / 1,878 | 4,602 / 1,810 | 2,552 / 1,303 | 11,062 / 2,495 |
| B1 | M1 | 995 / 275 | 4,667 / 1,493 | 4,815 / 1,890 | 2,120 / 1,123 | 1,198 / 432 |
| B2 | M1 + G + C | 995 / 275 | 2,112 / 742 | 4,815 / 1,890 | 2,120 / 1,123 | 1,198 / 432 |
| B3 | B2 + fix-up at the switch | 995 / 275 | 2,112 / 742 | 1,023 / 333 | 1,025 / 199 | 1,198 / 432 |
| B4 | B3 + B + L | = B3 (within 0.1) | = | = | = | = |
| B5 | B4 + K | = B4 (same hashes) | = | = | = | = |

**How to read B4 and B5.**
- Every run's hashes equal the reference to 30,000 and differ from 30,100, so the patches stay inert until the switch.
- B4's hashes differ from B3's, so batches B and L do change game state. But the tracked actors on this window use neither.
- B5's hashes equal B4's, so the camera interpolation code is not run on this window at all.

**What is left [C twin unless marked].**
- **Snake.** He still starts each run 5-13 vb late. After that he holds the reference path and speed. Neither the blends (B) nor
  the smoothing (L) is the cause; a counter in Raiden's own state machine is suspected [I].
- **Camera.**
  - Before Snake moves, the camera's error is a slow vertical sway of about ±150 units. B plays it in exact slow motion, so it is
    driven by a per-step phase whose site is not found yet [UNKNOWN].
  - After Snake moves, the camera follows his lag.
- **Guards: departure lags.**
  - w1 starts about 30 vb late right at the switch, and leaves its wait about 40 vb late.
  - w2 leaves about 30 vb late.
  - w3 starts its second walk 20-25 vb late.
  - At about 33 units a vblank of walking, these lags make all of their remaining error.
  - The cause is probably turn-in-place or look-around counters [I]: the next site hunt.

### 8.3 Parity gates and per-step chances

**Parity [C].** Every reached read of the vblank counter 0x18db64 counts elapsed vblanks or stores a timestamp, so none needs a change
at half rate:
- w00a 0x2d16d8;
- ELF GM_PutEneFind and Act1, the game clock;
- the DG_Dma* renderer functions.

No vblank-parity gate is reached on route i. SIG_BasicShoot (0x2b9478, parity plus a 1-in-4 roll) is not reached on this route.

**Chances.**
- 89 of the 816 inline LCG sites in w00a are reached. 78 of them are in effect modules: kano/mcman_step 51, okajima 16, okuta 11
  [C].
- The guards' reached rolls are:
  - KR_RandU x2, which picks a voice (sound 107+n);
  - KR_RandS x1;
  - four rolls near SIG_SetSpeak (slti 5, slti 101).
- As far as I read, these run on events, not every step [I]. So on this route no chance needs doubling.

### 8.4 Collision at twice the move [C twin]

**Code [C].** GM_ActControl moves an actor in this order:
1. HZX_OnlineHazardCheck tests the segment from pos to pos+move (0x106f64..0x106fbc; skipped under control flag 0x400);
2. it applies the move;
3. HZX_NearHazardCheck pushes the actor out, with the control's sizes (+64 and +66) passed in.

| | Snake | Guards |
|---|---|---|
| +64 / +66 | 474 / 500 | 400 / 500 |
| Fastest step at half rate | 156 (run, 78 a vblank) | 68 (walk) |

- Snake's 474 = 395 x 1.2, a PAL-scaled length.
- If either size is the push-out radius, a step smaller than it cannot cross a zero-thickness face before the push-out [I].

**Test design.**
- Pad-driven tests are confounded. Snake's lag at the start of a run moves where each pad leg begins, and the twin holds only
  covered code: two custom pads stopped at "missing function" (0x27ed08, 0x15c910).
- So Snake is forced instead (pad W): idle, then a poke of his x/z at 30,545, 30,730 and 30,930 to a point just before a contact
  of the free run, then one direction held.
- The step is scaled by the wait word: wait 4 or 8 with motion speed x4 or x8 (boot_hrV).

The contacts:
- A: a corner, pushing LEFT+DOWN;
- B: an angled wall sliding into a nearly vertical face, pushing LEFT;
- C: an obstacle on the right, pushing RIGHT.

**Distance from full rate's resting point.**

| Run | Step | A corner | B wall | C right | Off the full-rate path, max / mean |
|---|---|---|---|---|---|
| WH2 | 2x (156) | 165 | 397 | 91 | 393 / 153 |
| WL2 | 2x, + B + L | 188 | 410 | 116 | 392 / 165 |
| WH4 | 4x (312) | 510 | 1,021 | 314 | 1,011 / 371 |
| WH8 | 8x (624) | 624 | 265 | 1,540 | 1,550 / 482 |

**What the runs show.**
- **No pass-through, even at 8x.**
  - In C, Snake never crosses full rate's face (x = -12,249). He slides along it instead.
  - In B, full rate, 2x and 4x rest on one line, x = -13,570 - 0.2 (2,879 - z), at z = 2,879, 2,480 and 1,877: the same face.
  - A larger step carries him further along the face before he stops.
- **No snag.** At no contact does the half-rate run stop where full rate slides.
- **What changes is the resting point along a wall:** up to about one body width (410) at 2x. It is a behaviour difference, not a
  hole. Playtesting has to accept it [I].
- **Not covered:**
  - the deck's thin rails and ladders, which this pad cannot reach in covered code;
  - jumps and falls between levels (level check);
  - guards pushed by Snake.
- Reaching rails needs either a coverage run that includes them, or a pad that the coverage build can follow [I].

### 8.5 Files

All in /home/claude/hr/tw (container only; dumps deleted).

**Scripts:** patch_hr2.py (M1 plus batches G, C, B and V), patch_elf_hr.py (batch L), extract2.py (traj.tsv and ctl.tsv, with a
disk guard), cmp.py (worst and mean), coll.py, ../tools/ctr.py (step-counter scan), ../pad/pad_w.txt and pokes_w.txt.

**Binaries:**

| Binary | md5 | Contents |
|---|---|---|
| boot_hr2 | dfc60cbe | M1 + G + C |
| boot_hrV | 900f02a9 | + speed x wait word |
| boot_hrLB | 0f2feaa7 | + B + L |
| boot_hrK | f44028f4 | + K |

### 8.6 The 5-9 week deck estimate

It still holds within its 2x margin, but the evidence moves it up a little, to about 6-10 weeks [I]:
- **Collision: cheaper.** The "2-5 days, UNKNOWN" line becomes about 1-2 days to repeat the test at rails and ladders.
  Sub-steps are not expected.
- **Guard counters: as planned.** Finding them is quick: the dump scan found the wait timer and its conversion in about 1 hour.
- **Judging: slower than planned.**
  - Batches B, L and K (26 sites) moved none of the tracked trajectories on this window.
  - Each remaining error (Snake's lag at a run start, the guards' departure lags, the camera sway) needs its own hunt. The twin
    iteration line grows from 1-2 to 2-3 weeks.
- **New item: about 3-5 days to extend coverage.** The twin cannot drive the game beyond covered code, so off-route behaviour
  (rails, alerts, other moves) needs a coverage run and a new translation first.
- **Whole-game figure.** Unchanged at 4-9 months, with the same reasons to lean high.
