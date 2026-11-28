---
name: ff8-m7
description: FF8 trial m7 (delivered 2026-11-17 12:11, the one build out): "mvhold 1" removes the black pictures after a file movie, second movie from a disc-made file (mvd), comparison stretch gone; what to read in its log and ask Ffm
metadata:
  type: project
---

Follows [[ff8-flip]] (tearing solved on the PSP with m6, Ffm 2026-11-17 11:46). Notes: ff8/stage5_notes.md "2026-11-17 12:12"; sources ff8/src_trial_m7.

**m7 DELIVERED 2026-11-17 12:11** to `FF8\Builds\FF8T_m7\FF8T`: EBOOT.PBP 26b7c10c, ff8.cfg 23f5fe3a, README 1b7ab448, `mvd\d1_154480.ffv` a9435d50; logs to `FF8\LOGS\m7`. THE ONE BUILD OUT. Container psp/vv -> psp/rel_m7 (never make there).

**Why:** Ffm 11:46 on m6: "2nd movie, im not sure if it was a flask or blink at the end". Found in the emulator [C]: after a file movie the game draws two black 15-bit pictures (8 PS1 frames, 0.13 s at the second movie: frames 8631, 8635; after the intro 3388, 3392) before its own last 16 movie pictures; on the PS1 they come before the movie. That this is what Ffm saw is [I].

**What m7 has:**
- cfg `mvhold 1`: show_frame draws nothing between a file movie's end and the game's first 24-bit picture (16 frames at most), so the file's last picture stays. A visual change of the file route only. Log: "[movie] mvhold: N game pictures ... were not shown" (4 on this route).
- cfg `mvdir mvd`: the second movie from a file made by mkmovie_disc.py from the disc alone ([[ff8-movie-tools]]): pictures byte-equal to m3's file, sound = the movie's own track from the disc (no field background sound). First disc-made file to run on the PSP.
- No `at 6500 flip 0` / `at 7000 flip 1` lines any more.
- Emulator: all release checks equal (80/80, 57/57, 73/73, model 63.98 / 41.94 / 24.99 / 20.57), every hand-over picture after the held ones = m6's.

**m7 RAN ON THE PSP, log read 2026-11-17 12:30 (psp/psp_m7, notes "2026-11-17 12:30"):** clean to the end, out = m6's = the emulator's (5a65d45a), [mvst] / [show] lines = the emulator's, mvhold line "4 game pictures ... not shown", 0 display errors; second movie from the disc-made file: 183 pictures, 0 late, 0 bad, 585,060 of 585,060 samples; timed windows 29.928 ms a frame (m6 29.929), every window within 0.035 ms. Ffm 12:22 mid-run: "9000, everything looks good atm" (after the second movie's end). NO BUILD OUT. **Ffm 12:57: blink "seems good" (gone), second movie's sound right and in step "yes" = m7 PASSED:** mvhold 1 stays, the disc-made file method is confirmed on the PSP.

**Asked Ffm:** blink gone at the second movie's end? that movie's sound right and in step with the picture?

**How to apply:** no new PSP build before m7's log is read. If the blink is still there, the next suspects are the screen-mode change at the hand-over and the jump from the 388x272 Remastered picture to the game's 320x224 pictures. Wait for emulator runs on each run's own run.txt (the wide-check script writes nothing to its log file).
