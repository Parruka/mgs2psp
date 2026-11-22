# A leaner code generator: plan (2026-10-24)

Goal: less PSP code and fewer PSP instructions per PS2 instruction in the generated game code, with the game state unchanged. Every
step keeps the state hashes identical on x86, the emulator and the PSP (the VFPU's last-bit rounding stays the only consented
difference). Each step is one switch (an `RC_*` option of regcache.py, a pass, or a gen_pre.h define), so it is measured alone.
**[I]** marks an estimate; everything else is measured.

## 1. Where the generated code's time goes (measured)

Source: the free-play walk window of speed_notes §46 (vb 31,501-31,600, logic only, v26 build, emulator PspProf; model ms =
(1.4 x instructions + 70 x misses) / 333,000, which reads 6-9% below the PSP). Tools `tools/dev/lean/` (call_parts.py: PSP
instruction kinds on call / entry / exit / return lines; fill.py: each touched cache line's misses spread over its 16 slots;
parts2-4.py: callees, widths, check reuse); outputs `prof/lean_calls_free_v26.txt`, `prof/lean_fill_free_v26.txt`,
`prof/lean_funcs_free_v26.txt`.

The generated code is 32.7 of the 48.0 ms model logic step: about 4.1 M PSP instructions and 50.8 k I-misses per step (77% of
all I-misses, 10.7 ms). Its code lines are already 94% used (15 of 16 slots per touched line): the misses come from the amount of
code, not from its layout.

| Part | PSP instructions per step | Share of the generated code's I-misses | What it is made of |
|---|---|---|---|
| Call sites | 448 k: 33.0 per call, 13.6 k calls | 14.0% | R[] stores 12.0 per call (two words per register; per call 1.95 s-registers, 1.8 arguments, the return address, 0.7 of v0 / v1, 0.5 sp, 0.25 at), GCC's stack 5.2, reloads 2.3, constants 2.9 (mostly the return address), zero upper words 1.7, the call 1.0 |
| Function entries | 158 k: 12.9 per entry, 12.3 k entries | 8.2% | GCC's register saves 6.0, R[] loads 3.0 |
| Function exits | 89 k: 7.2 per entry | 4.1% | GCC's restores 5.2 |
| Returns (the jr line) | 161 k: 11.9 per call | 7.1% | R[] stores 5.8 |
| Function bodies | 3,231 k | 66.1% | |

- **64-bit functions: 50.7% of the generated instructions and 58% of its I-misses.** One 64 / 128-bit construct (qmfc2 / qmtc2,
  lq / sq, MMI) makes every register of a function int64_t (regcache.py `is_wide`). PSP instructions per PS2 instruction in them
  against the 32-bit functions: arithmetic 3.46 / 1.50, calls 29.2 / 18.2, 64 / 128-bit loads 9.9 / 5.5, 64 / 128-bit stores
  9.8 / 6.4, 8-32-bit loads 4.8 / 3.9. If they were 32-bit: about 0.41 M instructions per step fewer (1.7 ms) plus their share
  of the misses. `RC_PERREG` (§10) narrowed only 39% of their registers (cost-weighted), because it counts every value received
  from outside the function as possibly 64-bit.
- **GCC's own stack** (spills, prologue saves, epilogue restores, on all lines): 429 k instructions, 19.6% of the misses.
- **Guest memory check** (`srl 28` + branch): 124 k per step; RAM mask (`ext 0,25`): 113 k. Of the checked accesses, 34% use a
  base register already checked in the same block (not written since).
- **VU0:** 26.1 k macro operations at 24.9 PSP instructions each; the VFPU groups' VF loads and stores 172 k instructions;
  lqc2 / sqc2 49.3 k at 8.6 each.
- **Hot callees:** 12.3 k entries per step into 888 functions. Leaves of 160-320 PSP instructions take 3.9 k entries per step.
  The hottest is the libvu0 dot product P165de0: 7 PS2 instructions, 216 PSP instructions, 1,250 calls per step.

What regcache.py already does: interprocedural IN / OUT sets (a call stores only the dirty registers the callee may read, and
reloads only what it may write) and liveness of the locals. Why the calls still store and reload this much:
1. IRQ_POLL counts as reading every register (POLL_SETS), so every function with a loop, and every caller of one, has every
   register in IN.
