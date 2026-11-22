MGS2 gl36: a measurement build. Two new ways to save work on the ship's pieces run in turns, so the PSP can measure each one,
on a freshly trained build (MGS2T_gl36). Personal use only, do not share: it contains code built from your MGS2 disc.
2026-11-22. Build report: mgs2/main/attic/gl36/report.txt (addendum 3 covers this package).

What it is: gl35 with a new EBOOT (and the 8 files next to it, rebuilt with it) and three settings changed in mgs2.cfg. Each is
marked 'gl36' or 'gl36 cull' in mgs2.cfg. Every other setting is gl35's.
  1. MGS2_NAT_KEEP=1 (was MGS2_NAT_KEEP_CYCLE=15,15,0,0). gl35's one keep that paid off on the PSP, the drawn-area marks, is
     now on in EVERY turn. The other keeps are off. The keep's bookkeeping now starts again with each picture: gl35 left the
     last piece's bits standing, so a turn with no ship pieces still carried the texture keep from the turn before.
  2. The reuse, in turns: MGS2_NAT_REUSE_CYCLE=1,1,0,0.
       1  A run of ship pieces that is the same as in the last picture is replayed. The pieces whose results a later piece
          overwrites skip their copies, and each piece's position box is taken from the last picture. Its positions are only
          worked out when the cut keeps the piece. The camera work runs in full.
       0  Plain, as gl35.
     The reuse-off turns fall on gl35's keep-off turns, so gl36's plain turns next to gl35's log on the same vblanks show the
     base change alone.
  3. The early cull, in turns: MGS2_NAT_CULL_CYCLE=1,1,1,1,0,0,0,0.
       1  An effect strip whose every vertex is proven to lie outside one clip plane draws nothing. It is left out before its
          conversion, and the counts the conversion would add are added directly. The proof uses a box over the vertices
          with a proven rounding margin.
       0  Every strip is converted as before.
     There are eight turns against the reuse's four, so every reuse x cull pair gets one Media Engine turn and one main-CPU
     turn in each 800 vblanks.
  Turns are 100 vblanks from vblank 24,001, the same turns as before.
  Every reuse and every cull leaves exactly the state the full work would leave. On the PC twin, every frame, the picture
  code's whole state and every counter are the same with each on, off and in turns. The check modes do the full work anyway
  and compare:
    - the reuse's check found 0 differences;
    - the cull's check found 0 differences on 294,164 left-out strips on the PC twin and on 245,840 in the emulator.
  An independent check of the cull's bound found 0 wrong cuts in 28 million test calls, in both rounding modes, the PSP's
  included.
Also in the EBOOT, with no change for the game:
  - Fix A: at the boot, the disc's file reads no longer hold a second copy of their buffer. The boot's memory peak drops
    by one 354 KB buffer.
  - Fix B: the Media Engine's queue record is back to gl35's size (80 bytes), and one of the reuse's arrays moved out of the
    game's heap. This gives 4.8 KB of heap back.
  - A new profile and code order, trained with every reuse x cull pair. The order now weights the deck before the rain and
    the opening as much as the rain (see "speed" below).

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship and the rain's effects too: the way their pieces are prepared
  changes from turn to turn, and you should see no difference. If anything flickers, goes missing or changes look, say when.
  During the walk the picture pauses 9 times and freezes for a quarter second about every 3 seconds (captures). Logs come back
  as usual, now in LOGS\gl36."

What it saves next to the EBOOT (about 25 MB in all, as gl35):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt, as before.

