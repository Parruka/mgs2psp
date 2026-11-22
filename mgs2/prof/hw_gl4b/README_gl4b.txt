MGS2 harness gl4b crash catcher 3b (MGS2T_gl4b) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl4 with one change, crash catcher 3b. The game's code, the route, the timing windows, the pictures and the diagnostics are
gl4's; the Media Engine is off, and the game's state hashes must stay the PC's.

Why: gl4's run switched the PSP off about 17 minutes in, during the first codec call (vblanks 16,200-16,275). Every stop with the
Media Engine off has come in a codec call (v24, gl1, gl3, gl4), and every run was exact up to its stop. Catcher 2 leaves its record
on the screen only, so the files cannot say whether these stops are crashes or the power going off by itself. Catcher 3 was meant to
put the record in the files too, but its self-test switched the PSP off about 10 seconds after the record.

Crash catcher 3b. It draws the same red record first, with the CPU's interrupts off. Then, when the crash came from the game or the
harness (not from the system itself), it hands the crashed thread back to the system the way PSPLink, a proven PSP debugger, does it:
the thread moves onto its own system stack, gets the small record the system keeps for a thread that is inside a system call, and
goes to sleep in the system's own sleep function with interrupts back on. Catcher 3 skipped those two steps and kept the thread
spinning, and nothing ran after that. With 3b the PSP's other threads run again: a small thread writes the record to crash.txt and
log.txt, keeps battery.txt going and adds three green lines under the record. A crash inside the system itself keeps catcher 2's
behaviour (the screen only; the record says so).

What you will see: the same as gl4 (the emulator's pictures of this build are byte for byte gl4's). The logos, title and menus; a
black opening apart from some text; the codec screen about minutes 15 to 21 and again about 30 to 33 (faces and text not drawn
yet: the text box is plain grey); a plain grey picture inside a viewfinder about minute 28; from about minute 35 Snake on the aft
deck in the rain; from about minute 42 indoors: Snake in the corridors, the guard in the hallway, the alert and the evasion, with
the four timing settings taking turns. Still missing: the radar's map, and a few surfaces close to the camera show as flat colour.
gl3's full run took 57 minutes to "Press X to exit"; this one should take a little less.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl4b from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl4b/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The gl4 folder can
   stay or go.

First the self-test (about 3 minutes):
3. Start "MGS2 harness gl4b crash catcher 3b" from the Game menu. At the crash catcher's question press X.
4. The PSP should show a red record titled SELF-TEST, white text under it ("Interrupts are back on ..."), and within a second three
   green lines: "The system runs again: the writer thread is saving the record.", then "Saved to crash.txt and log.txt ... s after
   power-on." and "The stopped thread sleeps and is suspended; HOME quits." Please photograph it.
5. Leave it on for 2 minutes, then press HOME (and confirm quitting if the PSP asks). If nothing happens in 10 seconds, hold the
   power switch up.
6. Copy crash.txt, log.txt and battery.txt from ms0:/PSP/GAME/MGS2T_gl4b/ to the PC (for example into LOGS\gl4b\selftest): the
   next start of gl4b clears them. Please tell me what happened: how many green lines, whether the PSP stayed on, what HOME did.
   If you saw all three green lines and HOME took you back to the system, go on with the run below; otherwise send the three
   files and the photo and wait for me.

Then the run:
7. Start it again and press O (or wait 15 seconds). Please run it on the cheap battery, charged, with the USB cable pulled out
   before you start it (in gl4's run it was still in for the first 14 seconds), to the end. Do not put the PSP to sleep. HOME
   quits at any time. Optional but very useful: film the PSP's screen with a phone from about minute 14 to minute 35 (both codec
   calls). If it stops there, the last seconds of the video show whether a red record, a frozen picture or an instant black screen
   came first, which the files cannot tell.
8. If it stops: please note whether the PSP switched itself off, and photograph the screen if a red record shows. Then send
   log.txt, battery.txt and crash.txt: with 3b, crash.txt should hold the record of a crash even if the PSP goes off afterwards.
9. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv
   and the six shot_*.bmp files (copy them to the PC, for example into LOGS\gl4b, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started. Catcher 3b's new steps happen
only after a crash: it puts the crashed thread on the system's own stack for it and calls the system's own sleep function, as
PSPLink does (PSPLink is open source under the BSD licence; its method is credited in the catcher's source).
