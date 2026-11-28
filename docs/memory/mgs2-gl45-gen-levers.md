---
name: mgs2-gl45-gen-levers
description: 2026-11-28 gl45 batch 3: the three generator levers (no I/O test behind an entry guard, host pointers for bases, clock in a local) built as a post-pass, proven exact on the twin, sized on psp-gcc: about 0.65 ms a rain step at best
metadata:
  type: project
---

Asked by the MGS2 port lead after [[mgs2-gl45-native-ab]] batch 2 ("the GENERATOR is the lever"); delivered 2026-11-28 05:30 UTC in mgs2/main/attic/gl45_gen/ (gl45_gen.md FINAL, gen_lev.h, tools/gen_levers.py, out/sel_final.json, out/per_function.tsv, sizing chain). Work dir /home/claude/g45 (container, temporary).

Levers as built (post-pass over the generated headers + boot/gen_lev.h, active with -DGEN_LEV; each its own switch, per-function selection):
- (c) clock in a local, written back / read again around every call, slow path and return.
- (b1) host pointer of the guest stack, (b2) host pointer per base register (re-formed after each assignment).
- (a) entry guard: the translation's own GEN_FAST test on a base that cannot change in the body, moved to the entry; function becomes NAME_s (untouched, cold), NAME_f (no tests), NAME (wrapper). Guard false = untouched translation runs. No profile trusted.

Results:
- Exact [C twin]: 11 whole-route runs equal the reference (27,686 passes with seed/time, 685 hash lines, 37,831 picture lines): each lever alone, all three, K_COUNTDOWN form, forced fallback (every third guarded call), three per-function mixes.
- Size [C psp-gcc static, no profile, C twin weights], 200 hottest functions, rain wait: today 2,786,543 PSP slots a step (ELF 1.37 M, deck 1.41 M; 4.3 to 4.6 slots a guest line). Alone everywhere: (c) -0.7%, (b1) -0.5%, (b2) -0.8%, (a) -1.3%; all three everywhere -3.6%; best mix (144 functions) -4.7% = 131 k slots = about 0.65 ms of the 34.4 ms logic step [I, 4.95 ns a slot]. The pointer walk P14b008 (-20%) was the best case. Levers reinforce each other in pointer-heavy functions (single-lever results undersell).
- Not reachable by an exact lever: 89 k accesses a step through pointers that change inside their function; their I/O test = about 179 k slots = 0.9 ms [I].
- Code: (c) everywhere +3% code (hot list only); (b) neutral; (a) shrinks hot code but adds a cold copy: the mix adds 79 kB (ELF) and 305 kB (deck unit: 38 kB left under the 0x460000 arena before data, too tight); lean (a) list (ELF 10 + deck 20 functions) keeps 87% of (a) for 70 kB.
- Build: ELF + w00a units change; PGO and layout must be redone (converted functions lose their profile silently under the PUSE flags); a real version belongs in the generator. Not run on PSP: asm gen_hb form, G0 guard path, the PSP's rt_poll_slow, host stack margin.

Method lessons (mine):
- psp-gcc -g line tables + twin counters + a small linear program per function give PSP slots a step; ALWAYS cross-check slots a guest line per unit (4.3 to 4.6 here). A `#line` mark left inside one deck function hid the line data of 49 functions and made a first sizing wrong (deck 835 k instead of 1,413 k, (a) three times too good); notes before 05:00 UTC carried those figures.
- cc1 for the deck unit needs ~3 GB: at most 3 heavy psp-gcc compiles at once in the 16 GB container.
