# gl45 study: hand-written native code against the translation, batch 1 (2026-11-27)

Asked by the port lead: an A/B of hand-written native C against our recompiled code for hot game-logic functions, exact form, one
switch per function, to learn what "native" buys before a hot set is chosen.  Done on the study thread's own tree copy; nothing under
`main/` edited but this folder; no PSP run, no delivery.  Marks: [C twin] measured on the x86 twin, [C psp-gcc] read from the PSP
compiler's output, [C model] the fitted cost model's figure, [I] inferred, UNKNOWN.

## 0. Short answer

- Seven forms are written, each behind one bit of `g_n45`, each exact on the twin: every call done both ways and compared over the
  whole route (1.5 to 5.8 million calls a form, none different), and with all seven on the route gives the reference's seed / time log,
  all 685 hash lines (EE, IOP, game state, vfb) and all 37,831 picture lines.  EE RAM is equal too (the EE hash is equal; no stack
  leftover, no scratch difference: these are leaves that write no stack).
- What native buys on these, in PSP instruction slots on the hot path [C psp-gcc, static path]:

  | bit | form | class | translated | native | native / translated |
  |---|---|---|---|---|---|
  | 0 | MT_QuatNormalize 0x11cd68 | FPU leaf | 90 | 77 | 0.86 |
  | 1 | sceVu0MulMatrix 0x165d78 | VU0 leaf, loop of 4 | 286 | 141 | 0.49 |
  | 2 | MT_QuatToMat 0x11c828 | VU0 leaf, straight | 253 | 170 | 0.67 |
  | 3 | _sceVu0ecossin 0x166090 | VU0 + Q + flags | 247 | 205 | 0.83 |
  | 4-6 | sceVu0RotMatrixZ / X / Y 0x166108 / 0x1661b0 / 0x166258, merged with their ecossin call | FPU + call + VU0 + loop | 389 + 247 = 636 | 285 + 202 = 487 (X 495, Y 496) | 0.77 |

