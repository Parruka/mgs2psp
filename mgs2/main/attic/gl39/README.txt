MGS2 gl39: a MEASUREMENT build. From the deck on, in 34 of the 74 turns NOTHING is drawn and the picture stands still ON
PURPOSE, so the PSP can time the main CPU alone, the main CPU beside a busy Media Engine, and the rain without the frame
digest (a check's cost). The game itself (its state, every hash) is the same in every turn. Personal use only, do not
share: it contains code built from your MGS2 disc.
2026-11-23. Build report: mgs2/main/attic/gl39/report.txt. NOT DELIVERED by the build agent: the lead verifies and delivers.

What it is: gl38's measurement build with a new EBOOT (and the 8 files next to it, rebuilt with it) and these changes in
mgs2.cfg, each marked 'gl39' there. Every other setting is gl38's.
  1. MGS2_BOUND_CYCLE: gl39's turns (one digit for each turn of 100 vblanks from vblank 24,000, the KIND of the turn):
       K0  everything drawn.
       K5  the stage's own program off (two turns in the steady deck, as in gl38: one face of the radio screen).
       K7  nothing drawn: the Media Engine rests and the main CPU runs the game alone (the last picture stays).
       K8  NEW: nothing drawn, but the turn is the Media Engine's: empty pictures go through the hand-over, the Media
           Engine and the GE, and the Media Engine can be given a load (2.).
  2. New keys MGS2_LOAD_CYCLE (a digit a turn, read in K8 turns only) and MGS2_LOAD_US=100000: what the Media Engine
     does besides, for 100 ms for each picture (2 vblanks):
       0 nothing | 1 arithmetic in registers | 2 reads of the program's own code through its cache (every read fetches
       a line from RAM) | 3 writes to a 256-byte block of its own past the cache (every write goes to RAM).
     K8 with each load against K7 tells how much the main CPU's game logic slows when the Media Engine is busy that way.
  3. New key MGS2_DIG_CYCLE (a digit a turn): 1 = no frame digest for the pictures that end in the turn. The digest is a
     proof tool hashed by the Media Engine (gl38: the largest part of its "list build"); a game build makes none.
  4. New key MGS2_PROBE9=1 and a 'gl39 probe' line for each turn in log.txt: how often the main CPU served the picture
     queue and wrote its cache back for the Media Engine, in which timed part, and how long (one service call in 8 and
     every whole-cache write-back are timed: two clock reads each).
  5. F1 on in every turn but two (30,300 and 30,500). The re-use is off in the whole run (MGS2_NAT_REUSE_CYCLE taken out).
  6. MGS2_OPT_CYCLE, MGS2_OPT2_CYCLE, MGS2_HALF_CYCLE have 16 values (were 8); in two of the 16 MGS2_OPT2_CYCLE clears
     bit 18, which does nothing in this EBOOT (see 'NOT in the EBOOT' below). The effect is gl38's in every turn.
  7. MGS2_GL_RAIN_NEAR taken out (there is no K4 turn).
Also in the EBOOT, with no change for the game:
  - a new profile and code order, trained on gl39's own turns;
  - the picture code's hottest small tables in one block, and three pads (13 KB of memory in all) that put the tables
    and constants back on the data-cache positions they had in gl38.