What it touches: as gl28 to gl35. No new hardware writes, no new system calls or imports (the same 113 NIDs in 17 libraries as gl35; psp-nm counts them as 133 sce* names), nothing written to
flash, 333 MHz as always, no power or clock setting, no new timer.
  - The reuse's table is one block of the system's memory outside the game's heap: 1,536 entries, 96 KB, on the emulator.
    (The 2,304 in mgs2.cfg's comment is the PC twin's size; the comment was left alone so the cfg's md5 stays the same.)
  - Heap: the EBOOT is 15 KB larger than gl35's, so the game's heap starts 9.5 KB higher.
  - Heap low point: vb 23,677, about 64 KB on the emulator, the same as gl35's 64 KB.
  - At the end the allocator can still hand out 64 KB in 1 block.
  - gl30's PSP run had about 2.7 MB of heap free at its lowest; the emulator has less memory than the PSP.

What the run should show:
  - All 62 hashes equal to hashref.txt (c9ddb593, the same file as gl33 to gl35: the game's state is the same in every turn).
  - Every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's: 916 digests, the same 916 as gl35.
  - 75 'nat turn' lines, all 'arm 3', with the same zeros as before. 75 'nat inpl' lines, all 'inpl 1'. 75 'nat fast' lines,
    all 'fast 1'. Emulator, whole run: 993,411 pieces marked, 872,659 taken by the short way, 116,682 handed back for
    'entry', 103 for 'tables full'.
  - 75 'nat keep' lines, all 'keep 1'. Emulator: 866,954 drawn-area marks repeated; 'checked 0, differing 0'.
  - NEW: 75 'nat reuse vb' lines, one per turn, 'reuse 1' or 'reuse 0'. They follow 1,1,0,0, with the runs replayed and
    recorded and the boxes taken.
    - At the start: one 'nat reuse (gl36): the table: made, 1536 entries ...' line.
    - At the end: 'reuse totals (gl36)'. Emulator: 1,099 runs replayed, 3,528 recorded, 211,289 boxes taken, no table room
      63. 'replays with a group not taken' must be 0. The check mode is off, so 'checked 0'.
  - NEW: one 'nat: cull study v3: MGS2_NAT_CULL 0, MGS2_NAT_CULL_CYCLE 1,1,1,1,0,0,0,0 ... MGS2_NAT_CULLCHK 0' line near the
    start, and one 'natcull: totals (v3)' line at the end.
    - Emulator: 168,442 effect calls tested, 124,903 left out (4,385,102 vertices); 'a bad attribute 0, count 0, out of the
      clamps' range 0'.
    - The check mode is off here, so its part reads 'left-out calls converted 0, DIFFERING 0'. That DIFFERING count only
      means something in a check-mode run; a later build will do a PSP check run.
  - These counts depend on the game's state, not on speed, so the PSP's should be the same. gl34's and gl35's counts were the
    same on the PSP as in the emulator.
  - 'nat split' and 'me chains' lines as gl35, one pair per Media Engine turn: 39 and 38 in the emulator.
  - The 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown buffer 0' in every turn. The 16 picture pauses
    each have a 'the flip wait kept' line and no 'GAVE UP' line.
  - Every walk capture matches the emulator's. No R presses.

The run's measurements. (a), (b) and (c) are all worked out in the container from the per-turn table, split by both arms.
For a turn starting at vblank vb, let t = (vb - 24,001) / 100. The cull is on when t % 8 < 4; the reuse is on when t % 4 < 2.
rd36pc.py groups its turns by the reuse arm only, so its 'lever' and 'base change' lines mix the two cull arms. Use them only
as a first look.
  (a) Reuse 1 turns against reuse 0 turns, cull equal, Media Engine and main-CPU turns apart: the reuse.
  (b) Cull 1 turns against cull 0 turns, reuse equal: the cull.
  (c) The reuse 0 / cull 0 turns against gl35's log on the same vblanks (gl35's keep-0 turns): the base change. This covers
      keep 1 with the reset, fixes A and B, and the new profile and code order.
  (d) 'nat split': where the Media Engine's time per ship piece goes, by arm.

What to expect of the speed. These are PC cost-model figures, ms a step, not PSP times. On recent changes the model has
rated Media Engine savings 1.5 to 2.2 times higher than the PSP showed, so expect the PSP's gains to be smaller.
  The cull, reuse equal: rain wait (29,810-29,990) -4.8 ms in a Media Engine turn, -4.8 in a main-CPU turn; rain walk
  (31,010-31,190) -6.7 and -6.5. On the PSP perhaps -2 to -4.4.
  The reuse, cull equal: rain wait -3.5 and -2.8; rain walk +2.2 and +0.3 (SLOWER: few runs repeat there).
  Both on against gl35's keeps (the arms people will see most): rain wait -3.6 and -7.1, rain walk -2.8 and -6.1.
  KNOWN REGRESSION, the base (both off) against gl35's keep 0:
    - rain wait +4.0 ms in a Media Engine turn and 0.0 in a main-CPU turn, about +2 ms a step for the pair;
    - rain walk +2.8 and +0.5.
    The deck before the rain and the opening are AHEAD of gl35: -0.8 / -1.7 and -0.7 / -1.7.
    The new code order traded instruction-cache misses from the rain to the deck and the opening. Part of the regression is
    also data-cache misses from where the data now sits, which no code order fixes. gl37 will address both: an order that
    keeps every scene within 1 percent, and a data-placement test. The reuse 0 / cull 0 turns in this run will show the
    regression on the PSP.

How it runs: as gl35's. Claude copies the new files over MGS2T_gl35 on the stick over PSPLink (renaming it MGS2T_gl36) and starts
it from the PC, telling you first. At the end ("Press X to exit"), press X and start "PSPLink (PC control)" so the logs can be
copied to LOGS\gl36. A short run, about as long as gl35's.
Keep the USB cable in, as in gl35: PSPLink stays on during the run, so the cable powers the PSP.

Other settings the same EBOOT takes (for a later run, not this one), each checked on the PC twin:
  - MGS2_NAT_REUSE=1 or 0: the reuse in every turn, or never. MGS2_NAT_REUSE_CYCLE=0,0,1,1 swaps the reuse's turns.
  - MGS2_NAT_RCHK=1: the reuse's check mode.
  - MGS2_NAT_CULL=1: the cull in every turn. Remove MGS2_NAT_CULL_CYCLE for none.
  - MGS2_NAT_CULLCHK=1: the cull's check mode. It converts every strip the cull would leave out, compares, and prints
    DIFFERING in the natcull line. Note: it checks in every cull-on turn, so it also turns the cull's saving off there.

Files (md5 in MANIFEST.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, rd36pc.py, this README. The disc pack
(mgs2disc.pak) and SLES_503.83 already on the stick stay as they are; their md5s are in the manifest too.

Reading the logs: in the container  python3 -I /home/claude/main/build_gl36/rd36.py <log.txt> --bat <battery.txt> [--live <dir>]
                  on the PC         python -I rd36pc.py <LOGS\gl36> <LOGS\gl35>     (one file, no other files needed)
