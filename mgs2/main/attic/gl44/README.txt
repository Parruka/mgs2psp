MGS2 gl44: V44 IN ITS OWN TURNS (line_sph's hidden drops kept only as their dice and time, rain.c's hidden streaks without
their vertex loops), OPTION B ON IN EVERY RAIN TURN BUT THE TWO ANCHORS, AND A HARNESS THAT KEEPS UNFINISHED LOGS AND WRITES A
BLACK BOX. A MEASUREMENT build like gl39 to gl43: from the deck on (vblank 24,000), turns of 100 vblanks switch things on and off,
and in some turns NOT EVERYTHING or NOTHING is drawn, ON PURPOSE. Personal use only, do not share: it contains code built from
your MGS2 disc.
2026-11-28. Build notes: mgs2/main/attic/gl44 (state.md, README_A.txt, deviations_V44_draft.md, harness_draft.md) and
attic/gl44_vita (V44's design).

HOW TO RUN (please follow this):
  - Run on the AC ADAPTER (the round plug), not on USB power and not on the battery alone.
  - The in-game plugin OFF.
  - The files go over the last build's folder on the stick (renamed MGS2T_gl44). At the end ("Press X to exit") the build WAITS
    on its end screen: press X and start "PSPLink (PC control)" so the logs can be copied to LOGS\gl44.
  - About as long as gl43's run.

THE FILES OF THE RUNS BEFORE ARE KEPT, AT EVERY START (new in gl44). Before it writes anything, the build renames the files of
the run before: log.txt, battery.txt, crash.txt, steps.csv and bbox.txt become log_prev1.txt, battery_prev1.txt,
crash_prev1.txt, steps_prev1.csv and bbox_prev1.txt, and the _prev1 files of the run before that become _prev2 (older _prev2
files are deleted). It does this whether or not the run before reached its end, so a run that froze keeps its files even if
the game is started again. The shots (shot_*.bmp, live_*.bmp, zbuf_*.raw) are not renamed. The new log.txt's first lines say
what was renamed and whether the run before's log had its end line ("gl44 H1 ..."). Copy all of them with the logs (PSPLink).
Room: three runs' files, a few MB.

AFTER A FREEZE: start ONLY "PSPLink (PC control)", NEVER the game, and copy the logs to LOGS\gl44 (the black box and the
_prev files make a frozen run readable). And please note, no tool needed (it tells power, heat and a stuck chip apart):
  - the sound: did it stop, go on playing, or buzz (a short sound repeating)?
  - the power LED: steady, blinking, or off?
  - how many seconds from the freeze until the PSP switched off (if it did)?
  - was the back of the PSP hot?
On the PC:   python -I rdbbox.py <LOGS\gl44>   (-p1 reads bbox_prev1.txt) says how the black box ends (below).

THE BLACK BOX (mgs2.cfg's MGS2_BBOX=1, on in this mgs2.cfg). bbox.txt is made at its full size at the start (3,600 lines of 128
bytes, 461 KB; a run longer than an hour writes over the oldest lines, each line numbered). From the first window (vblank 100) a
thread above the game's priority writes one line a second into it (open, seek, write, close: the file's size never changes and a
power-off cannot cut it short). Each line: its number and the time since the start; the PSP's own vblank count; the game's
vblank and a counter of the game thread's steps; what the game thread was doing (a tag: logic, IOP and sound, the GS walk, the
draw pipeline, the translator, the picture, waiting for the GE or the ME, a disc read with its sector and size, the log's write,
a shot, a stage overlay's load, the vblank hook, the end); the Media Engine's heartbeats and place; the last GE list and its
state; the battery (mV, %, charging, external power); the black box's own lateness and write time.
rdbbox.py prints the last 20 lines and which case the end is:
  - "the game counter STOPPED while the heartbeat went on N s": the system survived a stall of N seconds, and the tag says where
    the game thread was;
  - "BOTH END IN THE SAME SECOND": the whole chip stopped (interrupts off, a bus lock or the power);
  - or the run's own end, or the program's orderly exit; and the last tag and the last disc read (the file named).
What it costs: one 128-byte write a second on its own thread. On the emulator [C emu]: write time mean 10 to 13 ms (the host's
file system), lateness 0.03 ms at most, the step time unchanged within the emulator's noise; 4 KB of stack. On the PSP: UNKNOWN
until a run; the log's last lines ("gl44 black box ...") give its write time and lateness, and every line of bbox.txt too.

