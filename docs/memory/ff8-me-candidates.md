---
name: ff8-me-candidates
description: FF8: what the Media Engine could take (measured 2026-11-16 from the speed model): nothing clean in fields today, 6.9 ms of 42 in mixed movies (decode ahead), the audible sound mix later; open risk = sharing the ME with Sony's JPEG library; no ME work now
metadata:
  type: project
  modified: 2026-11-16T23:24:09.743Z
---

Asked by Ffm 2026-11-16 22:52 / 22:54 ("why are we using all cpu and nothin in the GE?" "GE or ME"). Notes: ff8/stage5_notes.md "2026-11-16 23:30". Earlier: the general ME card was withdrawn 2026-11-12 ([[ff8-stage5]]); ME through Sony's libraries is OKed only for pure movies ([[ff8-movies]]).

**Figures (speed model on m6's file, ms a frame [C model]):**
- Quiet field 20.56 / busy 24.97: runtime 5.2 / 4.2; the PS1 GPU's command handling and timing model 4.8 / 8.6 (tri_time, exec_cmd, tri_now, draw_tri, rect_cost_fast); game code 4.1 / 3.4; renderer CPU side ~1.3 / ~2.1; SPU 0.44 / 0.72 (lazy). All in step with the game's logic: no clean ME job today.
- Mixed-movie window 41.94 (PSP 46.1 [C, s9]): pure picture work of the exact decoder 6.9 (idct_sym2 3.84, decode 2.28, rl_block 0.75); its timing model stays (mt_step_x 3.17, mdec_dma_out_run 1.26); sound on the full path 2.8.
- GE: it already draws every picture; renderer CPU work 1.9 / 3.3 ms, GE waits < 0.1 [C, s8].

**Candidates:** (1) the audible sound mix when sound for the whole game is built (cost on the main CPU UNKNOWN: measure with a dev build with the full mix on in field windows); (2) decoding mixed movies AHEAD on the ME (disc data known in advance, result used only when the game's input matches = exact): up to -6.9 ms in the 36 mixed movies; (3) the GPU timing model (up to 3.4 ms busy): needs a join before every use, feasibility UNKNOWN.

**Open risk:** the file movies use Sony's JPEG library on the ME under Sony's firmware [I]; MGS2's own ME code replaces that firmware (kernel call via ARK-4's KUBridge + register writes OKed for MGS2 only: [[mgs2-consents]], [[mgs2-me-audio]]). Whether our code and Sony's library can take turns in one run is UNKNOWN; only a PSP probe can say.

**How to apply:** no ME work now (told Ffm 2026-11-16 23:32). Bring the ME decision card when the sound stage starts, with the mix cost measured and the sharing risk explained; any ME code of ours, kernel module or register write needs Ffm's OK first, per port.
