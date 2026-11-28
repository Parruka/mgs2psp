---
name: ff8-autotest
description: FF8 test replays ("autoplayer", Ffm 2026-11-19): emulator-only work OKed 13:52, PSPLink not now; dev build t1 plays 13 saved stretches in one start, all exact; lessons (card, pad shift)
metadata:
  type: project
---

Ffm 2026-11-19 13:47 asked for an autoplayer with PSPLink to speed up build testing. Answer: yes, as a REPLAY of recorded sessions (padsim, deterministic, state hashes prove exactness); it cannot explore new places itself.
- 13:50: "dont start any run in the psplink at the moment" (card closed "Not now"). So: NO FF8 PSPLink use, no PSP runs, no FF8TEST on the stick until Ffm says so ([[psp-link]] stays MGS2-only).
- 13:52: "if you want you can start with the pad script and test it in the emu". Emulator-only work is OK.
- 14:04, Ffm: fights are RANDOM ENCOUNTERS (world map, and most at the start of the game). So stretches start from points saved in the exact replay of a real session, and the same fights come.

DONE 15:50 (all [C emulator]; full write-up /mnt/project-files/ff8/test_replays.md; sources ff8/src_t1/; stage5_notes entry "13:52-15:50"):
- Dev build t1 = p10 code and placement + section .text.t1 (p10's slots are full). Build folder psp/vt1, 3 LOAD segments.
- cfg keys:
  - "stsavex F,file": up to 7 more saved points in one run.
  - "seg file,F[,shift]": up to 32 stretches in one start, via rt_seg_run from rt_end_hook.
  - "blkframe N": a per-block-end trace of frame N.
- 13 stretches of 600 frames from the p8 session (2400 22200 24000 30000 44400 45600 48000 54000 57600 59400 62400 64200 74400). Each matches a window the PSP timed in p8.
- Results:
  - every load is exact (hash 600 frames later = the full session's);
  - chained = single starts;
  - hashref check "78 same, 0 differ", 13 end lines same;
  - 1 to 4 min in the emulator.
- PSP estimate: about 7 min [I], against 38.2 min for the whole p8 replay.
- The points are in the emulator folders FF8XA/FF8XB (container only, game data). Remake them with one emulator run (about 6 min) if the saved-state size changes.
- The old "7 cycles off" finding is SETTLED: it was the missing pad shift of file movies (p7: 12,256), not the load.
- LESSONS:
  - "card 1" writes card1.mcd back when the game saves. Copy the right card in again before each run, or use card 0 (tests use card 0).
  - Points must be made with the session's card (p8: 4691e69b put in at 2101).
  - A point made after file movies needs its pad shift.
- NEXT if Ffm wants it: put the t1 code into p11, then the FF8TEST folder layout from test_replays.md (needs Ffm's word for the PSP / PSPLink).
