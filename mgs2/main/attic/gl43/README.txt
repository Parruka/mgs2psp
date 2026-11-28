MGS2 gl43: RAIN OPTION "B" on the game side, in its own turns. A MEASUREMENT build like gl39 to gl42: from the deck on
(vblank 24,000), turns of 100 vblanks switch things on and off, and in some turns NOT EVERYTHING or NOTHING is drawn, ON
PURPOSE, so the PSP can time option B against the same scene without it. Personal use only, do not share: it contains code
built from your MGS2 disc.
2026-11-27. Build notes: mgs2/main/attic/gl43 (gl43_plan.md, state.md, deviations_B_draft.md, score43_final.txt). NOT DELIVERED
by the build agent: the lead decides when it goes out (one build out at a time).

IN B TURNS THE FAR RAIN, THE SPLASHES AND THE RIPPLES ARE MISSING ON PURPOSE. In the 9 B turns (t51, t53, t57, t59, t62, t65,
t69, t72, t73) the aft deck's rain shows only its streaks and the nearest drop sheet. These are missing on purpose:
  - the floor splashes and the falling splashes;
  - the ripple rings;
  - the far drop sheets and the foot splash.
This is the measurement (Ffm's consent 2026-11-22 13:03/13:05: everything of the deck's rain may go but the drop-sheet lines
nearest the camera), not a fault. Just after a B turn, the splashes that started during it may stay hidden until they end
[I]. On the emulator, against a run without B [C emu], the first 6 to 16 pictures after each B turn differ (up to about 30
vblanks), and one more picture at vb 31,062 (in the walk). From then on the pictures are equal.

