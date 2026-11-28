---
name: ff8-layout
description: FF8 function placement from a recorded instruction trace (2026-11-12): psp/layout tools (mk.py, lopt.c, split_rt.py, link.sh), same code relinked = model 28.53 quiet / 35.67 heavy (pu1 31.98 / 40.40), exact; part of every PSP build from s4 on
metadata:
  type: project
---

Part of [[ff8-stage5]], after [[ff8-pgo]]. Notes: /mnt/project-files/ff8/stage5_notes.md §1.16 (tools copied to /mnt/project-files/ff8/psp/layout).

- WHY: the PSP's I-cache is 16 KB, 2-way (128 sets); pu1 had 42 k I-misses a frame (7 ms). The runtime's hot lines (76 KB C + 10 KB shared asm) missed 18 times a frame each: functions called in a row whose hot lines share cache sets. A fully associative cache would miss 40-60% less (tool fa.c): that part is placement.
- METHOD: build with FSEC=-ffunction-sections (Makefile) + the shared asm routines one section each (layout/split_rt.py on gena/a_rt.S; all .text.rta.* must stay within 128 KB: 16-bit branches); link with our own script (psp/ld_default.x + placed units, then game objects at region start + span, span a multiple of 8 KB so nothing else changes cache sets). mk.py: units / first / script / place / check; link.sh <dir> [script] (never `make` in a layout folder). Trace: PPSSPP PSPPROF_ITRACE=<file> PSPPROF_ITRACE_ALL=1 PSPPROF_ITRACE_MARK=1 with short "prof A B" windows (harness takes 16). lopt.c: for each unit, most misses first, tries the 128 start lines of the 8 KB way, simulating the whole trace; ~12 min for 20 frames, 150 units, 4 passes.
- ORDER OF A BUILD: sources -> PGO=gen build -> recording run -> PGO=use FSEC build + split a_rt -> default link (map) -> "units", "first", "script", link = trace build -> trace run -> lopt -> "place", "script" with the printed span, link, "units", "check" -> profile run -> full run.
- RESULT on pu1's code (pf2, only the link differs; exact, the same instruction counts): quiet 31.98 -> 28.53, heavy 40.40 -> 35.67; I-misses 42.4 k -> 22.8 k and 43.4 k -> 15.7 k a frame. Eight scenes the tool never saw (BIOS, title, movie, five fields) are 2.2-6.5 ms faster too.
- GOTCHAS: a bare `crtend.o(.text)` pattern makes ld load the file twice (use `*crtend.o`); LTO library objects appear in the map as temporary names (pattern by section only; a plain .text of one cannot be placed); psp-nm's g_emuprof address changes with every link (PSPPROF_FLAG).
- ALSO 2026-11-12 (in s4's sources): ref/spu.c spu_samples(n) walks only the voices that are on (exact: the whole run's WAV byte-equal on x86 via the new --wav of psxrcc); gpu_lazy_set + cfg "at F lazy N" (lazy drawing off/on inside a run, exact on x86 with GPULAZYAT).
- WHERE THE REST IS (pu1 quiet, a frame): 4.78 M instructions = 22 ms even with no cache miss; game code 722 k (loads 198 k), rt_asm_icb 274 k (real PS1 cache fills), draw_tri timing walk 321 k (~500 instructions of set-up a triangle), spu 318 k before the voice fix. 16.7 ms (the PS1's speed) needs the instruction count down by a third as well.
