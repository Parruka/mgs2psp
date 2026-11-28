---
name: mgs2-builds-gl26-gl28
description: MGS2 PSP builds gl26-gl28 (2026-11-19/20): TWO + cut b, flip race fix, ME HALT rest + battery recorder, marks diagnostics; results and where the files are
metadata:
  type: project
  modified: 2026-11-20T16:22:13.707Z
---

Full entries: mgs2/psp_harness.md. Earlier: [[mgs2-builds-gl20-gl25]]. Marks story: [[mgs2-deck-marks]]. Power: [[mgs2-switch-offs]].
- gl26 (build_gl26, EBOOT 433896f3): TWO=1 in every ME turn + cut b (G1, ON in DEVIATIONS) in every block from vb 24,000 + ME walk split timers. RAN 2026-11-19 19:17-19:53: EXACT (372 = 72e0ca4c); deck 44.9 ms a step, rain 87-89. Split timers cost ~6 us a callback on the ME -> off from gl27. Ffm 19:52 first saw the black flickering marks.
- gl27 (EBOOT d6504990): fixed a REAL flip race (gl_show flips at the next vblank but the next list cleared the shown buffer; MGS2_GL_FLIPWAIT 2 from vb 30,500; 'gl27 display' lines). Run 1 21:15 on battery switched off at vb ~16,600 in codec call 1 (power signature [I]). Run 2 21:51-22:27 ran to the end (battery 90->82%), EXACT, speed = gl26 (+8-14% at the title: flip wait). Marks still there -> not the flip race.
- gl28 (tree /home/claude/t28, build_gl28, EBOOT ee11e43f, cfg 144c3244; gl28b = power only; sources in mgs2/main + attic/gl28/): ME HALT rest in main turns (MGS2_ME_REST), dense hash off, battery-line recorder (MGS2_BAT_REC) + marks diagnostics (EYE/R marker, GL_LIVE, FBCHK, GE2_AB, SHOT_FLIPWAIT, GL_DIG). Delivered 2026-11-20 12:35 (Builds\MGS2T_gl28, LOGS\gl28), stick updated gl27 -> gl28. RUN 1 13:18-13:52 on battery (100->93%): EXACT (62 hashes, 201 digests = twin), ME rest 157/157 clean, main turns -1..-4%. Reader attic/gl28/rd28.py; container copy runs/gl28 (rd28.out). MGS2T_gl28 still on the stick (delete when gl29 replaces it, logs already fetched).
- Mainline mirrored: mgs2/main = t28 sources (README "gl21-gl28" section).
