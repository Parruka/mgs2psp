MGS2 timing harness v14 - personal use only, do not share (it contains code built from your MGS2 disc).

1. The folder MGS2T_v14 on your PC holds everything (EBOOT.PBP, the eight ovl_*.bin files, SLES_503.83,
   mgs2disc.pak, pad.txt, mgs2.cfg and the memcard0 folder). Copy the whole folder to the memory stick as
   ms0:/PSP/GAME/MGS2T_v14/
2. Start "MGS2 timing harness v14" from the Game menu. Do not put the PSP to sleep during the run.
   Same as v13: text only, the hardware probe for about two seconds, then the game boots and plays its
   opening on a script (25200 vblanks), then the crew deck (w01a). The CPU runs at 333 MHz (the official power call).
3. At the end it shows "Press X to exit" and waits. HOME quits at any time.
4. Send back log.txt and steps.csv from the same folder.

New since v13:
- Fix: the check after each run of vector ops now reads its result through a VFPU branch. In v12 and v13 the
  PSP sometimes read it too early, so some overflow / NaN results were kept instead of redone exactly, and the
  game state left the PC reference at vb 16000. With the fix all hash lines should match, including the
  "vfb" count at the end of each line.
- New probe lines "probe vfpu cc" and "probe vfpu data": they measure that early read directly (how often each
  way of reading gives a stale answer), so we know the cause for sure.
- The compiled vector programs keep their registers in memory instead of copying them all to the stack:
  same results, fewer instructions (the top one, a rotation blend, 1.52 -> 1.02 ms per step).
- Code order redone. Emulator model: crew deck 17.9 -> 17.3 ms per step (deck 41.8 -> 40.8); all checks match
  the PC reference in the emulator.
