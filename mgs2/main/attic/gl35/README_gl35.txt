MGS2 gl35, the ship's short drawing path in every turn, with work kept from one ship piece to the next in turns, on a freshly
trained build (MGS2T_gl35) - personal use only, do not share (it contains code built from your MGS2 disc).
2026-11-21 (build report: mgs2/main/attic/gl35/report.txt).

What it is: gl34 with a new EBOOT (and the 8 files beside it, rebuilt with it) and two settings changed in mgs2.cfg. Each is
marked 'gl35' in mgs2.cfg; every other setting is gl34's.
  1. The ship's plain textured pieces take gl34's short way in EVERY turn (MGS2_NAT_FAST=1; gl34 did it in half of the
     turns, and its PSP run was exact).
  2. In turns (MGS2_NAT_KEEP_CYCLE=15,15,0,0; turns of 100 vblanks from vblank 24,001, the same turns as before):
       15  "keeps": a ship piece whose bookkeeping would repeat what an earlier piece just did repeats its effects directly
           instead of working them out again: the drawn-area marks (when nothing has changed since), the texture look-up
           (when the same texture was looked up earlier in the same picture and nothing it depends on has changed), and
           one call to the colour-table loader that can do nothing for that texture is left out;
        0  no keeps: gl34's short way exactly.
     The 15 turns fall on gl34's usual-way turns and the 0 turns on gl34's short-way turns, so the 0 turns next to gl34's
     log on the same vblanks show the new training alone, and the 15 turns next to the 0 turns show the keeps.
     Every keep leaves exactly the state the full work would leave. On the PC twin every frame, the picture code's whole
     state and every counter are the same with the keeps on, off and in turns, and a check mode that does the full work
     anyway and compares found 0 differences in 4.2 million keeps.
The EBOOT's code is gl34's plus point 2, compiled with a profile and a code order measured again on this code, with both
arms of point 2 in the training, and two small functions put back in line where gl33 had them (they had moved out in gl34).

What you do (the same as last time):
  "On the rain deck, hold R whenever you see the black flickering marks on the deck floor and let go when they stop. If the fix
  works you should not need to press it at all. Watch the ship too: every 200 vblanks the way its pieces are prepared
  changes; you should see no difference. If any part of the ship flickers, goes missing or changes look, say when.
  During the walk the picture pauses 9 times and freezes for a quarter second about every 3 seconds (captures). Logs come
  back as usual in LOGS\gl35."

What it saves next to the EBOOT (about 25 MB in all, as gl34):
  - shot_<vblank>.bmp: the 16 pictures named in mgs2.cfg (9 in the walk, 7 earlier);
  - live_<vblank>.bmp: the walk's captures, every 25 vblanks, and at your first 8 R presses;
  - log.txt, steps.csv, battery.txt and crash.txt as before.

What it touches: as gl28 to gl34. No new hardware writes, no new system calls, nothing written to flash, 333 MHz as always, no
power or clock setting, no new memory block, no new timer. The EBOOT's code is 5 KB larger than gl34's (4,408 KB against
4,402 KB), so the game's heap is that much smaller at the start; in the emulator the lowest free heap is 64 KB (gl34: 64
KB) and at the end the allocator can still hand out 64 KB in 1 block (gl34: 80 KB in 2). gl30's run on the PSP had about
2.7 MB of heap free at its lowest (the emulator has less memory than the PSP).

What the run should show:
  - all 62 hashes equal hashref.txt (c9ddb593, the same file as gl33's and gl34's: the game's state is the same in every
    turn);
  - every frame's list from 29,490 to 31,400 equal to the emulator's and the PC twin's (916 digests; the same 916 as gl34's
    and gl33's);
  - 75 'nat turn' lines, all 'arm 3', with the same zeros as gl34 ('the builder run instead 0', 'recorded with no array 0',
    'early fills 0', 'dropped 0', 'partly (dropped) 0', 'plans undone for the groups' room 0'); 75 'nat inpl' lines, all
    'inpl 1'; 75 'nat fast' lines, now all 'fast 1'. Emulator, whole run: 993,411 pieces marked, 872,659 taken by the
    short way, 116,682 handed back for 'entry', 103 for 'tables full', 0 for every other reason;
  - NEW: 75 'nat keep' lines, one per turn, 'keep 15' or 'keep 0', with the pieces flagged and the steps kept. Emulator,
    whole run: 379,186 pieces flagged, 329,029 drawn-area marks and 943,115 texture look-ups repeated, 330,823 colour-table
    calls left out; 'checked 0, differing 0' everywhere (the check mode is off on the PSP). These counts depend on the
    game's state, not on speed (gl34's short-way counts were the same on the PSP as in the emulator), so the PSP's should
    be the same. Expect two oddities, both harmless: a 'keep 0' turn in the rain still shows a few hundred keeps (the
    turn's first picture was prepared in the turn before), and on the deck before the rain (26,200-28,700, no ship pieces)
    EVERY turn, 0 or 15, shows 24,100 texture look-ups repeated (the setting is switched by the ship's pieces, and there
    are none there to switch it off; so the deck is not a keep-off reference);
  - 38 'nat split' lines and 38 'me chains' lines as gl34 (one pair per Media Engine turn), now also giving 'keep 15'
    or 'keep 0' turn by turn through the 'nat keep' line of the same turn;
  - three 'gl29 dz window' lines as gl34; the 'gl27 display' lines: 'flip wait on' and 'lists drawn into the shown
    buffer 0' in every turn; the 16 picture pauses each with a 'the flip wait kept' line and no 'GAVE UP' line;
  - every walk capture matching the emulator; no R presses.

The run's measurements (the PC reader prints them):
  (a) the 'keep 15' turns against the 'keep 0' turns next to them, Media Engine turns and main-CPU turns apart: the keeps;
  (b) the 'keep 0' turns against gl34's log on the same vblanks (gl34's short-way turns): the new training and code order;
  (c) the 'keep 15' turns against gl34's log on the same vblanks (gl34's usual-way turns): short way + keeps + training;
  (d) 'nat split': where the Media Engine's time per ship piece goes, keep 15 against keep 0.