NOT in the EBOOT, though the plan named them: M5 (the game kernel's interrupt walk). Its code went into the PC twin's
kernel file only, so MGS2_OPT2's bit 18 (cleared in the cfg for turns t0, t1, t16, t17, ...) does NOTHING on the PSP. The
plan's other main-CPU code levers (M1 to M4, M6) were dropped before the build. So this build measures; it does not make
the main CPU faster (see 'What to expect').

THE TURNS (100 vblanks each; digests: '-' = none exist before vblank 29,490, on / off = MGS2_DIG_CYCLE):
  vblanks        turn  kind                          F1   digests  ME load          scene
  24000-24100    t0    K0 all drawn                  on   -        -                deck, change of scene
  24100-24200    t1    K0 all drawn                  on   -        -                first deck scene
  24200-24300    t2    K7 nothing drawn, ME rests    on   -        -                first deck scene
  24300-24400    t3    K8 nothing drawn, ME's turn   on   -        -                first deck scene
  24400-24500    t4    K0 all drawn                  on   -        -                first deck scene
  24500-24600    t5    K0 all drawn                  on   -        -                first deck scene
  24600-24700    t6    K7 nothing drawn, ME rests    on   -        -                first deck scene
  24700-24800    t7    K8 nothing drawn, ME's turn   on   -        -                first deck scene
  24800-24900    t8    K0 all drawn                  on   -        -                first deck scene
  24900-25000    t9    K7 nothing drawn, ME rests    on   -        -                first deck scene
  25000-25100    t10   K0 all drawn                  on   -        -                deck, change of scene
  25100-25200    t11   K0 all drawn                  on   -        -                nothing drawn
  25200-25300    t12   K0 all drawn                  on   -        -                nothing drawn
  25300-25400    t13   K7 nothing drawn, ME rests    on   -        -                nothing drawn
  25400-25500    t14   K8 nothing drawn, ME's turn   on   -        -                nothing drawn
  25500-25600    t15   K0 all drawn                  on   -        -                nothing drawn
  25600-25700    t16   K7 nothing drawn, ME rests    on   -        -                nothing drawn
  25700-25800    t17   K8 nothing drawn, ME's turn   on   -        uncached writes  nothing drawn
  25800-25900    t18   K0 all drawn                  on   -        -                nothing drawn
  25900-26000    t19   K0 all drawn                  on   -        -                nothing drawn
  26000-26100    t20   K0 all drawn                  on   -        -                deck, change of scene
  26100-26200    t21   K0 all drawn                  on   -        -                deck, change of scene
  26200-26300    t22   K0 all drawn                  on   -        -                steady deck
  26300-26400    t23   K0 all drawn                  on   -        -                steady deck
  26400-26500    t24   K7 nothing drawn, ME rests    on   -        -                steady deck
  26500-26600    t25   K8 nothing drawn, ME's turn   on   -        -                steady deck
  26600-26700    t26   K8 nothing drawn, ME's turn   on   -        arithmetic       steady deck
  26700-26800    t27   K8 nothing drawn, ME's turn   on   -        cached reads     steady deck
  26800-26900    t28   K8 nothing drawn, ME's turn   on   -        uncached writes  steady deck
  26900-27000    t29   K0 all drawn                  on   -        -                steady deck
  27000-27100    t30   K5 stage program off          on   -        -                steady deck
  27100-27200    t31   K7 nothing drawn, ME rests    on   -        -                steady deck
  27200-27300    t32   K0 all drawn                  on   -        -                steady deck
  27300-27400    t33   K8 nothing drawn, ME's turn   on   -        -                steady deck
  27400-27500    t34   K8 nothing drawn, ME's turn   on   -        arithmetic       steady deck
  27500-27600    t35   K8 nothing drawn, ME's turn   on   -        cached reads     steady deck
  27600-27700    t36   K8 nothing drawn, ME's turn   on   -        uncached writes  steady deck
  27700-27800    t37   K7 nothing drawn, ME rests    on   -        -                steady deck
  27800-27900    t38   K0 all drawn                  on   -        -                steady deck
  27900-28000    t39   K5 stage program off          on   -        -                steady deck
  28000-28100    t40   K0 all drawn                  on   -        -                steady deck
  28100-28200    t41   K7 nothing drawn, ME rests    on   -        -                steady deck
  28200-28300    t42   K8 nothing drawn, ME's turn   on   -        -                steady deck
  28300-28400    t43   K8 nothing drawn, ME's turn   on   -        uncached writes  steady deck
  28400-28500    t44   K0 all drawn                  on   -        -                steady deck
  28500-28600    t45   K7 nothing drawn, ME rests    on   -        -                steady deck
  28600-28700    t46   K0 all drawn                  on   -        -                steady deck
  28700-28800    t47   K0 all drawn                  on   -        -                change of scene
  28800-28900    t48   K0 all drawn                  on   -        -                change of scene
  28900-29000    t49   K0 all drawn                  on   -        -                change of scene
  29000-29100    t50   K0 all drawn                  on   -        -                rain begins
  29100-29200    t51   K0 all drawn                  on   -        -                rain wait, first turns
  29200-29300    t52   K0 all drawn                  on   -        -                rain wait, first turns
  29300-29400    t53   K7 nothing drawn, ME rests    on   -        -                rain wait, first turns
  29400-29500    t54   K0 all drawn                  on   on       -                rain wait, first turns
  29500-29600    t55   K0 all drawn                  on   on       -                rain wait
  29600-29700    t56   K0 all drawn                  on   off      -                rain wait
  29700-29800    t57   K7 nothing drawn, ME rests    on   off      -                rain wait
  29800-29900    t58   K8 nothing drawn, ME's turn   on   off      -                rain wait
  29900-30000    t59   K8 nothing drawn, ME's turn   on   off      arithmetic       rain wait
  30000-30100    t60   K8 nothing drawn, ME's turn   on   off      cached reads     rain wait
  30100-30200    t61   K8 nothing drawn, ME's turn   on   off      uncached writes  rain wait
  30200-30300    t62   K0 all drawn                  on   off      -                rain wait
  30300-30400    t63   K0 all drawn                  OFF  off      -                rain wait
  30400-30500    t64   K7 nothing drawn, ME rests    on   off      -                rain wait
  30500-30600    t65   K0 all drawn                  OFF  on       -                rain wait
  30600-30700    t66   K0 all drawn                  on   off      -                rain wait
  30700-30800    t67   K7 nothing drawn, ME rests    on   on       -                rain wait
  30800-30900    t68   K0 all drawn                  on   on       -                rain walk
  30900-31000    t69   K0 all drawn                  on   off      -                rain walk
  31000-31100    t70   K7 nothing drawn, ME rests    on   off      -                rain walk
  31100-31200    t71   K8 nothing drawn, ME's turn   on   on       uncached writes  rain walk
  31200-31300    t72   K0 all drawn                  on   off      -                rain walk
  31300-31400    t73   K0 all drawn                  on   on       -                the walk ends
  (38 K0 turns, 2 K5, 15 K7, 19 K8; 12 of the K8 turns have a load.)

What you will see. Before vblank 24,000 (the title, the opening, the talks) the picture is gl37's. From the deck on, in
the K7 and K8 turns the picture STANDS STILL, up to five turns in a row: 26,400-26,900 and 27,300-27,800 on the radio
screen (about 2 seconds a turn) and 29,700-30,200 in the rain (about 7 seconds a turn: the picture stands for about
35 seconds there). THIS IS THE MEASUREMENT, not a fault: the game goes on underneath, and the sound with it. In the
K0 turns the picture is the normal one; its first half second can carry a fading trace of the turn before. In the two
K5 turns one face of the radio screen is missing. Nothing to press, and there is nothing to judge on the picture.
The picture pauses 16 times for the saved shots and about every 3 seconds in the walk for the captures, as before.

What it saves next to the EBOOT (about 25 MB in all, as gl38): shot_<vblank>.bmp (16), live_<vblank>.bmp (the walk's
captures every 25 vblanks), log.txt (about 1.1 MB), steps.csv, battery.txt, crash.txt.

What it touches: as gl28 to gl38. No new hardware writes, no new system calls or imports (the same 133 sce* names as
gl38, the stub sections the same sizes), nothing written to flash, 333 MHz as always, no power or clock setting.
  - The Media Engine's load (12 of the K8 turns): a loop in our own Media Engine code that does arithmetic, or reads the
    program's code, or writes a 256-byte block of its own, for 100 ms for each picture. It calls nothing, touches no
    hardware register, counts the same heartbeat word the Media Engine's other work counts, and stops by the clock the
    Media Engine's timers already read. The main CPU posts it only when the Media Engine has caught up.
  - The probes read the system's microsecond clock the main CPU's timers already read; no new call.
  - The ELF has gl38's layout for the loader: three loaded parts (code and constants, data, zeroed memory).
  - Heap: the EBOOT's code, tables and pads end 24 KB above gl38's, so the game's heap starts 24 KB higher and its
    highest point is 24 KB above gl38's, which is still 16 KB below gl36's (a build that ran on the PSP). Heap low point
    on the emulator: vb 23,677, the log line reads 64 KB (gl38: 64, gl36: 64). At the end the allocator can still hand
    out 64 KB in 1 block (gl38: 96 KB in 2; gl36: 64 KB in 1).

What the run should show:
  - All 62 hashes equal to hashref.txt (c9ddb593, the same file as gl33 to gl38): the game's state is the same in every
    kind of turn, with every load.
  - 266 frame digests (the digest-on turns of 29,490-31,400), each equal to the emulator's and the PC twin's digest of
    the same kind of turn. rd39pc.py carries the emulator's fingerprint: 'DIGESTS ... EQUAL'. By kind: K0 166, K7 50,
    K8 50 (in a K7 or K8 turn the digest is taken of the list with everything left out).
  - 75 'nat turn' lines; 75 'nat f1' lines; one 'gl38 bound' line and one 'gl39 probe' line per turn (315 each), the kind,
    F1, digest and load as in the table (rd39pc.py's SCHEDULE CHECK must say 'none').
  - 'bound totals (gl38)': frames begun with a mask 1799; transfers left out by chain: 0:1699 1:1699 2:1699 3:1699 4:1699
      5:1699 6:1699 7:1699 8:1699 9:1699 10:1799 11:1699 12:999 13:750 14:750 15:750 16:744; native groups recorded and
      not drawn 349588; rain owners' calls 0; rain owners' blocks over the table 0, not right 0
  - 'F1 totals (gl38)': runs planned 4739 (groups 577938, of them not written 504003, without the fast form 58953), runs
      not planned 0, planned runs with a problem 0; check mode: groups compared 0, DIFFERING 0
  - 'natcull: totals (v3)': effect calls tested 185558, left out 132655 (4887908 vertices); kept: vertex 0 inside 29850,
      no plane proven 22905, a bad attribute 0, count 0, out of the clamps' range 0, thinned 148
  - These counts depend on the game's state, not on speed, so the PSP's should be the same.
  - NAT PROBLEMS none; 'groups left untaken' in three turns (24,000: 524, 25,000: 423, 29,000: 2,037: changes of scene,
    as gl37 and gl38); 'planned runs with a problem 0'.
  - 'ME load entries not posted (the loop was behind)': the emulator run has 1. On the PSP it may be more (a load is
    left out when the Media Engine has not finished the one before); it is a count, not a fault.
  - 16 'the flip wait kept' lines, no 'GAVE UP' line.

The run's measurements (rd39pc.py prints them; all in ms a PSP step = a vblank):
  (a) THE COUPLING. gl38 showed the main CPU's game logic 7.3 ms a step slower with the Media Engine working than with
      it resting. 'K8 <load> minus K7' gives the logic with the Media Engine busy in that way and nothing drawn:
        K8 no load: the hand-over, the queue and the GE with empty pictures;
        K8 arithmetic: the Media Engine computing, with no memory traffic;
        K8 cached reads: the Media Engine fetching lines from RAM;
        K8 uncached writes: the Media Engine writing to RAM past its cache, as its list writes do.
      It is measured in the rain wait (29,700-30,500: K7, the four K8 turns, and K7 again at 30,400), twice in the
      steady deck (26,400-26,900 and 27,300-27,800) and once where nothing is drawn (25,300-25,800).
  (b) 'K0 digests off minus K7' and '... minus K8 no load': what real pictures add to the main CPU's parts.
  (c) THE DIGEST: 'K0 digests on minus off' (29,500 against 29,600, 30,200 and 30,600; the walk's 30,800 against 30,900
      and 31,200), for the step and for the Media Engine's 'lists'.
  (d) F1: 'K0 F1 OFF minus on' (30,300 against 29,600, 30,200, 30,600). The turn 30,500 (digests on, F1 off) is gl38's
      own K0 configuration: the link to gl38's 84.99 ms a step.
  (e) COUPLING PROBES by turn: the service calls and the whole-cache write-backs by the timed part they fell in, with
      their time; what falls in 'logic' is inside the logic figure.
  (f) MAIN-DRAWING STEPS before vblank 24,000 (the main CPU makes the picture in every other 100 vblanks there), for the
      comparison with gl36's and gl38's logs.
  How to read the rain wait:
    - 29,100-29,500 (K0, K0, K7, K0): no frame digests yet (they start at 29,490) and the scene still settles.
    - 29,500-30,700: one K0 turn with digests (29,500), then the digest-off turns; 30,500 has digests and F1 off.
    - 30,600-30,800 is a heavier part of the scene (about +5 ms a step in the K7 turn 30,700 against 29,700).
  The Media Engine's own times in a K8 turn ('busy', 'load') say how long the load ran; the picture in such a turn costs
  the Media Engine almost nothing.

What to expect of the speed. Figures marked [C model] come from the PC cost model (ms a PSP step); [I] are inferred from
them and from gl38's PSP run (K0 with digests 84.99 ms a step: main CPU 75.9, Media Engine 84.4; K7 69.6). Single
estimates of this kind have been wrong by a factor of two before.
  - K7 turns (the main CPU alone): about 68 to 69 at 29,700 and 30,400, about 73 at 30,700 [C model 68.2, 68.9, 72.9].
    The model puts gl39's main CPU 0.4 ms a step ABOVE gl38's in the same turns, not below.
  - K8 with no load: K7 + 0.5 [C model]. K8 with a load: not known; that is the measurement.
  - K0 without the digest, F1 on (29,600, 30,200, 30,600: what a game build would run): about 76 ms a step [I], which is
    6.6 pictures a second. The main CPU is then the limit (it needs about 76 [I]); the Media Engine needs about 65 to 70
    [I]. Against gl38's 84.99 the gain is the digest going away, not a faster game.
  - K0 with the digest, F1 on (29,500): about 83 to 84 [I]. K0 with the digest, F1 off (30,500): about 85, gl38's own.
  - The digest [C model]: 22.7 ms a step of Media Engine work in the rain wait, 17.7 in the walk.
  - F1 [C model]: 1.5 ms a step of Media Engine work in the rain wait, 0.2 in the walk. It shows in the Media
    Engine's 'busy'; the step only follows where the Media Engine is the limit (the digest-on turns).
  - gl39 against gl38 in the same turn [C model]: K0 without the digest and with F1 off in both: main CPU +0.4, Media
    Engine -0.05; with gl39's F1 on: Media Engine -1.6.

How it runs: as gl38's. The files go over the last build's folder on the stick (renamed MGS2T_gl39); at the end
("Press X to exit") press X and start "PSPLink (PC control)" so the logs can be copied to LOGS\gl39. About as long as
gl38's run. Keep the USB cable in, as before.

Other settings the same EBOOT takes (for a later run, not this one), each checked on the PC twin or the emulator:
  - MGS2_DIG_CYCLE absent: digests in every turn of MGS2_GL_DIG's range. MGS2_LOAD_CYCLE absent: no load.
    MGS2_PROBE9 absent: the probes count but time nothing.
  - MGS2_BOUND_CYCLE absent: gl37's picture in every turn, with gl37's Media Engine / main turns.
  - MGS2_NAT_F1=1: F1 in every turn. MGS2_NAT_F1CHK=1 (with MGS2_NAT_REUSE=1 MGS2_NAT_RRULE=0 MGS2_NAT_RCHK=1): F1's
    check mode ('F1 totals ... DIFFERING').
  - gl36's to gl38's keys as before (MGS2_NAT_REUSE, MGS2_NAT_CULL, the kinds K1 to K6, MGS2_GL_RAIN_NEAR).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd39pc.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are; their md5s are in the manifest too.

Reading the logs: on the PC   python -I rd39pc.py <LOGS\gl39>     (one file, standard library only, no other files needed)
It prints: the end of the run, the crash record, HASHES and DIGESTS against the emulator run of this package (must say
EQUAL), the main-drawing steps before 24,000, the turn table, the SCHEDULE CHECK, each scene's kinds with the
differences named above, the coupling probes by turn, the problem counters, the battery.
