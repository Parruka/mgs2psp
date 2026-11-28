# gl43 results (2026-11-27): rain option "B" on the game side, in turns

Package: /home/claude/main/build_gl43/deliver (15 files). NOT delivered.

| File | md5 |
|---|---|
| EBOOT.PBP | 318caa49 |
| mgs2.cfg | 533aa271 |
| hashref.txt | 58cdec28 |
| pad.txt | 7bc72ace |
| rd43pc.py | 24875760 |
| README.txt | 32ff6681 |
| MANIFEST.md5 | 2bf39121 |

A copy of README.txt and score43_final.txt is kept here.

## Form chosen and why

**The exact form** (rain_options s5.3), not DICE + NP: DICE alone moves the guards' random numbers on the aft deck (s5.2).

Each shadowed call:
- advances the seed 0x1fc954 by the translation's own draw count;
- keeps the slots' lives and counters, the spawn's 3 copies and the matrix call;
- adds the translation's polls × 64 to the clock, but only when no interrupt falls inside the call; otherwise the translation runs.

So the guards' draws, the interrupt positions and the route are the PS2's, and B can switch per turn.

The four functions are 0x318418, 0x317e88, 0x319600 and 0x319078. The prims hidden are floor, fall and ripple. In the same turns the picture runs bound 4 with MGS2_GL_RAIN_NEAR=1.

## Gates

### x86 twin [C twin]

**off** (B never on):
- = hashref_gl42, byte for byte (685/685).
- Digests and content = gl42's.

**chk** (every B call translated and compared with the prediction):

| Call | Compared |
|---|---|
| floor spawn | 964 |
| floor update | 11,036 |
| fall update | 9,560 |
| fall dead | 45,704 |

0 different in time, seed, lives and copies.

**Seed log:** identical in off, chk, sch, bon (B every turn from 29,000) and pgo0. It is logged at every act, including the guards:

| Part | Lines | md5 |
|---|---|---|
| sv | 12,699 | b27073b998d1 |
| sd | 65,070 | 99a5a9c90b85 |

**Route and IOP:**
- Same stage loads and same code path: the st lines are equal.
- IOP and vfb are equal at 684/684 points.

**RAM:**
- EE dumps, 6 pairs. The differences lie in:
  - the 5 rain actors and their prim buffers;
  - the scratchpad work area;
  - packet and heap areas.
- Read hash (GEN_DG_TRACE twin of every unit), loads outside the draw pipeline: 37,795 of 37,799 vblanks equal.
- The 4 others are vb 30,928/929/932/933 (t69). They hold 16 loads, all of one 8-byte GLDU copy of scratchpad 0x70003f88:
  - its low word was just written by S_w00a_P2e59b0;
  - its high word 0x70003f8c is a leftover of the translated splash code (0 with B).
- No later load sees a different value.

**Pictures:**
- After the deck: 3,072 frames, content and digests = off's.
- In the deck, content differs only in the B digest frames.

### Emulator, ME loop apart [C emu]

**n43** (no PGO):
- Hashes: 62/62 = hashref_gl43.
- 61 digests; content = the twin's sch in 61/61.
- b43 lines = the twin's.
- Heap: 234 KB.

**Final psp** (pk_f43):

| Run | Hashes |
|---|---|
| f2 | 62/62 |
| f3 | 62/62 |
| fh1 | 62/62 |
| fh0 | 62/62 (= hashref_gl38: nothing native and B off) |

- f2 and f3 against the twin: content 0 differ (t73's 11 B frames included).
- XP check: f3 114,608 plans, 0 DIFFERING.
- Textures refused 0; 'no thread' 0.
- **Heap (worst moment, vb 536):** 103 KB above the break + 34 KB free top = 137 KB. Gate ≥ 32 KB: **PASS**.

**fh1 against fh0:**
- K-B, K7B and K7 frames: list and content 0 differ. K-N1, K-N2R and K-N2B: content 0 differ.
- The frames right after each B turn differ in content: K-A 38, K-N2 15 (the first 6 to 16 frames of the next turn), plus 1 at vb 31,062. These are the B turn's splashes still hidden.

### Build [C]

- 3 LOAD segments.
- 133/133 imports = gl42's final; stub sections equal.
- mesec, me_call, xme_me and xme_rest checks PASS.
- PGO: 5 trainings, each exact:

| Trainings | Reference |
|---|---|
| rtb, pic | hashref_gl43pgo |
| game, w00a, w01a | hashref_gl43pgo0 |

- pgochk: no lost profile lines.
- Code order: s2 from a fresh search seeded with gl42's order. Every guard window is under its cap (-6.6 to -17.4 %). Real addresses = simulated.
- The final link is byte-equal twice. The pads put every data group at gl42's offsets mod 0x2000, and the image end at 0b24f900.

## Model [C model] and PSP expectation [I]

All figures are ms a PSP step.

| Comparison | Model main | Model ME | PSP [I] |
|---|---|---|---|
| K-B − K-A (7 pairs) | mean -6.57 (game -1.15); pairs -2.48 to -10.45 | -10.42 | main -5.7 (range -2 to -9) + bus |
| K-N2B − K-N2 (5 pairs) | mean -5.35; wait -5.73 to -6.10, walk -3.09 | -8.90 | -4.7 (range -2.5 to -5.5) + bus |
| K7B − K7 (B's logic alone) | -2.55 / -7.42 (mean -4.99) | 0 | -4.3 (range -2.2 to -6.5) |

**Bus: UNKNOWN.** The model has no main/ME bus contention. Taken as 0 to -2 ms on main. Basis: the ME does ~9 ms a step less (~13 % less traffic); 5-15 % of main's 73 ms is assumed lost to the bus.

**Expected step [I]:**
- K-B ~70 against K-A ~75.7 (wait).
- K-N2B ~69 against K-N2 ~74.

**No lever (gl43 − gl42):**
- K-A walk: -0.42 / -0.57.
- Guards: +0.02 to +0.28.

**Guard I-misses vs gl36:** -2.2 / -15.5 / -5.8 / -4.2 %: **PASS**.

## Issues

- gl38's guard cfg has OPT2 fffff9df, i.e. bit 21 set, which turns B on in gl43. The guard trace and model were redone with cfg_emu/mgs2_m37_rc0_b43off.cfg. Any old cfg with bit 21 set turns B on (noted in the README).
- The turn lines start at 24,001 + 100t, as in gl42, so the last turn's b43 line falls after the run's end (the reader allows for it).
