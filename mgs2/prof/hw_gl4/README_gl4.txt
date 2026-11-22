MGS2 harness gl4 faster characters (MGS2T_gl4) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl3 (whose second run went to the end, exact) with three changes. The game's code, the route, the timing windows, the
pictures and the diagnostics are gl3's; the Media Engine is off, and the game's state hashes must stay the PC's.

1. Faster characters. The per-vertex work for Snake, the crew and the guards (lighting, skinning, the extra layers) now runs in
   loops made for each kind of model, and the game's vertex data is unpacked by kinds. Every result is the same as before (checked
   vertex by vertex on the PC, 933,883 vertices), so the pictures are gl3's. On the cost model the deck's step goes from 137.7 to
   85.7 ms and the heaviest indoor step from 217.1 to 162.5 ms; the timing windows will show the PSP's own figures.
2. Memory checks. Every memory request of the harness's PS2-side services (disc and file reads, the memory card, the PS2's data
   channel) is checked: if one ever fails, the run stops with "no memory for ..." on the screen and in log.txt and waits for X,
   instead of crashing as gl2d did.
3. Crash catcher 3. Catcher 2 left its record on the screen only, and your PSP switched itself off a while after gl2d's caught
   crash, so a crash nobody watches looks like a switch-off without a record (the three unexplained stops, all during codec calls,
   may be such crashes). Catcher 3 draws the same red record first. Then, when the crash came from the game or the harness (not
   from the system itself), it turns the CPU's interrupts back on so the PSP's own threads go on: a small new thread writes the
   record to crash.txt and log.txt, stops the crashed thread, keeps battery.txt going and adds two green lines under the record.
   So the files keep the record even if the PSP still switches itself off afterwards, and battery.txt shows when. HOME should then
   quit as usual. A crash inside the system keeps catcher 2's behaviour (the screen only; the record says so).

What you will see: the same as gl3 (the emulator's pictures of this build are byte for byte gl3's). The logos, title and menus; a
black opening apart from some text; the codec screen about minutes 15 to 21 and again about 30 to 33 (faces and text not drawn
yet: the text box is plain grey); a plain grey picture inside a viewfinder about minute 28; from about minute 35 Snake on the aft
deck in the rain; from about minute 42 indoors: Snake in the corridors, the guard in the hallway, the alert and the evasion, with
the four timing settings taking turns. Still missing: the radar's map, and a few surfaces close to the camera show as flat colour.
gl3's second run took 57 minutes to "Press X to exit"; gl4 should take a little less.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl4 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl4/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The gl3 folder can
   stay or go.

First the self-test (about 3 minutes):
3. Start "MGS2 harness gl4 faster characters" from the Game menu. At the crash catcher's question press X.
4. The PSP should show a red record titled SELF-TEST, white text under it ("Interrupts are back on ..."), and within a second two
   green lines: "Saved to crash.txt and log.txt ... s after power-on." and "The stopped thread is suspended; the PSP's other threads
   run." Please photograph it.
5. Leave it on for 2 minutes, then press HOME (and confirm quitting if the PSP asks). If nothing happens in 10 seconds, hold the
   power switch up.
6. Copy crash.txt, log.txt and battery.txt from ms0:/PSP/GAME/MGS2T_gl4/ to the PC (for example into LOGS\gl4\selftest): the
   next start of gl4 clears them. Please tell me what happened: green lines or not, whether the PSP stayed on, what HOME did.
   If you saw the green lines and HOME took you back to the system, go on with the run below; otherwise send the three files and
   wait for me.

Then the run:
7. Start it again and press O (or wait 15 seconds). Please run it on the cheap battery, charged, with the USB cable pulled out
   before the start, to the end. Do not put the PSP to sleep. HOME quits at any time.
8. If it crashes, the record should now also be in crash.txt and log.txt: please photograph the screen anyway, note roughly when
   (minutes in) if you saw it, and send log.txt, battery.txt and crash.txt. The same if it switches itself off.
9. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files (copy them to the PC, for example into LOGS\gl4, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes; the Media Engine is not started. Catcher 3's one new step is turning interrupts
back on, a bit in the CPU's own status register that the system sets and clears all the time; it is not a new module.
