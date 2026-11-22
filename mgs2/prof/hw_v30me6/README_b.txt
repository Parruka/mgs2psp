MGS2 v30 ME Sony (me6b) (MGS2T_v30me6b) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: v30me5 with one change, for the switch-offs: the Media Engine is started the way Sony's own system starts it.
The game's code, the indoor route, the sound mix, the resting Media Engine, the crash catcher and battery.txt are v30me5's.
On the emulator every state hash and every sound checkpoint matched the PC's. The emulator cannot run the Media Engine, so
the new start is first tried on your PSP.
- What the study found (mgs2/me_startup_study.md in the project): the PSP's system keeps 64 bytes of start code at 0xBFC00000.
  The main CPU runs it on an NMI (an emergency interrupt); the Media Engine runs it when it starts and is sent on to
  0xBFC00040, where Sony's driver puts the Media Engine's own start code. Our builds so far copied our start code over
  0xBFC00000 itself, so an NMI on the main CPU would have run our Media Engine code there instead of the system's. I expect
  that would stop the PSP with no crash record, like the switch-offs (inferred, not seen). Nothing shows that NMIs happen in
  a normal run; this build tests the idea and logs the NMI word.
- The change: our start code now goes to 0xBFC00040, as Sony's and DaedalusX64's do, and the system's code at 0xBFC00000 is
  left alone. Before the copy, the main CPU reads the 16 words at 0xBFC00000 and compares the 7 that run with Sony's (as
  uOFW gives them); if any differs, the Media Engine is not started and the run goes on as with the Media Engine off
  (log.txt says why). The Media Engine's reset bit is now changed with interrupts held off for that moment, as Sony's
  driver does it.
- Also Sony's own start steps, made by the Media Engine: the bus word 0xBC100050 = 7 (ours was 6), and the reset and set-up
  of its vector unit (VME) with the four writes Sony's start code makes (0xBCC00010, 0xBCC00070, 0xBCC00030, 0xBCC00040).
  The wait for that reset is limited; log.txt shows how it went.
- New in log.txt: a "me start:" line with the 16 words found at 0xBFC00000 and the NMI word 0xBC100000 as the Media Engine
  found it, and how its vector unit's reset went. battery.txt adds to every line, while the Media Engine runs, the main
  CPU's NMI word, the reset register and how many of the 7 start words differ (0 = the system's code is still there).
- Expected time per step: v30me5's (37.58 ms indoors in its run 2).

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_v30me6b from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_v30me6b/ (EBOOT.PBP, the eight
   ovl_*.bin files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). mgs2disc.pak and
   SLES_503.83 are v30's, copied on your PC.
3. Start "MGS2 v30 ME Sony (me6b)" from the Game menu. The crash catcher's self-test is offered as before; press O (or wait 15
   seconds) to start the run.
4. Please run it on the cheap battery, charged, without the USB cable, to the end (about 38 minutes). Up to 3 runs: if
   the PSP switches itself off, stop there, as one stop rules this change out. Do not put the PSP to sleep. HOME quits at
   any time.
5. If a crash happens, the red text stays on the screen: please photograph it. If the PSP switches itself off, please say
   whether you found it off or frozen.
6. At the end it shows "Press X to exit". After each run, send back log.txt, battery.txt and crash.txt, plus steps.csv and
   me_steps.csv (the next run writes over them).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or
clock setting is changed (333 MHz as always). Register writes: v30me5's, plus Sony's start steps above (0xBC100050 = 7 and
the four vector unit writes, made by the Media Engine), which you agreed to on the card. Sleep is held off while the
Media Engine runs, and the Media Engine is put back in reset at the end and before any exit.