- In ms a PSP step [I]: the seven together cost about 1.9 ms a rain-wait step translated and 1.3 native: **about -0.55 ms a step in the
  rain wait (of 34.4 logic) and -0.43 indoors (of 17.4)**.  Basis: saved slots x calls a step [C twin] x 4.95 ns a slot (the cost
  model's instruction term [C model]); cache terms not counted, so only a PSP run by turns settles it.
- So on the maths leaves the answer to "what does native buy" is 1.2x to 2x on the function itself and about 1.4x on the set, and the set is
  small: these functions are already near-native in the translation (the FPU code is plain operators, the VU0 code is VFPU
  groups).  What the native form removes is the staging of VU0 registers through memory between groups, the per-access
  register-space tests and the per-call entry / exit work, not the arithmetic.  The exact form keeps every register write-back, which is
  10 to 25 slots a call of what is left.
- NOT DONE in this batch: an integer / pointer-walk function (the class most of a "hot ~200" would be made of), the indoor
  profile (the run died with a session stop at step 34,700 of 37,800), a PSP or emulator build.  Batch 2 would do the integer one first:
  section 6.

## 1. How the candidates were ranked

(a) The existing model ranking of the deck's steady window (`attic/top3/levers_top3.md` section 6, [C model]): of the game-logic row
its "native" verdicts are ecossin with RotMatrixX/Y/Z, MulMatrix, QuatToMat and QuatNormalize: the seven here.
(b) A callgrind run of the twin over the rain wait, vb 29,500 to 30,700 (x86 instructions, self cost, [C twin]; `boot_x86.c`'s
MGS2_PROF_VB).  The collection toggles did not isolate the logic (the draw pipeline's VU1 interpreter is in the totals), so the list
below is by function, draw-side and already stubbed ones left out by name; x86 instructions a step:

| x86 instr a step | function | note |
|---|---|---|
| 243 k | S_w00a_P3b89e0 CalcObjsCollisionWithInvM | VU0, 9 calls a step; not done |
| 202 k | P165d78 sceVu0MulMatrix | form 1 |
| 199 k | P14b008 (HZX trap walk) | integer pointer walk with calls; not done: the batch 2 candidate |
| 169 k | P166090 _sceVu0ecossin | form 3 |
| 157 k | P11c828 MT_QuatToMat | form 2 |
| 81 k | P11cd68 MT_QuatNormalize | form 0 |
| 78-80 k each | P166108 / P1661b0 / P166258 RotMatrixZ / X / Y | forms 4-6 |
| 74 k | S_w00a_P3b93a0 | not read |
| 69 k | P11ce30 MT_SinX4 | FPU leaf, not done |
| 57 k | S_w00a_P3ae448 | not read |
| 54 k | P165d48 sceVu0ApplyMatrix, S_w00a_P3c2898 MT_QuatMulFast | VU0 leaves, same recipe as form 1 |
| 44 k | S_w00a_P3a2280 (l2dGetParts part) | integer, not done |

A VU0 function's x86 self cost leaves out the twin's vu_rnmul / vu_rnadd helpers, so the x86 order is not the PSP order.
Left out as asked: the draw pipeline (0x11da90 and below, P125f18 DG_PutVector), G0 / H3 / NBS / V44 and B's four
(0x318418, 0x317e88, 0x319600, 0x319078: still translated in this tree; three of them show in the profile at 153 k, 71 k and 42 k).
Share of the logic [I]: the seven forms' translated cost is about 381 k PSP slots a rain-wait step = 1.9 ms of the 34.4 (5.5%) and
about 1.3 ms of the 17.4 indoors (7.5%).  Indoors by profile: UNKNOWN (no profile); the call counts of section 4 are measured there.

FACT FOR THE HOT SET: the translator inlines small leaves into their callers inside the ELF unit (MulMatrix 31 copies,
QuatNormalize 2, QuatToMat 1; none in the stage units).  A native form hooked on the function itself serves only the calls that are
real calls (all of the stage units' ones).  The model's "calls x instr" for such a leaf counts the real calls only.

## 2. The framework (boot/native45.h, boot/native45.c, tools/n45_apply.py)

- `n45_apply.py <gen_elf.h> [<out>]` renames each hooked translation `P<addr>` to `P<addr>_tr`, puts the wrapper `P<addr>` after it
  (`N45_WRAP`) and includes native45.h before the first generated function and native45.c after the last (generated functions keep their
  order, as native_elf).  It works on both headers (x86 `gen/wh`, PSP `gen/wgh`); the study's hooked copies are `gen/w45`, `gen/wgh45`.
- The wrapper: `FN_ENTER`, then if the form's bit of `g_n45` is set and the form accepts the call, return; else the translation.  A
  form that declines has written nothing (or has put back what it wrote), so the translation starts from the untouched state.
- One switch word: `uint32_t g_n45`, bit i = form i (enum in native45.h: QNORM 0, MULM 1, Q2M 2, ECS 3, ROTZ 4, ROTX 5, ROTY 6).  x86:
  `MGS2_N45=<hex>`.  PSP: the harness has to set it (a cfg key, a cycle by turns); with ROTZ/X/Y on, ECS's bit only serves the three
  remaining direct calls.  Default 0 = the translation everywhere (the wrapper then costs about 6 slots a call: count that when timing by turns).
- Time: `N45_ROOM(P)` is the translation's own poll test for P polls at once (K_COUNTDOWN: `k_left > 64 P`), `N45_ADV(P)` adds them.
  MulMatrix and the RotMatrix forms cover 3 polls; the others none.
- VU0: a form's VFPU work is ONE list of N45_L / N45_O / N45_K / N45_S lines.  On the PSP the list is one asm string made of the
  translation's own instruction forms (lv.q, vpfxs / vpfxt / vpfxd, vadd / vsub / vmul.q, vcmp ES + bvtl, sv.q); on x86 the same list
  runs through vu_rnadd / vu_rnsub / vu_rnmul.  So the sequence the twin proves is the sequence the PSP runs, as far as the two macro
  sets agree (about 30 lines each, side by side).  The PSP strings assemble with the project's psp-gcc (gen45.o built).  They have
  NOT run on a PSP or an emulator: the build agent's hash gates are the proof of the asm side.
- Check mode `-DN45_CHECK` (x86): every accepted call is done natively, its result kept, the state put back, the translation run, and
  everything compared: R, RH, HI / LO, F, FPU ACC, CC, VF0..31, VU ACC, VI, Q, P, I, R, MAC, status, clip, CMSAR0, g_vmac, the count of
  group fallbacks, the time, the words the form declared (N45_W); for the first MGS2_N45_FULL (50) calls of each form the whole 32 MB.
  A declined call is checked for an unchanged state.  Not compared: VN[] and vu_sv (fallback scratch).
- `-DN45_TICKS` (x86): rdtsc per call and arm (calls over 50,000 ticks dropped: the host was elsewhere).  `MGS2_N45_MARK=1` prints the
  counters at each hash line (per-window numbers; the one-line hook in boot_x86.c state_hash is quoted in section 7).

## 3. What each form keeps, and what made it easy or hard

0. **QuatNormalize** (easy).  Same operators and macros in the same order (FADD's zero rule, FDIV's x / 0 and clamp), the FPU
   accumulator's third sum written to ACC, R[1], F[0], F[4..8] written back.  Gain small because nothing was wrong with the translation:
   of its 90 slots 2 x 4 are register-space tests, 10 are host-register saves, the rest is the work; sqrt.s and div.s dominate the time.
1. **MulMatrix** (easy).  m0 kept in VFPU registers, the 4 results held until all 4 checks have passed, then stored with VF4..VF9 and
   ACC.  Reading all of m1 before the first store is the translation's order only when out is m1 itself or disjoint from it: otherwise
   it declines.  Declined 8 a step in the rain wait (a poll due within its 3, mostly).
2. **QuatToMat** (medium: 48 operations to transcribe; the check mode found no slip).  A value the translation checks before and after
   its doubling is checked once, after.  The three lane-rotating moves of VF0 become constants: this needs VF0 = (0, 0, 0, 1), which our
   runtime keeps (no write to VF[0]).
3. **ecossin** (hard part: the flags).  After the polynomial group the translation sets MAC / status three times (a clamped FPU
   subtraction, the square root's D / I bits, and two macro ops through VUM).  The form writes those with vu0.h's own vin / vout /
   vu_setflags.  Lesson: VUM with a constant word is already specialised by the compiler (always-inline), so "interpreted" there
   does not mean slow; the model's 405 slots a call is 247 on the path I walked [C psp-gcc static; the two differ, UNKNOWN which is
   the true mean].
4-6. **RotMatrixZ/X/Y merged with ecossin** (hard part: undo).  The fold of the angle, ecossin's body as a call of the same form, the
   moves as loads, the row set-up group and MulMatrix's loop as one list.  ecossin has already written VF4..VF8, Q and the flags when the
   loop's check can still fail, so copies taken at the start are put back on a late decline.  That copy, the many separate state
   symbols (each a lui / addiu pair) and the host-register saves make "other" instructions 181 of 285: a second pass could cut perhaps
   60 to 80 of them [I] (one base pointer for the state, no copies where a check provably cannot fail).

Sizing rule of thumb from this batch [I]: a leaf of this kind is 30 to 60 minutes including its proof once the framework exists; a
function with flags or with state written before its last check is 1.5 to 2 hours.  The check mode is what makes it safe: a slip shows at its first call,
with the register and both values (in this batch it reported none).

## 4. Measurements

Calls a step [C twin, MGS2_N45_MARK] and x86 ticks a call [C twin, N45_TICKS, whole-run pairs tk0b / tk7]:

| form | calls a step, rain wait | indoors | x86 ticks translated | native |
|---|---|---|---|---|
| QuatNormalize | 720 | 496 | 58.5 | 53.9 (-8%) |
| MulMatrix | 281 (+8 declined) | 306 (+7) | 1,717 | 1,970 (+15%) |
| QuatToMat | 258 | 197 | 1,440 | 2,684 (+86%) |
| ecossin (alone, bits 0-3) | 268 | 130 | 562 | 706 (+26%) |
| RotMatrixZ / X / Y (merged) | 87 / 91 / 89 | 41 / 45 / 44 | 2,656 / 2,589 / 2,571 | 3,323 / 3,191 / 3,192 (+24%) |

READ THIS BEFORE THE x86 COLUMNS: for the VU0 forms the x86 arm is the reference emulation of the VFPU (a generic operation routine
calling vu_rnmul and friends lane by lane, all four lanes even where the translation's x86 form computes one).  It is slower than the
translation's x86 form by construction and says nothing about the PSP.  Only QuatNormalize's x86 pair is like for like (-8 to -10%).

PSP slots on the path [C psp-gcc]: the table of section 0, from `tools/psp_path.py` on gen45.o (psp.sh's flags, no profile): the
wrapper's native path against the `_tr` function's path (loop back-edges followed 3 times, every other conditional as not taken, which
is the fast path of the register-space and NaN tests; VFPU share: MulMatrix 104 of 286 translated, 71 of 141 native).

PSP estimate [I]: slots saved x calls a step x 4.95 ns:
- rain wait: 720 x 13 + 281 x 145 + 258 x 83 + 268 x 149 = 111 k slots = **0.55 ms a step**;
- indoors: 496 x 13 + 306 x 145 + 197 x 83 + 130 x 149 = 86 k slots = **0.43 ms a step**.
The I-cache side is not in it: the hooked functions grow by the wrapper (the native path is short and contiguous, the translation
behind a jump), text +8.5 KB for the seven wrappers and the shared ecossin body [C psp-gcc].  Single estimates have been 2x off in this project; this one has no PSP anchor at all.

## 5. Proof runs (all on the twin, route i to vb 37,800, the reference's OPT2 word ffbff9df)

| run | binary, MGS2_N45 | result |
|---|---|---|
| chk3 | check build, 7f | calls equal / different / declined: QuatNormalize 5,762,281 / 0 / 0; MulMatrix 5,420,543 / 0 / 63,629; QuatToMat 4,846,886 / 0 / 0; ecossin 4,505,635 / 0 / 0; RotMatrixZ 1,499,989 / 0 / 4,036; X 1,506,091 / 0 / 5; Y 1,495,512 / 0 / 2; 50 calls of each with the whole RAM compared |
| on2 | plain build, 7f | seed / time log 27,686 passes identical to the reference run (md5 c153c8e8678c); 685 hash lines: EE, IOP, game state, vfb all equal; 37,831 picture lines equal |
| on1, chk1, chk2 | forms 0-3 only | the same equalities |

The read hash (rh43.sh) was not run.

## 6. What I would do next (batch 2), and what this batch says about a hot set

1. The integer walker P14b008 (199 k x86 instr a step in the rain wait; 27 calls x 5,608 PSP slots in the model): its loop keeps its
   counters in stack words (the original was compiled that way); a native form would hold them in registers and write the frame's
   final words at the end.  The model already rates it "no" (1.9 PSP instructions a guest instruction).  It is the honest test of
   the largest class, and the first one where a stack leftover is in play.
2. CalcObjsCollisionWithInvM (243 k), ApplyMatrix, QuatMulFast: the MulMatrix recipe.
3. The inlined copies: a native MulMatrix reaches the ELF unit's 31 inlined sites only if the translator stops inlining it there or
   the callers become native.

Reading for the route [I]: on this evidence a hand-native hot set of maths leaves is worth about half a millisecond a step, not the
1.5x on the logic that was hoped; the 1.5x would have to come from functions where the translation is NOT near-native (64-bit and
unaligned stores, stack-held loop state, call protocol in call-heavy code), and that is unmeasured until batch 2.

## 7. Batch 2: the integer pointer walk P14b008, and where the translation's extra PSP slots come from (2026-11-27/28)

STATUS 2026-11-28 00:20 UTC: done (7.1-7.5).  Asked by the port thread: ONE function, exact form, same check mode; the wanted
result is the table by class of the translated form's extra PSP slots (7.2).

SHORT ANSWER
- The translated walk runs 1.57 PSP slots a guest instruction (shipped form: about 5,550 slots a call for 3,537 guest
  instructions; 26.5 calls a rain-wait step).  The extra over the guest's own work is 61% ADDRESS FORMING and 27% THE CLOCK.
  64-bit halves, write-backs, entry/exit and call glue are together under 3% of this function, because 96% of it is one loop;
  they are most of the cost of the once-a-call part (226 slots for about 65 guest instructions).
- A generator change (no I/O test where proven, invariant bases formed once, the clock in a register) would take the loop from
  30 to about 21 slots an entry: about 1.4x on loop code like this [I].
- The hand-written loop is 6 slots an entry instead of 30 and exact (7.4): the whole function about 1,390 slots a call instead
  of about 5,550, about 4x, about -0.55 ms a rain-wait step on the instruction term [I from C parts].  But only 9 + 4 of the 24
  slots it saves an entry are the translation's; 11 are the GUEST'S OWN code (loop state kept in stack words by the original
  compiler), which no faithful translation removes.  So "native beats translated by 4x" here means "a rewrite beats the original
  game code", function by function, by hand.
- The rain wait's translated code is less flat than assumed: the top 50 functions hold 74% (7.5).

### 7.1 Method

1. The translated function compiled for the PSP with debug lines (psp.sh's gen.o flags plus -g): every PSP instruction is tied to
   the guest instruction it was generated for.  CAVEAT [C]: with -g this compiler lays the function out slightly differently
   (1,169 instructions against 1,158 in the form that ships; in the walk loop the debug form has one empty delay slot more, 31
   slots an entry against 30).  The table below is the debug object's; the shipped loop was walked by hand as the cross-check.
2. Execution counters on the twin (tools/n45_bc.py: one counter a guest instruction, one a taken branch, 302 in this function),
   read over the rain wait vb 29,500-30,700: 31,771 calls = 26.5 a step, 3,537 guest instructions a call, 96.4% of them in the
   walk loop 14b2e8-14b35c (19 guest instructions an entry, 180 entries a call, 98.2% of the entries do not match).
3. tools/psp_flow.py pushes a flow of 1.0 a call through the PSP code with the branch probabilities from 2 (flow_spec.txt), so
   every PSP instruction gets its executions a call; each instruction is put in a class by rule (below).  Check: the flow gives
   179.84 loop passes a call, the counters 179.84; total 5,726 slots a call (debug form) = about 5,550 in the shipped form; the
   cost model's figure was 5,608.

### 7.2 The table: PSP slots a call, translated form [C psp-gcc debug object, weights C twin]

| class | slots a call | share | in the walk loop (an entry) | once a call (entry + exit) | matched entries |
|---|---|---|---|---|---|
| the guest's own work (what a 1:1 form would also run) | 3,014 | 52.6% | 2,864 (16-17) | 89 | 61 |
| address forming | 1,666 | 29.1% | 1,618 (9) | 33 | 16 |
| clock / poll at loop back-edges | 730 | 12.7% | 708 (4) | 6 | 16 |
| empty delay slots | 182 | 3.2% | 180 (1; 0 in the shipped form) | 0 | 2 |
| host frame and spills (the C compiler's own prologue, epilogue, host-stack traffic) | 55 | 1.0% | 0 | 40 | 15 |
| 64-bit register halves (high words loaded, stored, sign-extended) | 41 | 0.7% | 0 | 29 | 12 |
| register write-backs to the register file (at calls and at the exit) | 24 | 0.4% | 0 | 15 | 9 |
| register loads from the file (at entry and after calls) | 15 | 0.3% | 0 | 14 | 1 |
| total | 5,726 | | 5,369 (30 an entry in the shipped form) | 226 | 131 |

Against 3,537 guest instructions a call: 1.62 PSP slots a guest instruction (1.57 in the shipped form).  The C compiler already
folds 15% of the guest's instructions away (3,014 "own work" slots for 3,537 guest instructions: the lui/ori/and of the 24-bit
mask becomes one ext, a reload of a word just stored is dropped).  THE EXTRA IS 2,712 SLOTS A CALL: address forming 61%, the
clock 27%, empty delay slots 7% (0 in the shipped form), everything else together 5%.

The nine address slots of one loop entry, by cause:
- 4 are the I/O-region test (`GEN_FAST`: "is the base in 0x1xxxxxxx"): srl + branch for the entry pointer, and for the invariant
  object pointer a reload of its spilled tag from the host stack + a branch, every pass.
- 4 are base adds (`addu x, $23, masked`): one for the entry pointer, one for the object pointer (invariant, formed again every
  pass), two for the guest stack pointer (the same value formed twice a pass: gen_hb's addu is an asm statement, so the compiler
  can neither merge nor hoist it).
- 1 is the mask of the entry pointer (ext 0,25); the masks of the two invariant bases are hoisted already.

Two different pictures in one function:
- IN A LOOP the extra is address forming and the clock: 14 of 30 slots an entry.
- ONCE A CALL (entry + exit, a function that saves ten registers) the extra is glue: 226 slots for about 65 guest instructions
  (3.5x): host frame 40, address forming 33, 64-bit halves 29 (each guest `sd`/`ld` of a saved register is four slots: low and
  high word through the register file), write-backs 15, register loads 14.  A call-heavy function with short bodies pays this
  profile, not the loop's.  The one call with a full write set in the matched path (P14aba8) costs about 55 slots of glue at the
  call site (13 registers written back, each with its sign-extended high word).

Static size [C]: the guest function is 230 instructions (plus inlined leaves); the PSP form is 1,158: 29 I/O slow-path blocks
(helper call + spills + rejoin) and 9 poll slow paths are in it.  None of the 29 ran in this window by the flow's assumption
[I: the I/O side of every region test was given probability 0; this is the pointer walk over game tables, but it was not counted].

### 7.3 What this says about a GENERATOR change [I unless marked]

What a generator could remove for every function, with this loop as the yardstick (30 slots an entry in the shipped form):
1. The I/O-region test where a site provably never sees an I/O address: -4 an entry (13%), and the 29 slow-path blocks leave the
   code (I-cache).  Needs a proof per site (the twin's check build over the route, or a rule "a base that is not a 0x1000xxxx
   constant is RAM" audited by GEN_MM_CHECK); an unproven site keeps the test.
2. Host pointers for invariant bases, formed once where the guest register is set (the stack pointer after the frame adjust, an
   object pointer that the loop never changes): -3 an entry here (10%).  Exact by construction.
3. The clock in a pinned host register, as the memory base is ($23): the poll becomes addiu + blez, -2 a back-edge (7%).
   Exact by construction; costs one more fixed register.
Together 30 -> about 21 slots an entry: about 1.4x on loop code of this kind, landing at 1.1 slots a guest instruction.
What NO generator change of this kind removes: the guest's own 16-17 slots.  This loop was compiled by the original compiler
with its counters in stack words, and a faithful translation keeps every load and store of them.

### 7.4 The hand-written loop: exact, checked, counted

Form (boot/native45.h `N45_HZX_FF`, bit 7 of g_n45; tools/n45_apply.py CLONES): a second copy of the translated function with a
hand-written block at the loop head L14b2f4; everything else in that copy is the translation's own text (entry, exit, the matched
entries with their four callees).  The block does n non-matching passes at once when [sp+8] and [sp+12] are 0: it compares the
low 24 bits of the entry's id word with the object's key word through host pointers, stops BEFORE the first match, the table's
end, or the first poll that would be due, then adds n to [sp+4], 64 n to the entry pointer and n polls to the clock.  Why this is
exact and not just equal in practice: a pass that does not match writes nothing but [sp+4] and the clock, and while no poll is
due nothing else can run between passes, so n passes at once leave the same state; the bound on n is the poll test itself solved
for the count (`N45_FREE`).  The temporaries the skipped passes would have left are set again by the text before any use or
write-back.  This shape was chosen over a whole-function rewrite because the matched path calls four translated functions whose
write-back sets would all have to be reproduced by hand for no measurable gain (2% of the function).

Proof [C twin]:
- Check mode (run chk4, bit 7 on): 1,526 calls compared with the WHOLE RAM, all registers and the clock, 1,526 equal, 0
  different.  This form calls translated functions and polls, so it cannot be run twice from declared regions as batch 1's
  leaves: it is compared every 100th call, only when no poll can be due inside the call (a poll that runs handlers cannot be
  put back; the runtime's own counter of slow polls and I/O stores proved none ran inside a compared call).  The other 151,071
  calls ran the native form alone.
- Whole route with all eight forms on (run on3, MGS2_N45=ff): the act log's 27,686 passes identical to the reference (md5
  c153c8e8678c), 685 hash lines equal (ee, iop, gs, vfb), 37,831 picture lines equal.  152,597 calls of the walk, 13,584,330
  entries skipped by 152,591 scans, 0 scans cut short by a poll, 0 refused.

PSP count [C psp-gcc for the parts, I for the sum]:
- the scan loop: 6 slots an entry (beq, an empty delay slot, lw, sll, bnel, addiu); hand assembly would make it 4;
- the block around it: 24 slots of guard + 9 of setup + about 15 to leave = about 48 a scan, one scan a call; a loop-head visit
  with [sp+8] set costs 8 more (2.2 a call); the wrapper costs about 15 a call;
- a call in the rain wait: 176.8 skipped entries x 6 + 48 + 18 + 15 = about 1,140 in place of 176.8 x 30 = 5,300 translated;
  the rest of the function (about 245 slots) is unchanged.  About 1,390 slots a call against about 5,550: 4.0x.
- at 26.5 calls a step and 4.95 ns a slot: about -0.55 ms a rain-wait step [I; instruction term only, no cache terms, not
  measured on the PSP].  For scale: batch 1's seven maths functions together were also about -0.55 ms.

Where the 24 slots an entry went:

| | translated (shipped) | hand-written | who could remove it |
|---|---|---|---|
| guest's own work | 17 | 4 (+2 loop overhead) | only a rewrite: the original keeps i, n and two flags in stack words and reloads the key every pass |
| address forming | 9 | 0 (two host pointers formed once a scan) | a generator, in part (7.3: -7 of 9) |
| clock | 4 | 0 (n polls added once) | a generator, in part (-2 of 4 with the clock in a register; all 4 only where a loop body is proven free of stores and calls) |

### 7.5 How flat the rain wait's translated code is [C twin, x86 instructions; read with the caveats]

From the callgrind profile of batch 1 (vb 29,500-30,700, 1,200 steps), self cost of every translated guest function that ran
(910 functions, 3.90 M x86 instructions a step together):

| the hottest | share of the translated code's instructions |
|---|---|
| 10 functions | 38.1% |
| 50 | 73.6% |
| 100 | 87.9% |
| 200 | 95.3% |
| 400 | 98.8% |

So it is less flat than "a hot 200": fifty functions hold three quarters.  Caveats: (1) x86 instructions of the -O1 twin, not PSP
slots; (2) the VU0 arithmetic of a function is counted in the twin's helper routines, not in the function, so the vector maths
functions weigh less here than on the PSP; (3) this is ALL translated guest code of the window, logic and draw preparation
together (the profile's logic-only switch did not isolate the logic, see section 1); (4) leaves that the translator inlines are
counted in their callers.

## 8. Files here

`native45.h`, `native45.c` (boot/), `n45_apply.py`, `psp_path.py` (tools/), `build45.sh` (the twin's three binaries), `run45.sh`,
`cmp45.py`, `psp_cc45.sh` (gen45.o for the PSP, for counts).  boot_x86.c's one added line, in state_hash before the hash line:
`{ void n45_mark(long vb) __attribute__((weak)); if (n45_mark) n45_mark(k_vblanks); }`.
Batch 2: `n45_bc.py` (execution counters in one translated function; build the counted header with -DN45_BCNT), `n45_gcount.py`
(guest instructions a call from the counters), `psp_flow.py` (weighted PSP slots by class from a debug-line listing and a branch
probability file), `flow_spec.txt` (P14b008's probabilities, rain wait).  The check run: `run45.sh chk4 boot45c 80
MGS2_N45_EVERY=100`; the whole route: `run45.sh on3 boot45 ff`.
