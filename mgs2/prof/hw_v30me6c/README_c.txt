MGS2 v30 ME Sony (me6c) (MGS2T_v30me6c) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: v30me6b with one change, the fix for the stop that runs 2 and 3 showed. The game's code, the indoor route, the
sound mix, the resting Media Engine, Sony's start, the crash catcher and battery.txt are v30me6b's.
- What happened in runs 2 and 3: to wake the resting Media Engine, the main CPU runs a tiny function of ours in kernel mode
  (through ARK-4's kernel call) that writes the wake register. That function ran at its normal EBOOT address (0x08b4ed08).
  The PSP's interrupt manager treats "kernel mode at a user address" as impossible: if an interrupt arrives at that moment,
  it stops with a break instruction (uOFW interruptman, the first check of its interrupt entry; ARK-4 patches the same kind
  of stop in the system-call path, not this one). Both crash records match that check register for register. It happened
  about once in 100,000 wakes, so about every second run.
- The change: all our kernel-mode functions (the wake, the Media Engine's start and stop, and the reads for battery.txt)
  now run at their kernel address, the same memory seen through the kernel's view of it (0x88b4ed08 for the wake), which
  is how the crash catcher's handler already runs. An interrupt there is handled as usual. No new register write.
- On the emulator every state hash and every sound checkpoint matched the PC's, as for v30me6b. The emulator cannot run the
  Media Engine or ARK-4's kernel call, so the fix itself is first tried on your PSP.
- Expected time per step: v30me6b's (37.58 ms indoors in its run 1).

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_v30me6c from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_v30me6c/ (EBOOT.PBP, the eight
   ovl_*.bin files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). Only EBOOT.PBP,
   mgs2.cfg and this README are new; the other files are v30me6b's, copied on your PC.
3. Start "MGS2 v30 ME Sony (me6c)" from the Game menu. The crash catcher's self-test is offered as before; press O (or wait 15
   seconds) to start the run.
4. Please run it on the cheap battery, charged, without the USB cable, to the end (about 38 minutes). Up to 3 runs, as for
   v30me6b: if the PSP switches itself off, stop there. Do not put the PSP to sleep. HOME quits at any time.
5. If a crash happens, the red text stays on the screen: please photograph it. If the PSP switches itself off, please say
   whether you found it off or frozen.
6. At the end it shows "Press X to exit". After each run, send back log.txt, battery.txt and crash.txt, plus steps.csv and
   me_steps.csv (the next run writes over them).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or
clock setting is changed (333 MHz as always). Register writes: v30me6b's (v30me5's, plus Sony's start steps you agreed to
on the card). Sleep is held off while the Media Engine runs, and the Media Engine is put back in reset at the end and before
any exit.
