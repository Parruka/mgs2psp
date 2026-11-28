---
name: ff8-pspgu-traps
description: PSP SDK (pspgu / sceDisplay) traps met in the FF8 harness: scissor arguments, per-list-kind scissor switch, latched format change, filtered pictures on 16-bit screens
metadata:
  type: reference
  modified: 2026-11-16T23:11:43.805Z
---

Met while building [[ff8-flip]] (2026-11-16); each cost a run. They hold for any harness built with our pspdev tarball (MGS2 too).

- **sceGuScissor(x, y, WIDTH, HEIGHT)** in this SDK (checked in the library's disassembly: end = start + size - 1; it also sets the drawing region). m5 passed the end row: right pixels, but every band redrew all rows below it.
- **The scissor switch is kept per kind of list** (GU_DIRECT / GU_SEND / GU_CALL each have their own shadow state). In a GU_SEND list sceGuScissor emits NOTHING until sceGuEnable(GU_SCISSOR_TEST) has been called in that kind of list; the last clip of whoever drew before then cuts the picture. Call the enable in every list's state set-up.
- **sceDisplaySetFrameBuf refuses "immediate" with another pixel format** ("must change latched framebuf first"): ask with PSP_DISPLAY_SETBUF_NEXTFRAME, wait for the vblank start, then draw. Check every result, and dump what sceDisplayGetFrameBuf says is shown, not what the code assumes.
- **A filtered (scaled) picture on a 5551 screen loses its in-between shades** (26,040 px of FF8's name menu): scaled pictures need an 8888 screen.
- **Banded drawing of a filtered picture** must use scissor bands over the same whole rectangles; bands made by splitting coordinates change pixels (7,523 px).
- **PPSSPP is lenient** where the PSP is not (it has no beam, and its 1.20.4 has no sceGeEdramSetSize / more than 2 MB VRAM): tearing and video-memory size are only proven on the PSP.
- pspDebugScreen characters are 7 px wide; pspDebugScreenSetColorMode / SetBase are pure setters.
