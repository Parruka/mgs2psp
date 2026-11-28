---
name: ff8-s5
description: FF8 s5 candidate (2026-11-12 08:30): READY in psp/rel_s5 (EBOOT b3028411), exact in the emulator, model 25.72 quiet / 30.65 busy ms a frame (s4 27.85 / 35.07); NOT delivered (s3's log, then s4 first); sources in /mnt/project-files/ff8/src_s5; the work after it: ff8-s6
metadata:
  type: project
  modified: 2026-11-12T08:32:32.234Z
---

Part of [[ff8-port]] / [[ff8-stage5]]; follows [[ff8-s4]] (s4 candidate psp/rel_s4, untouched; the shared folder's ref/ recomp/ psp/ sources are still s4's). Notes: /mnt/project-files/ff8/stage5_notes.md §1.18.

- FINDING: of s4's 4,630 k / 6,451 k instructions a frame (quiet / busy) the game code is only ~720-780 k; the rest is machinery (per drawing command ~2,500 instructions, I-cache check 274 k, DMA walk, sound 161/265 k, GTE 300/480 k).
- s5 CONTENT (each with its own x86 old-vs-new check, all 0 differ: HPCHECK, GTCHECK, RECTCHECK, TRICHECK, TTCHECK, GTECHECK): host_prim id instead of memcmp, gt_cmd 32-bit and inlined, rectangle timing rows unrolled + texture-window groups, tri_time (lean triangle timing), DMA empty-node runs and table clear, rtp_fast, mvmva_fast, hot GTE command entry, GE da_hits memo by state id, set_drawmode early return, rt.c gte_rd_inl / gte_wr_inl for all GTE data registers (only the PSP build runs them: the emulator's hashes are their check).
- CANDIDATE: container psp/rel_s5 = EBOOT.PBP b3028411219b9b9cdec1da1555fc0d51 (9,229,376 bytes, title "FF8 speed s5"), ff8.cfg 34b0b74f (= s4's), FF8T.elf, map, ff8_v3.x, place_v2.txt. Build folders s5g (PGO=gen) -> r5 (PGO=use + FSEC, trace_r5, lopt) -> r7 (ge_ff8.o recompiled) -> r8 (dbss.py 34 94 59 0). Model 25.72 / 30.65 ms (instr 4,152 k / 5,476 k, I-miss 23.6 / 16.8 k, D-miss 10.08 / 9.95 k). Checks: prof_r8 73/73, out.txt 8d5816bd, [vsum]/[blk]/[ge] = q7's; full run emu_r8r with the release cfg 80/80, end line, out.txt aae53db0, 12 shots + 2 gedumps byte-equal to emu_q7r's. README not written (after s4's PSP run).
- SOURCES that differ from s4's: /mnt/project-files/ff8/src_s5/ (ref/gpu.c, hw.c, gte.c, gpu_host.h, recomp/rt.c, psp/ge_ff8.c 80ae1393, link script, placement); container copies *.s5a and psp/ge_ff8.c.s5b.
- DEFERRED: tri_time's set-up (199 k / 575 k a frame, 11 divisions a triangle) until s4's PSP log gives the [probe] division figures.
- DATA-CACHE TRAP: an array of a multiple of 8 KB puts its two neighbours in one cache set; the dh_* memo landed behind ge_ff8.c's 128 KB snap array = 3 busy lines in one set (+1.5 k D-misses a frame). Fixed by defining the memo on tex_epoch's source line (GCC lays .bss out in reverse definition order; keep the line count so the PGO profile still matches). Scratchpad dsym.py <elf> <dhist> <dacc> names the lines that miss most.
- METHOD: dev pair for measuring (PGO=gen -> record in FF8QA -> PGO=use -> profile in FF8QB -> scratchpad pcmp.py per-function difference; needs PSPDEV=/home/claude/pspdev + its bin on PATH; never edit sources between the gen and the use build). Full pipeline: [[ff8-layout]]; since probe.o is in OBJS: always `EXTRA=probe.o sh layout/link.sh`, add `probe.o(.rodata .rodata.*)` behind `__lay1 = .;` in each script, give lopt __lay1 as the region's end, start lopt plainly (sh has no `time`).
- Lesson: a fast path inlined into a big function pays the big function's entry and stack traffic; keep references and fallbacks `noinline`.
- s6 work (steps 4-8, the game-code findings, what comes next): [[ff8-s6]].
- NEXT: s3's PSP log -> deliver s4 -> its log (read [probe]) -> then s5 or its successor. One build out at a time.