What it is: gl42's build (native steps 1 and 2, C2 on everywhere, gl42's turns t0-t49 unchanged). A new EBOOT and the 8 files
next to it, and these changes in mgs2.cfg, each marked 'gl43' there. Every other setting is gl42's.
  1. OPTION B (MGS2_OPT2 bit 21, 2097152). In a turn whose MGS2_OPT2_CYCLE word has the bit (fffff9ff), four of the deck's rain
     functions run in a short native form in place of the game's code: the floor splash's update and spawn, and the falling
     splash's update and dead step. The form keeps exactly what the rest of the game can see:
       - the random numbers each call draws from the stage's seed (the same count, so the guards' later numbers are the PS2's);
       - the slots' lives and counters, and the spawn's three copies into the splash work vectors;
       - the game time the call takes (its interrupt polls, so the interrupts land on the same instructions as on the PS2).
     A call with an interrupt due inside it, or with a guard condition, runs as the game's own code. The splash and ripple prims
     are hidden. Every other word is ffdff9ff (B off), as is MGS2_OPT2 before the turns.
  2. MGS2_BOUND_CYCLE has kind 4 in the B turns (BD_RAIN, gl38's): the rain's other effect calls are left out of the picture.
     New key MGS2_GL_RAIN_NEAR=1 keeps the nearest of each picture's 4 drop-sheet calls in those turns.
  3. The rain turns t50-t73 have new kinds:
       - K-B (K-A + B, bound 4), between two K-A turns;
       - K-N2B (K-N2 + B, bound 4), between two K-N2 turns;
       - K7B (K7 + B, nothing drawn: B's logic alone), between two K7 turns.
     Digests are on in t55 (K-N2, as gl42) and t73 (K-N2B: B's picture against the PC twin's). The table is below.
  4. hashref.txt is gl43's OWN (md5 58cdec28...): B leaves the splash slots' particles, prims and buffers different, so the
     game's state hash differs from gl42's from vblank 29,100 (the first B turn), by design. The game's choices are unchanged
     (checked on the PC twin, below).
NOT in gl43: anything not in gl42, except the above. B is never on outside its turns (OPT2_DEFAULT leaves it out). NOTE: an
older cfg whose OPT2 words carry bit 21 (gl38's fffff9df, say) would turn B on with this EBOOT; use only this mgs2.cfg.

THE GUARDS' RANDOM NUMBERS AND THE ROUTE (the form chosen and why). B uses the EXACT form: every call draws exactly the PS2's
count of random numbers and takes exactly the PS2's game time. It does not use the cheaper "dice" form, which changes the
guards' numbers on the aft deck (rain_options s5.2).
Checked on the PC twin of this build [C twin, route i to vblank 37,800]:
  - The seed log is identical in three runs: B never on, B in its turns as delivered, and B on in every turn from vblank
    29,000. It holds every act's seeds and game time (12,699 lines) and every seed change with the act that drew it (65,070,
    the guards' watcher and attacker draws among them).
  - The same stage loads and route; IOP and screen hashes equal at all 684 points.
  - In check mode all 67,264 B calls were compared with the game's own code: 0 different (time, seed, lives, copies).
  - Every load the game makes outside the drawing, hashed per vblank, is equal in 37,795 of 37,799 vblanks. The other 4 (t69)
    are one 8-byte copy that carries a leftover scratchpad word of the splash code; nothing reads a different value after it.
  - The pictures after the deck are equal (3,072 frames).

THE TURNS (100 vblanks each; B = option B; NGE = MGS2_NGE_CYCLE's digit; bound = MGS2_BOUND_CYCLE's kind; XP as gl40-gl42;
digests: none exist before vblank 29,490, on = digests on in that turn):
  vblanks       turn kind                      B    switches             digests               shots        scene
  24000-24099   t0   K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  24100-24199   t1   K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24200-24299   t2   K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  24246        first deck scene
  24300-24399   t3   K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  24400-24499   t4   K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24500-24599   t5   K-N1 ship native          off  NGE 1  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24600-24699   t6   K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24700-24799   t7   K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  24786        first deck scene
  24800-24899   t8   K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24900-24999   t9   K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  25000-25099   t10  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  25100-25199   t11  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25200-25299   t12  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25300-25399   t13  K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            deck (first play)
  25400-25499   t14  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25500-25599   t15  K-N1 ship native          off  NGE 1  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25600-25699   t16  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25700-25799   t17  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25800-25899   t18  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25900-25999   t19  K7 nothing drawn          off  NGE 0  bound 7  XP 3  none (before 29,490)  -            deck (first play)
  26000-26099   t20  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26100-26199   t21  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26200-26299   t22  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26300-26399   t23  K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  26400-26499   t24  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26500-26599   t25  K-N1 ship native          off  NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  26600-26699   t26  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26700-26799   t27  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  26800-26899   t28  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26900-26999   t29  K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27000-27099   t30  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27100-27199   t31  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27200-27299   t32  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27300-27399   t33  K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27400-27499   t34  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27500-27599   t35  K-N1 ship native          off  NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  27600-27699   t36  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27700-27799   t37  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27800-27899   t38  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27900-27999   t39  K7 nothing drawn          off  NGE 0  bound 7  XP 3  none (before 29,490)  -            steady deck
  28000-28099   t40  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28100-28199   t41  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  28200-28299   t42  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28300-28399   t43  K-N2R native, bound 9     off  NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  28400-28499   t44  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28500-28599   t45  K-N1 ship native          off  NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  28600-28699   t46  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28700-28799   t47  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  none (before 29,490)  -            change of scene
  28800-28899   t48  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            change of scene
  28900-28999   t49  K7 nothing drawn          off  NGE 0  bound 7  XP 3  none (before 29,490)  -            change of scene
  29000-29099   t50  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            rain begins
  29100-29199   t51  K-B K-A + B               on   NGE 0  bound 4  XP 3  none (before 29,490)  -            rain wait, first turns
  29200-29299   t52  K-A all drawn, today      off  NGE 0  bound 0  XP 3  none (before 29,490)  -            rain wait, first turns
  29300-29399   t53  K-B K-A + B               on   NGE 0  bound 4  XP 3  none (before 29,490)  -            rain wait, first turns
  29400-29499   t54  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   -            rain wait, first turns
  29500-29599   t55  K-N2 ship+people native   off  NGE 2  bound 0  XP 7  on                    29502        rain wait
  29600-29699   t56  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  off                   -            rain wait
  29700-29799   t57  K-N2B K-N2 + B            on   NGE 2  bound 4  XP 3  off                   -            rain wait
  29800-29899   t58  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  off                   -            rain wait
  29900-29999   t59  K-N2B K-N2 + B            on   NGE 2  bound 4  XP 3  off                   -            rain wait
  30000-30099   t60  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  off                   -            rain wait
  30100-30199   t61  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   -            rain wait
  30200-30299   t62  K-B K-A + B               on   NGE 0  bound 4  XP 3  off                   -            rain wait
  30300-30399   t63  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   -            rain wait
  30400-30499   t64  K7 nothing drawn          off  NGE 0  bound 7  XP 3  off                   -            rain wait
  30500-30599   t65  K7B K7 + B                on   NGE 0  bound 7  XP 3  off                   30502 30548  rain wait
  30600-30699   t66  K7 nothing drawn          off  NGE 0  bound 7  XP 3  off                   30646        rain wait (heavier)
  30700-30799   t67  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   30724        rain wait (heavier)
  30800-30899   t68  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   30826        rain walk
  30900-30999   t69  K-B K-A + B               on   NGE 0  bound 4  XP 3  off                   30926        rain walk
  31000-31099   t70  K-A all drawn, today      off  NGE 0  bound 0  XP 3  off                   31048        rain walk
  31100-31199   t71  K-N2 ship+people native   off  NGE 2  bound 0  XP 3  off                   31114        rain walk
  31200-31299   t72  K-N2B K-N2 + B            on   NGE 2  bound 4  XP 3  off                   31264        rain walk
  31300-31399   t73  K-N2B K-N2 + B            on   NGE 2  bound 4  XP 7  on                    -            the walk ends
Kinds: K-A all drawn, today's path (25 + 6 + 2 turns in deck / rain wait / walk); K-N1 (5 / 0 / 0); K-N2 (10 / 4 / 1); K-N2R
(7 / 0 / 0); K7 (3 / 2 / 0); K-B (0 / 3 / 1); K-N2B (0 / 2 / 2, t73 with digests); K7B (0 / 1 / 0). Each B turn sits between two
turns of its base kind, but t72 (K-N2 before it, t73 K-N2B after). The deck t0-t49 is gl42's.

What you will see. Before the rain the picture is gl42's. From vblank 29,000:
  - K-A and K-N2 turns: the rain as in gl42.
  - K-B and K-N2B turns (t51, t53, t57, t59, t62, t69, t72, t73): the far rain, splashes, ripples and foot splash are MISSING
    ON PURPOSE (see the top); the streaks and the nearest drop sheet stay.
  - K7 and K7B turns (t64, t65, t66): the picture STANDS STILL, as in gl40-gl42.
  THIS IS THE MEASUREMENT, not a fault: the game goes on underneath. The 16 shots and the walk's captures are as gl42's. Shots
in a B turn: 30502 and 30548 (t65 K7B: the still picture), 30926 (t69 K-B), 31264 (t72 K-N2B); 30646 is in t66 (K7, still).

What it saves next to the EBOOT (about 25 MB, as gl42): shot_<vblank>.bmp (16), live_<vblank>.bmp, log.txt, steps.csv,
battery.txt, crash.txt.
What it touches: as gl42. No new hardware writes, no new imports (the same 133 sce* names as gl42's final build); nothing
written to flash, 333 MHz as always, no power or clock setting; at the end it waits on its end screen as before.
Memory: the image ends where gl42's does (0b24f900; text +168 B). On the emulator [C emu]:
  - the route's worst moment is the texture table's event at vb 536: 103 KB above the break + 34 KB free top = 137 KB (gl42:
    103 + 35; THE GATE: at least 32 KB);
  - the ME heap events' lowest room + top: 126 KB (vb 23,676);
  - textures refused for want of memory: 0;
  - at the end, malloc can give 96 KB in 2 blocks (as gl42).

What the run should show (counts that depend on the game's state, not on speed, so the PSP's should be the emulator's run of
this package [C emu]; rd43pc.py checks the first three and prints the rest):
  - All 62 hashes equal to hashref.txt (gl43's own, 58cdec28): rd43pc.py says 'HASHES ... EQUAL'.
  - 61 frame digests (t55 K-N2, t73 K-N2B) and 61 content lines, equal to the emulator's: 'DIGESTS ... EQUAL' and 'CONTENT ...
    EQUAL'. t73's content is the PC twin's B picture in all 11 frames.
  - 23 'b43 turn' lines (the last turn's line would come after the run's end). Each B turn has 'B on' and every call shadowed
    (none with B off). Across the B turns to vb 31,300:
      floor spawn 964, floor update 11,036, fall update 9,560, fall dead 44,548;
      0 run as translated for a due interrupt or a guard (on the PSP a few 'a poll due' are possible and harmless: those calls
      run as the game's code).
    Every other turn has 'B off'. rd43pc.py's 'OPTION B by turn' table and the SCHEDULE CHECK check this.
  - 'bound totals (gl38)': rain owners' calls 2336 (sheets 1436), left out 1977, near sheet calls kept 359.
  - 'nge people totals': people calls 22427, meshes reused 22203, cooked 224, no room 0. 'nge totals': calls 69379, calls
    native 46029, no room 0.
  - 'gl40 XP totals': plans 114608; XP check: plans compared 720, DIFFERING 0 (MUST be 0).
  - 'F1 totals (gl38)': runs planned 8979, with a problem 0.
  - 'natcull: totals (v3)': effect calls tested 251368, left out 189249.
  - The carve after the opening at vb 24007, as gl42. The SCHEDULE CHECK must say 'none'; PROBLEM COUNTERS none.

The run's measurements (rd43pc.py prints them; ms a PSP step; main = the main CPU's step less its waits for the GE; ME = the Media
Engine's busy time; P = max(main, ME)):
  (a) OPTION B ON TODAY'S PATH: K-B minus its K-A neighbours (t51, t53, t62 in the rain wait; t69 in the walk).
  (b) OPTION B ON THE NATIVE PATH (gl42's default): K-N2B minus its K-N2 neighbours (t57, t59; t72 in the walk).
  (c) B'S LOGIC ALONE: K7B minus K7 (t65 against t64 / t66; nothing drawn, the ME resting): what the four functions' native
      form saves on the main CPU with no drawing at all.
  (d) The rest as gl42 (K-N2 minus K-A, K-A minus K7 in the deck; the people; native step 1).
Read main first: the rain is main-CPU-bound (gl40's PSP log: main side about 73.4 against ME busy 69.7).

What to expect of the speed. [C model] = the PC cost model of this build, ms a PSP step (main = game, runtime, the copy plan, the
hash, the rest; ME = the Media Engine; P = max(main, 0.8 x ME) [I]; tools/score43.py, attic/gl43/score43_final.txt). [I] = inferred
from those figures and gl40's PSP run. The model's main has been about 15 % above the PSP's (gl42: 84.7 against 73.4), so the PSP's
deltas are taken as 0.87 x the model's. Single estimates of this kind have been wrong by a factor of two before. On the PSP read
main and ME busy apart, and P as the larger; never the step alone.
  K-B - K-A [C model], 7 pairs:
      rain wait t51 - t52 -6.54, t53 - t52 -6.97, t53 - t54 -2.48, t62 - t61 -5.72, t62 - t63 -5.94; walk t69 - t68 -7.88,
      t69 - t70 -10.45; mean main -6.57 (of it the game's own code -1.15), ME -10.42.
      (t51 - t50 left out: the rain only begins in t50.) The K-A spread here is -4.49 / +0.22 / +2.57 (t54 - t52, t63 - t61,
      t70 - t68): single pairs move by up to about 4.5.
      PSP [I]: main about -5.7 (range -2 to -9), plus less bus slowdown (below); ME busy about -9. The rain stays main-bound, so
      the step falls with main: from K-A's about 75.7 to about 70 in the wait.
  K-N2B - K-N2 [C model], 5 pairs:
      t57 - t56 -6.10, t57 - t58 -5.83, t59 - t58 -5.73, t59 - t60 -5.98, walk t72 - t71 -3.09; mean main -5.35 (game -1.24),
      ME -8.90. (K-N2 spread -0.27 / +0.25.)
      PSP [I]: main about -4.7 (range -2.5 to -5.5 in the wait), plus the bus term; the step from K-N2's about 74 to about 69.
  K7B - K7 [C model]: t65 - t64 -2.55, t65 - t66 -7.42 (t66 is the heavier part); mean main -4.99 (game -1.55), ME 0.
      PSP [I]: B's logic alone about -4.3 (range -2.2 to -6.5), against the "about 5 ms a step" of rain logic expected for B.
  THE BUS (UNKNOWN). The model has no bus contention between the main CPU and the ME. In a B turn the ME does about 9 ms a
      step less [I], so the main CPU should wait less on memory. Basis: if the main CPU loses 5 to 15 % of its 73 ms to the bus
      in the rain, and that loss scales with the ME's traffic (about 13 % less), the extra saving is about 0.5 to 1.5 ms. Give
      it 0 to 2 ms a step on main: UNKNOWN until the PSP's K-B pairs are read against the model's.
  gl43 - gl42 in the same turn and kind (no lever: the new code, profile and order) [C model]: K-A walk t68 -0.42, t70 -0.57;
      the guard windows +0.05 / +0.02 / +0.28 / +0.02. PSP: as gl42's [I].
  MAIN-DRAWING STEPS before vblank 24,000 (gl38's guard cfg with B out of its OPT2 words) [C model]:
      gl43 59.39 / 57.97 / 24.49 / 60.46; instruction-cache misses -2.2 / -15.5 / -5.8 / -4.2 % of gl36's (the guard, at most
      +2 %: PASS).
  STEADY DECK, DIGEST TURNS: as gl42 (not modelled again; the deck turns are gl42's). The t73 digest turn (K-N2B with XP 7 and
      digests): about K-N2B + 4.5 + 6.5 [I: gl40's digest and XP 7 costs], about 80.

How it runs: as gl42's. The files go over the last build's folder on the stick (renamed MGS2T_gl43). At the end ("Press X to
exit") the build WAITS on its end screen: press X and start "PSPLink (PC control)" so the logs can be copied to LOGS\gl43.
About as long as gl42's run. Start with the battery charged, as for gl42.

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd43pc.py, this README. The disc pack (mgs2disc.pak)
and SLES_503.83 already on the stick stay as they are.

Reading the logs: on the PC   python -I rd43pc.py <LOGS\gl43>     (one file, standard library only, no other files needed)
It prints (rd42pc.py's sections plus option B's):
  - the end of the run, the crash record;
  - HASHES, DIGESTS and CONTENT against the emulator run of this package (must say EQUAL);
  - the HEAP lines, the carve after the opening, the main-drawing steps before 24,000;
  - the turn table and the SCHEDULE CHECK (B on exactly in the B turns, every B call shadowed);
  - each stretch's kinds with the differences (K-B minus K-A, K-N2B minus K-N2, K7B minus K7 among them), and the PAIRS (each
    lever turn against its neighbours of the base kind);
  - OPTION B by turn (calls shadowed / run as translated for a due interrupt / for a guard / with B off), with the totals;
  - the people, native step 1, the problem counters, the battery.