What to expect of the speed (PC cost model only, not a PSP time), a picture:
  The new training and code order (the 'keep 0' turns against gl34's short-way turns, same code path):
    rain wait (29,810-29,990):         -4.0 ms in a Media Engine turn, -5.1 ms in a main-CPU turn;
    walk in the rain (31,010-31,190):  -2.4 ms and -3.9 ms;
    deck before the rain (27,410-27,590): -1.6 ms and +0.1 ms; the opening (16,010-16,190): -1.7 ms and 0.0 ms.
  The code misses the instruction cache 10 to 13 percent less often than gl34's in every window. Against gl33 in the
  model: the Media Engine turns are 0.5 to 7.9 ms a picture faster everywhere, the main-CPU turns 2.8 and 5.8 ms faster
  in the rain, but on the deck before the rain and in the opening the main-CPU turns stay 2.1 to 2.6 ms slower than
  gl33's (as in gl34): there the gain in the instruction cache is eaten by 8 percent more data-cache misses.
  The keeps (keep 15 against keep 0, same build, in every turn of a model run):
    rain wait: -0.5 ms in a Media Engine turn, 0.0 ms in a main-CPU turn; walk in the rain: +1.3 and +0.9 ms (SLOWER).
  That is far short of the aim (-35 percent of the ship's native work on the Media Engine, about 63 to 40 millionths of a
  second a piece, and -10 ms a picture expected from the design). The model's reason: the keeps cut 0.4 percent of the
  instructions (the drawn-area marks save about 1.2 ms a picture, as planned), but the texture keep costs about as much as
  the look-up it replaces, and the new code adds instruction-cache misses in places the code order did not plan for. The
  same keeps without the 'every look-up' part (keep 7) model -0.6, -1.5, +0.3 and -0.2 ms. Twice before the model rated
  Media Engine savings about four times higher than the PSP showed; here it rates them near zero, so the PSP run's (a)
  may be about 0 or a small loss. Measuring it is the point of this run. All of this is the PC model; the PSP run settles
  the real figures.

How it runs: as gl34's. Claude copies the new files over MGS2T_gl34 on the stick over PSPLink (renaming it MGS2T_gl35) and
starts it from the PC, telling you first. At the end ("Press X to exit") press X and start "PSPLink (PC control)" so the logs
can be copied to LOGS\gl35. A short run, about as long as gl34's.
Keep the USB cable in, as in gl34: PSPLink stays on during the run, so the cable powers the PSP.

Other settings the same EBOOT takes (for a later run, not this one): MGS2_NAT_KEEP_CYCLE=0,0,15,15 swaps the two arms'
turns (checked on the PC twin); MGS2_NAT_KEEP=15 keeps in every turn and 0 never; any of the bits 1, 2, 4, 8 alone (each
checked on the PC twin); MGS2_NAT_KCHK=1 does the full work anyway and counts any difference from what a keep would have
done (a check for the PC and the emulator: 0 differing on the twin); MGS2_NAT_FAST_CYCLE=0,0,1,1 in place of
MGS2_NAT_FAST=1 brings back gl34's turns (the twin checked the short way in turns with the keeps on).

Files (md5 in MANIFEST_gl35.md5): EBOOT.PBP, ovl_d00t.bin, ovl_d01t.bin, ovl_init.bin, ovl_n_title.bin, ovl_r_title.bin,
ovl_r_tnk0.bin, ovl_w00a.bin, ovl_w01a.bin, mgs2.cfg, hashref.txt, pad.txt, this README. The disc pack (mgs2disc.pak) and
SLES_503.83 already on the stick stay as they are.

Reading the logs: in the container  python3 -I /home/claude/main/build_gl35/rd35.py <log.txt> --bat <battery.txt> [--live <dir>]
                  on the PC         python -I rd35pc.py <LOGS\gl35> <LOGS\gl34>     (one file, no other files needed)
