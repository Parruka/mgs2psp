---
name: ff8-u1-art
description: FF8 U1 history 2026-11-11: shape mock-ups A-D, room width study (189 rooms wide enough, 705 need new art), room decoder mimdec.py, the image-model new-art trial (a late optional step), approved hosts
metadata:
  type: project
---

Split from [[ff8-u1]] (the current screen-shape state is there). Notes: /mnt/project-files/ff8/stage5_notes.md §2.

- Shape mock-ups DELIVERED 20:18 to PC FF8\SHOTS\u1_shapes (28 PNG + README; container s5/u1_shapes): A side bars (363 wide, 4:3) / B stretch / C zoom and crop / D half zoom. CARD OPEN since 20:20 (cmsg_01Tzc8YgQrnPXLpp31PGjWVPKusGE4WPxy9Z8xUZYA5f2H): shape, recommended A. Ffm's earlier wish: full 480x272, no side bars if possible.
- Room study (Remastered PC data, s5/field_extents.txt): the game culls background tiles to 320 px; at shape A a 480 px screen shows 424 PS1 px. Of 894 rooms 189 are >= 424 wide (fill the screen from the game's own data), 197 are 321-423, 508 <= 320: 705 need new art. Ffm was told the corrected count 21:15.
- Room decoder tools/mimdec.py (shared; container s5/mimdec.py; `list | room <name> <png> | match <shot>`): type 1 .mim/.map layout in notes §2.4; rooms with text in the background use <room>_en.mim/.map. VERIFIED: room bgroad_6 (320x512, infirmary walkway) at scroll row 98 = exact shot 35,201 in 98.91% of pixels, the rest are the two people. Not verified: type 2 rooms (4), blended and animated tiles.
- Ffm 20:32 (marked-up shape A): "can we reimagine and recreate the background?" TRIAL DELIVERED 21:14 to PC FF8\SHOTS\u1_newart (shape A + 3 seeds of the same frame with the bars filled + sheet + README fdafe11f; container s5/u1_newart): Stable Diffusion 1.5 inpainting on CPU in /home/claude/ai (venv 5.7 GB, outpaint.py / outpaint_room.py; HF_HUB_DISABLE_XET=1; ~9 min a picture). Result: believable, some faults (doubled lamp, wide pillar, softer strips).
- Whole-room picture delivered 21:25 (bgroad_6_*.png, README a34aed21). Ffm 21:22 "nice, this is just an experiment to see"; 21:25 asked if the art can be done at the end when the game is almost done: answered YES. STATUS: new art = a late, optional step on top of shape A (needs wide-background support in the renderer, which the 189 wide rooms need anyway); no more time on it until Ffm asks. /home/claude/ai + /root/.cache/huggingface (~8 GB) may be deleted when disk is needed.
- Cost told: ~1,500-2,000 pictures [I], 10-14 days CPU, ~20-30 MB [I].
- Network hosts Ffm approved for this (environment Default): the huggingface.co / hf.co hosts (cdn-lfs, cas-bridge.xethub, us.aws.cdn) and download.pytorch.org. Refused, never ask again: download-r2.pytorch.org.
- UNKNOWN: whether the game draws people past the old screen edge (test: widen the renderer's drawing area in the emulator); animated tiles at the edges.
