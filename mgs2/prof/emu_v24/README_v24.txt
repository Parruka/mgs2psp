MGS2 timing harness v24 - personal use only, do not share (it contains code built from your MGS2 disc).

Two folders this time, with the same program in both:
- MGS2T_v24:  the Tanker deck, as in v23.
- MGS2T_v24w: the crew deck (the next Tanker area, stage w01a), last measured in v18.

1. Copy both folders from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_v24/ and ms0:/PSP/GAME/MGS2T_v24w/
   (each holds EBOOT.PBP, the eight ovl_*.bin files, SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and
   the memcard0 folder).
2. Start "MGS2 timing harness v24" (the deck) from the Game menu and let it run to the end. Then start
   "MGS2 timing harness v24 crew deck". Do not put the PSP to sleep during a run. Each run plays the opening on a
   script (25200 vblanks) as v23 did; the CPU runs at 333 MHz (the official power call).
3. At the end it shows "Press X to exit" and waits. HOME quits at any time.
4. Send back log.txt and steps.csv from each folder. If a file split.<number> was written, keep it on your PC
   (32 MB, a copy of the game's RAM: personal use only) and tell me it is there.

New since v23. The game code is the same; this build collects the facts the audit asked for:
- The log's first line is now the build's own title (v23's still said v22).
- Memory: at the start and at the end the log adds the C library's own count of the heap (in use, free, and in
  how many pieces) and, at the end, every block that can still be allocated. v23's largest free block read 4096 KB
  at the start and 2960 KB at the end; this tells a leak from fragmentation, and whether 4096 was a limit of the
  old reading.
- Disc reads: steps.csv has three new columns (pack reads, KB and milliseconds in each step) and the log a line for
  every 100 vblanks that had reads. The deck has a dozen slow steps (20-51 ms of IOP and sound) at the same
  vblanks in every run since v19; this shows whether they are memory stick reads.
- The game state is checked every 100 vblanks on the deck, the last time at the stop (v23: 3 checks on the deck).
- Every speedup is on all the time (no more taking turns between settings), so the deck should come close to
  v23's "all speedups on" time of 70.0 ms per step.
- A new code order for this build, because the new log code moved the functions after it.
