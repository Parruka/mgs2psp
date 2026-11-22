MGS2 timing harness v13 - personal use only, do not share (it contains code built from your MGS2 disc).

1. The folder MGS2T_v13 on your PC holds everything (EBOOT.PBP, the eight ovl_*.bin files, SLES_503.83,
   mgs2disc.pak, pad.txt, mgs2.cfg and the memcard0 folder). Copy the whole folder to the memory stick as
   ms0:/PSP/GAME/MGS2T_v13/
2. Start "MGS2 timing harness v13" from the Game menu. Do not put the PSP to sleep during the run.
   Same as v12: text only, the hardware probe for about two seconds, then the game boots and plays its
   opening on a script (25200 vblanks), then the crew deck (w01a). The CPU runs at 333 MHz (the official power call).
3. At the end it shows "Press X to exit" and waits. HOME quits at any time.
4. Send back log.txt and steps.csv from the same folder.

New since v12 (speed only, same game state):
- The interrupt check that runs on every loop pass is cheaper: it counts down a 32-bit distance to the next
  due event instead of adding to and comparing a 64-bit clock (exact: same results).
- Two more of the crew deck's VU0 micro programs are compiled instead of interpreted (exact).
- Code order redone for the new code (deck and crew deck).
- Emulator model: crew deck logic 18.7 -> 17.9 ms per step (deck 42.9 -> 41.8). Checked in the emulator: same
  game state as the PC reference of this route (with the vector unit's rounding) at all checks.
