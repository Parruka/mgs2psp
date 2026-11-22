MGS2 gl34, the ship's new drawing path made cheaper for the Media Engine in turns, with timers that say where the Media
Engine's time goes, on a freshly trained build (MGS2T_gl34) - personal use only, do not share (it contains code built from
your MGS2 disc).
2026-11-21 (build report: mgs2/main/attic/gl34/report.txt).

What it is: gl33 with a new EBOOT (and the 8 files beside it, rebuilt with it) and three settings changed in mgs2.cfg. Each
is marked 'gl34' in mgs2.cfg; every other setting is gl33's.
  1. The ship's fixed mesh data is read in place by the Media Engine in EVERY turn (MGS2_NAT_INPL=1; gl33 did it in half
     of the turns, and its run was exact).
  2. In turns (MGS2_NAT_FAST_CYCLE=0,0,1,1; turns of 100 vblanks, the same turns as before):
       0  each piece of the ship goes to the picture code the way gl33 did it;
       1  the plain textured pieces (about 94 in 100 in the rain) take a shorter way made for them: the same result with
          fewer steps, and the pieces of one mesh share one matrix calculation instead of repeating it.
     Each arm meets one Media Engine turn and one main-CPU turn every 400 vblanks. The picture is the same in both arms: on
     the PC twin and in the emulator every frame is the same, and so is the picture code's whole internal state.
  3. Timers (MGS2_NAT_SPLIT=1): in the Media Engine's turns one ship piece in about 32 is timed step by step, and each of
     the frame's 12 drawing lists is timed. Two new lines per Media Engine turn in log.txt. Nothing drawn changes.
The EBOOT's code is gl33's plus points 2 and 3, compiled with a profile and a code order measured again on this code, with
both arms of point 2 in the training.

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship too: every 200 vblanks the way its pieces are prepared
  changes; you should see no difference. If any part of the ship flickers, goes missing or changes look, say when.
  During the walk the picture pauses 9 times and freezes for a quarter second about every 3 seconds (captures). Logs come
  back as usual in LOGS\gl34."

What it saves next to the EBOOT (about 25 MB in all, as gl33):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28 to gl33. No new hardware writes, no new system calls, nothing written to flash, 333 MHz as always, no
power or clock setting, no new memory block. The timers read the Media Engine's own counter, the one gl31 to gl33 already
read for the 'native time' figure (a read, nothing written). The EBOOT's code is 24 KB larger than gl33's (4,402 KB
against 4,378 KB), so the game's heap is that much smaller at the start; in the emulator the lowest free heap is 64 KB
(gl33: 64 KB) and at the end the allocator can still hand out 80 KB in 2 blocks (gl33: 128 KB in 4). gl30's run on the
PSP had about 2.7 MB of heap free at its lowest (the emulator has less memory than the PSP).

