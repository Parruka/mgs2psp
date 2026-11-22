MGS2 harness gl2d system diagnostics (MGS2T_gl2d) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl2 (the game's picture on the PSP, the same game work and the same settings) with one addition for the switch-offs:
battery.txt now also records what the PSP's own system does while the game runs. Nothing else changes: the game's code, the route,
the picture and the timing windows are gl2's, the Media Engine is off, and the game's state is not touched, so the state hashes must
stay the PC's.

Why: the PSP has switched itself off in some runs with no crash record, the last time in gl1 with the Media Engine off. In the parts
of the route where that happened, the game hardly ever pauses, so the PSP's own background tasks (those below the game's priority)
may get no time for many seconds. That is a guess until it is measured; this run measures it, and also writes down any power notice
the system sends (low battery, standby, the power switch and so on) the moment it comes.

What the new lines record (battery.txt only, read-only):
- "power event ...": any power notice the system sends to games, with its flags, at once.
- Every 5 s, at the end of the usual battery line: how long a low-priority task of the harness had to wait for the processor (it asks
  for a turn every 10 ms), how many waits were over 100 ms, and, from the system, how often the PSP switched between tasks and how
  long it was idle.
- Every minute: the list of the tasks (threads) the harness can see, with their priorities and states.
Nothing is written to flash, no setting is changed (333 MHz as always), no register is written, and it all runs in user mode like
the rest of the harness. The power-notice hook is taken out before every exit, and the two small tasks it adds end with the harness.

What you will see: the same as gl2 (see gl2's README): the logos, title and menus; a black opening apart from some subtitles; the
codec screen about minutes 15 to 21 (faces and text not drawn yet); mostly black again with a few short pictures; from about minute
34 the Tanker deck with the life bar, radar and weapon boxes, then about 8 minutes indoors with the four timing settings taking
turns. Snake and the guards are not drawn yet.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl2d from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl2d/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). The gl2 folder can stay or go.
3. Start "MGS2 harness gl2d system diagnostics" from the Game menu. The crash catcher's self-test is offered as before; press O (or
   wait 15 seconds) to start the run.
4. Please run it on the cheap battery, charged, with the USB cable pulled out before the start, to the end (about 45 to 50
   minutes). Do not put the PSP to sleep. HOME quits at any time.
5. If it switches itself off, please note roughly when (minutes in) if you saw it, and send log.txt, battery.txt and crash.txt:
   battery.txt's last lines are the point of this run. If a crash happens, the red text stays on the screen: please photograph it.
6. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No register writes: the Media Engine is not started.