2. Indirect calls and host calls read and write everything, and that spreads to the callers in the same way.
3. The liveness treats a register read and written on the same line as read first. A call line writes the arguments and the
   return address and then stores them, so both are reloaded after the previous call for nothing.
4. Returns store every register the function wrote.
5. The IN / OUT sets are computed per header file, so a call from a stage's code into the ELF counts as reading and writing
   everything. These are the most frequent calls: per step in free play, stage to ELF 5,807 calls (8.31 R[] registers stored
   and 4.48 reloaded per call), stage to its own code 4,275 (6.13 / 3.18), ELF to ELF 2,692 (4.75 / 2.01), ELF indirect 583
   (8.51 / 3.40), host calls 342 (`prof/lean_callkinds_free_v26.txt`, `tools/dev/lean/parts5.py`).

## 2. The steps

**Step 0: one script for the headers.** Today's PSP headers carry a history of passes (v9 headers, then vg / vgb, native hooks,
stage tables). One script from the disc files to the PSP headers, checked to reproduce v26's headers, or failing that v26's hashes
on all three x86 routes. Every later step is an option of this script.

**Step 1: register traffic at calls (regcache.py).** [I] 0.9-1.3 ms. Measured with 1b, 1c and 1e done (callcost.py, model, free
play): -1.55 ms game logic and -0.51 ms draw pipeline per step against v26.
- 1a. Liveness inside a line, in statement order: a register written before it is read on the same line is not live before
  that line.
