MGS2 gl40: a MEASUREMENT build like gl39. From the deck on, in some turns NOTHING is drawn and the picture stands still ON
PURPOSE (14 K7 and 15 K8 turns), so the PSP can time the main CPU alone and the picture's work by turns. The game itself
(its state, every hash) is the same in every turn. Personal use only, do not share: it contains code built from your MGS2
disc.
2026-11-23. Build report: mgs2/main/attic/gl40/report.txt. NOT DELIVERED by the build agent: the lead verifies and delivers.

What it is: gl39's measurement build with a new EBOOT (and the 8 files next to it, rebuilt with it) and these changes in
mgs2.cfg, each marked 'gl40' there. Every other setting is gl39's.
  1. New key MGS2_XP_CYCLE (one digit for each turn of 100 vblanks from vblank 24,000): how the main CPU makes the "copy
     plan" (the list of memory pieces the Media Engine copies, made as each recorded transfer ends):
       0  gl39's way;
       1  its sort alternates between two arrays instead of copying the array back after every pass;
       3  that, and a new piece that does not touch the last one is joined with any of the 7 before it that it touches
          (fewer pieces to sort);
       7  3, plus a CHECK: the plan is made gl39's way too and compared piece by piece (a check costs time).
     Every form gives exactly the same copies (checked on the PC twin in all 201,799 plans of the route, and on the
     emulator in all 100,604 plans of this package's run: 0 differing); only the main CPU's time changes. XP 3 is the
     lever; XP 1 alone shows part of it.
  2. New key MGS2_PROBE10=1: a 'gl40 plan' line for each turn in log.txt: the copy plan's time by part (walk, sort and
     join, the rest), its counts, the waits inside the transfer's end, M5's walks, and the check's count.
  3. M5 IS in this EBOOT now (k_take_m5: the game kernel's interrupt walk, a shorter loop). MGS2_OPT2's bit 18 switches it:
     off in turns t0, t1, t16, t17, t32, t33, t48, t49, t64, t65 (t % 16 = 0 or 1), on in the others. Its walks are
     counted in the 'gl40 plan' line (0 to 5 a step in an off turn, 15 to 450 in an on turn).
  4. MGS2_BOUND_CYCLE / MGS2_DIG_CYCLE / MGS2_LOAD_CYCLE: gl40's turns (table below). From t51 the rain wait and the walk
     are mostly K0 turns that take XP in turns; frame digests on only in t55, t65, t68 (and the last 10 vblanks of t54);
     no Media Engine load after t50. MGS2_NAT_F1_CYCLE: F1 on in every turn.
Also in the EBOOT, with no change for the game:
  - a new profile and code order, trained on gl40's own turns; the order search now also had the four main-drawing turns
    of the opening and the deck (16,100 / 21,100 / 23,100 / 27,500 in the guard settings) with a ceiling of gl36's
    instruction-cache misses + 2 % in each (gl38 and gl39 were slower there than gl36). The ceiling held: the model puts
    gl40's instruction-cache misses there BELOW gl36's in all four;
  - the data pads chosen again for this build: the constants, the data and the Media Engine's tables on the data-cache
    positions they had in gl39, and the game's heap starting at gl36's address (two new pads, 7.25 KB and 6.5 KB, beyond
    gl39's).
NOT in gl40 (later builds): H1, the dice cut, the lamps.

THE TURNS (100 vblanks each; XP as above, '-' where nothing is drawn; digests: '-' = none exist before vblank 29,490,
on / off = MGS2_DIG_CYCLE; M5 OFF = MGS2_OPT2 bit 18 cleared):
  vblanks        turn  kind                          XP         digests  ME load          M5   scene
  24000-24100    t0    K0 all drawn                  XP0        -        -                OFF  deck, change of scene
  24100-24200    t1    K0 all drawn                  XP0        -        -                OFF  first deck scene
  24200-24300    t2    K7 nothing drawn, ME rests    -          -        -                on   first deck scene
  24300-24400    t3    K8 nothing drawn, ME's turn   -          -        -                on   first deck scene
  24400-24500    t4    K0 all drawn                  XP0        -        -                on   first deck scene
  24500-24600    t5    K0 all drawn                  XP0        -        -                on   first deck scene
  24600-24700    t6    K7 nothing drawn, ME rests    -          -        -                on   first deck scene
  24700-24800    t7    K8 nothing drawn, ME's turn   -          -        -                on   first deck scene
  24800-24900    t8    K0 all drawn                  XP0        -        -                on   first deck scene
  24900-25000    t9    K7 nothing drawn, ME rests    -          -        -                on   first deck scene
  25000-25100    t10   K0 all drawn                  XP0        -        -                on   deck, change of scene
  25100-25200    t11   K0 all drawn                  XP0        -        -                on   nothing drawn
  25200-25300    t12   K0 all drawn                  XP0        -        -                on   nothing drawn
  25300-25400    t13   K7 nothing drawn, ME rests    -          -        -                on   nothing drawn
  25400-25500    t14   K8 nothing drawn, ME's turn   -          -        -                on   nothing drawn
  25500-25600    t15   K0 all drawn                  XP0        -        -                on   nothing drawn
  25600-25700    t16   K7 nothing drawn, ME rests    -          -        -                OFF  nothing drawn
  25700-25800    t17   K8 nothing drawn, ME's turn   -          -        uncached writes  OFF  nothing drawn
  25800-25900    t18   K0 all drawn                  XP0        -        -                on   nothing drawn
  25900-26000    t19   K0 all drawn                  XP0        -        -                on   nothing drawn
  26000-26100    t20   K0 all drawn                  XP0        -        -                on   deck, change of scene
  26100-26200    t21   K0 all drawn                  XP0        -        -                on   deck, change of scene
  26200-26300    t22   K0 all drawn                  XP3        -        -                on   steady deck
  26300-26400    t23   K0 all drawn                  XP0        -        -                on   steady deck
  26400-26500    t24   K7 nothing drawn, ME rests    -          -        -                on   steady deck
  26500-26600    t25   K8 nothing drawn, ME's turn   -          -        -                on   steady deck
  26600-26700    t26   K8 nothing drawn, ME's turn   -          -        arithmetic       on   steady deck
  26700-26800    t27   K8 nothing drawn, ME's turn   -          -        cached reads     on   steady deck
  26800-26900    t28   K8 nothing drawn, ME's turn   -          -        uncached writes  on   steady deck
  26900-27000    t29   K0 all drawn                  XP3        -        -                on   steady deck
  27000-27100    t30   K5 stage program off          XP0        -        -                on   steady deck
  27100-27200    t31   K7 nothing drawn, ME rests    -          -        -                on   steady deck
  27200-27300    t32   K0 all drawn                  XP3        -        -                OFF  steady deck
  27300-27400    t33   K8 nothing drawn, ME's turn   -          -        -                OFF  steady deck
  27400-27500    t34   K8 nothing drawn, ME's turn   -          -        arithmetic       on   steady deck
  27500-27600    t35   K8 nothing drawn, ME's turn   -          -        cached reads     on   steady deck
  27600-27700    t36   K8 nothing drawn, ME's turn   -          -        uncached writes  on   steady deck
  27700-27800    t37   K7 nothing drawn, ME rests    -          -        -                on   steady deck
  27800-27900    t38   K0 all drawn                  XP3        -        -                on   steady deck
  27900-28000    t39   K5 stage program off          XP0        -        -                on   steady deck
  28000-28100    t40   K0 all drawn                  XP0        -        -                on   steady deck
  28100-28200    t41   K7 nothing drawn, ME rests    -          -        -                on   steady deck
  28200-28300    t42   K8 nothing drawn, ME's turn   -          -        -                on   steady deck
  28300-28400    t43   K8 nothing drawn, ME's turn   -          -        uncached writes  on   steady deck
  28400-28500    t44   K0 all drawn                  XP3        -        -                on   steady deck
  28500-28600    t45   K7 nothing drawn, ME rests    -          -        -                on   steady deck
  28600-28700    t46   K0 all drawn                  XP0        -        -                on   steady deck
  28700-28800    t47   K0 all drawn                  XP0        -        -                on   change of scene
  28800-28900    t48   K0 all drawn                  XP0        -        -                OFF  change of scene
  28900-29000    t49   K0 all drawn                  XP0        -        -                OFF  change of scene
  29000-29100    t50   K0 all drawn                  XP0        -        -                on   rain begins
  29100-29200    t51   K0 all drawn                  XP0        -        -                on   rain wait, first turns
  29200-29300    t52   K0 all drawn                  XP3        -        -                on   rain wait, first turns
  29300-29400    t53   K7 nothing drawn, ME rests    -          -        -                on   rain wait, first turns
  29400-29500    t54   K0 all drawn                  XP3        last 10v -                on   rain wait, first turns
  29500-29600    t55   K0 all drawn                  XP7 check  on       -                on   rain wait
  29600-29700    t56   K0 all drawn                  XP0        off      -                on   rain wait
  29700-29800    t57   K0 all drawn                  XP3        off      -                on   rain wait
  29800-29900    t58   K7 nothing drawn, ME rests    -          off      -                on   rain wait
  29900-30000    t59   K0 all drawn                  XP1        off      -                on   rain wait
  30000-30100    t60   K0 all drawn                  XP3        off      -                on   rain wait
  30100-30200    t61   K0 all drawn                  XP0        off      -                on   rain wait
  30200-30300    t62   K8 nothing drawn, ME's turn   -          off      -                on   rain wait
  30300-30400    t63   K0 all drawn                  XP3        off      -                on   rain wait
  30400-30500    t64   K7 nothing drawn, ME rests    -          off      -                OFF  rain wait
  30500-30600    t65   K0 all drawn                  XP0        on       -                OFF  rain wait
  30600-30700    t66   K0 all drawn                  XP3        off      -                on   rain wait
  30700-30800    t67   K0 all drawn                  XP0        off      -                on   rain wait
  30800-30900    t68   K0 all drawn                  XP7 check  on       -                on   rain walk
  30900-31000    t69   K0 all drawn                  XP0        off      -                on   rain walk
  31000-31100    t70   K0 all drawn                  XP3        off      -                on   rain walk
  31100-31200    t71   K7 nothing drawn, ME rests    -          off      -                on   rain walk
  31200-31300    t72   K0 all drawn                  XP3        off      -                on   rain walk
  31300-31400    t73   K0 all drawn                  XP3        off      -                on   the walk ends
  (43 K0 turns, 2 K5, 14 K7, 15 K8; of the K0/K5 turns: XP0 28, XP1 1, XP3 14, XP7 2.)

What you will see. Before vblank 24,000 (the title, the opening, the talks) the picture is gl37's to gl39's. From the deck
on, in the K7 and K8 turns the picture STANDS STILL: up to five turns in a row on the radio screen (26,400-26,900 and
27,300-27,800, about 2 seconds a turn), and in the rain single turns of about 7 seconds each (29,300, 29,800, 30,200,
30,400, 31,100). THIS IS THE MEASUREMENT, not a fault: the game goes on underneath, and the sound with it. In the K0
turns the picture is the normal one; its first half second can carry a fading trace of the turn before. In the two K5
turns one face of the radio screen is missing. The XP turns look exactly like the others. Nothing to press, and there
is nothing to judge on the picture. The picture pauses 16 times for the saved shots and about every 3 seconds in the
walk for the captures, as before.

What it saves next to the EBOOT (about 25 MB in all, as gl39): shot_<vblank>.bmp (16), live_<vblank>.bmp (the walk's
captures every 25 vblanks), log.txt (about 1.2 MB: one 'gl40 plan' line more a turn), steps.csv, battery.txt, crash.txt.
What it touches: as gl28 to gl39. No new hardware writes, no new system calls or imports (the same 133 sce* names as
gl39, the stub sections the same sizes), nothing written to flash, 333 MHz as always, no power or clock setting; at the
end it waits on its end screen as before.
  - XP and M5 are main-CPU code changes only (the plan's sort, the kernel's interrupt walk); the probe reads the system's
    microsecond clock the main CPU's timers already read; no new call.
  - The Media Engine's load loop is gl39's (only in t17, t26-t28, t34-t36 and t43 now).
  - The ELF has gl39's layout for the loader: three loaded parts (code and constants, data, zeroed memory); the code part
    is exactly gl39's size.
  - Heap: the EBOOT's image ends 14.5 KB above gl39's, so the game's heap block starts at 0b23b900, the address gl36's had
    (gl36 ran on the PSP). Heap low point on the emulator: vb 23,677, the log line reads 64 KB (gl39: 64, gl36: 64). At
    the end the allocator can still hand out 64 KB in 1 block (as gl39).

What the run should show:
  - All 62 hashes equal to hashref.txt (c9ddb593, the same file as gl33 to gl39): the game's state is the same in every
    kind of turn, with every XP form and M5 on or off.
  - 155 frame digests (the digest-on frames of 29,490-31,000: t54's last 10 vblanks, t55, t65, t68), each equal to the
    emulator's and the PC twin's digest of the same kind of turn; all are K0. rd40pc.py carries the emulator's
    fingerprint: 'DIGESTS ... EQUAL'. t55 and t68 are drawn with XP 7 (the check), t54 with XP 3: the digests prove the
    new plan's copies on the PSP itself.
  - 315 'gl40 plan' lines (one a turn), 75 'nat turn' and 75 'nat f1' lines, 315 'gl38 bound' and 315 'gl39 probe' lines;
    the kind, F1, digest, load, XP and M5 as in the table (rd40pc.py's SCHEDULE CHECK must say 'none').
  - 'gl40 XP totals': plans 100604 (failed: ranges 0, extents 0, arena 388, chain 0); XP check: plans compared 1200,
    DIFFERING 0 (MUST be 0: 600 plans in each of t55 and t68). M5 walks about 3.6 million in all.
  - 'bound totals (gl38)': frames begun with a mask 1549; transfers left out by chain: 0:1449 1:1449 2:1449 3:1449 4:1449
      5:1449 6:1449 7:1449 8:1449 9:1449 10:1549 11:1449 12:999 13:750 14:750 15:750 16:744; native groups recorded and
      not drawn 208976; rain owners' calls 0
  - 'F1 totals (gl38)': runs planned 6403 (groups 778885, of them not written 682828, without the fast form 76858), runs
      not planned 0, planned runs with a problem 0; check mode: groups compared 0, DIFFERING 0
  - 'natcull: totals (v3)': effect calls tested 222315, left out 157085 (5865302 vertices); kept: vertex 0 inside 35898,
      no plane proven 29142, a bad attribute 0, count 0, out of the clamps' range 0, thinned 190
  - These counts depend on the game's state, not on speed, so the PSP's should be the same.
  - NAT PROBLEMS none; 'groups left untaken' in three turns (24,000: 524, 25,000: 423, 29,000: 2,037: changes of scene,
    as gl37 to gl39); 'planned runs with a problem 0'.
  - 'ME load entries not posted (the loop was behind)': the emulator run has 2. On the PSP it may be more (a load is
    left out when the Media Engine has not finished the one before); it is a count, not a fault.
  - 16 'the flip wait kept' lines, no 'GAVE UP' line.

The run's measurements (rd40pc.py prints them; all in ms a PSP step = a vblank):
  (a) XP, the lever: 'K0 XP3 minus K0 XP0' in the rain wait: t57 - t56, t60 - t61, t63 - t61; heavier part t66 - t67;
      walk t70 - t69 (the walk's content moves between those turns: read the plan probe's sort+join part there).
      'K0 XP1 minus K0 XP0': t59 - t56 (the ping-pong alone). In the play form (digests off, F1 on) the step is limited
      by the main CPU, so the plan's gain shows in the step.
  (b) THE PLAN PROBE ('gl40 plan' lines, PLAN PROBE table): the copy plan's own time a step by part (walk = the tag walk,
      sort+join = the merge and the joins, rest), in every K0 turn of the run, XP0 against XP3 directly; and the share of
      the K0 - K7 logic gap (10.7 ms a step in gl39's run) that is the plan.
  (c) M5: 'K7 M5 on minus off' (t58 against t64; other turns, so the content moves too) and the M5-off K0 turns t0, t1,
      t32 (XP3), t48, t49, t65 against their neighbours.
  (d) The digest with the check: t55 (XP 7, digests on) against t65 (XP 0, digests on, M5 off).
  (e) MAIN-DRAWING STEPS before vblank 24,000, against gl36's, gl38's and gl39's logs (the order's guard).
  (f) As gl39: the kinds among themselves (K0 against K7, K8 no load against K7), the coupling probes by turn.
  How to read the rain wait:
    - 29,100-29,500 (K0, K0, K7, K0): no frame digests yet (they start at 29,490) and the scene still settles.
    - 29,500-30,800: t55 (XP 7 with digests), then digest-off turns with XP 0, 1, 3 in turns, K7 at 29,800 and 30,400,
      K8 at 30,200; t65 (30,500) has digests and XP 0 and M5 off.
    - 30,600-30,800 is a heavier part of the scene (about +5 ms a step).
What to expect of the speed. Figures marked [C model] come from the PC cost model (ms a PSP step; the model put gl39's K7
turn at 68.28 against 67.06 on the PSP); [I] are inferred from them and from gl39's PSP run (K0 digest-off, F1 on, rain
wait: 78.11-78.16 ms a step, main-CPU-limited). Single estimates of this kind have been wrong by a factor of two before.
  - THE LEVER, XP 3 against XP 0, main CPU [C model, the plan's own functions]: -2.0 ms a step in the rain wait (t57 - t56
    -2.01, t60 - t61 -2.02, t63 - t61 -1.99) and -3.1 in the heavier part (t66 - t67). Of it: the merge -1.8, the copies
    back after each pass -0.9, the look-back joins +0.7. Whole windows, main CPU: -2.4, -2.4, -1.5, -2.8 [C model].
    On the PSP: -2.0 to -2.6 a step in the rain wait, -3.1 to -4.0 in the heavier part [I: x 1.0-1.3 as the plan].
  - XP 1 alone: -0.9 [C model: the copies back are gone, the merge itself is unchanged]; PSP -0.9 to -1.2 [I].
  - Expected steps, rain wait, play form (digests off, F1 on):
        K0 XP 0 (t56, t61)         about 78.1 (gl39's same turns 78.11-78.16; gl40's main CPU there -0.16 model) [I]
        K0 XP 1 (t59)              about 77.0 to 77.2 [I]
        K0 XP 3 (t57, t60, t63)    about 75.5 to 76.1 [I]
        heavier t66 XP 3 / t67 XP 0   about 3 to 4 apart [I]
        walk t70 XP 3 / t69 XP 0   the walk's content grows between them (+6 model in the whole window): compare the
                                   plan probe's sort+join part, not the step
        K7 (t58 M5 on, t64 M5 off) about 67.3 / 68.2 [I: gl39's K7 67.06 at t57 + the model's +0.25 / +1.11]
        K8 no load (t62)           K7 + 0.7 [C model]
    The Media Engine needs about 65 to 70 a step there [I], so the K0 step stays main-limited with XP 3 and the gain
    shows in full.
  - THE PLAN PROBE, the copy plan's time a step in the rain wait [C model x 1.0-1.3, I]: XP 0 about 6.5 to 8.5 ms (the
    tag walk 1.9, the merge 2.6, the joins 1.1, the copies back 0.9 model), XP 3 about 4.5 to 5.8. If it reads about 8 in
    the XP 0 turns, the plan is most of the K0 - K7 logic gap (10.7); if about 4, the rest of the gap is still UNKNOWN.
    (The emulator's own clock is no guide to the PSP's here.)
  - M5 [C model, the kernel's interrupt walk by function]: 0.28 ms a step on against 0.60 off: -0.32 a step in every kind
    of turn, main CPU. On the PSP about -0.3 to -0.4 [I], about one turn's spread, so a single K7 pair may not show it.
  - t55 (XP 7 check + digests): the check adds about +4.6 model to the main CPU; the step may then be main-limited at about
    86 to 89 [I] (gl39's digest-on turn: 84.6, Media-Engine-limited). t65 (XP 0, digests on, M5 off): about 84.6 [I].
  - gl40 against gl39 in the same turn and form [C model]: K0 XP 0 main CPU -0.16 (with M5's -0.32 in it), Media Engine
    +0.9 (code placement; the step is main-limited there); K7 (M5 off in both) +0.50; the walk's K0 XP 0 main -0.02.
  - MAIN-DRAWING TURNS before vblank 24,000 [C model, the four guard windows]: gl40 against gl39 -1.52, -1.22, -0.12,
    -1.35; against gl36 +0.28, +0.23, +0.03, +0.22 (instruction-cache misses below gl36's in all four; what is left is
    data). Expected PSP: about 55.6 to 55.9 a step (gl39 56.63, gl36 55.56) [I: model deltas x ~1].

How it runs: as gl39's. The files go over the last build's folder on the stick (renamed MGS2T_gl40); at the end
("Press X to exit") the build WAITS on its end screen: press X and start "PSPLink (PC control)" so the logs can be
copied to LOGS\gl40. About as long as gl39's run. Keep the USB cable in, as before.

Other settings the same EBOOT takes (for a later run, not this one), each checked on the PC twin or the emulator:
  - MGS2_XP_CYCLE absent: XP 0 (gl39's plan) in every turn. MGS2_PROBE10 absent: the plan probe counts but times nothing.
  - MGS2_OPT2 / MGS2_OPT2_CYCLE without bit 18: M5 off (gl39's interrupt walk) everywhere.
  - gl39's keys as before (MGS2_DIG_CYCLE, MGS2_LOAD_CYCLE, MGS2_PROBE9, MGS2_BOUND_CYCLE, MGS2_NAT_F1 and F1's check mode,
    MGS2_NAT_REUSE, MGS2_NAT_CULL, the kinds K1 to K6, MGS2_GL_RAIN_NEAR).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd40pc.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are; their md5s are in the manifest too.

Reading the logs: on the PC   python -I rd40pc.py <LOGS\gl40>     (one file, standard library only, no other files needed)
It prints: the end of the run, the crash record, HASHES and DIGESTS against the emulator run of this package (must say
EQUAL), the main-drawing steps before 24,000, the turn table (with XP and M5), the SCHEDULE CHECK, each scene's kinds with
the XP and M5 differences, the coupling probes by turn, the PLAN PROBE by turn, the problem counters (with the XP check),
the battery.
