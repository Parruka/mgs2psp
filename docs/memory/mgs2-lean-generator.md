---
name: mgs2-lean-generator
description: MGS2 leaner code generator (task #32): plan, steps 1-2 in PSP build v28 (run: free play 83.55 ms/step), step 3 in v29 (PSP run: free play 78.25 ms/step, -6.3%; model said -10%), the regcache fixes, next steps
metadata:
  type: project
  modified: 2026-10-25T22:40:00.000Z
---

- Plan: mgs2/recomp/lean_plan.md; measurements speed_notes §50-52; tools mgs2/tools/dev/lean/. Free play (model): generated code 32.7 of 48 ms. Steps 0-6, total [I] 6-10 ms (told Ffm 2026-10-24 13:44).
- STEP 1 (v27 headers = mk_headers.sh RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1; 1b: Ffm chose "Clear them"). Hash refs: deck/w01a prof/*_v27.txt; FREE PLAY prof/*_free_v28.txt (v27 had a quadword-copy bug, RC_SQWIDE).
- STEP 2 (proved widths, recomp/widths_step2.md, wpd_all6). Rule: every 64-bit store keeps its full value (the game reads stale stack). regcache.py RC_CONDCLEAN=1 (+ RC_SQWIDE=1 without widths).
- v28 = steps 1+2 + v26me4's ME layer (psp/v28.sh, v28me.sh). PSP RUN 2026-10-25 (prof/hw_v28): exact, free play 83.55 ms/step vs v26me4 88.97 (-6.1%); model walk gain 5.33 vs PSP 5.53.
- STEP 3 (2026-10-25): tools/mm_base.py on finished headers (mk_headers.sh MM_BASE=1): loads/stores through reg+k -> RD32B(b,k) etc. (gen_pre.h base forms; exact while b and b+k share the segment and 32 MB window, x86 -DGEN_MB_CHECK), lq/sq halves, GLDU/GSDU inline (GLDUB/GSDUB), VLQC2/VSQC2 with k%16==0. Plus -DGEN_HB_SP: stack host base via gen_hb asm (GCC can't reassociate) so a stack access is one lw/sw. Old headers + no flag = old objects byte for byte. x86: all hashes equal on 3 routes, 0 invariant breaks (lean_plan.md step 3).
- v29 = v28 + step 3 (psp/v29.sh, v29me.sh; psp_harness.md 'Build v29'). Emulator exact. Model 69.01 vs 76.70 (-10.0%), told Ffm 'about 75'. PSP RUN (files 2026-10-25 22:11, prof/hw_v29): exact (431/431, vuh 311/311, mix 73/73), free play 78.25 ms/step (v28 83.55, -6.3%) = logic 42.40 / draw 28.34 / IOP 2.60 / GS 4.90. PSP got ~0.67 of the model gain (speed_notes §55; model now 10.4% low): ALU-removal changes get ~0.7 of model gain [I].
- PGO SIZE EFFECT (task #40): profiled units grew (w00a +14%) while plain shrank 7%: the profile's sum_max fell 428M -> 23M (helpers inline), so GCC's hot threshold (sum_max/10000) fell 42.8k -> 2.3k execs. Candidate: per-unit hot-bb-count-fraction.
- R ITEMS FAIR TEST (lean/rv/fair29.sh, re-profile + use build on v29 d, model1 walk window): v29d 68.43 ms; R1 69.96 = -217 k instr but +11.1 k I-misses in draw code that moved (io_flush shrank, io_dma_start new, v29's order kept). Needs a fresh code order (or same-address padding) to judge; R2 was running 22:45.
- Next: step 4 VU0 in VFPU across groups, step 5 hot libvu0 leaves inlined, step 6 register args. Biggest runtime function: io_flush 6.7 ms/step model (DMA/VIF).
- Not taken: DSR (stale stack reads), hot/cold split, -Os, VU0 regs as C locals, $gp base, trapped I/O.
