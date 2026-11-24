MGS2 gl42: the RENDERER CHECKPOINT build. A MEASUREMENT build like gl39 to gl41: from the deck on (vblank 24,000), turns of 100
vblanks switch the native renderer on and off, and in some turns NOT EVERYTHING or NOTHING is drawn, ON PURPOSE, so the PSP can
time the native picture against today's picture in the same scene. Personal use only, do not share: it contains code built from
your MGS2 disc.
2026-11-24. Build notes: mgs2/main/attic/gl42 (gl42_plan.md, state.md). NOT DELIVERED by the build agent: the lead delivers it
after gl41's PSP log has been read (one build out at a time).

What it is: gl41's build (the same source: native step 1 = the ship drawn natively, gl41's levers P1 and P3, gl41's heap fix)
plus native renderer STEP 2 (the people drawn natively), with a new EBOOT and the 8 files next to it, and these changes in mgs2.cfg,
each marked 'gl42' there. Every other setting is gl41's.
  1. MGS2_NGE_CYCLE (one digit a turn) has a new digit 2: the ship AND the people drawn natively (each person's mesh is made once,
     lit and skinned the way C2 does it, into the native region, and the GE draws it from there; a blended person's changing
     vertices are redone each frame by the Media Engine). Digit 1 is gl41's: the ship alone. 0 is today's path.
  2. MGS2_BOUND_CYCLE has a new kind 9 (K-N2R): only the chains that carry the native ship and people are drawn (the people's
     chain and the ship's chains, 2 to 4): the native path's own cost with little else on the GE. In a K-N2R turn the rest of the
     picture (the backdrop, the rain, the effects) is MISSING ON PURPOSE.
  3. C2 ON IN EVERY TURN (DEVIATIONS.md B7): MGS2_OPT2 and every MGS2_OPT2_CYCLE word are fffff9ff (gl41: fffff9df). The native
     people exist only in the C2 form, so every turn, K-A included, draws the people with C2. P1, P3 and M5 stay on everywhere.
  4. MGS2_NGE_KB=256 and MGS2_NGE_KB2=384: each native region is 256 KB until vblank 24,000, then 384 KB with the people's block
     beside it (the carve after the opening; the log says when it grew, about vblank 24,007). MGS2_NGE_P6LAG=100: the people turn
     native 100 vblanks after the growth.
  5. MGS2_NGE_HPROBE=1 (new): the re-use check of each person's mesh (a content hash of the mesh's streams, on the main CPU) is
     timed with the system's microsecond clock: 'reuse hash checks' and 'reuse hash us' in each 'nge people turn' line.
  6. gl41's one-time probes MGS2_PROBE41 and MGS2_PROBE41_CULL are taken out (gl41's run reads them).
  7. hashref.txt is gl42's OWN (md5 0cbcb5b9...): the native people leave the game's packet memory unwritten (the game's own
     builder does not run for them), so the state hash differs from gl41's from vblank 24,100 (the people turn native there), by
     design. Checked on the PC twin: no value the game reads afterwards differs (step 2's read check), and the pictures' content
     is equal.
NOT in gl42: anything not in gl41, except the above.

THE TURNS (100 vblanks each; NGE = MGS2_NGE_CYCLE's digit; bound = MGS2_BOUND_CYCLE's kind; XP as gl40/gl41; digests: none
exist before vblank 29,490, on = digests on in that turn):
  vblanks       turn kind                      switches             digests               shots        scene
  24000-24099   t0   K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  24100-24199   t1   K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24200-24299   t2   K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  24246        first deck scene
  24300-24399   t3   K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  24400-24499   t4   K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24500-24599   t5   K-N1 ship native           NGE 1  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24600-24699   t6   K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24700-24799   t7   K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  24786        first deck scene
  24800-24899   t8   K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24900-24999   t9   K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  25000-25099   t10  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  25100-25199   t11  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25200-25299   t12  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25300-25399   t13  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            deck (first play)
  25400-25499   t14  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25500-25599   t15  K-N1 ship native           NGE 1  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25600-25699   t16  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25700-25799   t17  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25800-25899   t18  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25900-25999   t19  K7 nothing drawn           NGE 0  bound 7  XP 3  none (before 29,490)  -            deck (first play)
  26000-26099   t20  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26100-26199   t21  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26200-26299   t22  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26300-26399   t23  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  26400-26499   t24  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26500-26599   t25  K-N1 ship native           NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  26600-26699   t26  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26700-26799   t27  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  26800-26899   t28  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26900-26999   t29  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27000-27099   t30  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27100-27199   t31  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27200-27299   t32  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27300-27399   t33  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27400-27499   t34  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27500-27599   t35  K-N1 ship native           NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  27600-27699   t36  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27700-27799   t37  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27800-27899   t38  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27900-27999   t39  K7 nothing drawn           NGE 0  bound 7  XP 3  none (before 29,490)  -            steady deck
  28000-28099   t40  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28100-28199   t41  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  28200-28299   t42  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28300-28399   t43  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  28400-28499   t44  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28500-28599   t45  K-N1 ship native           NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  28600-28699   t46  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28700-28799   t47  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            change of scene
  28800-28899   t48  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            change of scene
  28900-28999   t49  K7 nothing drawn           NGE 0  bound 7  XP 3  none (before 29,490)  -            change of scene
  29000-29099   t50  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            rain begins
  29100-29199   t51  K-N2 ship+people native    NGE 2  bound 0  XP 3  none (before 29,490)  -            rain wait, first turns
  29200-29299   t52  K-A all drawn, today       NGE 0  bound 0  XP 3  none (before 29,490)  -            rain wait, first turns
  29300-29399   t53  K-N2R native, bound 9      NGE 2  bound 9  XP 3  none (before 29,490)  -            rain wait, first turns
  29400-29499   t54  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait, first turns
  29500-29599   t55  K-N2 ship+people native    NGE 2  bound 0  XP 7  on                    29502        rain wait
  29600-29699   t56  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait
  29700-29799   t57  K-N1 ship native           NGE 1  bound 0  XP 3  off                   -            rain wait
  29800-29899   t58  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait
  29900-29999   t59  K-N2 ship+people native    NGE 2  bound 0  XP 3  off                   -            rain wait
  30000-30099   t60  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait
  30100-30199   t61  K-N2R native, bound 9      NGE 2  bound 9  XP 3  off                   -            rain wait
  30200-30299   t62  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait
  30300-30399   t63  K7 nothing drawn           NGE 0  bound 7  XP 3  off                   -            rain wait
  30400-30499   t64  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   -            rain wait
  30500-30599   t65  K-N2 ship+people native    NGE 2  bound 0  XP 3  off                   30502 30548  rain wait
  30600-30699   t66  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   30646        rain wait (heavier)
  30700-30799   t67  K-N2R native, bound 9      NGE 2  bound 9  XP 3  off                   30724        rain wait (heavier)
  30800-30899   t68  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   30826        rain walk
  30900-30999   t69  K-N2 ship+people native    NGE 2  bound 0  XP 3  off                   30926        rain walk
  31000-31099   t70  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   31048        rain walk
  31100-31199   t71  K-N2R native, bound 9      NGE 2  bound 9  XP 3  off                   31114        rain walk
  31200-31299   t72  K-A all drawn, today       NGE 0  bound 0  XP 3  off                   31264        rain walk
  31300-31399   t73  K-N2 ship+people native    NGE 2  bound 0  XP 7  on                    -            the walk ends
Kinds: K-A all drawn, today's path (25 + 9 + 3 turns in deck / rain wait / walk); K-N1 the ship native (5 / 1 / 0); K-N2 the ship
and the people native (10 / 4 / 2); K-N2R native, bound 9 (7 / 3 / 1); K7 nothing drawn (3 / 1 / 0). Every K-N1, K-N2, K-N2R and K7
turn sits between two K-A turns of the same scene. Digests on in t55 and t73 only (both K-N2: the native people's lists on the PSP
against the emulator's and the PC twin's).

What you will see. Before vblank 24,000 the picture is gl41's (the people with C2: up to 3.8 % of a picture's pixels differ by a
colour level, DEVIATIONS.md B7). From the deck on:
  - K-A, K-N1 and K-N2 turns look the same: the native picture is meant to be the same picture (checked on the PC twin and the
    emulator, content equal in every checked picture).
  - K-N2R turns (11 turns: t3, t9, t13, t23, t29, t33, t43, t53, t61, t67, t71): ONLY THE SHIP AND THE PEOPLE are drawn; the rest
    of the picture is missing or stands still.
  - K7 turns (t19, t39, t49, t63): the picture STANDS STILL, as in gl40/gl41.
  THIS IS THE MEASUREMENT, not a fault: the game goes on underneath. The picture pauses 16 times for the saved shots and every 25
  vblanks in the walk for the captures, as before. Shots by kind: K-A 24246, 30646, 30826, 31048, 31264; K-N2 24786, 29502,
  30502, 30548, 30926 (the native picture on hardware against gl41's shots at the same vblanks); K-N2R 30724, 31114 (only the ship
  and the people); before the deck 8762, 10518, 15763, 23265.