V44 (MGS2_OPT2 bit 22, 4194304, new in gl44; attic/gl44_vita's design, reviewed and built here). In a turn whose word has the
bit (fffff9ff), two of the aft deck's rain acts do less, in work that is never shown:
  - line_sph (the drops' lines): every prim it makes is already hidden by G0H (gl3x), so in a V44 turn the native form keeps
    only what the rest of the game can see: the random numbers each prim draws (the same count, from the same seed), the game
    time it takes (its interrupt polls), the hide bits, and the first 32 vertices of its last scratchpad image (a later act,
    putmodel, reads 6 of them: an uninitialised read in the game itself). The vertex maths of hidden prims is not done.
  - rain.c (the streaks): of the 16 entries of each act, the 15 whose prims are all hidden skip their two vertex loops and the
    fill loop and add the loops' game time (their polls) in one go; the 16th runs as before.
  What you see: NOTHING CHANGES on screen (the prims it leaves are never drawn). Checked on the PC twin of this build [C twin,
  route i to vblank 37,800, V44 never on / in its turns / on in every word]:
  - The seed log is identical in all arms: every act's seeds and game time at entry (27,764 lines from vblank 23,000) and every
    seed change with the act that drew it (98,024, the guards' among them); the act log's 13,059 scheduler passes have the same
    acts, seeds and time.
  - IOP (sound) and frame-buffer hashes equal at all 685 points; every picture equal (37,800 live lines, 14,203 digest and
    14,203 content lines).
  - Read hash (what the game reads, vblank by vblank, in the emulator's trace build): the number of reads is equal in all
    37,799 vblanks; the values differ in 188 walk vblanks (30,903-31,271), all in the w lane of quadwords that one routine
    (S_w00a_P3420e8) stores with whatever its VF9-11 held before (4 instead of 0), which copies carry along and DG_RotVector reads
    only as x, y, z. Nothing the game decides on differs.
  - The game's state hash differs from vblank 29,300 (the end of the first V44 turn) by design: only rain.c's hidden vertex
    buffers (15 stale entries x 2 KB and the 16th's, which gets other vertices: VU0's random register has moved less), which
    nothing reads; after the deck they are freed blocks nothing reuses, so the hash stays different to the end of the route.
    So hashref.txt is gl44's OWN (md5 b6a845bf...): to vblank 29,200 it is gl43's line for line.
  On the emulator [C emu]: 62/62 hashes = gl44's hashref; the V44 counters ("v44 turn" lines) equal the twin's turn for turn.
  NOTE: gl43's words have bit 22 set (its "off" word ffdff9ff): with this EBOOT an older cfg would turn V44 ON. Use only this
  mgs2.cfg.

OPTION B (bit 21, gl43's) is now ON in every rain turn from t50 but the two anchors (t50 K-A, t56 K-N2): gl43's PSP run showed it
works (rain wait K-N2B - K-N2 -4.75 ms a step) and it is the new baseline. IN B TURNS THE FAR RAIN, THE SPLASHES AND THE RIPPLES
ARE MISSING ON PURPOSE (as gl43: only the streaks and the nearest drop sheet stay; Ffm's consent 2026-11-22). Just after a B turn
the splashes that started during it may stay hidden until they end [I]; with B in almost every rain turn now, the anchors t50 and
t56 show the full rain only partly (t56 follows the B turn t55).

What it is: gl43's build (native steps 1 and 2, C2 on everywhere, option B, the deck's turns t0-t49 unchanged) + V44 + the
harness changes above. A new EBOOT, the 8 files next to it, and these changes in mgs2.cfg, each marked 'gl44' there:
  1. Option B on in every rain turn but the anchors (above).
  2. V44 in alternate rain turns (the table): words ff9ff9ff (B and V44 off: before the turns, the deck t0-t49 and the anchors),
     ffbff9ff (B), fffff9ff (B + V44). Bit 22 is CLEAR in every word without V44 and bit 21 in every word without B.
  3. The rain turns t50-t73 have new kinds: K-BV = K-B + V44, K-N2BV = K-N2B + V44, K7BV = K7B + V44, each beside turns of its
     kind without V44; digests in t55 and t73, both K-N2BV (V44's picture against the PC twin's).
  4. MGS2_BBOX=1 (the black box, above).
  5. hashref.txt is gl44's own (above).

THE TURNS (100 vblanks each; B = option B; V = V44; OPT2 word = MGS2_OPT2_CYCLE's; NGE = MGS2_NGE_CYCLE's digit; bound =
MGS2_BOUND_CYCLE's kind; XP as gl40-gl43; digests: none exist before vblank 29,490, on = digests on in that turn):
  vblanks       turn kind                      B    V    OPT2 word  switches             digests               shots        scene
  24000-24099   t0   K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  24100-24199   t1   K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24200-24299   t2   K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  24246        first deck scene
  24300-24399   t3   K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  24400-24499   t4   K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24500-24599   t5   K-N1 ship native          off  off  ff9ff9ff   NGE 1  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24600-24699   t6   K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24700-24799   t7   K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  24786        first deck scene
  24800-24899   t8   K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            first deck scene
  24900-24999   t9   K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            first deck scene
  25000-25099   t10  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  25100-25199   t11  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25200-25299   t12  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25300-25399   t13  K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            deck (first play)
  25400-25499   t14  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25500-25599   t15  K-N1 ship native          off  off  ff9ff9ff   NGE 1  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25600-25699   t16  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25700-25799   t17  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25800-25899   t18  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck (first play)
  25900-25999   t19  K7 nothing drawn          off  off  ff9ff9ff   NGE 0  bound 7  XP 3  none (before 29,490)  -            deck (first play)
  26000-26099   t20  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26100-26199   t21  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            deck, change of scene
  26200-26299   t22  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26300-26399   t23  K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  26400-26499   t24  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26500-26599   t25  K-N1 ship native          off  off  ff9ff9ff   NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  26600-26699   t26  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26700-26799   t27  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  26800-26899   t28  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  26900-26999   t29  K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27000-27099   t30  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27100-27199   t31  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27200-27299   t32  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27300-27399   t33  K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  27400-27499   t34  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27500-27599   t35  K-N1 ship native          off  off  ff9ff9ff   NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  27600-27699   t36  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27700-27799   t37  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  27800-27899   t38  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  27900-27999   t39  K7 nothing drawn          off  off  ff9ff9ff   NGE 0  bound 7  XP 3  none (before 29,490)  -            steady deck
  28000-28099   t40  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28100-28199   t41  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            steady deck
  28200-28299   t42  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28300-28399   t43  K-N2R native, bound 9     off  off  ff9ff9ff   NGE 2  bound 9  XP 3  none (before 29,490)  -            steady deck
  28400-28499   t44  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28500-28599   t45  K-N1 ship native          off  off  ff9ff9ff   NGE 1  bound 0  XP 3  none (before 29,490)  -            steady deck
  28600-28699   t46  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            steady deck
  28700-28799   t47  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  none (before 29,490)  -            change of scene
  28800-28899   t48  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            change of scene
  28900-28999   t49  K7 nothing drawn          off  off  ff9ff9ff   NGE 0  bound 7  XP 3  none (before 29,490)  -            change of scene
  29000-29099   t50  K-A all drawn, today      off  off  ff9ff9ff   NGE 0  bound 0  XP 3  none (before 29,490)  -            rain begins
  29100-29199   t51  K-B K-A + B               on   off  ffbff9ff   NGE 0  bound 4  XP 3  none (before 29,490)  -            rain wait, first turns
  29200-29299   t52  K-BV K-B + V44            on   on   fffff9ff   NGE 0  bound 4  XP 3  none (before 29,490)  -            rain wait, first turns
  29300-29399   t53  K-BV K-B + V44            on   on   fffff9ff   NGE 0  bound 4  XP 3  none (before 29,490)  -            rain wait, first turns
  29400-29499   t54  K-B K-A + B               on   off  ffbff9ff   NGE 0  bound 4  XP 3  off                   -            rain wait, first turns
  29500-29599   t55  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 7  on                    29502        rain wait
  29600-29699   t56  K-N2 ship+people native   off  off  ff9ff9ff   NGE 2  bound 0  XP 3  off                   -            rain wait
  29700-29799   t57  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  29800-29899   t58  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  29900-29999   t59  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  30000-30099   t60  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  30100-30199   t61  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  30200-30299   t62  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  30300-30399   t63  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   -            rain wait
  30400-30499   t64  K7B K7 + B                on   off  ffbff9ff   NGE 0  bound 7  XP 3  off                   -            rain wait
  30500-30599   t65  K7BV K7B + V44            on   on   fffff9ff   NGE 0  bound 7  XP 3  off                   30502 30548  rain wait
  30600-30699   t66  K7BV K7B + V44            on   on   fffff9ff   NGE 0  bound 7  XP 3  off                   30646        rain wait (heavier)
  30700-30799   t67  K7B K7 + B                on   off  ffbff9ff   NGE 0  bound 7  XP 3  off                   30724        rain wait (heavier)
  30800-30899   t68  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   30826        rain walk
  30900-30999   t69  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 3  off                   30926        rain walk
  31000-31099   t70  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   31048        rain walk
  31100-31199   t71  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 3  off                   31114        rain walk
  31200-31299   t72  K-N2B K-N2 + B            on   off  ffbff9ff   NGE 2  bound 4  XP 3  off                   31264        rain walk
  31300-31399   t73  K-N2BV K-N2B + V44        on   on   fffff9ff   NGE 2  bound 4  XP 7  on                    -            the walk ends
Kinds: deck t0-t49 gl43's (K-A 25, K-N1 5, K-N2 10, K-N2R 7, K7 3); rain wait t50-t67: K-A 1 and K-N2 1 (the anchors, the same
turns as gl43's K-A t50 and K-N2 t56), K-B 2 / K-BV 2 (B b b B), K-N2B 4 / K-N2BV 4 (t55 with digests; C c C c C c C from t57),
K7B 2 / K7BV 2 (D d d D); walk t68-t73: K-N2B 3 / K-N2BV 3 (C c C c C, t73 with digests). The 4-turn groups are mirrored
(B b b B, D d d D) so that a steady drift over the 4 turns cancels in the means; every K-N2BV turn sits between two K-N2B turns.

What you will see. Before the rain the picture is gl43's. From vblank 29,000:
  - t50 (K-A) and t56 (K-N2): the rain as in gl43's K-A / K-N2 turns (but see above: splashes of the turn before may be hidden).
  - every other rain turn is a B turn: the far rain, splashes, ripples and foot splash are MISSING ON PURPOSE; the streaks and the
    nearest drop sheet stay. V44 changes nothing you can see.
  - K7B and K7BV turns (t64-t67): the picture STANDS STILL, as in gl40-gl43.
  THIS IS THE MEASUREMENT, not a fault: the game goes on underneath. The 16 shots and the walk's captures are as gl43's.

What it saves next to the EBOOT (about 25 MB, as gl43, plus bbox.txt 461 KB and the _prev files): shot_<vblank>.bmp (16),
live_<vblank>.bmp, log.txt, steps.csv, battery.txt, crash.txt, bbox.txt, and the *_prev1 / *_prev2 files of the runs before.
What it touches: as gl43, plus one new import (sceIoRename: the _prev renames; 134 sce* names, gl43's 133 + 1). No new hardware
writes, nothing kernel-mode; nothing written to flash, 333 MHz as always, no power or clock setting; at the end it waits on its
end screen as before (it never exits by itself).
Memory: the image ends at 0b253900, 16 KB above gl43's 0b24f900 (text +18,112 B: the black box and V44;
the link's pads keep every ME-shared group at gl43's cache offsets, as before). On the emulator [C emu]:
  - the route's worst moment is the texture table's event at vb 536: 87 KB above the break + 34 KB free top = 122 KB (gl43:
    103 + 34 = 137; THE GATE: at least 32 KB);
  - the ME heap events' lowest room + top: 110 KB (vb 23,676; gl43 126);
  - textures refused for want of memory: 0;
  - at the end, malloc can give 80 KB in 1 block (gl43: 96 KB in 2).

What the run should show (counts that depend on the game's state, not on speed, so the PSP's should be the emulator's run of
this package [C emu]; rd44pc.py checks the first three and prints the rest):
  - All 62 hashes equal to hashref.txt (gl44's own, b6a845bf): rd44pc.py says 'HASHES ... EQUAL'.
  - 61 frame digests and 61 content lines (t55, t73, both K-N2BV), equal to the emulator's: 'DIGESTS ... EQUAL' and 'CONTENT ...
    EQUAL'. The content is the PC twin's in all 61 frames (V44's picture).
  - 23 'v44 turn' lines (t50-t72; t73's would come after the run's end). Each V44 turn: 'V44 on', line_sph stubbed 8,200, by G0
    0, rain.c entries skipped 1,500, in place and run 0; each other rain turn 'V44 off', stubbed 0, by G0 8,200, skipped 0, run
    1,500 (t50: 2,706 and 495, the rain beginning); short tails 0 and placed first 0 everywhere. Totals to t72: stubbed 82,000,
    by G0 101,106, skipped 15,000, in place and run 18,495.
  - 23 'b43 turn' lines: 'B on' in every rain turn but t50 and t56, every call shadowed (none with B off). B turns: rain wait
    floor spawn 2,540, floor update 29,420, fall update 19,325, fall dead 88,885; walk fall update 688, fall dead 33,292; 0 run as
    translated for a due interrupt or a guard (on the PSP a few 'a poll due' are possible and harmless).
  - 'bound totals (gl38)': rain owners' calls 4255 (sheets 3436), left out 3396, near sheet calls kept 859.
  - 'nge people totals': people calls 38319, meshes reused 38095, cooked 224, no room 0. 'nge totals': calls 103802, calls
    native 70254, no room 0.
  - 'gl40 XP totals': plans 114008; XP check: plans compared 720, DIFFERING 0 (MUST be 0).
  - 'F1 totals (gl38)': runs planned 8575, with a problem 0.
  - 'natcull: totals (v3)': effect calls tested 197396, left out 151772.
  - The carve after the opening at vb 24007, as gl43. The SCHEDULE CHECK must say 'none' (it now also checks each turn's OPT2
    word and its V44 line); PROBLEM COUNTERS none.
  - The black box: rdbbox.py says 'REACHED ITS END'.

The run's measurements (rd44pc.py prints them; ms a PSP step; main = the main CPU's step less its waits for the GE; ME = the Media
Engine's busy time; P = max(main, ME)). Read main first: the rain is main-CPU-bound (gl43's PSP log: K-N2B main 69.32, ME 55.25).
  (a) V44 ON THE NATIVE PATH (the delivery's kind): K-N2BV minus its K-N2B neighbours (t58, t60, t62 in the rain wait; t69, t71
      in the walk).
  (b) V44 ON TODAY'S PATH: K-BV minus K-B (t52, t53 against t51, t54).
  (c) V44'S LOGIC ALONE: K7BV minus K7B (t65, t66 against t64, t67; nothing drawn, the ME resting).
  (d) The anchors: t50 K-A and t56 K-N2 against gl43's same turns (ties gl44's figures to gl43's run); K-N2B against gl43's
      K-N2B 69.70.

What to expect of the speed. [C model] = the PC cost model of this build, ms a PSP step (main = game, runtime, the copy plan, the
hash, the rest; ME = the Media Engine; P = max(main, 0.8 x ME) [I]; tools/score44.py, attic/gl44/score44_final.txt). [I] = inferred.
The model's main has been above the PSP's (gl43: K-N2B - K-N2 model -5.35, PSP -4.67), so read the PSP's deltas as about 0.87 x
the model's. Single estimates of this kind have been wrong by a factor of two before.
  [C model] V44, ms a PSP step, main (the figure that counts in the rain):
  - (a) K-N2BV - K-N2B: rain wait 6 pairs -1.02 to -1.56, mean -1.32 (game -0.88, ME about 0); walk 4 pairs -1.21 to -8.18,
    mean -5.29 (the walk's own spread between two K-N2B turns is +2.39 / -4.78, so read the walk as 'smaller, by an unknown
    amount'). All 10: -2.91. On the PSP expect about -1.1 in the rain wait (0.87 x the model), about 1.6 % of K-N2B's 69.3.
  - (b) K-BV - K-B: -2.08 and +1.83, mean -0.13 (two K-B turns differ by -3.98 between themselves: no reading).
  - (c) K7BV - K7B: +0.13 and -0.96, mean -0.42 (V44's own logic on the main CPU: small).
  - (d) gl44 - gl43 in the same turns with no lever: t50 +0.05, t56 +0.14, t51 +0.42, t57 +0.11, t59 +0.02, t72 +0.19; so the
    PSP's anchors should be close to gl43's [I] (K-A 51.6 model). The guard windows' I-misses are 5-16 % under gl36's.

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd44pc.py, rdbbox.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are.

Reading the logs: on the PC   python -I rd44pc.py <LOGS\gl44>     (one file, standard library only)
It prints rd43pc.py's sections with the V44 kinds:
  - the end of the run, the crash record;
  - HASHES, DIGESTS and CONTENT against the emulator run of this package (must say EQUAL);
  - the HEAP lines, the carve after the opening, the main-drawing steps before 24,000;
  - the turn table and the SCHEDULE CHECK (each turn's OPT2 word, B and V44 on exactly in their turns);
  - each stretch's kinds with the differences (K-BV minus K-B, K-N2BV minus K-N2B, K7BV minus K7B among them), and the PAIRS
    (each lever turn against its neighbours of the base kind);
  - OPTION B by turn and V44 by turn, with the totals;
  - the people, native step 1, the problem counters, the battery.
And   python -I rdbbox.py <LOGS\gl44>   for the black box (bbox.txt; -p1 for bbox_prev1.txt).
