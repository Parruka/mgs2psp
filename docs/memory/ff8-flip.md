---
name: ff8-flip
description: FF8 tearing fix (cfg "flip"): m5 ran on the PSP 2026-11-16 (swap free in game scenes, a tear left in file movies); m6 = file movies on two real 8888 screens + 4-row first band; VRAM budget, GE times
metadata:
  type: project
  modified: 2026-11-16T23:11:36.494Z
---

Notes: ff8/stage5_notes.md "2026-11-16 22:38" (m5 delivered), "23:00" (m5 ran), then m6. Follows [[ff8-m4]]; SDK traps in [[ff8-pspgu-traps]]; layout limits in [[ff8-harness-layout]].

**Why:** Ffm 21:41 / 21:43: tearing everywhere, less in movies. Cause: one 8888 screen both drawn and shown.

**m5 (EBOOT 3e18ad16, psp/rel_m5) RAN, log read 23:00 (psp/psp_m5):** clean, out = the emulator's, 0 display errors.
- Game scenes (two 5551 screens at VRAM 0 / 0x44000, swap with SETBUF_NEXTFRAME): FREE, within 0.03 ms a frame of m4b [C].
- Title / logos / menus (the 8888 screen, drawn after a vblank wait): +3 to +9.6 ms a frame in this unpaced run; in a paced build that wait is the pacing.
- GE times [C]: 400x272 8888 movie picture 6.03 ms; 320x224 15-bit 1:1 0.91 ms; 640x478 scaled 3.36 ms (inflated by my scissor error).
- Ffm 22:48: "first movie still got a little tear in the top right corner" ([I]: first 34-row band = 0.75 ms, the beam reaches row 0 at ~0.8 ms); 22:50: "why cant the movie just play without this ?"
- **Ffm 2026-11-17 11:39-11:41 (PSP):** the old-drawing stretch 6500-7000 tore (expected); outside it the GAME SCENES ARE TEAR-FREE ("1 yes"); 16-bit against full-colour screens: "coulnt realy tell" = no visible difference. So game scenes keep the two 16-bit screens; drop the `at 6500 flip 0` / `at 7000 flip 1` lines from the next cfg. Still open: file movies and title / menus under m6, flashes at mode changes; LOGS\m6 empty at 11:40.

**m6 (EBOOT dd9e06b6, psp/rel_m6, sources src_trial_m6) RAN, log read 2026-11-17 11:46 (psp/psp_m6):** clean, out = the emulator's, 0 display errors, windows = m5's within 0.1 ms. Two-screen file movies [C]: 0 late, 0 bad, GE 5.99 ms a picture before the swap, mirror saved in 17.3 ms and put back in 5.3 ms once a movie. Scaled picture on the GE 1.80 ms (m5 3.36). **Ffm 11:46 (PSP, m6): file movies, title and menus "all good" = TEARING SOLVED EVERYWHERE; 4 MB video memory not needed (never asked).** Only left: "2nd movie, im not sure if it was a flask or blink at the end" = the one black game picture after a file movie [I]. Next build: [[ff8-m7]].

**m6 design:**
- FILE MOVIES on two real 8888 screens: A = VRAM 0, B = VRAM 0x88000 = the first 544 KB of the renderer's mirror, saved to main memory at the movie's start, put back at its end (the game stands still). Each picture drawn hidden, GE waited for, shown with NEXTFRAME. End: B copied to A, A shown, mirror restored, next game picture clears the whole screen. Fallback if malloc fails: m5's path.
- OTHER 8888 PICTURES (title, menus, exact-route movie pictures): bands of 4, 6, 9, 13, 20, 30, then 34 rows; black only around a whole-pixel picture; list built with GU_SEND before the wait, sent right after the blank starts.

**VRAM is full (2 MB):** screen 0x88000 + mirror 0x100000 + textures 0x78000. 4 MB (sceGeEdramSetSize, PSP-2000/3000, not in PPSSPP 1.20.4) = a hardware setting needing Ffm's OK: NOT asked; told Ffm 23:00 I'd come back to it only if m6 still tears. A scaled screen shape makes every picture filtered, so all go to the 8888 screen.

The speed model leaves the picture hand-over out: flip's cost is only in the PSP's [pic] / [flip] lines. The emulator has no beam: tear-freedom is proven only on the PSP.