What it saves next to the EBOOT (about 25 MB, as gl41): shot_<vblank>.bmp (16), live_<vblank>.bmp, log.txt, steps.csv,
battery.txt, crash.txt.
What it touches: as gl41. No new hardware writes, no new imports (the same 133 sce* names as gl41's final build and gl40); the
hash probe reads the system's microsecond clock the harness already reads (sceKernelGetSystemTimeLow); nothing written to flash,
333 MHz as always, no power or clock setting; at the end it waits on its end screen as before.
Memory: the image is about 32 KB larger than gl41's (step 2's code and tables: text +24.6 KB, the rest data and pads), so the
game's heap (newlib's) has about 32 KB less room than in gl41; its end is gl41's (0be9bfff on the emulator) and its block starts
at 0b24f900 (gl40/gl41's offset in its 8 KB page). The carve after the opening (the 384 KB regions and the people's 76 KB
blocks) comes from the frame memories' arenas, outside newlib's heap. On the emulator [C emu]: the route's worst moment is the
texture table's event at vb 536 with 103 KB above the break + 35 KB free top = 139 KB (gl41: 135 + 34 = 170; THE GATE: at least
32 KB); the ME heap events' lowest room + top 126 KB (vb 23,676; gl41 158); textures refused for want of memory 0; the end's
report takes the break to 35 KB of room (gl41's stayed below the route's high point) and at the end malloc can give 96 KB in 2
blocks (gl41 144 in 1); the same with nothing native, so this comes from the larger image, not from the native turns. The PSP
has had MORE room than the emulator so far (gl40: 2,608 KB in one block at the end on the PSP, 64 KB on the emulator).

