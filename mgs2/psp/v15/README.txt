MGS2 timing harness v15 - personal use only, do not share (it contains code built from your MGS2 disc).

1. The folder MGS2T_v15 on your PC holds everything (EBOOT.PBP, the eight ovl_*.bin files, SLES_503.83,
   mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder). Copy the whole folder to the memory stick as
   ms0:/PSP/GAME/MGS2T_v15/
2. Start "MGS2 timing harness v15" from the Game menu. Do not put the PSP to sleep during the run.
   Same as v14: text only, the hardware probe for a few seconds, then the game boots and plays its
   opening on a script (25200 vblanks), then the crew deck (w01a). The CPU runs at 333 MHz (the official power call).
3. At the end it shows "Press X to exit" and waits. HOME quits at any time.
4. Send back log.txt and steps.csv from the same folder, and the file split.<number> if one was written
   (32 MB, a copy of the game's RAM: personal use only, it stays on your PC).

New since v14:
- Split finder: from vblank 15700 to 16010 the PSP hashes the game state every vblank and compares it with the PC's
  value (hashref.txt). If they differ it prints "split:" lines in log.txt with the vector unit's registers and saves
  the game RAM once, so the PC can see exactly which value differs first. No separate diagnostic folder needed.
- Three new groups of probe lines: "probe vfpu ovf" (what the vector unit does past the largest float),
  "probe vfpu bits" (whether its register moves keep every bit) and "probe vfpu arith" (multiply / add / subtract
  of NaN, infinity, tiny and huge values). They decide how the next speed-ups must be written.
- New speed profile from the emulator and code order redone for the deck and the crew deck.
- All checks match the PC reference in the emulator on both routes.