- 1b. A poll reads only what an interrupt handler can read. Facts (runtime/kernel.h): k_call_handler and k_alarm copy the whole
  R[] into a host KCtx (not in the hashed RAM), set a0-a2, sp and ra, run the handler and copy R[] back; a thread switch saves
  R[] the same way. So a register's value at a poll reaches guest RAM only through the handler's code reading it (a callee-saved
  register saved in its frame, gp). The poll's read set becomes the union of IN over the handler entries (the functions passed to
  AddIntcHandler / AddDmacHandler / SetAlarm / iSetAlarm, found in the code; the x86 kernel reports any handler outside the list)
  minus a0-a2, sp and ra. Dev check on x86: before each handler, the registers outside the set are overwritten with a pattern in
  R[] (the kernel restores them after); the hashes must not move.
  **Measured 2026-10-24 (tools/pollset.py, tools/dev/lean/pollfix.sh; /home/claude/lean, ephemeral): no gain as the code stands.**
  The same 7 handlers run on all three routes (x86 -DK_HANDLER_LOG): alarm P15d9a0 P15dc78, DMAC P123b28 P175cd0, INTC P11d930
  P11d9b8 P123b70, all in the ELF. Their IN is every register but a0-a2 / sp / ra / zero even with polls reading nothing (the
  least fixpoint is reached in 1 round): they call syscalls (iSignalSema-style stubs P173720 / P173810 under P174718) and
  COP0OP 0x38 (EI), which regcache counts as reading everything, and P175cd0 (the SIF command dispatcher) calls the registered
  command handler through a table (JALR at 0x175d98 / 0x175dd4). A smaller set needs all three: SYSCALL reading v1, a0-a3
  (k_syscall reads R[4..7] and the number) plus the poll set where it can dispatch an interrupt, EI reading the poll set, and the
  dispatcher's targets (1d, or the targets seen on the routes with a guard in the x86 kernel). Even then the poll set keeps
  s0-s7, fp and gp (the handlers' prologues save them into the hashed stack), so the gain is the temporaries: per call site
  0.7 v0 / v1 and 0.25 at of the 12.0 stored words' 6 registers (§1 table), about 2 instructions x 13.6 k calls, ~0.1 ms per
  step [I]. Left for after 1a, 1c and 1e.
  **Measured again 2026-10-24 evening (dev, ephemeral: /home/claude/lean/rc4 regcache.py `RC_SPECIAL=1`, rc_jsif.py,
  native_apply_dev.py, mk_headers_dev.sh; /home/claude/lean/kz).** `RC_SPECIAL=1` gives SYSCALL (reads v1, a0-a3 and the poll set,
  writes v0), COP0OP / COP0W (nothing) and the dispatcher's two table calls (renamed JSIF: the union of the ten handlers' sets; the
  x86 build stops on any other target, kz/jsif.h) their real sets. The exact poll set stays R 1, 2, 7-28, 30 and every FPR (fixpoint
  in one round): the handlers' closure (63 functions) holds 33 BREAKs (libgcc's division traps), 19 JALRs (the printf formatter's
  output callback x15, P174f98, P163bd0, P1761a8's end callback, P15dd40) and varargs spills (t0-t3, a3), and the prologues save
  s0-s7 / fp. Exact on x86 (hZ = h7 + `RC_SPECIAL` with that set: every hash line equal to h7's on the three routes) but small:
  callcost -0.064 ms logic, -0.048 ms draw per free-play step.
  **What removes the poll set: a handler starts in a zeroed register file**, as a thread does (k_thread_main), with a0-a2, sp and ra
  set as before (dev `-DK_HZERO` in a copy of runtime/kernel.h, kz/rt). Then a poll reads nothing and SYSCALL reads v1 and a0-a3 (hY:
  `RC_POLL_R= RC_POLL_F=`): callcost -0.248 ms logic, -0.343 ms draw per step (call instructions -14.4% / -39.8%; the poll sites'
  stores, 24,814 in the ELF + w00a, all gone). Checked on x86, three routes, with `MGS2_GS_MASK` = 0x70000-0x80000 (the interrupt
  stack): (1) h7's code with the zeroed start against h7: the state hash differs from the first interrupt on; the masked hash, IOP,
  VU0 (vuh) and VFPU counts are equal at every hash point (559 / 559 / 672 lines); RAM at the route ends differs in 5-7 words at
  0x7fe28-0x7fec7 only, the handlers' `sd s0-s3` saves. (2) The same with every register poisoned at handler entry (`-DK_HPOISON`,
  0xdead<reg>00): the same, the poison visible only in those saves, so no handler uses the thread's values. (3) hY's code with the
  zeroed start equals (1) on every field of every line. A real PS2's kernel leaves its own values in those words. It needs new hash
  references (x86, emulator, PSP hashref): put to Ffm on a card 2026-10-24 18:36 (recommended: clear them).
  **Ffm chose "Clear them" (2026-10-24 19:24). In the project:** runtime/kernel.h `k_handler_regs` (every build from now on;
  `K_HKEEP` gives v26's start, `K_HPOISON` the dev check), recomp/regcache.py `RC_SPECIAL=1` (it also renames the two table calls
  JSIF) with `RC_POLL_R= RC_POLL_F=`, boot/gen_pre.h JSIF (any other target stops the machine). New x86 references (§3): against
  the old ones only the EE hash differs, from vb 100 on; IOP, vfb and VU0 (vuh) are equal on every line of the three routes.
  Emulator (2026-10-24 20:15, a plain PSP build of the v27 headers, 1b + 1c + 1e): every hash line equal to the new references
  on the three routes, no split (psp_harness.md "Hash references from v27 on").
- 1c. Returns store only what a caller can read: whole-program liveness after each call site (through returns in tail position),
  plus every register the function itself stored to R[] earlier (so R[] holds the restored value again).
  **Measured 2026-10-24 (container /home/claude/lean, rc3/regcache.py = the project's + `RC_RETSCR`, ephemeral): the ABI shortcut
  is not exact; 1c needs the liveness, and the liveness needs 1b's poll set.** The shortcut: returns skip every scratch register
  (at, a0-a3, t0-t9, ra; f1-f19), trusting the EE ABI that a caller never reads them after a call. Faithful test
  (`RC_RETSCR=poison`, h11 on 1e's sets: those registers get a pattern naming the register and the function, `abi/retpoison.h`):
  free play 365 / 365 and crew deck 252 / 252 exact, the deck tutorial 250 / 252 (vb 23,500 and 25,200 differ, 23,600-25,100
  equal). At vb 23,500 six words of a stack frame differ (0x1b4634-0x1b4654): a3 and t0 left by P110410 and two FPU registers left
  by P166550, saved by a later function's prologue (`sd` / `swc1`, a varargs-style spill of argument registers it never uses)
  and overwritten by vb 23,600. So the game does read scratch registers after a call (dead saves into the hashed stack, as TT's A4),
  through a later callee's IN: exactly what the liveness of 1c sees, and the shortcut does not. The poll set is the catch: a poll
  reads the handlers' IN, today every register (1b), which would keep almost every scratch register live after calls in loops.
  So the order is 1b's smaller poll set (SYSCALL reading v1 and a0-a3, EI and SYSCALL reading the poll set, the SIF dispatcher's
  table targets), then 1c's fixpoint (a function's return set = what any caller's continuation reads, through tail returns; ELF
  functions called from stages and every indirect or runtime entry keep all their stores). Upper bound of the gain (callcost.py,
  gi_mixc2, 500 free-play steps, h7 against h11's GPR stores): return stores in game logic 87,457 -> 39,361 and in the draw
  pipeline 66,988 -> 24,577 per step, about 0.5 + 0.45 ms [I] (h11's FPU numbers are not usable: regcache wraps each RETPOISON
  in FPU stores and reloads as if it were a call).
  **Done 2026-10-24 evening (`RC_RETLIVE=1`: recomp/regcache.py, tools/ret_open.py, tools/mk_headers.sh).** A function is open when
  a call other than a direct call in its own unit can reach it: the ELF's entry; an address that is a word of an image (the ELF's
  loaded segment, each stage image) or a lui + addiu / ori pair in one, or a hex literal of the generated text; an ELF function
  that stage code calls by name; a stage function below the stage base. ret_open.py lists them from every unit's unpruned
  survey.py output, so the lists hold for the whole game (ELF 867 of 2,303 open, w00a 1,879 of 6,657). Every other function is
  closed and gets RET / FRET, the least fixpoint over its direct call sites of the registers of its OUT that may still be read
  after the call (the caller's reloads, a later callee's IN, direct R[] reads, the caller's own return set; an open caller's
  return reads everything), callers first, then a worklist (ELF: 2,309 evaluations, 16 s). A closed function's returns store
  only dirty & (NOREL | RET) (FPU: FNOREL | FRET), and it leaves the dispatch switch (gen_call / S_<stage>_call), so a call
  through a register, the kernel or a stage that reaches one stops the machine instead of running on. Static, ELF: return
  stores kept 18,678, dropped 5,414 (GPR); 2,111 / 295 (FPU). Checks (x86, three routes, against 1b with the cleared handler
  start): every hash field of every line equal, with the dev lists from the pruned headers and with the project's; also with
  every dropped store writing a pattern instead (`RC_RETLIVE=poison`, gen_pre.h RETPOISON: the GPRs, then GPRs and FPUs); a dev
  guard on every call through a register or from the kernel (tools/dev/lean/retlive) never met a closed function. Gain
  (callcost.py, free play, 500 steps, against 1b): -0.066 ms game logic (return stores -4.8%), -0.163 ms draw pipeline (return
  stores -21.7%) per step. Small, because most remaining return stores are in open functions and their callers (free play per
  step, dev lists: closed R 81 k / F 17 k, open R 55 k / F 21 k). Next for returns: 1d, then a return set for open functions too
  (OPENSET, tools/dev/lean/retlive: its first round showed calls through a register reading everything) [I] 0.4-0.7 ms.
- 1d. Indirect calls use the union of IN / OUT over the functions whose address is taken, not "everything".
- 1e. Calls from stage code into the ELF use the ELF's IN / OUT sets: the ELF's sets are written to a table when the ELF header
  is made and read when each stage header is made. Every stage is linked against the same ELF, so the sets hold for all of
  them. Calls from the ELF into stage code go through the stage table and keep "everything" (or the union of 1d).
  **Done 2026-10-24 (`RC_SOUND=1 RC_XUNIT=1`, §5): -1.232 ms of game logic per free-play step against v26 (callcost.py, model),
  the draw pipeline +0.008 ms; x86 hashes exact on the three routes.**
- Base: R[] stores at calls 164 k, at returns 66 k, reloads 32 k, entry loads 37 k instructions per step, plus the sign words
  and zero upper words that go with them; these lines hold about 11% of the generated code's misses.

**Step 2: whole-program widths (narrow.py, regcache.py).** [I] 1.5-3 ms.
- For each function, which registers can hold a value that is not a sign-extended 32-bit one, at entry and after each call, from
  a fixpoint over the call graph (indirect calls as in 1d). A register gets int64_t only where such a value can reach it, instead
  of every register of a function with one 64-bit construct. 128-bit carriers (lq / sq copies, qmfc2 into a register that only
  moves it on) keep their width; the pointers and counters beside them become 32-bit.
- This also turns the NARROW assumption of the 32-bit functions (today checked only on the routes, GEN_NCHK) into a proof where
  it holds, and lists the places where it does not.
- PS2RC measured 96.8% of its wide operations as really 32-bit with such an analysis (sandstone_review.md §2.4).
- Progress (2026-10-25, paused): recomp/widths_step2.md. The checker runs on regcache's output; its x86 checks found two
  rules the plan lacked: every 64-bit store keeps its full value (the game reads stale stack), and a regcache bug (RC_CONDCLEAN:
  a restore in a branch-likely delay slot dropped a store before a call; v27 has it at 35 sites, hashes unchanged with the fix).
- Result (2026-10-25, widths_step2.md "Fixed references and the choice"): the proved widths (recomp/widths/wpd_all6.json.gz) are the
  choice. The proof found a real v27 bug (RC_SQWIDE: 71 functions' quadword copies through a register lost their upper word; free play
  from vb 33,531), so free play gets new references (prof/*_free_v28.txt); four builds (proved widths, v27 with both fixes, the
  hybrid, all-64-bit) give one state on all three routes. v27's NARROW assumption breaks 11.8-25.9 M times per route (GEN_NCHK), so
  the hybrid is not taken. Model, plain PSP builds, free-play walk: 88.84 ms per step against v27plain 93.04 (-4.2 ms, -4.5%;
  instructions -0.59 ms, I-misses -3.57 ms, part of it layout [I]). PSP build v28 (psp/v28.sh) measures it with profiles and code order.
- v28 (steps 1 and 2 with profiles, code order and v26me4's ME layer; psp_harness.md "Build v28", prof/model_v28_free.txt): exact on
  the emulator on all three routes; model, free play, four windows: 76.70 ms per step against v26's 82.38 (-6.9%; walk window, the
  clean one, 76.01 against 81.34), the generated code -4.46 ms (instructions -8.7%, I-misses -14.1%), code 12-15% smaller. PSP [I]
  about 83 ms against v26me4's 88.97. Delivered 2026-10-25 15:10; the PSP run waits for Ffm.

**Step 3: guest memory checks.** [I] 0.5-1 ms.
- The I/O check once per base register per block (34% of the checked accesses repeat a base), then once per base where it
  dominates its uses and the register is not written in between.
- The RAM mask on the base where the offset cannot cross a 32 MB boundary; a dev build checks every such access on all routes.
- Constant addresses and sp-based accesses already skip the check.
- **Measured 2026-10-25 on v28's code** (dev /home/claude/lean/s3, ephemeral: a copy of boot/ whose gen_pre.h counts accesses by
  address view (`GEN_VIEW_COUNT`) and per header line at the generated code's access macros (`GEN_SITE_COUNT`); x86 hashes equal to the
  proof build's).  PSP (plain build pW6, free-play walk window, generated functions): the path costs srl 28 279.7 k, li 1 91.6 k, the
  branch 243.6 k, ext 0,25 246.7 k, addu s7 429.4 k (stack bases included) instructions per step, about 1.29 M, **20.6% of the generated
  code's instructions** (~5.4 ms of instruction time, plus its share of the code size).  x86, free play vb 30,500-36,500, checked
  accesses by view: kuseg 53.4%, **scratchpad 46.6%** (62% of the stores), uncached 132 k in all, kseg none.  So an inline test for
  kuseg RAM alone (srl 25 + bnez, no mask) would send half the accesses out of line: the uniform test (srl, beq, ext, addu) stays.
  Accesses per straight-line segment and base register (x86 weights): 1.71 on average; group size 1 39.3%, 2 21.0%, 3 8.5%, 4-7
  20.1%, 8+ 11.0%.  A per-base host pointer (about 8 instructions per group, then 1-2 per access) pays only from 3-5 accesses:
  0.2-0.8 ms per step [I].  The upper halves of 128-bit stores (two WR64) are 4.6% of the checked accesses: one check per quadword
  [I] ~0.3 ms.  GCC rematerializes the `li 1` in a third of the checks (91.6 k per step).  Step 3 total [I] about 1-1.5 ms.
- **Implemented and measured (2026-10-25), exact.** `tools/mm_base.py` on the finished headers (`mk_headers.sh MM_BASE=1`) rewrites
  the accesses through a register base plus a constant offset to gen_pre.h's base forms: `RD32B(b, k)` / `WR32B(b, k, v)` and the
  other widths (the I/O test and the host base taken from the base register, k the host offset), the two halves of lq / sq (base =
  the aligned address, k = 0 / 8), ldu_fuse's 8-byte unaligned pairs inline (`GLDUB` / `GSDUB`: lwl / lwr, swl / swr) and lqc2 / sqc2
  with k a multiple of 16 (`VLQC2B` / `VSQC2B`, base = rN & ~15). A base form is exact while base and base + k lie in the same 256 MB
  segment and the same 32 MB window (for the 8- and 16-byte forms also base + k + 7 / + 15); the x86 check build (`-DGEN_MB_CHECK`,
  26,314 checks in gen.o and 51,126 in w00a) reports any access that breaks it. Plus `-DGEN_HB_SP`: the guest stack's host base
  through `gen_hb` (an `addu %0, $23, %1` GCC cannot reassociate), so a stack access is one load or store at its offset instead of
  GCC's `addiu`, `addu` and often a spill. Without MM_BASE and GEN_HB_SP the objects are byte for byte the old ones.
  - **Why the sizing above was low:** GCC's jump threading reuses one base register's test across many blocks (not only within a
    straight-line segment), and the out-of-line GLDU / GSDU (17.6 k calls per step, ~15-21 instructions each plus the call) become
    inline code.
  - **x86** (headers hB: loads, stores, lq / sq; hBL: + unaligned pairs; hBLV: + lqc2 / sqc2; all with GEN_MB_CHECK and GEN_HB_SP):
    deck 252 / 252, crew deck 252 / 252, free play 365 / 365, every dense line equal, no invariant report, on all three.
  - **Model** (plain PSP builds without profile or code order, free-play walk window 31,501-31,600, ms per step; hashes and VFPU
    fallbacks identical to the base build's in the emulator):

    | build | instructions | I-misses | D-misses | ms |
    |---|---|---|---|---|
    | pW6 (v28's headers) | 11.813 M | 117.3 k | 69.0 k | 88.84 |
    | pS: + stack form | 11.730 M | 123.1 k | 71.3 k | 90.19 |
    | pBS: + base forms (loads, stores, lq / sq) | 10.971 M | 109.9 k | 68.9 k | 83.71 |
    | pBSL: + inline unaligned pairs | 10.545 M | 103.5 k | 68.2 k | 80.43 |
    | pBSLV: + lqc2 / sqc2 (v29's code) | 10.404 M | 99.3 k | 68.4 k | **78.99** |

    Instructions -11.9% (-5.92 ms; the generated code 6.12 -> 5.12 M, -16.4%: `srl 28` 273 -> 133 k, the mask 240 -> 91 k, `addu s7`
    421 -> 259 k per step); by part: stack form -0.35 ms, base forms -3.19, unaligned pairs -1.79, lqc2 / sqc2 -0.59. The I-misses
    (-3.78 ms in all) also carry the plain builds' layouts (pS +5.8 k with fewer instructions). Code: gen.o 2,554,556 -> 2,370,728
    bytes (-7.2%), w00a 3,816,584 -> 3,571,208 (-6.4%), the overlays w01a -6.3%, d00t -8.4%, d01t -8.1%.
  - **PSP:** build v29 (psp/v29.sh: v28 with MM_BASE=1 and -DGEN_HB_SP, profiles and code order redone; psp_harness.md "Build v29").

**Step 4: VU0 work in the vector unit.** [I] 1.5-2.5 ms.
- VF registers stay in VFPU registers from one VFPU group to the next inside a function when nothing that can use the VFPU runs
  in between (calls, slow paths of guest memory, polls and fallbacks store them first); lqc2 / sqc2 load and store those VFPU
  registers directly; VN as function-local values (§20). Same operation order as today's groups, so the results stay those of
  today's VFPU form.

**Step 5: small hot leaves inlined.** [I] 0.8-1.2 ms.
- At their hottest call sites, the PS2 instructions of small leaf functions (no calls, no loops) are translated into the caller,
  so their registers become the caller's locals and the call protocol disappears. Same PS2 instructions in the same order: exact.
- The libvu0 helpers (P165de0 dot product, P165d48 / P165d78 matrix x vector, P165f60 / P165f78, the rotations P166108 /
  P1661b0 / P166258) first, by call count.

**Step 6: arguments in registers.** [I] 0.5-1 ms after steps 1-2.
- Two entries per function: a register entry for direct calls (the callee's IN registers as C parameters, up to 8 in registers
  with psp-gcc's EABI; the return value as the C result), and today's R[] entry for indirect calls and the runtime.

**Not taken:** dead save / restore removal (the game reads uninitialised stack, TT lesson A4, and the stack is in the hashed RAM);
hot / cold splitting (the link fails, §10); -Os (slower); VU0 registers as C locals (slower, §20).

## 3. How each step is checked and measured

- **x86:** hashes on the three routes against `prof/hashes_x86_deck_v27.txt` (252), `prof/hashes_x86_w01a_v27.txt` (252) and
  `prof/hashes_x86_free_v27.txt` (365), with their hashref files to find a split (the cleared handler start, 1b; up to v26:
  `_deck_v21`, `_w01a_v21`, `_free_v25f`). Built by tools/dev/x86_ref.sh, run by tools/dev/lean/runh.sh. Width changes also run
  with GEN_NCHK.
- **Emulator** (PPSSPP_VFPU_RN=1): the PSP build's hashes on the same routes.
- **Model:** PspProf on the three windows (tutorial vb 24,001-24,100, free play 31,501-31,600, crew deck); instructions,
  misses and ms per step; `.text` of the ELF, w00a and w01a.
- **PSP:** in the next harness run after the model numbers, with PGO and the code order redone (as for v26). Exact steps measured
  alone in the model may share one PSP build, with each one's model gain listed.

## 4. Expected total

Steps 1-6: [I] 6-10 ms of the generated code's 32.7 ms on the deck in free play (model). The earlier single figure of about
11 ms (speed_notes §49) was the top of this range. Every area gains, not only the deck. Code size [I]: about a third smaller,
which also helps the whole game's code fit in memory.

Order: step 0, then 1 (smallest, one file), 2 (largest single gain), 3, 4, 5, 6 (largest change; its gain shrinks after 1 and 2).
Between steps: the free-play timing run indoors (task #33) and the renderer on the PSP (task #30), as in outlook_25fps.md.

## 5. Soundness of regcache's register sets (2026-10-24, before relying on 1e)

regcache.py decides what a call site stores (the dirty locals in the callee's IN) and reloads (the callee's OUT that is live after
it); its liveness decides which reloads are needed. A review before building 1e on these sets found five holes, all in v26's code
generator too:
1. **Conditional writes counted as made** (ipo_function): movn / movz (`if (R[x] != 0) R[d] = R[y];`, 489 in the ELF), a
   branch-likely's delay slot (`if (c) { R[d] = ..; goto L; }`), writes in an inlined body after its first branch, one switch case's
   writes for the next case. A read after them on the other path was missing from IN.
2. **A callee's may-write OUT counted as written** for what follows a call; the call site stored only dirty & IN, then reloaded OUT,
   so a register the callee writes on some paths only was reloaded from a stale R[] on the others.
3. **liveness() let a movn / movz line kill its destination** (its "conditional line" test looked for `if (...) { `): a register
   needed after a call only on the movn's false path was not reloaded.
4. **An inlined body's own unconditional branch** (`{ int c_ = (R[0] == R[0]); ...; if (c_) goto L<a>_i<k>; }`) made its whole line
   count as one that never falls through, in ipo_function, the GPR and FPR liveness, dsr_in and narrow.py's MUST: the code after it
   was invisible to those flows (39 lines in 27 ELF functions). A goto also matched label prefixes (`goto L1003ac_i13` as L1003ac).
   Measured alone (`RC_UNCOND_FIX=1`): 10 ELF call sites gain a reload v26 lacks (r6 after P16d800 in P1003c0, r9 after P17b390 /
   P17b3d8 / P1757d8, r31); the stages are unchanged.
5. **narrow.py's MUST** counted a conditional tail call's (`if (c_) { P..(); return; }`, 3 in the ELF) must-writes on the
   fall-through path.

**The fix, `RC_SOUND=1`** (implies `RC_UNCOND_FIX`): IPO entries carry (RIN, ROUT, FIN, FOUT, MOUT, FMOUT). MOUT / FMOUT = the
registers written on every path to a return, a greatest fixpoint over the unit computed first; then IN / OUT as a least fixpoint
with MOUT fixed. Per line, a write counts as made at a later point only if every conditional part holding the write also holds the
point (an if's body unless its condition is a nonzero constant, its else part, a switch block and each case of it). An inlined body
counts as one call of its leaf (whose own flow sees its labels at line starts). A call site stores dirty & (RIN | (ROUT - MOUT))
(FPRs alike); a conditional write kills nothing in the liveness; narrow.py gets the sound IN / OUT and a sound MUST (callee-saved
restores kept). With `RC_XUNIT=1` the ELF's saved sets carry MOUT too, and a stage refuses ELF sets saved without `RC_SOUND`.
Checked assumptions: no while / for / do, ternary or && / || with a register write in the translation; every goto targets a label
at a line start; switch cases hold only gotos and polls; no inlined body holds a return; no known function is called with register
arguments in the text (SYSCALL / BREAK take constants, JALR a captured temporary). Tests: t_sound.py / t_sound2.py (movn,
branch-likely, call then read, `if (2)` lq, an inlined leaf with an inner branch, an inlined unconditional branch, a switch).

**Headers** (mk_headers.sh): the ELF changes 574 statement lines against v26, the stages 2-6 lines each (n_title, d00t, d01t, w00a,
w01a), r_* / init are equal; narrow.py still rewrites the same 131 + 143 dsll32. ELF call sites: R[] stores 6,634 -> 6,884, reloads
4,230 -> 4,254. Cost on free play (callcost.py, gi_mixc2, 500 steps): game logic +2,505 PSP instructions per step (+0.011 ms),
draw pipeline +1,981 (+0.008 ms).

**x86 hashes, three routes** (v26's x86 recipe with the nine units' objects rebuilt): v26's headers remade (h1), the fixed ones
(`RC_SOUND=1`, h5), a cautious reference (`RC_NOIPO=1 RC_NOLIVE=rf RC_NONARROW=1`: every call stores every dirty register and
reloads every caller-saved one, no liveness, no 64 -> 32-bit narrowing; h6) and 1e on the fixed sets (`RC_SOUND=1 RC_XUNIT=1`,
h7): each 252 / 252 (deck), 252 / 252 (w01a) and 365 / 365 (free play) against the references, and all hash lines (every 100
vblanks, and every vblank of 15,700-16,010 with the VU0 state hash) equal to h1's on all three routes. So the holes changed nothing
on these routes: they were latent, and another area of the game could have hit them.

**1e on the fixed sets (h7)**, callcost.py on free play: game logic call + return traffic 967,315 -> 671,694 PSP instructions per
step against h5 (-1.243 ms at 1.4 CPI), the draw pipeline unchanged. Against v26 (h1): -1.232 ms logic, +0.008 ms draw. The fix
takes 0.026 ms of 1e's gain (1e on the old sets, h2: -1.258 ms).

**The premise every build shares** (v26's, the fixed one and the cautious reference): the EE ABI, a callee leaves s0-s7, gp, sp, fp
and f20-f31 as it found them (a caller keeps them in its locals and never reloads them). Checked on x86 with a dev wrapper
(`tools/dev/lean/abi_wrap.h`: every translated function compares those registers in R[] / F[] at each return with their values at
its entry) on h7's build: 0 differences in 143 M (deck), 132 M (w01a) and 271 M (free play) returns, hashes exact.

**In the project** (recomp/regcache.py, recomp/narrow.py): the default options give v26's nine headers byte for byte, and
`RC_SOUND=1 RC_XUNIT=1` gives h7's (mk_headers.sh, REF). Headers from v27 on: `RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R=
RC_POLL_F= RC_RETLIVE=1` (1b, 1c, 1e).