What the run should show:
(counts that depend on the game's state, not on speed, so the PSP's should be the emulator's run of this package [C emu];
rd42pc.py checks the first three and prints the rest)
  - All 62 hashes equal to hashref.txt (gl42's own, 0cbcb5b9): rd42pc.py says 'HASHES ... EQUAL'.
  - 61 frame digests (t55, t73: both K-N2) and 61 content lines, equal to the emulator's: 'DIGESTS ... EQUAL' and 'CONTENT ...
    EQUAL'. The K-N2 lists differ from today's by design (the people and the ship are CALLs into the native regions); their
    content (a native batch hashed as today's) is the PC twin's in every frame.
  - The carve after the opening: 'nge: step 2: the carve after the opening at vb 24007: 2 region(s) of 384 KB now whole' (the
    PSP may grow it a few vblanks earlier or later; the people start at vb 24,100 in any case).
  - 'nge people totals': people calls 27436, groups 171045 (referenced 167832), meshes reused 27212, cooked 224, re-cooked 0,
    no room 0, not cookable 0, table full 0, groups left to today's path 94; reuse hash checks 27212, stream bytes hashed at use
    93685232 (the 'reuse hash us' figure is the PSP's own time). 'nge converter people': groups converted 167832, N-ref batches
    116681, N-copy batches 51151, blended vertices redone 731349, every 'declined' count 0.
  - 'nge totals' (the ship and the people's regions): pictures 959, calls 85603, calls native 58205 (reused 57934, cooked 271),
    evicted 212, no room 0, not cookable 0, table full 0.
  - Turns t11, t13 and t17 (deck, first play) have no people in the scene: 0 people calls there, as on the emulator (rd42pc.py
    expects it and does not flag them).
  - 'gl40 XP totals': plans 114008 (arena 388, the other failures 0); XP check: plans compared 720, DIFFERING 0 (MUST be 0: the
    digest turns t55, t73). M5 walks about 3.8 million.
  - 'F1 totals (gl38)': runs planned 9495 (groups 854584, not written 724772), runs not planned 0, with a problem 0.
  - 'bound totals (gl38)': frames begun with a mask 699; native groups recorded and not drawn 133405.
  - 'natcull: totals (v3)': effect calls tested 272681, left out 197460 (7110810 vertices).
  - HEAP: as above ('memory (gl41)' and 'picture heap (gl41)' lines); 'textures refused for want of memory: 0'.
  - 314 'gl41 levers' lines, 75 'nat turn' and 75 'np6 turn' lines, 47 'nge people turn' lines; 16 'the flip wait kept' lines,
    no 'GAVE UP' line. The SCHEDULE CHECK must say 'none'; PROBLEM COUNTERS none.
  - The 'me picture loop (gl41)' stack line is the emulator's (deepest use 5312 of 16384 B there); on the PSP the loop runs on
    the Media Engine itself.

The run's measurements (rd42pc.py prints them; ms a PSP step; main = the main CPU's step less its waits for the GE; ME = the Media
Engine's busy time; P = max(main, ME), the step a turn of that kind would take if the faster side waited for the slower):
  (a) THE PEOPLE NATIVE: K-N2 minus its K-A neighbours, by stretch (deck, rain wait, walk), main and ME apart, and P. The rain
      is main-CPU-bound (gl40: logic + pipe 69.0 of the step's 76.4, main side about 73.4, ME 69.7), so read main first: the
      people native take the people's packet building off the main CPU but add the re-use hash; the ME stops converting the
      people's packets each frame.
  (b) K-N2R minus K-A: the native path with little else drawn (an upper bound on what the picture's side costs natively).
  (c) K-N2 minus K-N1: the people's share alone (not neighbours: a scene's content moves by about 1 ms between turns).
  (d) THE RE-USE HASH: 'hash ms a step' by turn and by stretch (the 'nge people turn' lines; two clock reads a check): its cost on
      the main CPU, which matters while the rain is main-bound.
  (e) K-A minus K7: the drawing's cost on the main CPU, as gl40/gl41 (K7 now with C2 on before it).
  (f) gl42 against gl41 in the same turn kind (K-A: C2 on now, and gl42's new profile and code order).

What to expect of the speed. [C model] = the PC cost model, ms a PSP step: main = the main CPU (game, runtime, the copy plan, the
reuse hash, the rest), ME = the Media Engine's work; the model's step is P = max(main, 0.8 x ME) [I]. [I] = inferred from those
deltas (taken x 1) and gl40's PSP run (rain wait, K-A XP 3: step 76.44, main side = logic 42.74 + pipe 26.3 + gsw 4.4 = 73.4 [I:
the step less a GE wait of about 3], ME busy 69.68; steady deck 40.3 to 40.7, ME-bound). gl41 has not run on the PSP yet, so its
figures in this chain are inferred too. The model's main is about 15 % above the PSP's (84.7 against 73.4), so the PSP's deltas
may be about 0.87 x the model's. Single estimates of this kind have been wrong by a factor of two before. On the PSP read main and
ME busy apart, and P as the larger of the two; never the step alone.
  RAIN WAIT (t57-t64; main-CPU-bound in every drawn turn: model main 75.1-85.1 against 0.8 ME 29.8-51.9):
    K-A (t58, t60, t62, t64)  gl42 - gl41 the same turn: main -0.07 / +0.35, ME +0.18 / +0.19 [C model] (C2 on now, step 2's
                          code, the new profile and order); gl41 - gl40 t60: main -0.84, ME +0.23 [C model, gl41's].
                          PSP: main side about 72.7 (72.5 to 72.9), ME busy about 70.1, P = main, step about 75.7 [I]
    K-N2 (t59)            minus K-A t58 / t60: main -1.79 / -1.80 (the reuse hash +0.95 inside it), ME -5.38 / -4.98
                          [C model]. PSP: main -1.0 to -2.0 (about 71), ME busy -4.5 to -5.5 (about 65), P = main: the step
                          about 1 to 2 under K-A's (about 74) [I]
    K-N2R (t61)           minus K-A t60 / t62: main -9.63 / -9.90, ME -26.81 / -27.61 [C model]. PSP: main about 63 to 64,
                          ME busy about 43, still main-bound, step about 66 [I]
    K-N1 (t57)            minus K-A t58: main +0.47, ME -2.56 [C model] (gl41's K-N1: ME -3.10 / -2.71). PSP: the step about
                          K-A's (+-0.5), ME busy -2 to -3 [I]
    K-N2 - K-N1           t59 - t57 (not neighbours): main -2.26, ME -2.82 [C model]: the people's own share.
    K7 (t63)              main 68.19 [C model]; K-A - K7 main +16.75 / +16.92; gl42 - gl41 K7 +0.56. PSP: gl40's K7 step
                          - 0.3 [I] (gl41 -0.86, gl42 +0.56)
    THE REUSE HASH        0.95 ms a step of main in the wait's K-N2 and K-N2R turns [C model]; 26.1 checks and 84 KB hashed a
                          step there on the emulator [C emu]. PSP: 0.5 to 1.0 ms a step, plus about 0.1 for the probe's own
                          clock reads [I]. rd42pc.py's 'hash' column is the PSP's own figure: if it is well above 1 ms, the
                          hash is worth a cheaper form while the rain is main-bound.
  HEAVIER PART (t65 K-N2, t66 K-A, t67 K-N2R): t67 - t66 main -11.20, ME -24.33 (hash +1.32) [C model]. t65 - t66 main -6.92,
    ME -11.66, but t65 sits before the heavier part (gl41's t65 - t66 read main -5.1 with K-N1 there), so that pair is mostly
    content: read t59 for the people. gl42 - gl41 t66: main +0.32, ME +0.60 [C model].
  THE WALK (t68 K-A, t69 K-N2, t70 K-A, t71 K-N2R, t72 K-A): the content moves from turn to turn (K-A t70 - t68 main +2.72, ME
    -3.06 [C model]). t69 - t68 / t70: main -6.82 / -9.55, ME -13.77 / -10.71 (hash +0.98); t71 - t70 / t72: main -15.31 /
    -10.48, ME -22.96 / -20.87 (hash +1.20) [C model]. PSP: K-N2 -5 to -9 on main, K-N2R -10 to -15, partly content [I].
  STEADY DECK (t22-t46; ME-bound: model main 30.1-30.5 against ME 39.3-39.7): few people there (49 people calls a turn, 2.4
    hash checks a step [C emu]): K-N2 - K-A main +0.33 / -0.01, ME +0.26 / -0.17 (the K-A spread is +0.34); K-N1 - K-A main
    +0.40 / +0.05, ME +0.44 / +0.48; K-N2R - K-A main -5.44 / -5.04, ME -22.58 / -22.19; hash 0.05; K7 (t39) main 20.26
    [C model]. gl42 - gl41 K-A t30 / t32 / t34: main +0.07 / +0.26 / +0.03, ME -0.40 / -0.39 / -0.41 [C model]. PSP: K-A,
    K-N1 and K-N2 about gl40's 40.3 to 40.7, perhaps 0.3 less from the ME [I]; K-N2R: the ME stops being the larger side (ME
    busy about 18, main about 19 to 20 [I]); its step is UNKNOWN (the GE's wait in a K-N2R turn is not modelled; between 20
    and 35).
  DECK, FIRST PLAY (t10-t19): not modelled, UNKNOWN. t11, t13 and t17 have no people in the scene (as on the emulator).
  DIGEST TURNS (t55, t73: K-N2 with XP 7 and digests on): not modelled for gl42. PSP: about K-N2 + 4.5 (digests) + 6.5 (the XP
    check) = 84 to 86 [I: gl40's digest and XP 7 costs]. Measurement turns only.
  MAIN-DRAWING STEPS before vblank 24,000 (gl38's guard cfg windows, C2 off there): gl42 59.36 / 58.12 / 24.21 / 60.55; gl42 -
    gl41 +0.64 / +0.23 / -0.36 / +0.29; gl42 - gl36 +0.12 / -0.69 / -0.06 / -0.07; instruction-cache misses -2.4 / -10.2 / -8.1
    / -3.5 % of gl36's (the guard, at most +2 %: PASS) [C model]. PSP: about gl41's + 0.2 (about 55.2) [I]. C2's own cost in
    these steps (on in gl42, off in gl41) is not in these windows: UNKNOWN, read it against gl41's log.

How it runs: as gl41's. The files go over the last build's folder on the stick (renamed MGS2T_gl42); at the end ("Press X to
exit") the build WAITS on its end screen: press X and start "PSPLink (PC control)" so the logs can be copied to LOGS\gl42. About as
long as gl41's run. Start with the battery charged, as for gl41 (gl40's runs read no external power with the cable in).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd42pc.py, this README. The disc pack (mgs2disc.pak)
and SLES_503.83 already on the stick stay as they are.

Reading the logs: on the PC   python -I rd42pc.py <LOGS\gl42>     (one file, standard library only, no other files needed)
It prints: the end of the run, the crash record, HASHES, DIGESTS and CONTENT against the emulator run of this package (must say
EQUAL), the HEAP lines, the carve after the opening, the main-drawing steps before 24,000, the turn table (step, main, ME, P, the
people's counts, hash checks and hash ms a step), the SCHEDULE CHECK, each stretch's kinds with the differences, the PAIRS, the
people by turn, native step 1 by turn, the problem counters, the battery.