What the run should show:
  - all 62 hashes equal hashref.txt (c9ddb593, the same file as gl33's: the game's state is gl33's in every turn);
  - every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's (916 digests, each with arm X; the
    same 916 as gl33's);
  - 75 'nat turn' lines, all 'arm 3': in every line 'the builder run instead 0', 'recorded with no array 0', 'early fills 0',
    'dropped 0', 'partly (dropped) 0', 'plans undone for the groups' room 0';
  - 75 'nat inpl' lines, all 'inpl 1';
  - 75 'nat fast' lines, one per turn: 'fast 0' or 'fast 1', the pieces marked for the short way, taken by it and handed back
    to the usual way with the reason. Emulator, whole run: 614,225 pieces marked, 541,836 taken by the short way,
    70,956 handed back for 'entry' (pieces of another kind), 100 for 'tables full', 0 for every other reason. These
    counts do not depend on speed, so the PSP's
    should be the same; a picture that straddles a turn's change is booked in the next turn (so a 'fast 0' turn shows a few
    hundred taken). 'state', 'buffers', 'stream counts', 'material tag' and 'path 1 busy' should stay 0;
  - 38 'nat split' lines and 38 'me chains' lines, one pair per Media Engine turn from 24,000 on:
      'nat split': the pieces timed, the cost of one counter read (the emulator cannot say: its clock is the PC's), the
      time of each of 17 steps in millionths of a second a piece, the turn's whole time in the ship's new path, and the
      size of the hand-over to the Media Engine;
      'me chains': for each of the frame's drawing lists (0 to 11 in the rain; 0 to 16 on the deck before it) the Media
      Engine's time and the number of draw calls; list 3 is the ship;
  - the run's measurements: (a) the 'fast 1' turns against the 'fast 0' turns next to them, separately for the Media Engine's
    turns and the main CPU's turns: the short way's gain; (b) the 'fast 0' turns against gl33's log on the same vblanks,
    which were gl33's in-place turns: the new training and the timers alone; (c) the two new lines: where the Media Engine's
    time goes, step by step, to be put next to the PC model's split (report, section 4);
  - three 'gl29 dz window' lines as gl33; the 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown buffer 0'
    in every turn; the 16 picture pauses each with a 'the flip wait kept' line and no 'GAVE UP' line;
  - every walk capture matching the emulator; no R presses.

What to expect of the speed (PC cost model only, not a PSP time), a picture:
  The short way against the usual way, same build (the run's own comparison, turn by turn):
    rain wait (29,810-29,990):         -5.9 ms in a Media Engine turn (1.6 percent), -2.4 ms in a main-CPU turn;
    walk in the rain (31,010-31,190):  -1.4 ms and -0.3 ms;
    deck before the rain (27,410-27,590) and the opening (16,010-16,190): no ship pieces there, so no difference
    (within 0.2 ms, which is what two runs of the same code differ by in the model).
  The new build with the usual way, against gl33's turns with the data read in place:
    rain wait +2.0 and +1.7 ms; walk in the rain -1.0 and +1.4 ms; deck before the rain +1.0 and +2.5 ms; opening +0.8 and
    +1.9 ms. So the new build by itself is 0.5 to 2 percent SLOWER than gl33 in the model, except in the walk's Media
    Engine turns (0.3 percent faster): its code is 24 KB larger and misses the caches more often (the instruction cache
    5 to 8 percent more often on the deck before the rain and in the opening). The timers are at most 0.5 ms of that.
  Together: the short way's turns should be about 4 ms a picture (1 percent) faster than gl33 in the rain wait's Media
  Engine turns, 2 ms faster in the walk's, and within about 1 ms of gl33 in the main CPU's turns; the usual way's turns
  in the rain wait about 2 ms slower than gl33, and the deck before the rain and the opening 1 to 2.5 ms slower.
  The aim was to take 30 percent off the Media Engine's work for the ship (72 ms of a 363 ms picture in the rain wait, in
  the model). The short way takes 8 percent off it (5.9 ms). The rest of that work is spread over many steps, and that is
  what the timers are for: the PSP's 'nat split' and 'me chains' lines will say which steps to go after next.
  All of this is the PC model; the PSP run settles the real figures.

How it runs: as gl33's. Claude copies the new files over MGS2T_gl33 on the stick over PSPLink (renaming it MGS2T_gl34) and
starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs
can be copied to LOGS\gl34. A short run, about as long as gl33's.
Keep the USB cable in, as in gl33: PSPLink stays on during the run, so the cable powers the PSP.

Other settings the same EBOOT takes (for a later run, not this one): MGS2_NAT_FAST_CYCLE=1,1,0,0 swaps the two arms' turns
(checked on the PC twin); MGS2_NAT_FAST=1 takes the short way in every turn and MGS2_NAT_FAST=0 never (both checked on the
PC twin); MGS2_NAT_SPLIT=0 turns the timers off (checked in the emulator); MGS2_NAT_HCHK=1 checks every pointer the short
way finds against the usual search (a check for the PC and the emulator: 0 differing in 9.6 million and 1.6 million).

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, this README. The disc pack (mgs2disc.pak) and
SLES_503.83 already on the stick stay as they are.

Reading the logs: in the container  python3 -I /home/claude/main/build_gl34/rd34.py <log.txt> --bat <battery.txt>
                  on the PC         python -I rd34pc.py <LOGS\gl34> <LOGS\gl33>     (one file, no other files needed)
