MGS2 gl33, the ship drawn the new way in every turn, on a freshly trained build, with the ship's fixed mesh data read
in place by the Media Engine in turns (MGS2T_gl33) - personal use only, do not share (it contains code built from your MGS2
disc).
DRAFT by the build agent, 2026-11-21, for the lead to edit (build report: mgs2/main/attic/gl33/report.txt).

What it is: gl31 with a new EBOOT (and the 8 files beside it, rebuilt with it) and two settings changed in mgs2.cfg. Each is
marked 'gl33' in mgs2.cfg; every other setting is gl31's.
  1. The ship's meshes are drawn the new way in EVERY turn from vblank 24,001 (MGS2_NAT_CYCLE=3,3,3,3, was 0,0,3,3). The
     game's own packet builder for the ship no longer runs in this part of the run.
  2. In turns (MGS2_NAT_INPL_CYCLE=0,0,1,1; turns of 100 vblanks, the same turns as before):
       0  as gl31's new way: the ship's mesh data is copied for the Media Engine before it draws;
       1  the mesh data the game keeps fixed (positions, colours and texture coordinates of the plain textured ship parts) is
          NOT copied: the Media Engine reads it where the game keeps it.
     Each arm meets one Media Engine turn and one main-CPU turn every 400 vblanks. In the main CPU's turns nothing is copied
     in either arm, so there the two arms are the same work. The picture is the same in both arms: on the PC twin and in the
     emulator every frame is the same.
The EBOOT's code is the same source as gl31's plus point 2, but compiled with a profile and a code order measured on this
code (gl31 still ran on the profile and order made for gl20, before the ship's new way existed).

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship too: every 200 vblanks the way its data reaches the second
  processor changes; you should see no difference. If any part of the ship flickers, goes missing or changes look, say when.
  During the walk the picture pauses 9 times and freezes for a quarter second about every 3 seconds (captures). Logs come
  back as usual in LOGS\gl33."

What it saves next to the EBOOT (about 25 MB in all, as gl31):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28 to gl31. No new hardware writes, no new system calls, nothing written to flash, 333 MHz as always, no
power or clock setting, no new memory block. The EBOOT's code is 42 KB larger than gl31's (4,378 KB against 4,336 KB), so the
game's heap is 32 KB smaller at the start (12,576 KB free in the emulator against 12,608); gl30's run on the PSP had about
2.7 MB of heap free at its lowest.

What the run should show:
  - all 62 hashes equal hashref.txt (c9ddb593: the PC twin's run with the same keys; with the builder never run from 24,001
    on, the state hash differs from gl31's 177108d4 from vblank 24,100 on. The in-place arm does not change the state);
  - every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's (916 digests, each with arm X);
  - 75 'nat turn' lines as in gl31, now all 'arm 3': in every line 'the builder run instead 0', 'recorded with no array 0',
    'early fills 0', 'dropped 0', 'partly (dropped) 0', 'plans undone for the groups' room 0';
  - 75 'nat inpl' lines, one per turn, after the 'nat turn' lines: 'inpl 0' or 'inpl 1' and the groups flagged as fixed.
    Emulator and PC twin, whole run: 543,036 groups flagged (256,419 in the Media Engine's inpl-1 turns, 284,296 in the main
    CPU's inpl-1 turns, 2,321 in the first picture of a turn that follows an inpl-1 turn). These counts do not depend on
    speed, so the PSP's should be the same; a picture that straddles a turn's change may be booked one turn later;
  - in the 'nat turn' lines of the Media Engine's inpl-1 turns, 'the groups' data in the copy' falls and 'read from RAM'
    rises: in the rain wait about 48,800 in the copy and 83,900 read from RAM a turn, against 130,000 and 1,700 in the inpl-0
    turns (emulator);
  - the run's measurement: the 'nat turn' and 'window' times of the inpl-1 turns against the inpl-0 turns, separately for the
    Media Engine's turns and the main CPU's turns (the reader prints them by arm), and the same turns against gl31's log;
  - three 'gl29 dz window' lines as gl31; the 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown buffer 0'
    in every turn; the 16 picture pauses each with a 'the flip wait kept' line and no 'GAVE UP' line;
  - every walk capture matching the emulator; no R presses.

What to expect of the speed (PC cost model only, not a PSP time), a picture, against gl31's new-way turns:
  rain wait (29,810-29,990):         the new build alone -24 ms in a Media Engine turn, -21 ms in a main-CPU turn (6 percent);
                                     with the data read in place -36 ms in a Media Engine turn, -21 ms in a main-CPU turn;
  walk in the rain (31,010-31,190):  the new build alone -22 and -18 ms (6 percent); with the data read in place -25 and -18;
  deck before the rain (27,410-27,590) and the opening (16,010-16,190): the new build -7 ms and -6 ms (4.5 percent).
So most of the gain is the new build (fewer instruction-cache misses: about 31 percent fewer in the rain windows, 25 percent
fewer before the rain), in every part of the run; reading the data in place adds 12 ms a picture in the rain wait's Media Engine turns and 4 ms in the walk's, and
nothing in the main CPU's turns. On the PSP the Media Engine's copy is made while the main CPU waits, so the in-place gain
should show as a shorter wait there. The PSP run settles it.
Which turns to compare with gl31's log: gl31 drew the ship the new way in the turns that are 'inpl 1' here (for example
29,800-30,000). In the main CPU's turns of those the in-place arm does nothing, so they measure the new build alone on the
same vblanks; the Media Engine's turns of those measure the new build and the in-place reading together.

How it runs: as gl31's. Claude copies the new files over MGS2T_gl31 on the stick over PSPLink (renaming it MGS2T_gl33) and
starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs
can be copied to LOGS\gl33. A short run, about as long as gl31's.
Keep the USB cable in, as in gl31: PSPLink stays on during the run, so the cable powers the PSP.

Other settings the same EBOOT takes (for a later run, not this one): MGS2_NAT_INPL=1 reads the fixed data in place in every
turn; MGS2_NAT_INPL_CYCLE=1,1,0,0 swaps the two arms' turns, so that the plain turns are the ones gl31 drew the new way
(checked on the PC twin and in the emulator: the build folder's cfg/mgs2_gl33_alt.cfg); with the gl33 key removed and
MGS2_NAT_CYCLE=0,0,3,3 the build draws as gl31.
