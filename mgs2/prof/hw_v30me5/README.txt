MGS2 v30 ME rests (me5) (MGS2T_v30me5) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: v30 with one change, for the switch-offs. The game's code, the indoor route, the sound mix on the Media Engine, the
crash catcher and battery.txt are v30's. On the emulator every state hash and every sound checkpoint matched the PC's. The
emulator cannot run the Media Engine, so its rest and wake-up are first tried on your PSP.
- The change: when it has no sound commands to mix, the Media Engine now waits in its halt instruction instead of polling at full
  speed, and the main CPU wakes it with one interrupt after it queues new commands. In v30 run 5 the mix kept it busy about 5% of
  the time in the indoor part (2-16%, the most in the alert) and 7% over the opening, by its idle polls; the rest of the time it
  polled at full speed.
- Why: with the Media Engine on, v30 switched the PSP off about 20 minutes in on the cheap battery, 2 runs of 2; with it off v30
  finished 2 of 2, one of them on that battery all the way. The full-speed Media Engine is the main suspect.
- New in log.txt: a "me rest:" line at the start, then every 100 vblanks a "mix vb ... ME rests" line: how many times it rested,
  how it woke, the main CPU's wake calls and their time, and the words the PSP's own system uses to send requests to the Media
  Engine (a change there would show the system asking it something). battery.txt adds the same counts to every line, plus the
  battery's remaining and full capacity and the system's "suspend required" flag.
- If the Media Engine ever fails to wake: after 2 seconds the main CPU stops the sound mix and puts the Media Engine back in reset
  ("mix: stopped" in log.txt); the game goes on to the end, exact, as with the Media Engine off.
- mgs2.cfg: MGS2_ME_REST=1 is the rest; 0 turns it off (the Media Engine polls at full speed, as in v30).
- Expected time per step: v30's with the Media Engine (37.41 ms indoors in run 5) plus the wake calls, which log.txt measures.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_v30me5 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_v30me5/ (EBOOT.PBP, the eight ovl_*.bin
   files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). mgs2disc.pak and SLES_503.83 are
   v30's, copied on your PC.
3. Start "MGS2 v30 ME rests (me5)" from the Game menu. The crash catcher's self-test is offered as before; press O (or wait 15
   seconds) to start the run.
4. Please run it on the cheap battery, charged, and without the USB cable if you can: those are the conditions of v30's first
   switch-off, and of the ME-off run that finished on that battery. Let it run to the end (about 38 minutes). Do not put the
   PSP to sleep. HOME quits at any time.
5. If a crash happens, the red text stays on the screen: please photograph it. If the PSP switches itself off, please say
   whether you found it off or frozen.
6. At the end it shows "Press X to exit". Send back log.txt, battery.txt and crash.txt, plus steps.csv and me_steps.csv.

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or
clock setting is changed (333 MHz as always). New in this build, the three register writes you agreed to on the card: the
Media Engine's own interrupt enable and flags (0xBC300008 and 0xBC300000, written by the Media Engine) and the main CPU's
interrupt to the Media Engine (0xBC100044, through the same kernel bridge as the Media Engine's start). Sleep is held off while
the Media Engine runs, and the Media Engine is put back in reset at the end and before any exit.
