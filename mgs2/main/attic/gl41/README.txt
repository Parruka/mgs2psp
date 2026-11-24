MGS2 gl41: a MEASUREMENT build like gl39 and gl40. From the deck on, each turn of 100 vblanks has a KIND: everything drawn as
today (K-A), the ship drawn by the new native renderer (K-N1, step 1), K-A with one of the two new main-CPU levers off (P1 or
P3), or nothing drawn (K7, the picture stands still ON PURPOSE). The game itself (its state, every hash) and the picture's
content are the same in every turn. Personal use only, do not share: it contains code built from your MGS2 disc.
2026-11-24. Build report: mgs2/main/attic/gl41/report.txt. NOT DELIVERED by the build agent: the lead verifies and delivers.

What it is: gl40's measurement build with a new EBOOT (and the 8 files next to it, rebuilt with it) and these changes in
mgs2.cfg, each marked 'gl41' there. Every other setting is gl40's.
  1. New key MGS2_NGE_CYCLE (one digit a turn of 100 vblanks from vblank 24,000): 1 = NATIVE STEP 1: each of the ship's
     meshes is made once (cooked, on the main CPU) into a list the GE calls, kept in a 256 KB region at the end of each frame
     memory (fixed, made at the start); the Media Engine then writes a short batch for each ship mesh (its matrix and a CALL
     into the cooked list) instead of converting it again every frame. 0 = today's path. Everything else (the people, the
     effects, the rain, any mesh that does not fit, every fallback) is drawn today's way in both. The picture is the same:
     on the PC twin the native batches were compared with today's in every ship mesh of vblanks 24,000-37,800 (1,626,909
     meshes, 0 differences); on the emulator every picture's content in this package's K-N1 turns equals the same run with
     MGS2_NGE_CYCLE all 0 (811 of 811 pictures), and the shots taken in K-N1 turns are pixel-equal to it.
  2. MGS2_OPT2's bit 19 is P1 (a call of one of the game's compiled VU0 programs takes a fast path: a small table keyed by
     the program's image and entry gives the two functions to call, so the full dispatch is skipped; a miss takes gl40's
     way) and bit 20 is P3 (the box cull, one of those programs, without its 8 corner checks, which never change its
     result: probe b checks the VFPU rule this relies on). Both ON (fffff9df) in every turn but their own: P1 off
     (fff7f9df) in 8 turns, P3 off (ffeff9df) in 8 turns, each between two K-A turns of the same scene. MGS2_OPT_CYCLE /
     _OPT2_CYCLE / _HALF_CYCLE now hold a word for each of the 74 turns (gl40: 16).
  3. MGS2_BOUND_CYCLE / MGS2_DIG_CYCLE / MGS2_XP_CYCLE: gl41's turns (table below). Kinds K-A, K-N1, K7 only: no K5, no K8,
     no Media Engine load (MGS2_LOAD_CYCLE and MGS2_LOAD_US taken out). XP 3 in every turn (gl40's PSP log: XP 3 is the best
     form), XP 7 (3 + the check) in the digest turns. M5 ON in every turn (gl40's log: 0 to +1.1 ms when off). F1 on.
     Frame digests on in t55 (K-A), t56 (K-N1), t73 (K-N1) and the last 10 vblanks of t54.
  4. New keys MGS2_PROBE41=3 and MGS2_PROBE41_CULL=5000, MGS2_L41_LOG=1: lines 'L41 probe a / b / c' and 'gl41 levers':
       a  (at the start, after b) the time a missed 64-byte line costs, in a timed walk that only reads the EBOOT's own
          code, with and without a data-cache fill ('cache 0x1e') one or two lines ahead;
       b  (at the start) the VFPU rule P3 relies on (a value that is not finite stays not finite through the multiplies the
          box cull uses, and vmov copies bits), on fixed values;
       c  (at the box cull's 5,000th call) the cull's forms on the same recorded input, timed (gl39's form, P3's, the C form),
          and 690 forced inputs through both forms (the results must be equal);
       'gl41 levers' every 100 vblanks: the MGS2_OPT2 word in force and P1's own counts (calls that missed the fast path,
          and entries filled in its table).
     If the run stops at its very start with an exception and the last lines of log.txt name 'L41 probe a', set
     MGS2_PROBE41=2 (probe b only) and run again: probe a's cache op is the one thing in this build never run on a PSP.
Also in the EBOOT, with no change for the game or the picture:
  - THE HEAP FIX. gl40 left too little room for one moment of the route: at vblank ~10,850 the texture cache's table grew
    from 64 to 128 KB, and with step 1's code added it no longer fitted (on the emulator, textures went missing; the
    hashes stayed equal, so only the picture showed it). Now:
      (a) the texture table is 128 KB from its first entry (no growth on the route; growths refused are counted);
      (b) the log says how much room the game's heap (newlib's) has: its end, the highest point its break reached on the
          route (with the vblank) and the room above it then, and the room at each of the picture heap's events and at the
          texture table's allocation ('memory (gl41)' and 'picture heap (gl41)' lines, start and end);
      (c) native step 1's 9.2 KB of scratch is a block of the system's memory outside the game's heap, and its cooker is
          built for size;
      (d) the main thread's own stack part is 128 KB (was 256 KB); it has used 2.9 KB in every PSP and emulator run since
          gl16 (the 'stack:' line at the end says it again). Those 128 KB go to the game's heap. Without (d) the new
          profile's code (+72 KB) would have left about 12 KB at the worst moment.
    Emulator, this package: the worst moment of the route is the texture table's allocation at vb 536: 135 KB above the
    break + 34 KB free at the heap's top = 170 KB (the gate: at least 32 KB); 0 textures refused for want of memory.
  - Native step 1's own fix, found in this build: with the picture loop running apart from the main CPU (as on the PSP's
    Media Engine), the batches read their mesh references from the main CPU's buffers after these were filled again for a
    later picture. The references now travel with the recorded meshes. (Checked: emulator with the loop apart, every K-N1
    picture's content equal to the switch-off run; the PC twin unchanged.)
  - A 'picture: vb N content X' line in each digest picture: the picture's content, the same in K-A and K-N1 turns (a
    native batch writes a CALL, so the list and frame digests of K-N1 pictures differ from K-A's by design).
  - A new profile (trained with native turns in it) and a new code order in two stages: gl40's guard first (the four
    main-drawing windows of the opening and the deck at or under gl36's instruction-cache misses + 2 %), then weighted to
    the rain wait and the walk with the guard kept. The model puts all four guard windows 5 to 19 % under gl36's misses.
  - The data pads chosen again (the constants at gl40's address, the Media Engine's tables at gl40's data-cache sets, the
    game's heap block at gl40's offset in its 8 KB page).
NOT in gl41 (later builds): people drawn natively (step 2), E6, H1, the dice cut, the lamps.

THE TURNS (100 vblanks each; OPT2 = MGS2_OPT2's word; XP as in gl40; digests: '-' = none exist before vblank 29,490,
on / off = MGS2_DIG_CYCLE; M5 and F1 on in every turn):
  vblanks        turn kind                         OPT2      XP        digests  scene
  24000-24100    t0   K-A all drawn (today)        fffff9df  XP3       -        deck, change of scene
  24100-24200    t1   K-N1 ship drawn natively     fffff9df  XP3       -        first deck scene
  24200-24300    t2   K-A all drawn (today)        fffff9df  XP3       -        first deck scene
  24300-24400    t3   K-A with P1 off              fff7f9df  XP3       -        first deck scene
  24400-24500    t4   K-A all drawn (today)        fffff9df  XP3       -        first deck scene
  24500-24600    t5   K-N1 ship drawn natively     fffff9df  XP3       -        first deck scene
  24600-24700    t6   K-A all drawn (today)        fffff9df  XP3       -        first deck scene
  24700-24800    t7   K-A with P3 off              ffeff9df  XP3       -        first deck scene
  24800-24900    t8   K-A all drawn (today)        fffff9df  XP3       -        first deck scene
  24900-25000    t9   K7 nothing drawn, ME rests   fffff9df  XP3       -        first deck scene
  25000-25100    t10  K-A all drawn (today)        fffff9df  XP3       -        deck, change of scene
  25100-25200    t11  K-N1 ship drawn natively     fffff9df  XP3       -        deck (first play)
  25200-25300    t12  K-A all drawn (today)        fffff9df  XP3       -        deck (first play)
  25300-25400    t13  K-A with P1 off              fff7f9df  XP3       -        deck (first play)
  25400-25500    t14  K-A all drawn (today)        fffff9df  XP3       -        deck (first play)
  25500-25600    t15  K-N1 ship drawn natively     fffff9df  XP3       -        deck (first play)
  25600-25700    t16  K-A all drawn (today)        fffff9df  XP3       -        deck (first play)
  25700-25800    t17  K-A with P3 off              ffeff9df  XP3       -        deck (first play)
  25800-25900    t18  K-A all drawn (today)        fffff9df  XP3       -        deck (first play)
  25900-26000    t19  K7 nothing drawn, ME rests   fffff9df  XP3       -        deck (first play)
  26000-26100    t20  K-A all drawn (today)        fffff9df  XP3       -        deck, change of scene
  26100-26200    t21  K-N1 ship drawn natively     fffff9df  XP3       -        deck, change of scene
  26200-26300    t22  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  26300-26400    t23  K-A with P1 off              fff7f9df  XP3       -        steady deck
  26400-26500    t24  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  26500-26600    t25  K-N1 ship drawn natively     fffff9df  XP3       -        steady deck
  26600-26700    t26  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  26700-26800    t27  K-A with P3 off              ffeff9df  XP3       -        steady deck
  26800-26900    t28  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  26900-27000    t29  K7 nothing drawn, ME rests   fffff9df  XP3       -        steady deck
  27000-27100    t30  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  27100-27200    t31  K-N1 ship drawn natively     fffff9df  XP3       -        steady deck
  27200-27300    t32  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  27300-27400    t33  K-A with P1 off              fff7f9df  XP3       -        steady deck
  27400-27500    t34  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  27500-27600    t35  K-N1 ship drawn natively     fffff9df  XP3       -        steady deck
  27600-27700    t36  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  27700-27800    t37  K-A with P3 off              ffeff9df  XP3       -        steady deck
  27800-27900    t38  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  27900-28000    t39  K7 nothing drawn, ME rests   fffff9df  XP3       -        steady deck
  28000-28100    t40  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  28100-28200    t41  K-N1 ship drawn natively     fffff9df  XP3       -        steady deck
  28200-28300    t42  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  28300-28400    t43  K-A with P1 off              fff7f9df  XP3       -        steady deck
  28400-28500    t44  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  28500-28600    t45  K-N1 ship drawn natively     fffff9df  XP3       -        steady deck
  28600-28700    t46  K-A all drawn (today)        fffff9df  XP3       -        steady deck
  28700-28800    t47  K-A with P3 off              ffeff9df  XP3       -        change of scene
  28800-28900    t48  K-A all drawn (today)        fffff9df  XP3       -        change of scene
  28900-29000    t49  K7 nothing drawn, ME rests   fffff9df  XP3       -        change of scene
  29000-29100    t50  K-A all drawn (today)        fffff9df  XP3       -        rain begins
  29100-29200    t51  K-N1 ship drawn natively     fffff9df  XP3       -        rain wait, first turns
  29200-29300    t52  K-A all drawn (today)        fffff9df  XP3       -        rain wait, first turns
  29300-29400    t53  K-A with P1 off              fff7f9df  XP3       -        rain wait, first turns
  29400-29500    t54  K-A all drawn (today)        fffff9df  XP3       last 10v rain wait, first turns
  29500-29600    t55  K-A all drawn (today)        fffff9df  XP7 check on       rain wait
  29600-29700    t56  K-N1 ship drawn natively     fffff9df  XP7 check on       rain wait
  29700-29800    t57  K-A with P3 off              ffeff9df  XP3       off      rain wait
  29800-29900    t58  K-A all drawn (today)        fffff9df  XP3       off      rain wait
  29900-30000    t59  K-N1 ship drawn natively     fffff9df  XP3       off      rain wait
  30000-30100    t60  K-A all drawn (today)        fffff9df  XP3       off      rain wait
  30100-30200    t61  K-A with P1 off              fff7f9df  XP3       off      rain wait
  30200-30300    t62  K-A all drawn (today)        fffff9df  XP3       off      rain wait
  30300-30400    t63  K-A with P3 off              ffeff9df  XP3       off      rain wait
  30400-30500    t64  K7 nothing drawn, ME rests   fffff9df  XP3       off      rain wait
  30500-30600    t65  K-N1 ship drawn natively     fffff9df  XP3       off      rain wait
  30600-30700    t66  K-A all drawn (today)        fffff9df  XP3       off      rain wait (heavier)
  30700-30800    t67  K-N1 ship drawn natively     fffff9df  XP3       off      rain wait (heavier)
  30800-30900    t68  K-N1 ship drawn natively     fffff9df  XP3       off      rain walk
  30900-31000    t69  K-A all drawn (today)        fffff9df  XP3       off      rain walk
  31000-31100    t70  K-A with P1 off              fff7f9df  XP3       off      rain walk
  31100-31200    t71  K-A all drawn (today)        fffff9df  XP3       off      rain walk
  31200-31300    t72  K-A with P3 off              ffeff9df  XP3       off      rain walk
  31300-31400    t73  K-N1 ship drawn natively     fffff9df  XP7 check on       the walk ends
  (35 K-A turns, 17 K-N1, 8 P1 off, 8 P3 off, 6 K7.)
  Native step 1 draws only where the ship's meshes are on screen: in the first deck scene, the deck's first play and the
  whole rain. In the steady deck (26,000-29,000) the K-N1 turns had no ship mesh to draw on the emulator (0 native calls)
  and are K-A turns in effect (a control: they must read as K-A). The first picture of each K-N1 turn is counted in the
  line of the turn before (the switch acts per picture; the line is written at the next turn's first picture).

What you will see. Before vblank 24,000 the picture is gl40's. From the deck on, in the 6 K7 turns the picture STANDS
STILL for one turn: about 2 seconds at 24,900, 25,900, 26,900, 27,900 and 28,900, about 7 seconds at 30,400 in the rain.
THIS IS THE MEASUREMENT, not a fault: the game goes on underneath, and the sound with it. Every other turn shows the normal
picture: K-N1, P1-off and P3-off turns look exactly like K-A. The first half second after a K7 turn can carry a fading
trace of it. Nothing to press, and nothing to judge on the picture. The picture pauses 16 times for the saved shots and
about every 3 seconds in the walk for the captures, as before; 4 of the shots are in K-N1 turns (30502, 30548, 30724,
30826): the native picture on the PSP itself, to compare with gl40's shots at the same vblanks.

What it saves next to the EBOOT (about 25 MB in all, as gl40): shot_<vblank>.bmp (16), live_<vblank>.bmp (the walk's
captures every 25 vblanks), log.txt (about 1.4 MB), steps.csv, battery.txt, crash.txt.
What it touches: as gl28 to gl40. No new hardware writes, no new system calls or imports (the same 133 sce* names as
gl40, the stub sections the same sizes), nothing written to flash, 333 MHz as always, no power or clock setting; at the end
it waits on its end screen as before.
  - Native step 1: the GE runs CALLs into two 256 KB regions of main RAM made at the start (lists stay in main RAM).
  - P1, P3: main-CPU code only. Probe a: reads of the EBOOT's own code and data-cache fills of those lines (cache op 0x1e),
    once, at the start. Probe b: VFPU multiplies on fixed values. Probe c: the box cull run on a copy of one recorded call
    (the entry state restored before the real call). The probes read the system clock the runtime already imports.
  - The ELF has gl40's layout for the loader: three loaded parts (code and constants, data, zeroed memory).
  - Heap: the game's heap block starts at 0b247900 (gl40's offset in its page); the game's heap ends 128 KB higher than
    gl40's (the main thread's part, see (d)).

