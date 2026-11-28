---
name: mgs2-builds-gl20-gl25
description: MGS2 PSP builds gl20-gl25 (2026-11-19): ME start fix, ME timers, GE2, copy-arena lock-step, two-area turns; key measured numbers and lessons
metadata:
  type: project
  modified: 2026-11-20T16:22:05.412Z
---

Full entries: mgs2/psp_harness.md ("Build glNN"). All runs short (to vb 31,400), all EXACT on the PSP (372 hashes; shots = previous build).
- gl20 (read 2026-11-19 11:50): ME never started (timer clock = syscall, [[mgs2-me-code-rules]]); main turns -14..-21% vs gl19.
- gl21 (11:56-12:32, first psprun.py launch): THE ME RAN. Deck ms a step: ME turns 60.6 (gl19 72.3), main 66.0; rain 116.9 / 135.3; ME +16-25% slower than main before the deck. ME timers per deck step: walk 34.5, GE waits 16.1 (rain 41.5), main waits arena 19.9. Printed ME idle includes the main turn before: use turn minus the rest. ~14-30% of PSP ME-turn lines carry stale counters: skip them.
- gl21r ring (several frames share the arena): NOT exact (late walks read RAM changed by the next step), code in mgs2/main/attic/ring_gl21r/.
- gl22 (13:01-13:37): GEFAST in half the ME turns: deck ME -1.1%, rain -2.3%. GE draws a frame in 22-27 ms (not the wall); main CPU notices 5-10 ms late (rain 55); early signal turns ME GE-wait into idle.
- gl23 (14:08-14:45, headroom 2,800 KB): two frames on the GE side (GE2) work but gain only -2.5% (49.8 vs 51.1): the LOCK-STEP IS THE COPY ARENA (main CPU's first copy plan waits for the ME's whole walk of the step before). GE2 kept on.
- gl24 (16:04-16:40): counters fixed, NO GAIN: the game draws a picture in TWO steps (drawing step ends at H1 posting the walk ~32 ms; logic-only step ends at H2 ~20 ms); the walk takes ~67 ms a picture.
- gl25 (17:34-18:10, build_gl25d, DEVIATIONS C7): two-area turns (no wait at a hook without records): steady deck 50.1 -> 42.3 ms a step (-15.6%), rain 119.3 -> 92.1 (-22.8%) [C]. Deck: ME is the limit (walk 35.0/step); rain: main CPU is the limit.
- Tools: mesec_chk takes gxl2 as optional; psp.sh stops on a mesec_chk FAIL. Emulator shot_24783 varies run to run: judge by PSP shots. hashref for cut b in every turn from vb 24,000: main/cfg/hashref_g22b.txt 72e0ca4c (twin build_gl19/x86 out_g22b).
- Later builds: [[mgs2-builds-gl26-gl28]].
