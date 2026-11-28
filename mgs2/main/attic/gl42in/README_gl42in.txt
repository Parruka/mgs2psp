MGS2 harness gl42in: the INDOOR measurement (2026-11-27)
=========================================================

What it is
----------
gl42's own EBOOT and overlays (nothing rebuilt) with a new settings file and its hash reference.  The run goes on past the deck
(vb 31,400) through the indoor stretch of the test route (the crew deck, stage w01a) to vb 37,800, and from vb 31,800 to 36,800 it
takes the same turns as on the deck, each 100 vblanks:
  K-A   everything drawn on today's path           K-N1  the ship's builder native
  K-N2  that and the people native                  K-N2R K-N2 with only those drawn (the picture is incomplete in these turns, on purpose)
  K7    nothing drawn (black for 2 s; the main CPU alone)
Turns up to vb 31,400 are exactly gl42's.  vb 36,800-37,800 run on today's path with F1 off and XP 0 (the EBOOT holds 128 turns).
It is a MEASUREMENT run: the picture changes from turn to turn on purpose.  The game's state is exact in every turn.

Files in this zip (5)
---------------------
  mgs2.cfg            the new settings (replaces gl42's)
  hashref.txt         this run's hash reference (replaces gl42's; the PC twin with these settings)
  rd42in.py           the log reader for this run (new; rd42pc.py stays in the folder, not used)
  README_gl42in.txt   this file
  MANIFEST.md5        the 18 files of Builds\MGS2T_gl42in (gl42's 16 with the new mgs2.cfg and hashref.txt, plus rd42in.py and this README)

To make the build folder on the PC
----------------------------------
  1. Copy Builds\MGS2T_gl42 to Builds\MGS2T_gl42in (a copy: MGS2T_gl42 stays as it is).
  2. Put this zip's 5 files into Builds\MGS2T_gl42in (mgs2.cfg, hashref.txt and MANIFEST.md5 replace gl42's).
  3. Check every file's md5 against MANIFEST.md5 (18 lines; README.txt and rd42pc.py are gl42's, unchanged).
  4. Make the empty folder LOGS\gl42in.

To run it (the usual procedure, utils\psplink\README.md)
--------------------------------------------------------
  - Free check first (no build loaded, no run in progress on the stick).
  - gl42's logs are already on the PC (LOGS\gl42 and LOGS\gl42\r2).  Either
        update MGS2T_gl42 MGS2T_gl42in "<Builds\MGS2T_gl42in>" "<LOGS\gl42\r2>"
    or, since the owner allowed it (2026-11-27), remove MGS2T_gl42 "<LOGS\gl42\r2>" and then stage MGS2T_gl42in.
    Then check, plugin-on, launch MGS2T_gl42in.
  - Wall charger plugged into the PSP for the whole run.
  - The run ends on "Press X to exit" and WAITS there.  Expected length: about 37 minutes [I] (gl42's 30 + the indoor stretch).
  - After the end: the owner presses X and starts "PSPLink (PC control)"; fetch MGS2T_gl42in "<LOGS\gl42in>";
    python -I rd42in.py "<LOGS\gl42in>" > "<LOGS\gl42in>\rd42in_out.txt"

What the reader should say (as the emulator's run of the same files, 2026-11-27)
---------------------------------------------------------------------------------
  HASHES 124 lines ... EQUAL      DIGESTS 161 lines ... EQUAL      CONTENT 161 lines ... EQUAL
  textures refused for want of memory: 0      SCHEDULE CHECK: none      PROBLEM COUNTERS: none / 0, XP check DIFFERING 0
  Memory: the step record is 6,400 steps (deck 31400), smaller than gl42's; on the emulator 159 KB of newlib room at the worst moment.
The answers we want are in the reader's INDOOR tables: ms a PSP step by kind (step, main, ME, P), K-N2 and K-N2R against K-A (what the
native renderer gives indoors) and K-A against K7 (what drawing costs the main CPU indoors), and INDOOR BY TURN for the scenes.

If the PSP switches off or the run stops early: nothing to do on the PSP side but start it again later; send what is in the folder
(log.txt, battery.txt, crash.txt) all the same.
