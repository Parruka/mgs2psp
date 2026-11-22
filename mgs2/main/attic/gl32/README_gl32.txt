MGS2 gl32, the ship drawn the new way in every turn, with a vertex cache in turns (MGS2T_gl32) - personal use only, do not
share (it contains code built from your MGS2 disc).
DRAFT by the build agent, 2026-11-21, for the lead to edit (build report: mgs2/main/attic/gl32/report.txt).

What it is: gl31 with a new EBOOT (and the 8 files beside it, rebuilt with it) and three settings changed in mgs2.cfg. Each is
marked 'gl32' in mgs2.cfg; every other setting is gl31's.
  1. The ship's meshes are drawn the new way in EVERY turn from vblank 24,001 (MGS2_NAT_CYCLE=3,3,3,3, was 0,0,3,3). The
     game's own packet builder for the ship no longer runs in this part of the run.
  2. The vertex cache, in turns (MGS2_NAT_VC_CYCLE=0,0,1,1,3,3; turns of 100 vblanks, the same turns as before):
       0  no cache: the ship drawn as in gl31's new turns;
       1  the cache: a ship mesh whose data the game keeps unchanged is converted for the PSP's graphics chip once and kept;
          when it is drawn again its points, colours and strips are taken from the cache;
       3  the cache, and in the Media Engine's turns that mesh data is no longer copied for the Media Engine first: it reads
          it where the game keeps it.
     Each arm meets one Media Engine turn and one main-CPU turn every 600 vblanks. The picture is the same in all three: on
     the PC twin and in the emulator every frame is the same with the cache on and off.
  3. The cache's memory: 256 KB taken from the heap at the start (MGS2_NAT_VC_KB=256).

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship too: every 100 vblanks the way it is drawn changes (no cache,
  cache, cache without the copy); you should see no difference. If any part of the ship flickers, goes missing or changes
  look, say when. During the walk the picture pauses 9 times and freezes for a quarter second about every 3 seconds
  (captures). Logs come back as usual in LOGS\gl32."

What it saves next to the EBOOT (about 25 MB in all, as gl31):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28 to gl31. No new hardware writes, no new system calls, nothing written to flash, 333 MHz as always, no
power or clock setting. New in memory: one block of 256 KB from the game's heap at the start (the cache), with the heap call
the picture code already uses. gl30's run on the PSP had about 2.7 MB of heap free at its lowest, so it should fit. If the
heap could not give it, the log says 'NOT MADE' in the 'vertex cache (gl32)' line, the ship is drawn without the cache in
every turn and the pictures stay the same.

What the run should show:
  - all 62 hashes equal hashref.txt (c9ddb593: a new reference, the PC twin's run with the same keys; with the builder never
    run from 24,001 on, the state hash differs from gl31's 177108d4 from vblank 24,100 on);
  - every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's (916 digests, each with arm X);
  - near the start a 'vertex cache (gl32): block 256 KB' line that names the heap (the emulator's line names 'the copy
    arena's end': the emulator has no Media Engine and keeps the copy area in the heap, so it takes the block from that
    area; the PSP's copy area stays as in gl31);
  - 75 'nat turn' lines as in gl31, now all 'arm 3': in every line 'the builder run instead 0', 'recorded with no array 0',
    'early fills 0', 'dropped 0'. In the emulator one turn, 25,800-25,900, has 'plans undone for the groups' room 41': its
    copy area is 256 KB smaller there. The PSP's is not, so the PSP should show 0 as gl31's emulator run did (this is the
    one line where the PSP is expected to differ from the emulator);
  - 75 'nat vc' lines, one per turn: the arm (vc 0, 1 or 3), groups looked up in the cache, found, not found, drawn from
    the cache, drops. Emulator and PC twin, whole run: found 393,659 (211,699 with their box only, 181,960 with their
    points), not found 6,154, stored 2,365, drawn from the cache 170,781, not looked up 0, colour byte rewritten 0, the
    cache emptied 14 times for a new generation (an object made or freed, another stage), 0 times for want of entries, once
    for want of room (turn 26,000-26,100), most in use 1,492 of 2,048 entries and 174 of 175 KB; on the deck (from 29,000)
    at most 617 entries and 116 KB, and in the rain wait every look-up is a find but for the first picture after a new
    generation. These counts do not depend on speed, so the PSP's should be the same;
  - the run's measurement: the 'nat turn' and 'window' times of the vc 1 and vc 3 turns against the vc 0 turns, separately
    for the Media Engine's turns and the main CPU's turns (the reader prints them by arm);
  - three 'gl29 dz window' lines as gl31; the 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown buffer 0'
    in every turn; the 16 picture pauses each with a 'the flip wait kept' line and no 'GAVE UP' line;
  - every walk capture matching the emulator; no R presses.

What to expect of the speed (PC cost model only, not a PSP time). Against this EBOOT's own 'no cache' turns, a picture:
  rain wait (29,810-29,990):  the cache alone -4.1 ms in a Media Engine turn, -3.1 ms in a main-CPU turn (about 1 percent);
                              the cache without the copy -15.8 ms in a Media Engine turn, -3.1 ms in a main-CPU turn;
  walk in the rain (31,010-31,190):  the cache alone -0.5 and +0.4 ms (nothing); without the copy -3.4 and -0.2 ms.
So nearly all of the gain is the copy that is no longer made, and only the Media Engine's turns have it. The same model puts
this EBOOT's 'no cache' turns 7 to 13 ms a picture ABOVE gl31's new turns (more code on the ship's path, so more
instruction-cache misses), which is more than the cache gives back: against gl31 only the vc 3 Media Engine turns of the
rain wait come out ahead, by 2.6 ms. The PSP run settles it. Twelve turns are 'new way, no cache' in both gl31's run and
this one (for example 29,400-29,600 and 30,600-30,800): comparing them between the two logs measures the code's cost.

How it runs: as gl31's. Claude copies the new files over MGS2T_gl31 on the stick over PSPLink (renaming it MGS2T_gl32) and
starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs
can be copied to LOGS\gl32. A short run, about as long as gl31's.
Keep the USB cable in, as in gl31: PSPLink stays on during the run, so the cable powers the PSP.

Other settings the same EBOOT takes (for a later run, not this one): MGS2_NAT_VC_CYCLE=0,0,2,2 times the 'no copy' part
alone (2 = the mesh data read in place, no cache); MGS2_NAT_VC_CYCLE=0,0,3,3 gives each arm twice as many turns; with both
gl32 keys removed and MGS2_NAT_CYCLE=0,0,3,3 the build draws and runs as gl31.
