---
name: ff8-harness-layout
description: FF8 placed-layout limits for harness code (main_ff8.c): fixed .text slot, .text.mv room before the anchor, linker script ff8_v6.x, dead build folders, structure check before delivery
metadata:
  type: project
  modified: 2026-11-16T23:11:50.809Z
---

Applies to every FF8 release build made with the placed layout (place_v2.txt + layout/link.sh); see [[ff8-layout]], [[ff8-m4]], [[ff8-flip]].

- **main_ff8.o(.text) has a fixed 20,416-byte slot** in the placed layout. m6 uses 19,528. More code than that = link error.
- **Extra harness helpers go to section .text.mv** (macros FLSEC / MVSEC in main_ff8.c). Its room ends at the anchor 0x08be80d8: m6's .text.mv is 15,128 bytes and the room is nearly gone (vt overflowed by 241 bytes; fixed by moving two log helpers back to .text). Overflow shows as "cannot move location counter backwards" at the .pad_a line. Next growth needs a new layout step (move the anchor = re-baseline the model) or trimming.
- **Linker script ff8_v6.x** (use from now on; copy from psp/vs or psp/vu): v5's anchors by padding inside sections (3 LOAD segments; v4's address jumps made 6 and crashed the PSP) + ge_ff8.o's .bss pinned at 0x09784000 so harness .bss growth moves nothing.
- **A changed source needs a NEW build folder**; never run make in a layout folder after its first build, link only with layout/link.sh. Dead folders: vp, vq, vr, vt. Live: vs = m5, vu = m6.
- **Before any delivery:** extract DATA.PSP from EBOOT.PBP and compare `psp-readelf -l / -h / -S` with a build that ran on the PSP (3 LOAD segments, same kinds and order), check data symbols kept their addresses (psp-nm against the last release), g_emuprof at 0x09778ac0, _end at 0x097b73d4.
- **p1 (2026-11-17): link script ff8_v7.x** (psp/vw, shared src_play_p1) = v6 + `*(.ptext)` right after the catch-all text line + the pad_a / rodata / pad_d anchors moved +0x4000 (.bss line unchanged). New harness code goes to section .ptext (macro PLSEC): 5,868 bytes used, ~10.9 KB left before the anchor 0x08bec0d8. Against m7: 3 LOAD, same shape; data symbols +0x4000 (same cache sets), text unmoved; model equal. g_emuprof now 0x0977cac0, _end 0x097bb3d4. main .text 20,124 of 20,416; .text.mv still full. psp/vw is rebuilt with src_play_p1/psp/build.sh until frozen (rel_p1 is frozen).