What the run should show (counts that depend on the game's state, not on speed, so the PSP's should be the same as the
emulator's run of this package; rd41pc.py checks the first three):
  - All 62 hashes equal to hashref.txt (c9ddb593, the same file as gl33 to gl40): the game's state is the same in every
    kind of turn, with native step 1, P1 and P3 on or off.
  - 116 frame digests (t54's last 10 vblanks, t55 K-A, t56 K-N1, t73 K-N1), equal to the emulator's: rd41pc.py says
    'DIGESTS ... EQUAL' and 'CONTENT ... EQUAL' (116 content lines). The K-A digests are also the PC twin's of the same
    turns; the K-N1 lists differ from K-A's by design, their content does not.
  - 'nge totals': pictures 510, calls 52902, calls native 37748 (reused 37476, cooked 272), evicted 37, no room 0, not
    cookable 0, table full 0. 'nge converter': native batches 76469, every 'declined' count 0.
    'native step 1 totals': no array 0, the builder run instead 0.
  - 'gl40 XP totals': plans 118546 (arena 388, the other failures 0); XP check: plans compared 1320, DIFFERING 0 (MUST be
    0: the digest turns t55, t56, t73). M5 walks about 3.8 million in all.
  - 'F1 totals (gl38)': runs planned 9298 (groups 851258, not written 722734), runs not planned 0, with a problem 0.
  - 'bound totals (gl38)': frames begun with a mask 250; native groups recorded and not drawn 136878.
  - 'natcull: totals (v3)': effect calls tested 318935, left out 231224 (8309362 vertices).
  - 'L41 probe b: ... holds (0 cases broke it)'; 'L41 probe c: ... DIFFERENT results 0, different return 0'.
  - HEAP (rd41pc.py prints it): 'textures refused for want of memory: 0 (the pool 0, the table 0)'; the texture table's
    event at vb 536 with about 135 KB above the break + 34 KB free top; the ME heap events' lowest room + top about 158 KB;
    'memory at the end: blocks malloc can still give (KB): 144'. The PSP has had MORE room than the emulator so far
    (gl40's end of run: 2,608 KB in one block still free on the PSP, 64 KB on the emulator), so these are likely the low
    side [I];
    the first 'memory (gl41)' line gives the heap's end on the PSP (the emulator's: 0be9bfff).
  - 'stack: the main thread above the pool ... uses about 2948 of 129472 bytes' (well under the new 128 KB).
  - The 'me picture loop (gl41)' stack line is the emulator's (the loop runs there as a thread: deepest use 5276 of 16384 B);
    on the PSP the loop runs on the Media Engine itself and the line is not printed.
  - 314 'gl41 levers' lines, 315 'gl40 plan' lines, 75 'nat turn' and 75 'nat f1' lines; the kind, OPT2 word, digests, XP,
    M5 and native calls as in the table (rd41pc.py's SCHEDULE CHECK must say 'none').
  - NAT PROBLEMS none; 'groups left untaken' in three turns (24,000: 524, 25,000: 423, 29,000: 2,037: changes of scene, as
    gl37 to gl40); 16 'the flip wait kept' lines, no 'GAVE UP' line.

The run's measurements (rd41pc.py prints them; all in ms a PSP step = a vblank). The rain is MAIN-CPU-bound (gl40's PSP
log: main logic 42.7 + pipe 26.3 + gsw 4.4, the ME busy 69.7 of a 76.44 step) and the steady deck ME-bound (40.3, GE wait
15.6), so every pair is read on BOTH sides: the main CPU (logic, pipe, gsw) and the ME (ME busy), and the step follows the
larger of the two.
  (a) NATIVE STEP 1: K-N1 minus its K-A neighbours: rain wait t59 - t58 / t60, the heavier part t67 - t66, the first rain
      t51 - t50 / t52, the walk t68 - t69 (the walk's content moves between turns), the deck's first play t11 / t15 against
      t12, t14, t16; the steady deck's K-N1 turns are the control (no ship mesh: must read as K-A).
  (b) P1: P1-off minus K-A: rain t61 - t60 / t62, first rain t53 - t52 / t54, walk t70 - t69 / t71, deck t3, t13, t23, t33,
      t43 against their neighbours. Main-CPU code: in the rain it shows in the step.
  (c) P3: P3-off minus K-A: rain t57 - t58, t63 - t62, walk t72 - t71, deck t7, t17, t27, t37, t47. Small (see below); probe
      c gives the cull's own time a call on the PSP.
  (d) K-A minus K7 (t64 in the rain, t9 / t19 / t29 / t39 / t49 in the deck): the picture's cost on the main CPU.
  (e) MAIN-DRAWING STEPS before vblank 24,000 against gl40's log (the order's guard).
  (f) Probe a (the miss and fill times: whether a fill ahead helps on the PSP), probe c (the cull's forms), the PLAN PROBE.
What to expect of the speed. [C model] = the PC cost model, ms a PSP step, read on BOTH sides: main = the main CPU (game,
runtime, the copy plan, the rest), ME = the Media Engine's work (with the digests where they are on); the model's form of
the step is P = max(main, 0.8 x ME) [I]. [I] = inferred from those and gl40's PSP run (rain wait, K0 XP3: step 76.44, main
logic 42.74, ME busy 69.68; steady deck 40.3 to 40.7, ME-bound). On the PSP read P as max(main side, ME busy), never the
step alone. Single estimates of this kind have been wrong by a factor of two before.
  RAIN WAIT (t57-t64; main-CPU-bound in every drawn turn, model main 84.3-86.1 against 0.8 ME 51.1-51.8):
    K-A (t58, t60, t62)   gl41 - gl40 same turn t60: main -0.84, ME +0.23 [C model] (P1 and P3 on, the new profile and
                          order). PSP: step about 75.6 (75.2 to 76.0), ME busy about 69.9 [I]
    K-N1 (t59)            minus K-A t58 / t60: ME -3.10 / -2.71, main +0.29 / -0.14 [C model]. PSP: ME busy about 66.5 to 67.3
                          (-2.7 to -3.4), the step about K-A's (+-0.5): main-bound, so step 1's gain shows in ME busy and
                          ME idle, NOT in the step [I]
    P1 off (t61)          minus K-A t60 / t62: main +1.37 / +0.94, ME +0.66 / -0.20 [C model]. PSP: step +0.9 to +1.4 [I]
                          (main-bound: P1's gain shows in the step)
    P3 off (t57, t63)     minus K-A t58 / t62: main +0.03 / +0.41, ME -0.00 / -0.34 [C model]. PSP: +0 to +0.4 [I], inside the
                          K-A spread (K-A - K-A: main +0.43 / +0.43); probe c gives the cull's own time a call
    K7 (t64)              main 67.64; gl41 - gl40 (t58, M5 on in both) -0.86 [C model]. PSP: gl40's K7 step - 0.9 [I]
  HEAVIER PART (t66 K-A, t67 K-N1): K-N1 - K-A: main +0.55, ME -0.54 [C model] (a smaller ME gain than in the wait).
    gl41 - gl40 t66: main -1.11, ME -0.51. t65 (K-N1) sits before the heavier part (main -5.1 against t66): not a pair.
  THE WALK (t68 K-N1, t69 K-A, t70 P1 off, t71 K-A, t72 P3 off): the walk's content grows from turn to turn (K-A t71 - t69:
    main +2.39, ME +4.06 [C model]), so whole-turn pairs there are mostly content: t68 - t69 main +5.45 / ME +9.06, t70 - t69
    +8.65 / +7.75, t70 - t71 +6.27 / +3.70, t72 - t71 +0.63 / +1.69 [C model]. Read P1 there by 'gl41 levers' and the main
    logic of the neighbours, not by the step.
  FIRST RAIN (t51 K-N1, t52 K-A, t53 P1 off, t54 K-A; the scene still settles): t51 - t52 main +2.01, ME -2.04; t53 - t52
    main +0.75, ME -1.18; t53 - t54 +5.12 / +5.49 (content) [C model].
  DIGEST TURNS (XP 7 + digests on: t55 K-A, t56 K-N1, t73 K-N1): t55 main 91.47, ME 103.44 (digests 34.64); t56 - t55 main
    -0.49, ME -0.45 (the K-N1 digest costs +1.7 more) [C model]. PSP: about K-A + 4.5 (digests) + 6.5 (the check) = 86 to 87
    [I: gl40's digest and XP 7 costs]. Measurement turns only.
  STEADY DECK (t22-t46; ME-bound, model main 30.1-30.4 against ME 39.7-40.1): K-N1 - K-A +0.05 to +0.38 (no ship mesh there:
    the K-A spread is +0.34), P1 off - K-A -0.04 / +0.26, P3 off - K-A +0.33 (t37 - t34) [C model]: every kind within about
    0.4 of K-A, because the levers act on the main CPU, which has ~10 ms of room there. PSP: about gl40's 40.3 to 40.7 for
    every kind [I; the deck's package windows were not modelled for gl40, so gl41 - gl40 there is UNKNOWN to about +-0.5].
    K7 (t29, t39): main 19.95 [C model].
  DECK, FIRST PLAY (t10-t19; K-N1 t11, t15 draw 3,000 and 1,346 ship calls natively a turn): not modelled, UNKNOWN.
  MAIN-DRAWING STEPS before vblank 24,000 (the order's guard, gl38's guard cfg windows): gl41 - gl40 -0.79 / -1.14 / +0.28 /
    -0.57, gl41 - gl36 -0.51 / -0.91 / +0.30 / -0.35; instruction-cache misses 5.5 / 18.6 / 6.9 / 5.9 % under gl36's
    [C model]. PSP: about 55.0 (gl40 55.51, gl36 55.56) [I: the model deltas x 1].

How it runs: as gl40's. The files go over the last build's folder on the stick: update MGS2T_gl40 MGS2T_gl41 (from the
zip). At the end ("Press X to exit") the build WAITS on its end screen: press X and start "PSPLink (PC control)" so the
logs can be copied to LOGS\gl41. About as long as gl40's run (the same route, to vblank 31,400). Start with the battery
charged: gl40's log read charging 0 / external power 0 with the cable in (both runs), so the run cannot count on USB
power; gl40's run 2 used 9 % of the battery in its 30 minutes.

Other settings the same EBOOT takes (for a later run, not this one), each checked on the PC twin or the emulator:
  - MGS2_NGE_CYCLE absent and MGS2_NGE absent or 0: today's path in every turn. MGS2_NGE=1: native step 1 in every turn.
  - MGS2_OPT2 / MGS2_OPT2_CYCLE without bit 19 / 20: P1 / P3 off. MGS2_PROBE41 absent: no probes a, b; MGS2_PROBE41_CULL
    absent: no probe c; MGS2_L41_LOG absent: no 'gl41 levers' line.
  - gl40's keys as before (MGS2_XP_CYCLE, MGS2_PROBE10, MGS2_DIG_CYCLE, MGS2_LOAD_CYCLE, MGS2_PROBE9, MGS2_BOUND_CYCLE with
    the kinds K1 to K8, MGS2_NAT_F1 and F1's check mode, MGS2_NAT_REUSE, MGS2_NAT_CULL, MGS2_GL_RAIN_NEAR).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd41pc.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are; their md5s are in the manifest too.

Reading the logs: on the PC   python -I rd41pc.py <LOGS\gl41>     (one file, standard library only, no other files needed)
It prints: the end of the run, the crash record, HASHES, DIGESTS and CONTENT against the emulator run of this package (must
say EQUAL), HEAP, the L41 probes, the main-drawing steps before 24,000, the turn table (kind, OPT2, XP, digests; main CPU
and ME columns; native counts; P1's misses), the SCHEDULE CHECK, each scene's kinds with their differences (main CPU and
ME apart), each lever turn against its K-A neighbours, native step 1 by turn, the plan probe, the problem counters, the
battery.
