---
name: mgs2-ge-hardware
description: What the PSP's GE costs for MGS2 and the drawing rules measured on it (MGS2GE v1-v6, VRAM budget, 16-bit depth, picture metric)
metadata:
  type: project
  modified: 2026-10-27T20:19:17.079Z
---

Measured on Ffm's PSP-3000 with the GE replay builds MGS2GE v1-v6 (mgs2/renderer_notes.md §9b-9c, §11.8-11.11; logs mgs2/prof/hw_ge_v*). Code work: [[mgs2-renderer-work]].

- Deck frame: 38.93 ms on the GE with the CPU idle, 41.8-45.1 ms with the CPU busy, and the CPU then runs at 75-83% speed (they share the RAM bus). Budget for 25 fps is 40 ms, so the frame fits only when the CPU is light.
- Hardware transform costs the GE nothing (11.43 vs 11.71 ms through mode); fog +0.47 ms; effects transformed = through mode.
- Index lists are the cost, not joins: draw arrays in order (16.48 vs 11.92 ms for df0).
- Fastest forms found: depth blur top-left corner first or in 16-row tiles (27.2 -> 2.65 ms), motion trail in 16-pixel columns (7.48 -> 1.36), textures in VRAM. Together the frame went 74.34 -> 38.32 ms with no visual change.
- Picture against the PS2: 2.00 mean levels (tools/dev/piccmp.py, 3x3 nearest-pixel rule). PPSSPP pictures depend on data addresses, so compare same-binary only. Open [I]: on the PSP, v4 and v5 in the same mode differ on 1,275 px on Snake (PPSSPP identical): address or timing dependent.
- 16-bit depth rule: per depth buffer and frame, map the written Z24 range [lo, hi] (widened by (hi-lo)/16) to 0..65535; GU_CLIP_PLANES is the depth clamp.
- VRAM 2 MB: two 8888 480x272 buffers (stride 512) plus 16-bit depth leave 688 KB, which is the hot-texture set with no room for a blur copy or vertex buffers. Converted deck textures about 1.5 MB, the whole stage 3.53 MB.
- Draw pipeline + GS walk on the PS2 side (v23 model, deck) 39.5 ms/step: walk 9.6, packet builders 8.4, sort 6.8, VU0 box cull 5.3, helpers 5.1, culls 3.7.
- pspgu: sceGuScissor also sets the drawing region; sceGuClear in a GU_CALL list has no size.
