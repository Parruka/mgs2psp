---
name: mgs2-me-code-rules
description: Rules for any code the PSP's Media Engine runs in the MGS2 port (no system calls, Count, pointers, checks), learned from real ME failures
metadata:
  type: project
---

- The ME may NEVER call a system function: an import stub (`jr ra; syscall`) is a syscall exception on the ME. gl20 (2026-11-19) lost its whole ME: XmeCtx.clk still pointed at the main CPU's clock (sceKernelGetSystemTimeLow) at the ME's first idle wait; the take-back worked, the main CPU drew everything, the run stayed exact but measured nothing on the ME. Fix in gl21: XME_CLK (ge/xme.c) picks the clock by the call's wait (x->w_me = xme_me_wait -> clk_me = Count), pointers set once before the start, never switched across CPUs.
- **Why:** PPSSPP 1.20.4 runs the loop as a main-CPU thread, where syscalls are legal: the emulator cannot see ME-only faults. Only the PSP log's "me picture loop (step 4d-4)" start line shows them.
- **How to apply:** every PSP build runs tools/me_call_chk.py (psp.sh stops on FAIL): jal/j/branch graph from the ME-safe objects + ctx's ME-only pointers; FAIL on a stub, syscall or sce* name. Gap: other pointer calls are not followed, so never hand the ME a pointer the main CPU switches. After each PSP run, read the ME start line FIRST (exception none, heartbeat alive) before reading ME timings.
- Count must be started by the ME (`mtc0 $zero,$9`; gl18me hung without it). Count runs ~331,310 ticks a ms [C, probe 2].
- A candidate check: PPSSPP master 83e616ea has `--me-core` (UNKNOWN whether it boots our ME start); see [[mgs2-me-step4]], [[mgs2-pipeline-clock]].
