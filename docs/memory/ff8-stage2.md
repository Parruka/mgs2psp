---
name: ff8-stage2
description: FF8 stage 2 recompiler (done to frame 40,000 on 2026-11-10 23:10): where it lives, how to rebuild and check it, design choices, what is open
metadata:
  type: project
  modified: 2026-11-10T23:11:44.936Z
---

Part of [[ff8-port]]; timing it mirrors: [[ff8-timing-calibration]]. Notes: /mnt/project-files/ff8/stage2_notes.md (shared copies of our own code in ff8/recomp/ and ff8/ref/).

- Container: /home/claude/ff8/recomp: gen.py (generator), rt.c/rt.h (runtime + main = psxrc), build.sh (gen + make), check.sh OUT (40,000-frame check: 80 hashes + end line vs hashes_ref_d1_start.txt AND per-frame blocks+cycles vs fb/ref_perframe_d1_start.txt). Feedback in recomp/fb/ (seeds*.txt, snap/ = RAM snapshots, game data, container only); generated C in recomp/gen (game-derived, container only). Kernel images recomp/img/d1200, d2500 (psxref --vram).
- Result 2026-11-10: power-on to frame 40,000 with 0 interpreted blocks, exact at every frame; resume from states3 works (states shared with psxref). x86: 80.4 s vs interpreter 216.2 s for 40,000 frames.
- Design: blocks = interpreter blocks, every block start a label; model 2 costs mirrored (ref/m2cost.h shared); on IRQ/syscall/code overwrite all generated functions return to the dispatcher (unwind), not nested calls (changed from stage2_plan §6). Modules checked by content (code words); hooks in hw.c (bus store, DMA) mark overwritten code.
- Lesson: the 80 hashes once matched while the run drifted for a moment (interpreted stores into the kernel patch not seen). Always compare per-frame blocks+cycles too.
- Feedback loop: run with --seedsout/--snapdir, add files to fb/, rebuild. New scenes (battles, later discs) need pad scripts and new rounds.
- Block-end fast path DONE 2026-11-10 23:25 (x86 86.1 s -> 56.0 s); stage 3 continues in [[ff8-stage3]]. Still open: merge duplicate modules; block duplication (field module 3.1x).
