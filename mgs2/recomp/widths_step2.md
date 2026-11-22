# Step 2 of the leaner generator: whole-program register widths (2026-10-24/25; paused 02:26, resumed 10:32)

Plan: recomp/lean_plan.md step 2. Tools: tools/dev/lean/widths/ (README there). Widths: recomp/widths/*.json.gz.
State at the pause: proof widths exact on the deck tutorial and crew deck routes (x86, every checkpoint and every per-frame hash of
the window); free play differs from vb 33,600 in the proof-only variant. The HYBRID with the regcache fix (hH6: proof widths in
v27's 64-bit functions, v27's all-32 elsewhere) is exact on all three routes: 252 / 252 / 365 checkpoints and 559 / 559 / 672
per-frame hashes. No PSP build yet.

## Method, assumptions, findings

**Tools (copies in tools/dev/lean/widths; working copies in the container /home/claude/lean/w2/tools: wpo2.py, wpd.py, rcdrv.py, wstat.py, wpstate.py;
regcache.py `RC_WPW=<widths json>`).**
- **What it proves.** Per function and register, whether the int32_t local is exact: the checker parses regcache.py's own output
  (the C it emits under the current widths) and runs (1) a forward may-analysis of the guest values over the whole program (W: may
  not be a sign-extended 32-bit value; per function a summary, entry sets E[f], the calls through a register as the union over the
  address-taken functions), independent of the widths; (2) a backward demand analysis: a full use (compare, 64-bit shift right,
  sd / sq to memory, sdl / sdr, mthi / mtlo, qmtc2, the MMI helpers, the qword-copy idiom's check; not a SYSCALL argument,
  which the runtime reads as 32 bits) needs the full value; demand passes through 64-bit moves / logic / adds / shifts left, reloads after calls (callee summaries T / U
  and the return demand DRET), stack slots; a narrow local where a W value meets a demand is widened. Rounds until nothing widens
  (12 rounds, 16 min, all 9 units, 33,282 functions). regcache.py then emits with those widths (`RC_WPW`); with v27's widths given
  explicitly the pipeline rebuilds v27's 9 headers byte for byte.
- **Assumptions** (each one a property of compiled code, written down so a failure can be traced):
  A1 (option, on): a daddiu / dnegu of an S value stays S. A2: no store through a pointer writes a function's register save slots,
  and no read through a pointer reads them (only the function's own restores). SYSCALL results are S; the runtime reads SYSCALL and
  kernel-call arguments as 32-bit values. A function's frame is written only by itself by its sp, or through an address it has taken
  (checked: no function stores into its caller's frame by sp). A caller does not read its outgoing argument slots after the call.
  A stack slot read before the function writes it is W (with exact memory: the same bytes as v27's). taken.py finds every function entered other than by a direct
  call. Keep functions (native replacements: P12dbc0, S_w00a_P333380) stay at v27's widths. Blocks no path from the entry reaches
  take no part.
- **Precision items** (each checked monotone: no function gained a wide register): branch-likely delay slots and inlined bodies that
  end in their own jump are blocks of their own (they were counted as conditional writes); an ldl / ldr pair loading all 8 bytes
  does not read its destination's old value.
- **Soundness bug found by the x86 check (2026-10-25)**: the first full proof (hW3) split from vb 4,300 on all routes. Bisected
  (mixed ELF / stage objects, then the ELF's 769 changed functions in halves and quarters) to P12cb58: it copies a 4x4 matrix into
  its frame with ld / sd and reads it back with lqc2 (VLQC2) and lwc1; the demand on stack slots came only from 64-bit loads into a
  local, so the sd's sources were taken as unused and became 32-bit. Fixed: every read of the stack (8-32-bit loads, lwc1, lqc2,
  ldl / ldr, lwl / lwr, the upper qword of lq) demands the slots whose upper word it reads; a call demands the slots its callee can
  read by its sp (stack arguments: the callee's extent, 5 of 1,623 ELF functions, 44 bytes at most; every non-save slot when the
  callee's frame is taken or the call is through a register to such a function); a return demands stores into the caller's frame
  (none exist). ELF alone: 4 functions change (P12cb58, P12cd78, P128d90, P1068b0).
- **Second difference (2026-10-25), and the rule it changes**: with that fix the three routes were equal to vb 20,100 and differed
  from vb 20,200 (d01t loaded) to vb 23,200 in the EE RAM hash only (IOP hash and frame counts equal), equal again once w00a loaded,
  then on and off. EE RAM at vb 20,200 (container dumps, deleted): 5 of 4 M 8-byte slots differ, 0x1b45d8-0x1b4618 every 16 bytes,
  in the game thread's stack (bss): v27 stored (float, 1.0f) pairs with sd, the proof build (float, 0). The function that stored them
  reads no upper word of those slots afterwards, so demand let the stored value be the 32-bit local. Dead for that function, not for
  the game: MGS2's code reads stale stack words (speed_notes §15: the actor work area 0x1b4700-0x1b48ff that the next actor partly
  reads before writing; §40: P11cf88's lane z inputs), so what a store leaves on the stack is state like any other. Rule from now on
  (wpd.py WPD_EXM=1, the default): every 64-bit store, to the stack too, needs the stored value in full, so memory stays byte for byte
  v27's. regcache's 64-bit pass-through already stores R[n] for a register not yet written on any path (the save of a callee-saved
  register) and RCLD restores R[n] exactly, so a save / restore pair needs only the caller's value to be exact at the call. The
  stack-slot demand of the first fix is implied by the rule; the stack tracking still serves the forward pass (a value spilled and
  reloaded keeps its S).
  Cost of the rule (all units, 8 rounds, 16 min): 2,409 functions gain 3,938 64-bit locals (1,415 of them 64-bit in v27), none
  loses one; 59,102 64-bit locals in all (55,164 without the rule). The hot functions barely move: P11fa58 7 of 21 registers as
  before, S_w00a_P31af28 5 (4), P14d860 14 (10), P14fe30 13 (11), P1226c8 12 (11), P12f130 11 (10).
- **Third difference (2026-10-25): a regcache bug, not the width logic.** With exact memory the deck tutorial and crew deck were
  equal at all 252 checkpoints, but the per-frame window (vb 15,700-16,010) differed at vb 15,739-15,757 in the EE hash on all
  routes (hybrid too). EE RAM at vb 15,739: one word, scratchpad 0x3eb0 (a stack in the scratchpad), 0x01da1950 in v27 and 0 in
  the proof build. Write-traced builds (GEN_DG_TRACE, MGS2_WWATCH): the last writer is S_d00t_P32f8a0 saving a caller's s0 by the
  64-bit pass-through (WR64(sp, R[16])). Its caller S_d00t_P32e1f0 (64-bit in v27, 2 wide registers in the proof) sets s0 = a0
  with a plain write (a0 is wide, so no RCMV), and has an early exit in a branch-likely delay slot:
  `{ if (r2 != r0) { RCLD(r16, 16, ..); goto L32e39c; } }`. regcache's dirty dataflow applied that RCLD to the whole line, so on
  the fallthrough s0 counted as clean and the stores of R[16] before the next calls were dropped: the callees saved a stale R[16].
  The checker could not see it (it takes regcache's stores and reloads as the semantics). Fix (dev copy, RC_CONDCLEAN=1): an RCLD /
  RCMV inside an if-block cleans only the goto edges inside that block (and positions inside it), not the line's fallthrough.
  v27's own headers have the same pattern at 35 call sites (gen_elf.h 5, d01t 2, w00a 16, w01a 12: a stale callee-saved register
  saved into a dead stack slot); v27 with the fix keeps all 252 / 252 / 365 checkpoint hashes and all 559 / 559 / 672 per-frame
  hashes of v27's references, so the fix changes no reference.

## Results so far (x86, three routes, v27 references)

| build | widths | deck 252 | crew deck 252 | free 365 | per-frame window (559 / 559 / 672) |
|---|---|---|---|---|---|
| hW3 (first proof) | all3 | split vb 4,300 | split vb 4,300 | split vb 4,300 | - |
| hW4 (stack fix) | all4 | split vb 20,200 | split | split | 54 differ (deck) |
| hW5 (exact memory) | all5 | 252 | 252 | 335 (vb 33,600 on: vfb) | 19 / 19 / 49 (vb 15,739-15,757) |
| hH5 (hybrid) | hyb5 | 252 | 252 | 365 | 19 / 19 / 19 |
| v27 + RC_CONDCLEAN | v27 | 252 | 252 | 365 | 0 / 0 / 0 |
| hW6 (fix + exact memory) | all6 | 252 | 252 | 335 (vb 33,600 on: vfb) | 0 / 0 / 30 |
| hH6 (hybrid, fix + exact memory) | hyb6 | 252 | 252 | 365 | 0 / 0 / 0 |

- all6 vs all5: 82 functions keep one more 64-bit local (87, mostly s0: the stores of R[16] before calls that the fix restores).
- Free play in the proof-only build (hW5, same in hW6): EE RAM at vb 33,600 differs in 130 8-byte slots at 0x16eee80-0x16ef8a0
  (a per-frame array of float pairs whose upper floats differ, and list links), equal again from vb 33,700; the VFPU fallback count
  stays one lower from then on. Absent from the hybrid, so it comes from a v27-32-bit function the proof widens (unproved list).
  Not found yet.
- Calls through a register dominate the imprecision: v27-32-bit functions the proof widens: 5,887 (17,815 locals) with them,
  1,400 (2,646) in the bound that ignores them (WPO_NOIND=1, not sound); 64-bit locals in v27-64-bit functions 41,287 vs 27,238.

## Free play's split is v27's fault (2026-10-25)

- First difference: the state after vblank 33,530's work (the hash at vb 33,531): 96 words, all lane y (+4) of 16-byte records at
  0x16eee80-0x16eeff7 (24 records) and 0x16ef850-0x16efe47 (two copies 0x300 apart), and one more VFPU fallback in v27; equal again
  from vb 33,603.  (Per-vblank hashes of the proof build from vb 33,400 against v27's every-vblank reference; RAM dumps at 33,530 /
  33,531.)
- Traced (x86 GEN_DG_TRACE builds xPt and xW6t, MGS2_READ_LOG=33530:33530: 269,088 loads each, paired by index, 302 differ; writers by
  MGS2_WWATCH): P125fd8 copies a 4x4 matrix that P165e40 wrote into its frame, `lq t0-t3` from the frame, `sq` to v0.  v27 makes
  P125fd8 a 32-bit function: RCLD keeps R[8] full but the int32_t local is truncated, and the sq stores the local, so lane y of
  each row becomes the sign extension of lane x (0, or 0xffffffff: a NaN, hence v27's extra VFPU fallback).  The proof widens
  r8-r11 there (a frame lq gives W, an sq is a full use) and copies what the PS2 copies.
- Cause, in regcache's WIDE list: it matches sd's `WR64((uint32_t)(R[b] + k), ..)` but not sq's `WR64(((uint32_t)(R[b] + k)) & ~15u, ..)`
  (one more parenthesis), so 37 functions with an sq to a base other than $sp are 32-bit in v27 (17 store a real register); and in
  any 32-bit function a copy through a register from a frame ld / lq to an sd / sq loses the upper word (50 functions).  In the route
  builds (coverage list) 5 such functions are compiled: P11c5c8, P125fd8, P158758, P15f270, S_w01a_P2bc000.  So v27's free-play
  references from vb 33,531 on are not the PS2's.
- Fix: regcache.py RC_SQWIDE=1 (dev copy /home/claude/lean/rcnew; makes 71 functions 64-bit: the sq form, and a frame ld / lq reaching
  an sd / sq directly or by moves in line order).  The proved widths cover every case, loops included.
- Loads that differ earlier in the same vblank (actor stack words 0x1b4700-0x1b48ff, e.g. a saved s0: 0x1ecee8 in v27, 0x41a1b0 in the
  proof build) are overwritten before the vblank ends: the builds leave different values in R[] registers nothing reads later [I:
  a wide local's move reaches R[] later than RCMV, which writes R[] at once].
- The hybrid with the NARROW check (hH6n: GEN_NCHK in v27's 32-bit functions) keeps every hash and counts 13.7 M (deck), 11.8 M (crew
  deck), 25.9 M (free play) loads of a 32-bit local from a non-sign-extended R[] (the 128-entry table fills on every route, so the
  list is cut): v27's NARROW assumption is broken often, mostly without effect on the state.

## Fixed references and the choice (2026-10-25, late morning)

- **v27 with the copy fix** (v27's options + RC_CONDCLEAN=1 + RC_SQWIDE=1; headers /home/claude/lean/hV27f, x86 xV27f): interprocedural
  and return sets equal to v27's in every unit (dev/setcmp.py); deck and crew deck 252 / 252 checkpoints and 559 / 559 per-frame lines
  equal to v27's references; free play 335 / 365 (first difference at the vb 33,600 checkpoint; 30 per-frame lines differ).  **Against
  the proof-only build (hW6 / xW6): every checkpoint and per-frame line equal on all three routes** (252, 252, 365; 559, 559, 672).  Two
  independent fixes agree.  RC_SQWIDE is in the project's regcache.py (md5 c79518699e4c41d1510498afb4e963ba; without the option the
  ELF header is v27's byte for byte, checked) and noted in tools/mk_headers.sh (sets with RC_WPW leave it off: is_wide also picks the
  inline candidates).
- **v27's NARROW assumption on the routes** (hH6n with GEN_NCHK, the full table; /home/claude/lean/xH6m, hashes all equal to v27's): deck
  13,672,409 loads of a 32-bit local from a non-sign-extended R[] at 752 places, crew deck 11,793,021 at 680, free play 25,853,752 at
  957; 1,071 (function, register) places in all, most at function entry (666 of 680 on the crew deck).  265 of them are registers the
  narrow-in hybrid (hyb7, below) keeps 32-bit only on that assumption, i.e. registers the proof widens: P13f140 r3 (2.19 M loads),
  P14d1c8 r2 (1.91 M), P16b690 r11 (1.03 M), P13f140 r2, S_d01t_P32bb78 r3, ...  They change no hash on the three routes, but nothing
  shows they are harmless elsewhere.  **So the proof-only widths are the choice** (exact by the proof, not by the routes).
- **hyb7** (dev/hybrid2.py over wpd_all6.json and a narrow-in run, WPD_V32=w3/wpw_v27x.json: v27's 32-bit functions get no W at entry
  or from callees, only from their own frame loads): 50 of v27's 32-bit functions get 207 wide registers (P125fd8 and P15f270 among
  them); 5,855 functions (17,632 registers) rest on the assumption.  Checker 1,215 s.  Kept for the record (x86 and model below).
- **Model** (dev/model1.sh: free-play walk window 31,501-31,600, plain PSP builds without profile or code order): proof widths (pW6)
  instr 11.813 M, I-miss 117.3 k, D-miss 69.0 k per step = **88.84 ms**; v27plain 11.954 M, 134.3 k, 69.3 k = 93.04 ms (**-4.20 ms,
  -4.5%**: instructions -0.59 ms, I-misses -3.57 ms).  Code: main ELF text 3,877,316 bytes (v27plain 3,896,052), w00a 3,819,472
  (3,841,888), w01a 2,319,456 (2,328,880), d01t 2,677,888 (2,698,448).  The I-miss part also carries the plain builds' layouts [I]:
  the build with profiles and code order (v26's method) gives the figure to quote.  Emulator: 63 / 63 hash lines equal to the x86
  free reference (to vb 31,610).
- **Cross-checks (12:15):** the all-64-bit build (RC_WIDE=1 with w3/cov_a64.txt; hA64 / xA64: no width decided at all) and hyb7
  (hH7 / xH7) equal the proof build on every checkpoint and per-frame line of the three routes, as v27 + both fixes does: four builds,
  one state.  hyb7's plain PSP model: 90.19 ms (instr 11.825 M, I-miss 118.8 k, D-miss 73.7 k), slower than the proof widths' 88.84,
  so speed gives no reason for the hybrid either.  The v28 headers made from the project's tools (psp/v28.sh hdr: mk_headers.sh with
  v27's options + RC_CONDCLEAN=1 + RC_WPW) equal hW6's byte for byte (all nine).  New free references in the project:
  prof/hashes_x86_free_v28.txt and prof/hashref_x86_free_v28.txt (psp_harness.md "Free play's hash references from v28 on").

## Next

1. Done.  Per-vblank free reference prof/hashes_x86_free_v28_every.txt.gz (runs/rune.sh on xW6, 36,500 lines, equal to the
   checkpoint file at all 365 checkpoints): against v27's, ee differs at exactly the 72 vblanks 33,531-33,602, iop at none, and
   the VFPU fallback count is one lower from vb 33,531 on.
2. PSP build v28 (psp/v28.sh): headers from tools/mk_headers.sh with v27's options + RC_CONDCLEAN=1 + RC_WPW=recomp/widths/
   wpd_all6.json.gz (compared byte for byte with hW6), profiles and code order as v26, the Media Engine mix + crash catcher 2 +
   battery log on top as v26me4; emulator against the references; model on v26's four windows; described to Ffm before it ships.
3. Later: a precise treatment of calls through a register (the bound says up to a third fewer 64-bit locals in 64-bit functions);
   then steps 3-6.
