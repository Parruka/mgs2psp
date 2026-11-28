# gl45 study, batch 3: three generator levers as a sized prototype (2026-11-28)

Asked by the port thread after batch 2 (attic/gl45_native/gl45_native.md section 7): the hand-native route is closed, the
generator is the lever.  Wanted: the three levers of 7.3, each behind its own switch and exact by construction; sized over the
200 hottest translated functions of the rain wait; the whole route equal on the twin with all three on; what it means for the
build.  Scope: these three levers only, no hand-native forms, no PSP build.  Marks: [C twin], [C psp-gcc], [I], UNKNOWN.

STATUS 2026-11-28 05:27 UTC: FINAL.  All three levers are exact (section 4: eleven whole-route twin runs equal).  Sized over the
200 hottest functions (section 3): together they take 3.6 to 4.7% off their PSP instructions, about 0.50 to 0.65 ms of the
34.4 ms rain-wait logic step [I, the cost model's instruction term]; the chosen mix is out/sel_final.json.  A first sizing of
the deck unit was wrong and is corrected here (section 2, point 6): versions of this file from before 05:00 UTC said 2.21 M
slots and -5%, and gave (a) 62 k slots; those deck figures are void.

## 1. The three levers as built here (a post-pass over the generated headers, tools/gen_levers.py, plus boot/gen_lev.h)

The prototype does not change the recompiler itself: like tools/patch_half.py and the poll and stack passes before it, it rewrites
the text of functions in the generated headers and redefines a few access macros (boot/gen_lev.h, included at the end of both
gen_pre.h files, active only with -DGEN_LEV).  The switches: the pass's third argument is the set of levers (a, b, c in any
combination; b1 and b2 separately through the selection), its fourth says where (all, or a per-function list).  Which functions
and registers a lever is applied to is a speed choice; it never decides exactness.  A function is left EXACTLY as it was when its
text holds any call-like word outside a fixed list (native hooks, the VU0 control-register moves, the QCOPY idiom, the
missing-code stubs), a return with a value, another poll form, a preprocessor line, or when it is a G0 body (its stores call the
runtime's guard; left alone also so that those bodies keep their profile).

(c) THE CLOCK IN A LOCAL.  A converted function opens with GEN_KC_ON: a local kl_ takes the countdown (PSP: k_left; twin:
k_time).  The loop polls count on kl_ with the translation's own test, at the same points.  kl_ is written back before, and read
again after, every way out of the function's own code: the poll's slow path, the slow (I/O) side of every memory access (an I/O
read may read the time, an I/O store may re-arm it), every call of a translated function, every indirect call, system call,
break and COP0 / hardware-register form; and written back before every return.  So everything outside the function sees the
value the translation would have stored, and the function goes on with whatever the callee left.  Not applied (the function
keeps its other levers): one that still has a plain, non-base-register access form, because that form's slow side sits in an
inline function the pass cannot bracket.

(b) HOST POINTERS FOR BASES.  (b1) the stack: the host address of the guest $sp is formed once at entry and again after every
assignment to $sp, instead of at every stack access (GEN_HS_ON; MMS then reads the local).  (b2) a base register gets a local
host pointer that is formed again after every assignment to the register (plain assignments, the conditional-move and reload
forms, and the entry load); its accesses use the _H forms, which keep the I/O test and the slow side.  Not applied to a register
that is assigned inside an expression the pass does not take apart, or when an access through it follows inside the assigning
statement.

(a) NO I/O TEST BEHIND AN ENTRY GUARD.  "Proven RAM" here is a run-time fact, not a profile.  A base register that holds an
argument, or one copy of an argument made in the function's first straight-line run, and that is otherwise assigned only by the
exit's restores (with no access through it after them), has ONE value for the whole body.  The function becomes three: NAME_s,
the translation as it was (noinline, cold); NAME_f, a copy in which the accesses through those registers have no test and no
slow side (always inlined); and NAME, a wrapper: if GEN_FAST(entry value) holds for every guarded register, run NAME_f, else
NAME_s.  The test is the translation's own test, moved to the entry; when it fails the untouched translation runs.  So a wrong
"proof" cannot happen, and nothing is assumed from the route.  What this does NOT cover: a base that changes inside the function
(a walking pointer) keeps its test at every access, because nothing short of the test proves the next value.  Batch 2's 7.3
counted 4 slots an entry for (a) in the walk loop; the exact form reaches the 2 that belong to the fixed base, not the 2 of the
walking one.  A profile ("this site never saw an I/O address on the route") would reach those too, but it could differ on a path
the route never took: not built.

Where each applies when asked for everywhere [C, the pass's own report on the PSP headers]:

| | ELF unit (2,303 functions) | deck unit w00a (6,657) |
|---|---|---|
| left as it was, whole | 1,115 (1,097 missing-code stubs, 14 with VU0 control-register moves, 2 G0 bodies, 2 hooks) | 4,800 (4,784 stubs, 3 G0 bodies, 13 with native hooks or idioms) |
| (c) clock local | 475 (694 more have no poll; 19 refused for a plain access form) | 502 (1,335 without a poll; 20 refused) |
| (b1) stack pointer | 815 | 1,485 |
| (b2) base pointers | 3,234 registers in 1,006 functions (596 registers refused) | 4,855 registers in 1,623 (618 refused) |
| (a) guarded copy | 349 functions, 440 guarded registers | 760 functions, 883 registers |

## 2. Method of the sizing [C psp-gcc static, weights C twin]

1. WEIGHTS.  The twin was run with a counter at every guest instruction and every taken branch of the 200 hottest translated
   functions (tools/g45_count.py; ranking: callgrind's self cost in the rain wait, batch 2's listing; 89 in the ELF, 111 in the
   deck unit; they hold 95% of the translated code's work).  Window: the rain wait, vb 29,500 to 30,700 (1,200 steps); 633 k
   counted guest lines a step.
2. PSP CODE.  Each variant's two headers are compiled by psp-gcc with scripts/psp.sh's flags for the generated units, WITHOUT a
   profile: once as shipped (sizes), once with -g from a copy with one guest instruction a line (tools/g45_split.py: white
   space only), so that every PSP instruction names the guest instruction, or the taken block, it was compiled from.
3. COUNTS OF THE PSP CODE (tools/g45_size.py).  A line's count is not its block's count: the compiler copies and moves code (a
   loop's test copied in front of the loop still carries the test's line; a slow side carries its access's line).  So each
   function's block counts are rebuilt as a flow through its branch graph: it enters with the function's calls a step, is
   conserved at every block, puts nothing on a slow side (poll slow path, I/O side of an access, the untouched copy, a VU0
   group's fallback, a break), and otherwise comes as close as it can, in summed absolute difference, to every instruction's
   line count and to the twin's taken count of every guest branch (a small linear program a function).
4. CHECKS of 3: the pointer walk P14b008 comes out at 5,735 slots a call against 5,726 from batch 2's hand-set flow (+0.2%);
   five maths leaves against batch 1's hand-walked paths: 88 / 90, 274 / 286, 239 / 253, 372 / 389 (-2 to -6%: a branch-likely's
   delay slot is counted only when taken here) and 301 / 247 for ecossin (whose true mean was unknown in batch 1 too).
5. LIMITS.  (i) No profile: the shipped build is profile-guided, so its block order, inlining and delay-slot fills differ; the
   figures say what a lever does to psp-gcc's code, not the shipped ms.  (ii) -g shifts a few instructions (about 1%).  (iii) A
   branch inside one guest instruction's code that is neither a guest branch nor a slow side (a clamp, a compare made in two
   steps) is split by the fit, which prefers the longer side; the same on both sides of a comparison.  (iv) A slot here is an
   executed instruction, as in the cost model; an annulled delay slot is not counted.  (v) The 200 are 95% of the translated
   code's work; the rest is not sized.  (vi) The VU0 groups' fallback is counted as never taken.
6. A FAULT FOUND AND FIXED (2026-11-28 05:00 UTC; every deck figure written into this file before that was wrong).  The deck
   header carries #line marks (the profile-guided build's file identity).  tools/g45_split.py dropped them between functions
   but not the one inside a hot body, so every deck function after it (49 of the 111 hot ones) reached the debug object under
   another file's line numbers, had no line data, and was sized as its SHORTEST PATH (flow conservation alone).  That
   understated the deck (835 k instead of 1,413 k slots a step) and made (a) look three times too good there (a guarded
   function's shortest path is its wrapper).  Fixed: the split tool drops the marks inside bodies too; the deck's debug objects
   were recompiled and sized again; tools/g45_size.py now warns when a function has no line data and counts a jump to the
   untouched copy as a slow side.  The ELF unit was not affected (its figures moved by under 0.01% with the second change).
   Cross-check after the fix: 4.26 PSP slots a guest line in the ELF unit, 4.56 in the deck unit (the faulty pass had 2.69
   in the deck, which should have been caught earlier).

## 3. Results [C psp-gcc static without a profile, weights C twin; ms figures I]

SHORT ANSWER.  The three levers are exact and they work, but over the whole logic they are small: together they take
3.6 to 4.7% off the PSP instructions of the 200 hottest functions (101.1 k to 131.0 k of 2.79 M slots a rain-wait step), which is
about 0.50 to 0.65 ms of the 34.4 ms logic step by the cost model's instruction term (4.95 ns a slot) [I].  The pointer walk of
batch 2 was the best case (-20% with all three), not the typical one: most of the hot code is VU0 maths and call-heavy actor code,
where there is little address forming or polling to remove.  For scale, by the same term the 200 functions' 2.79 M slots are
13.8 ms of the step [I].  The levers help each other: in the walk the three alone give 169, 358 and 6 slots a
call ((c), (b2), (a)), together 1096; so a list made from single-lever results undersells them there (3.5).

### 3.1 PSP slots a rain-wait step, the 200 hottest functions (89 ELF + 111 deck)

Today's code: 2,786,543 slots a step (ELF unit 1,373,600, deck unit 1,412,943).  "Macro layer only" is today's headers compiled
with -DGEN_LEV and no lever applied (section 5): its distance from today's code, 0.11%, is the noise floor of this table.
"Where it pays" keeps a variant only in the functions whose count it lowers by at least 0.3% and 20 slots a step.

| variant | slots a step | against today | against the macro layer only | where it pays (against the macro layer) | functions lowered / raised |
|---|---|---|---|---|---|
| macro layer only | 2,783,467 | -0.11% | | | 5 / 25 |
| (c) clock local, everywhere | 2,762,935 | -0.85% | -20.5 k = -0.74% | -33.7 k = -1.21% in 61 | 61 / 43 |
| (b1) stack pointer, everywhere | 2,770,159 | -0.59% | -13.3 k = -0.48% | -15.0 k = -0.54% in 64 | 64 / 16 |
| (b2) base pointers, every register | 2,762,352 | -0.87% | -21.1 k = -0.76% | -32.0 k = -1.15% in 65 | 65 / 41 |
| (b2) base pointers, chosen registers | 2,760,187 | -0.95% | -23.3 k = -0.84% | -26.3 k = -0.94% in 69 | 69 / 11 |
| (a) guarded copy, everywhere | 2,746,880 | -1.42% | -36.6 k = -1.31% | -37.1 k = -1.33% in 55 | 55 / 4 |
| a + b + c, everywhere | 2,682,385 | -3.74% | -101.1 k = -3.63% | -113.8 k = -4.09% in 121 | 121 / 37 |
| mix 1: each lever where it pays alone | 2,678,089 | -3.89% | -105.4 k = -3.79% | -105.4 k = -3.79% in 139 | 139 / 1 |
| THE CHOSEN MIX (mix 1, and all three where they beat it) | 2,652,503 | -4.81% | -131.0 k = -4.71% | -131.0 k = -4.71% in 143 | 143 / 1 |

By unit against the macro layer: a + b + c everywhere ELF -64.5 k (-4.7%), deck -36.6 k (-2.6%); the chosen mix ELF -78.7 k (-5.7%),
deck -52.3 k (-3.7%).  The deck's hot code gains less: fewer accesses through a fixed base (5,794 a step against 15,424 in the ELF)
and more calls out (6,561 against 2,373), where the clock local pays its gain back.
In ms [I]: 101.1 k to 131.0 k slots x 4.95 ns = 0.50 to 0.65 ms a rain-wait step, before any change in cache misses (UNKNOWN: the hot
code gets a little smaller, the order must be searched again; only a PSP run settles it).

"Chosen registers" for (b2) is tools/g45_b2sel.py's rule from the twin's counters: a register whose accesses executed exceed
1.25 times its assignments executed (232 registers in 120 functions).  out/per_function.tsv has the 200 rows (calls, slots under
every variant, bytes, the levers the chosen mix keeps).

### 3.2 The ceilings, from what the sites execute (tools/g45_sites.py) [C twin]

A step of the rain wait, the 200 functions: 632,791 guest lines; 8,934 calls out.
- (c) 23,297 polls executed.  Two slots each (the load and the store of the countdown) = 46.6 k at most; the function-wide
  form pays about 16.9 k back at the calls and returns of the functions that poll (11.5 k of it in the deck unit).  Measured:
  20.5 k everywhere, 33.7 k where it pays.  A form that holds the clock only inside call-free loops would get nearer the
  46.6 k; the difference is about 0.1 ms and was not built.
- (b1) 64,199 stack accesses, 10,160 assignments to $sp: 54.0 k at most.  Measured only 13.3 k to 15.0 k: inside one block
  psp-gcc ALREADY shares the add between accesses (the asm form is merged like any expression there); the lever only saves
  across blocks.
- (b2) 110,515 accesses through a base register; through the chosen registers 69,830 against 15,818 assignments.  Measured
  23.3 k (chosen registers, everywhere) to 32.0 k (every register, where it pays), for the same reason as (b1).
- (a) 21,219 of those accesses go through a guarded (fixed) base: 2 slots each = 42.4 k expected; measured 36.6 k (3.3).
- NOT REACHED BY ANY EXACT LEVER: the other 89,296 accesses a step go through a pointer that changes inside its function.
  Their I/O test is 2 slots each = about 179 k slots a step = 0.9 ms [I], more than everything in the table together.  Only a
  proof that is not a run-time test (a profile, or an argument about where the game's pointers can point) removes it; that
  is the "could differ" class the brief excluded.  It is the one large item left in address forming.

### 3.3 Lever (a): what its sites promise, in few functions

Measured 36.6 k against the 42.4 k of the plain count (ELF 23.4 k against 30.8 k, deck 13.2 k against 11.6 k).  Two effects
beside the plain count: where
the guarded accesses were a function's only calls into the runtime, the copy behind the guard is a leaf for the compiler (no
return-address save, fewer saved registers): the quaternion normalise P11cd68 (720 calls a step) goes from 88 to 72 slots
a call although only 8 of its slots were tests; elsewhere psp-gcc had already shared a test between two accesses of one block.
(a) alone also shrinks the hot code (3.4).
The gain is concentrated.  By gain a byte of untouched copy: in the ELF unit 5 functions give 84% of it for 8.7 kB of
copies and 10 give 91% for 18 kB (all 20: 73 kB); in the deck unit 20 functions give 79% for 52 kB (all 35: 282 kB).  A
LEAN LIST for (a), ELF 10 + deck 20, keeps 32.1 k of the 37.1 k slots for 70 kB of copies instead of 355 kB [I: sums
of the per-function results of (a) alone; not built as its own variant].

### 3.4 Code size [C psp-gcc static, the shipping flags, no profile]

Whole units, a lever applied everywhere, and the two mixes:

| | ELF unit (2,430,428 bytes) | deck unit (4,258,972 bytes) |
|---|---|---|
| macro layer only | -1,648 | +4,888 |
| (c) | +72,996 (+3.0%) | +120,220 (+2.8%) |
| (b1) | -5,232 | +1,940 |
| (b2) every register | +28,548 (+1.2%) | +45,868 (+1.1%) |
| (b2) chosen registers of the 200 only | -1,752 | +1,856 |
| (a) | hot and normal sections -142,404, plus 602,324 of untouched copies | -266,220, plus 1,612,656 of untouched copies |
| a + b + c | -52,088, plus 602,324 | -145,220, plus 1,612,656 |
| mix 1 | +64,116 (hot and normal sections -8,296, plus 72,412 of untouched copies) | +263,444 (hot and normal sections -15,528, plus 278,972 of untouched copies) |
| the chosen mix | +69,896 (hot and normal sections -9,420, plus 79,316 of untouched copies) | +290,744 (hot and normal sections -13,804, plus 304,548 of untouched copies) |

The 200 hot functions only (their own sections, 1,176 kB today: ELF 368, deck 809), against the macro layer: (c) +39.7 k
everywhere; (b1) -3.4 k; (b2) chosen registers -3.6 k; (a) -44.6 k of hot code and 435 k of untouched
copies; a + b + c -10.1 k and 435 k; mix 1 -28.5 k and 355 k; the chosen mix -28.7 k and 388 k.

Reading: (c) everywhere GROWS code by 3% for a 0.7% gain: only by a hot list.  (b) is neutral in size.  (a) makes the hot
code smaller (good for the I-cache) but doubles each converted function in RAM (the copy is cold code, placed away from the
hot area); everywhere it cannot fit (section 5), by a hot list it is tens to a few hundred kB, by the lean list 70 kB.

### 3.5 The chosen mix (out/sel_final.json)

Mix 1 (out/sel_singles.json, 139 functions) keeps a lever in a function when, alone, it lowers that function's count by at
least 0.3% and 20 slots a step against the macro layer (tools/g45_table.py -sel; (b2) on tools/g45_b2sel.py's registers).
Built and sized: 2,678,089 slots a step, -105.4 k (-3.79%).  It undersells the levers where they help each other, so the
CHOSEN MIX takes all three (every base register) in the 26 functions where the a + b + c build beats mix 1 by 20 slots a step
or more, and mix 1's choice elsewhere: 144 of the 200 functions; ELF 56 (clock local in 27, stack pointer in 33, 63 base
registers, 23 guarded), deck 88 (40, 44, 173, 40).

Built for the PSP and sized: 2,652,503 slots a step, -131.0 k against the macro layer (-4.71%), -134.0 k against today's
code (-4.81%); functions raised by 20 slots or more: 1.  In ms [I]: 0.65 to 0.66 a rain-wait step.  Proven equal on the
twin over the whole route (section 4, run 11).  Code: the 200's own sections -28.7 kB (the hot area gets a little
smaller), untouched copies +79 kB in the ELF unit and +305 kB in the deck unit (cold code).  The deck unit would
be 4,549,716 bytes of code in an arena of 4,587,520 (38 kB under it before the unit's data; today 329 kB): whether it still
fits is UNKNOWN here and looks too tight.  Keeping the guard only in the lean list's 20 deck functions (3.3) brings the deck's
copies to about 52 kB, for about 3 k slots less by the single-lever figures [I].  A generator-side form that guards a function only when
the guard pays is the same thing done properly.

## 4. Exactness on the twin

Run 1 [C twin x86], 2026-11-28 00:26 UTC: the twin built from the x86 headers after the pass in its widest mode (levers a + b + c,
every function not refused, every base register), compiled with -DGEN_LEV, route i from boot to vb 37,800 with batch 1's settings,
compared with the same reference run as batches 1 and 2:

- 27,686 actor passes identical, with the seed and time hash on every line (md5 of the pass lines c153c8e8678c on both sides);
- 685 hash lines equal (EE, IOP, the rest of the line); 37,831 picture lines equal.

What the widest mode touched: ELF unit 1,126 functions changed (clock local in 476, stack host pointer in 816, 3,238 base-register
host pointers, 350 functions behind an entry guard); deck unit 1,798 changed (503, 1,487, 4,867, 761).  The twin ran 910
translated functions on this route, so most of what ran was a converted form.

Runs 2 to 7 [C twin x86], 00:35 to 00:49 UTC, the pass in its final form (the G0 bodies left alone, the wrapper's guard
through GEN_GUARD), each the whole route against the same reference, each with the same result as run 1 (27,686 passes
identical with seed and time, 685 hash lines equal, 37,831 picture lines equal):

- lever a alone, lever b alone, lever c alone (each its own generator switch: the pass's third argument), and a + b + c;
- the twin built with the PSP's clock form (-DK_COUNTDOWN: the 32-bit countdown k_left that the PSP build uses, every unit
  rebuilt with it) without the levers: equal to the reference, so that twin is a valid base;
- that twin with a + b + c: equal.  This is lever (c) in the form the PSP would run (kl_ = k_left kept in a local).

Run 8 [C twin x86], 00:57 UTC: the fallback.  The twin of a + b + c built with -DGEN_LEV_FBTEST=3, which sends every third
guarded call whose guard holds to the untouched copy all the same: whole route equal again (same three counts).  Over the route:
38.49 M guarded calls whose guard held, 12.83 M of them forced to the untouched copy, and 1.08 M calls whose guard was false by
itself (2.7%: a base in the register space; those run the untouched translation, as designed).  So the two copies are
interchangeable call by call, and the fallback is exercised by the real route too.

Run 9 [C twin x86], 04:44 UTC: a first per-function mix (123 functions, chosen before the fault of section 2 point 6 was
found; kept as out/sel_first.json for the record only): whole route equal (same three counts).

Run 10 [C twin x86], 05:11 UTC: mix 1 (out/sel_singles.json, 139 functions, each with its own set of levers): whole route equal.

Run 11 [C twin x86], 05:23 UTC: the chosen mix (out/sel_final.json, 144 functions, all three levers in 26 of them): whole route
equal: 27,686 passes identical with seed and time (md5 of the pass lines c153c8e8678c on both sides), 685 hash lines equal,
37,831 picture lines equal.  So per-function mixtures are exact too, not only the all-on and one-lever builds.

No lever was found not exact in any of the eleven runs.

## 5. What it means for the build

WHICH UNITS CHANGE.  The pass works a header at a time.  Applied everywhere it touches every generated unit (the ELF and each
stage); applied by a hot list it touches only the units that hold listed functions: here the ELF unit and the deck unit w00a
(the list is the rain wait's).  Indoors (w01a) and the cutscenes need their own lists from their own windows.  The two gen_pre.h
files (boot/ and boot_g0/) gain one include line; boot/gen_lev.h is the only new source file.  Without -DGEN_LEV nothing
changes (the include is empty).

A PROTOTYPE ARTEFACT TO KNOW.  With -DGEN_LEV the access macros are re-expressed for every function of the unit, also where no
lever is applied.  The meaning is the same (the twin runs prove it), but psp-gcc then emits slightly different code for some
untouched functions: ELF unit -1,648 bytes (117 functions larger, 69 smaller of 2,303), deck unit +4,888 bytes (375 larger, 149
smaller) [C psp-gcc static].  A generator-side implementation should emit the new forms only in the functions it converts
(new macro names, the old ones untouched), so that the other nine thousand functions compile byte for byte as today.  In the
tables of section 3 the levers are measured against this "macro layer only" build, and its own distance from today's build is
given beside it.

PGO AND CODE ORDER MUST BE REDONE.  (1) A converted function has other code (and a guarded one is three functions), so its old
profile does not match: with psp.sh's PUSE flags (-Wno-error=coverage-mismatch, partial training) the build would go through and
compile exactly the hot functions WITHOUT a profile, silently worse.  A PGO=gen build of the new headers, a training run and a
PGO=use build are owed, as for any generator change.  (2) The pass inserts lines; the headers' #line marks keep later functions'
numbers only if the insertion is made by the tool that writes those marks, so a generator-side implementation is cleaner here
too.  (3) Function sizes change, so layout_elf.ld / layout_w00a.ld are stale: the names still match (a guarded function keeps
its name and its section; the untouched copy is NAME_s in .text.unlikely.NAME_s, which psp/ovl.ld's `*(.text .text.*)` and the
ELF's default rule place after the listed sections, out of the hot area), but the conflict pattern in the I-cache is new: trace
and order search again.

MEMORY.  The deck overlay's code is 4.26 MB in an arena of 0x460000 (4.59 MB).  Lever (a) everywhere would add 1.6 MB of
untouched copies to that unit and 0.6 MB to the ELF: it does not fit and is not wanted.  (a) is only for a hot list: the
chosen mix's copies are 79 kB (ELF) and 305 kB (deck), the lean list's about 18 and 52 kB (3.3, 3.5).  (c) everywhere adds
120 kB to the deck unit and 73 kB to the ELF (the write-back and read-again around calls); by a hot list it is small.

WHAT I COULD NOT CHECK HERE (PSP-only paths; all COMPILED by psp-gcc, none RUN):
- gen_hb as the PSP's asm form (`addu` with the pinned $23, GEN_PIN23 + GEN_HB_SP): the host-pointer locals of (b) are that
  same expression evaluated earlier; on the twin gen_hb is the C form.  psp-gcc keeps the asm form where the pass put it (it
  cannot merge or move it), so the PSP code does what the text says, but it has not executed.
- The G0 guards (boot_g0/gen_pre.h): G0 bodies are left whole; in every other function the guard is the constant 0, and the
  store forms of gen_lev.h keep the guard call in front of the store as gen_pre.h has it.  Not run (the twin builds from boot/).
- The countdown clock IS covered on the twin (run 6 and 7 of section 4 use -DK_COUNTDOWN), but with the twin's rt_poll_slow
  (boot_x86.c) and not the PSP's (boot.c): both re-arm k_left inside the slow path, which is bracketed by write-back and
  read-again either way.
- GEN_FAST has one form on both sides ((address >> 28) != 1); there is no PSP-only variant of the test in gen_pre.h.
- Host stack: kl_, hs_ and the base pointers are extra locals; where the compiler spills them the host frame grows by a few
  words.  The margin of the host stack pools (K_HSTACK, IOP_HSTACK) with the levers on is UNKNOWN until a PSP run prints it.
- Guest thread switches and interrupt handlers happen inside the poll's slow path and inside system calls: the clock local is
  written back before and read after both, and the host pointers are plain values of registers those paths do not change for
  the function (the translation's own flush-only rule).  Covered on the twin by the whole route; the PSP's own switch code
  (HOST_STACK_POOL) is not exercised here.
- The profile-guided build itself: every PSP figure here is from a compile without a profile.

## 6. Files in this folder

- gen_lev.h: the macro layer (goes to boot/, included at the end of both gen_pre.h files; active with -DGEN_LEV).
- tools/gen_levers.py (+ glib.py): the pass.  `gen_levers.py <in.h> <out.h> <levers: a, b, c in any combination> <where: all |
  all-b1 | all-b2 | selection.json> <report.json>`.
- out/sel_final.json: the chosen mix (function -> c, b1, b2 registers or "all", a).  out/sel_singles.json: mix 1.
  out/sel_first.json: the superseded first mix (record only).  out/sel_b2s.json: the chosen registers of (b2).
- out/per_function.tsv: the 200 functions under every variant.  out/size_<variant>_<unit>.json: tools/g45_size.py's results
  (base = today's code, baseL = macro layer only, sel = mix 1, mix = the chosen mix).
- The sizing chain: tools/g45_count.py + g45cnt.c (twin counters), g45_split.py (one guest instruction a line), psp_var.sh
  (the PSP compiles of a variant), g45_size.py (block counts and slots), g45_table.py, g45_sites.py, g45_bytes.py,
  g45_b2sel.py, g45_perfn.py, g45_rows.py; resplit.sh, redo3.sh, sizew.sh are the batch scripts as run.
- The twin runs: buildx.sh, runx.sh, batch_x.sh (runs 2 to 5 and 8), buildk.sh (runs 6 and 7), selx.sh / selx2.sh (runs 9 to
  11), g45fb.c (the fallback test's counters).
No generated header, object or listing is kept here: they are rebuilt from the tree by the scripts.
