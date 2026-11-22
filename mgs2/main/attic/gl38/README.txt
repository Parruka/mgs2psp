MGS2 gl38: a MEASUREMENT build. From the deck on, parts of the picture are LEFT OUT ON PURPOSE in turns, so the PSP can time
what each kind of drawing costs. The game itself (its state, every hash) is the same in every turn. Personal use only, do not
share: it contains code built from your MGS2 disc.
2026-11-22. Build report: mgs2/main/attic/gl38/report.txt. NOT DELIVERED by the build agent: the lead verifies and delivers.

What it is: gl37's test build (cull always on, the re-use by its rule in turns 1,1,0,0) with a new EBOOT (and the 8 files
next to it, rebuilt with it) and these changes in mgs2.cfg, each marked 'gl38' there. Every other setting is gl37's.
  1. MGS2_BOUND_CYCLE: one digit for each turn of 100 vblanks from vblank 24,000 (turn t = (vb - 24000) / 100), the KIND of
     the turn:
       K0  everything drawn (gl37's picture).
       K1  the ship off: the native ship pieces are recorded but not drawn.
       K2  the people off (chain 2 of the frame's list in the rain).
       K3  the sorted primitives and meshes off (chain 6), but for the rain's own calls.
       K4  the rain off (drop sheets, splashes, ripples, slow drops, thunder parts), but for the drop sheet nearest the
           camera (your choice of 13:05: MGS2_GL_RAIN_NEAR=1 keeps the last 1 of each picture's 4 sheet calls; they come
           far to near). MGS2_GL_RAIN_CUT=1 (NOT set here) would do the same in the whole run: the later default's switch.
       K5  the stage's own program off (chain 10: the blur / post pass).
       K6  ship, people, sorted primitives and rain off together.
       K7  nothing drawn: the Media Engine rests for the turn and the main CPU runs the game alone (the last picture stays).
     From vblank 24,000 EVERY turn is a Media Engine turn, but for K7's (gl37: every other turn).
  2. MGS2_NAT_F1_CYCLE: F1 on in four K0-type turns (24,400; 29,900; 30,100; and the walk's 31,000). F1: in a run of fast
     ship pieces that is not replayed, a piece whose VU memory writes a later piece of the run overwrites does not write
     them (gl37's fix design, option A). Exact: its check mode compared 2,226,860 pieces on the PC twin and 567,299 in the
     emulator with 0 differing.
  3. MGS2_ME_PIC_GE2_AB=30500 taken out: it switched the GE overlap off in every other 200 vblanks from 30,500, a second
     thing changing in the walk's turns. The overlap stays on in the whole run.
  4. New timer lines for each Media Engine turn (timers only): 'gl38 timers' (the Media Engine's frame ends, list builds
     by part and records by part) and 'people split' (a people piece by stage, as 'nat split' does for a ship piece).
Also in the EBOOT, with no change for the game:
  - the log's RAM buffer is 64 KB (was 128 KB), which gives the game's heap 64 KB back (see 'What it touches');
  - a new profile and code order, trained with every kind, F1 and the re-use in turns.

THE TURNS (100 vblanks each; R = the re-use's arm, 1 when t % 4 < 2; F1 = F1 on):
  vblanks        turn  kind                                  R  F1  scene
  24000-24100    t0    K0 all drawn                          1      deck, change of scene
  24100-24200    t1    K0 all drawn                          1      first deck scene
  24200-24300    t2    K3 sorted off (rain kept)             0      first deck scene
  24300-24400    t3    K4 rain off but the near sheet        0      first deck scene
  24400-24500    t4    K0 all drawn                          1  F1  first deck scene
  24500-24600    t5    K1 ship off                           1      first deck scene
  24600-24700    t6    K2 people off                         0      first deck scene
  24700-24800    t7    K0 all drawn                          0      first deck scene
  24800-24900    t8    K6 ship + people + sorted + rain off  1      first deck scene
  24900-25000    t9    K7 nothing drawn, ME rests            1      first deck scene
  25000-25100    t10   K0 all drawn                          0      deck, change of scene
  25100-25200    t11   K0 all drawn                          0      nothing drawn
  25200-25300    t12   K0 all drawn                          1      nothing drawn
  25300-25400    t13   K7 nothing drawn, ME rests            1      nothing drawn
  25400-25500    t14   K0 all drawn                          0      nothing drawn
  25500-25600    t15   K0 all drawn                          0      nothing drawn
  25600-25700    t16   K7 nothing drawn, ME rests            1      nothing drawn
  25700-25800    t17   K0 all drawn                          1      nothing drawn
  25800-25900    t18   K0 all drawn                          0      nothing drawn
  25900-26000    t19   K0 all drawn                          0      nothing drawn
  26000-26100    t20   K0 all drawn                          1      deck, change of scene
  26100-26200    t21   K0 all drawn                          1      deck, change of scene
  26200-26300    t22   K0 all drawn                          0      steady deck
  26300-26400    t23   K0 all drawn                          0      steady deck
  26400-26500    t24   K0 all drawn                          1      steady deck
  26500-26600    t25   K5 stage program off                  1      steady deck
  26600-26700    t26   K7 nothing drawn, ME rests            0      steady deck
  26700-26800    t27   K0 all drawn                          0      steady deck
  26800-26900    t28   K5 stage program off                  1      steady deck
  26900-27000    t29   K7 nothing drawn, ME rests            1      steady deck
  27000-27100    t30   K0 all drawn                          0      steady deck
  27100-27200    t31   K0 all drawn                          0      steady deck
  27200-27300    t32   K5 stage program off                  1      steady deck
  27300-27400    t33   K7 nothing drawn, ME rests            1      steady deck
  27400-27500    t34   K0 all drawn                          0      steady deck
  27500-27600    t35   K5 stage program off                  0      steady deck
  27600-27700    t36   K7 nothing drawn, ME rests            1      steady deck
  27700-27800    t37   K0 all drawn                          1      steady deck
  27800-27900    t38   K0 all drawn                          0      steady deck
  27900-28000    t39   K5 stage program off                  0      steady deck
  28000-28100    t40   K7 nothing drawn, ME rests            1      steady deck
  28100-28200    t41   K0 all drawn                          1      steady deck
  28200-28300    t42   K5 stage program off                  0      steady deck
  28300-28400    t43   K7 nothing drawn, ME rests            0      steady deck
  28400-28500    t44   K0 all drawn                          1      steady deck
  28500-28600    t45   K5 stage program off                  1      steady deck
  28600-28700    t46   K7 nothing drawn, ME rests            0      steady deck
  28700-28800    t47   K0 all drawn                          0      change of scene
  28800-28900    t48   K0 all drawn                          1      change of scene
  28900-29000    t49   K0 all drawn                          1      change of scene
  29000-29100    t50   K0 all drawn                          0      rain begins
  29100-29200    t51   K0 all drawn                          0      rain wait, first turns
  29200-29300    t52   K6 ship + people + sorted + rain off  1      rain wait, first turns
  29300-29400    t53   K1 ship off                           1      rain wait, first turns
  29400-29500    t54   K0 all drawn                          0      rain wait, first turns
  29500-29600    t55   K2 people off                         0      rain wait
  29600-29700    t56   K3 sorted off (rain kept)             1      rain wait
  29700-29800    t57   K4 rain off but the near sheet        1      rain wait
  29800-29900    t58   K0 all drawn                          0      rain wait
  29900-30000    t59   K0 all drawn                          0  F1  rain wait
  30000-30100    t60   K0 all drawn                          1      rain wait
  30100-30200    t61   K0 all drawn                          1  F1  rain wait
  30200-30300    t62   K5 stage program off                  0      rain wait
  30300-30400    t63   K1 ship off                           0      rain wait
  30400-30500    t64   K6 ship + people + sorted + rain off  1      rain wait
  30500-30600    t65   K7 nothing drawn, ME rests            1      rain wait
  30600-30700    t66   K3 sorted off (rain kept)             0      rain wait
  30700-30800    t67   K7 nothing drawn, ME rests            0      rain wait
  30800-30900    t68   K0 all drawn                          1      rain walk
  30900-31000    t69   K7 nothing drawn, ME rests            1      rain walk
  31000-31100    t70   K4 rain off but the near sheet        0  F1  rain walk
  31100-31200    t71   K2 people off                         0      rain walk
  31200-31300    t72   K6 ship + people + sorted + rain off  1      rain walk
  31300-31400    t73   K0 all drawn                          1      the walk ends

What you will see. Before vblank 24,000 (the title, the opening, the talks) the picture is gl37's. From the deck on the
picture changes with every turn (a turn is about 3 to 8 seconds on the PSP): the ship, the people, the rain or the blur go
missing and come back, and in K7 turns the picture stands still. THIS IS THE MEASUREMENT, not a fault. In the K0 turns
the picture is the normal one; its first half second can carry a fading trace of the turn before (the blur pass reads the
last picture). Nothing to press: no R presses are needed, and there is nothing to judge on the picture this time.
The picture pauses 16 times for the saved shots and about every 3 seconds in the walk for the captures, as before.

What it saves next to the EBOOT (about 25 MB in all, as gl36): shot_<vblank>.bmp (16), live_<vblank>.bmp (the walk's
captures every 25 vblanks), log.txt (about 1.1 MB now: more lines per turn), steps.csv, battery.txt, crash.txt.

What it touches: as gl28 to gl37. No new hardware writes, no new system calls or imports (the same 133 sce* names as
gl36, the stub sections the same sizes), nothing written to flash, 333 MHz as always, no power or clock setting. The new
timers read the clocks the old timers read (the ME's Count register on the ME, the system's microseconds on the main CPU).
  - Heap: the EBOOT's code and lines are 18 KB larger than gl36's; the log's RAM buffer is 64 KB smaller, so the
    game's heap starts 38 KB lower than gl36's and its highest point is 40 KB lower. Heap low point on the emulator:
    vb 23,677, the log line reads 64 KB (gl36: 64 KB). At the end the allocator can still hand out 96 KB in 2 blocks
    (64 + 32; gl36: 64 KB in 1).
  - The smaller log buffer: the log goes to the stick every 100 vblanks as before (outside the timed steps); the largest
    burst between two writes in a run is 22 KB. The boot report (80 KB) now takes two writes instead of one, before the
    game starts.

What the run should show:
  - All 62 hashes equal to hashref.txt (c9ddb593, the same file as gl33 to gl37): the game's state is the same in every
    kind of turn.
  - 916 frame digests (29,490-31,400), each equal to the emulator's and the PC twin's digest OF THE SAME KIND OF TURN (a
    K2 picture's list is not a K0 picture's). rd38pc.py carries the emulator's fingerprint: 'DIGESTS ... EQUAL'.
    By kind: K0 266, K1 50, K2 100, K3 100, K4 100, K5 50, K6 100, K7 150.
  - 75 'nat turn' lines; 75 'nat f1' lines; one 'gl38 bound' line per turn (315), the kind as in the table (rd38pc.py's
    SCHEDULE CHECK must say 'none').
  - 'bound totals (gl38)': frames begun with a mask 1849; transfers left out by chain: 0:650 1:650 2:1000 3:650 4:650 5:650 6:850 7:650
      8:650 9:650 10:1050 11:650 12:400 13:350 14:350 15:350 16:346; native groups recorded and not drawn 100144; rain
      owners' calls 20390 (sheets 800), left out 9146, near sheet calls kept 100; chain 6's other calls left out
      15230, its GS primitives 0; rain owners' blocks over the table 36498, not right 0
  - 'F1 totals (gl38)': runs planned 606 (groups 52498, of them not written 42892, without the fast form 8071), runs not planned 0
      (state 0, a group without an MSCAL 0), planned runs with a problem 0; check mode: groups compared 0, DIFFERING
      0, written by the plan and not by the notes 0, skipped by the plan ahead of a group that declined when it ran 0
  - 'reuse totals (gl36)': table 1536 entries; runs replayed 417 (groups 113319, skipping 110693), recorded 483 (groups 19714; kept 46, not
      kept 437), checked 0 (groups 0; S differing 0, boxes differing 0); in S 9041; boxes recorded 6255, taken 105035,
      cut 56299, positions then needed 2552; no table room 63; replays with a group not taken 0, with a decline 39
  - 'natcull: totals (v3)': effect calls tested 268914, left out 195616 (6872956 vertices); kept: vertex 0 inside 41937, no
      plane proven 31269, a bad attribute 0, count 0, out of the clamps' range 0, thinned 92; check mode: left-out
      calls converted 0, DIFFERING 0, kept calls that drew nothing 0
  - These counts depend on the game's state, not on speed, so the PSP's should be the same.
  - NAT PROBLEMS none; 'planned runs with a problem 0'; 'rain owners' blocks ... not right 0'. 'over the table 36,498' is
    expected: in the first deck scene (24,200-24,400) the rain's owners have more blocks than the 16 the table holds, so K3
    and K4 there are indicative only (the rain wait and the walk have 0 to 10 a turn).
  - 16 'the flip wait kept' lines, no 'GAVE UP' line.

The run's measurements (rd38pc.py prints them; all in ms a PSP step = a vblank):
  (a) per scene, each kind's turn(s) MINUS the K0 turns of the same scene, for the step, the main CPU's parts (logic, draw
      pipeline, GS walk, GE wait) and the Media Engine's parts (busy, walks, lists, records): what each kind of drawing
      costs, and on which processor. K7 MINUS K0 is the coupling question: the main CPU's logic with the Media Engine
      resting against with it working.
  (b) F1 on MINUS off in the K0 turns (29,900 against 29,800; 30,100 against 30,000), and the 'nat split' register stage.
  (c) NEW TIMERS: the Media Engine's list build by part, with the frame digest apart; the records by part.
  (d) PEOPLE SPLIT: where a people piece's time goes, by stage.
  (e) The Media Engine's walks by chain.
  How to read the rain wait:
    - 29,100-29,500 (K0, K6, K1, K0): NO frame digests yet (they start at 29,490; the last 5 pictures of the 29,400 turn
      have them) and the scene still settles; K6 and K1 are read against the two K0 turns around them.
    - 29,500-30,600 (K2, K3, K4, K0, K0+F1, K0, K0+F1, K5, K1, K6, K7): frame digests in every turn. The digest is hashed by
      the Media Engine inside its list step. In the emulator's clock it is about 12 of the 16 ms a step of 'lists' there,
      and it shrinks when a kind leaves data out, so a kind's 'lists' difference is partly the digest's.
    - 30,600-30,800 (K3, K7): second copies; the scene is heavier there (gl37's emulator run: +5 ms a step).
    - The step of 29,100/29,400 (no digest) against 29,800/30,000 (digest) shows what the digest itself costs the step.
  K3 and K4 leave out the conversion and the drawing of their calls, not the walk of their packets: they measure a little
  less than the whole cost. In the steady deck (26,200-28,700) only K0, K5 and K7 are scheduled: the ship pieces, the
  people's chain and the sorted chain are not there. A chain's number is its place in the frame's list: 'chain 2' is
  the people only in the rain.

What to expect of the speed. PC cost-model figures [C model], in ms a PSP step. They count the work left out on BOTH
processors together, so they are not step times; on gl36's levers the PSP showed about half of the model's figure for
Media Engine work, and the step only falls while the Media Engine is the limit (in gl36's rain wait the main CPU needed
about 52 ms a step against the Media Engine's 83).
  Rain wait, each kind's turn minus the K0 turn of the same re-use arm (29,500-30,600):
    K1 ship off -29.4    K2 people off -39.1    K3 sorted off (rain kept) -0.9    K4 rain off but the near sheet -9.4
    K5 stage program off -5.1    K6 ship + people + sorted + rain off -92.7    K7 nothing drawn -102.6
  Rain wait's first turns (29,200 / 29,300 against 29,100 and 29,400, no digests): K6 -76.8, K1 -21.0 (the two K0 turns
  are 12.9 apart: the scene settles).
  Steady deck (26,700-27,000): K5 -17.6, K7 -48.6.   Walk (indicative): K2 -63.0, K4 -5.3, K6 -93.6, K7 -102.7.
  F1 (29,900 against 29,800, re-use off): -2.2, close to the model's turn-to-turn spread there (1.5); with the re-use
  on (30,100 against 30,000) +1.7, i.e. nothing measurable, as expected (the replayed runs are not F1's).
  The retrain against gl36 (no kinds, the same cfg files; limit +0.25): the Media Engine's turns are at or under gl36's
  (rain wait -2.0 / -1.0, walk -0.4 / -0.9, opening +0.0) but for the steady deck's (+0.6). The turns in which the MAIN
  CPU draws are OVER the limit in every scene, +1.5 to +2.8: not the code order (its misses are at or under gl36's) but
  data-cache misses of the main CPU [static data placement, inferred] and the new code's own instructions. In this run
  the main CPU draws only before vblank 24,000 (in K7 turns nothing is drawn), and a kind's cost is a difference between
  turns of this one build, so the kinds' figures do not depend on it. Whether the PSP shows it at all is unknown (gl36's
  PSP run did not show the model's data-placement part for Media Engine turns). The report has the table.

How it runs: as gl36's and gl37's. The files go over the last build's folder on the stick (renamed MGS2T_gl38); at the end
("Press X to exit") press X and start "PSPLink (PC control)" so the logs can be copied to LOGS\gl38. A short run, about as
long as gl36's (the K6 and K7 turns are faster, the rest as before). Keep the USB cable in, as before.

Other settings the same EBOOT takes (for a later run, not this one), each checked on the PC twin:
  - MGS2_BOUND_CYCLE absent: gl37's picture in every turn, with gl37's Media Engine / main turns.
  - MGS2_GL_RAIN_CUT=1 with MGS2_GL_RAIN_NEAR=1: the rain cut (all but the nearest drop sheet) in the whole run. NOTE: the
    rain owners' table holds 16 blocks and the first deck scene (24,200-24,400) has more, so this is not ready to be a
    default as it stands (the report says what it needs).
  - MGS2_NAT_F1=1: F1 in every turn. MGS2_NAT_F1CHK=1 (with MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1): F1's check mode;
    it plans, does the full work and compares ('F1 totals ... DIFFERING').
  - gl36's and gl37's keys as before (MGS2_NAT_REUSE, MGS2_NAT_RCHK, MGS2_NAT_CULL, MGS2_NAT_CULLCHK).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd38pc.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are; their md5s are in the manifest too.

Reading the logs: on the PC   python -I rd38pc.py <LOGS\gl38>     (one file, standard library only, no other files needed)
It prints: the end of the run, the crash record, HASHES and DIGESTS against the emulator run of this package (must say EQUAL),
the turn table, the SCHEDULE CHECK, each scene's kinds against its K0 turns by processor, F1 on against off, the ship
pieces' stage timers, the NEW TIMERS, the PEOPLE SPLIT, the Media Engine's walks by chain, the problem counters, the battery.
