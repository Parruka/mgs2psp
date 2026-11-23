# top3 levers study: state (dated blocks, oldest first)

Study of every lever on rows A (characters/game systems 17.8), D (libdg drawing lists 16.7), E (VU0 programs as C 8.7) of
where_78ms_go.md, other than H1. Container only; nothing to the PSP. Work copies: scratchpad/top3 (container, temporary).

## 2026-11-23 (start)
- Read: where_78ms_go.md, gl39_plan.md §2, gl39 state.md + report.txt, memory (lean-generator, speed-work, renderer, native,
  rules, working-facts, e1), DEVIATIONS E1.
- First finding: gl39 dropped M4 for "not exact by the owner's 2026-11-08 rule (everything toward zero)". That rule was superseded
  by the lead's decision of 2026-11-18 (E1 accepted, the VFPU stays): the reason no longer holds as stated. To check: what M4's
  native must equal (today's VFPU-rounded generated form), not the PS2's.

## 2026-11-23 15:20 UTC: tools and first measurements (container scratchpad/top3)
- gen.o and gen_s_w00a.o recompiled with -g (gl39 use build's commands + profiles, in scratchpad/top3/g; text +0.16 / +0.2 % vs u/),
  linked alone (gen_g.elf, w00a_g.elf); tools/pg2.py = prof_guest.py (speed_notes §49) with gl39's cost model, #line mapping of the
  w00a header, per-function guest counts.  Output pg_ka39_class.txt, pg_ka39g_fn_named.txt.
- [C model, ka39.1 = t56 K0 digest-off] generated code 37.11 ms a step = 4.225 M instr (21.1 ms) + 34.6 k I-miss (7.1) + 35.3 k
  D-miss (9.1); ~1.05 M guest instr a step: 4.0 PSP instr a guest instr (v26: 5.9).  Classes: VU0 macro work (macro ops 3.18,
  lqc2/sqc2 3.45, inlined libvu0 leaves 1.30, moves 0.21) = 8.1 ms; call protocol (call 2.18, entry 1.83, return 1.39, exit 0.73)
  = 6.1 ms; guest memory 13.8 ms (of it D-miss ~7.7); loop-poll branches 1.88.
- Row E per call [C model]: box cull 811 instr a call (1,011 calls), VU0 call path vu_run_shared + vuc_entry + vuc_dispatch =
  ~132 instr a call (1,977 calls; dispatch saves 9 GPR + 12 FPR each call), vif0_mscal rehash 7.9 a step (0.24 ms), slerp
  vup_33268338 628 instr a call (459 calls; 107 mul.s 44 add.s, 144 lwc1/swc1 of VF lanes in memory).

## 2026-11-23 15:45 UTC: prototype P1 (VU0 call path), compile-only
- scratchpad/top3/p1: runtime/vu0.h copy (+ guest.h shadow so the copy is used) with vu_run_shared = a 16-entry (hash, entry) ->
  (VFPU form, C form) cache; miss or dirty image -> old path, which fills it; vuc_fns() made from vuc_dispatch's cases by script.
  boot_rt.o compiled with the use build's command + profile: baseline byte-equal to build_gl39/u/boot_rt.o [C].
- Hot path [C static, disassembly]: ~34 instructions a call (VFPU form) / ~27 (C form only), against ~132 today (vu_run_shared 38.6
  + vuc_entry 19.2 + vuc_dispatch 75 [C model per call]).  1,977 calls a step -> about -200 k instr = -1.0 model ms (instruction
  part).  Side effect: programs GCC had inlined into vuc_dispatch stay out of line (vuc_dispatch 0x4b8c -> 0x564 bytes; boot_rt
  text +8.2 KB).  Not run anywhere: exact by construction (same functions, same FPU-mode test, VPF on fallback); proof would be
  the usual hashes.

## 2026-11-23 16:40 UTC: row split, I/D-miss census, prototype P3 (box cull checks), compile-only
- [C model, ka39.1] rows of generated code: A 17.80 = i 9.29 + I 5.29 + D 3.23 (memory 5.85, call protocol 4.06, VU0 3.26,
  branches 2.70, alu 1.90); D 14.61 = i 8.96 + I 1.04 + D 4.61 (memory 6.92, VU0 2.80, branches 2.21, call protocol 1.57).
- I-cache [C model]: generated code touched 14,112 lines (882 KB); 34.5 k misses a step: 8.0 k on lines that miss ~once a step,
  26.4 k on 4.9 k lines that miss more often.  Capacity-bound: shrinking hot text is the lever; placement (M7) gained nothing.
- Census of executed generated code [C model, MGS2T.elf + ovl_w00a.elf]: address checks 114 k a step (srl 28 + beql = 222 k instr,
  1.1 ms instr part); hi-word making (sra 31 + sw) only 41 k instr = 0.2 ms (not a lever); loop polls 45 k a step, 6.7 instr each.
- P3 (scratchpad/top3/p3): gen_vu.h copy with the box cull's 8 per-corner `vcmp.q ES` checks blanked (line numbers kept).  Proof
  sketch: between each corner's check and the out_ check of the same register the register is only multiplied (vmul, z/w masked),
  so a non-finite lane stays non-finite and the out_ check still sends the call to vfail_, which wrote nothing.  Control = the
  unmodified copy at a scratch path (both lose the PGO profile of gen_vu.h, whose checksum includes the file name).
  [C static] vupv_96936c0f_0 882 -> 831 instr (-51, all on the straight path); x 1,011 calls = -0.26 model ms.
- Not run anywhere (no twin, no emulator): P1 and P3 are exact by construction; their proof is the usual hashes plus a forced
  NaN/Inf fallback test (VPC on x86, VFPU in the emulator).

## 2026-11-23 16:55 UTC: deliverable written
- levers_top3.md written: drop-reason review (M2 wrong, M4 stale, M1 mostly holds, M6 holds for layout but prefetch is open),
  ranked exact levers per row + generator-wide, top-25 verdicts, not-exact list, order for gl41 / gl42.
- proto/: vu0_p1.diff + vuc_fns.h (P1), p3_boxcull_checks.py (P3).  tools/: pg2, fnann, calls, census2, dtop, dmix, dh, iline.
- Open for the next builder: twin + emulator hashes of P1 and P3, forced NaN/Inf fallback test, model run of each.
