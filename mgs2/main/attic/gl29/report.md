# gl29 build report (2026-11-20)

Built by the build agent from `mgs2/main/attic/gl29/gl29_plan.md`. The thread lead saved this report and made the changes listed under "After the build".

## Build
- Tree: `/home/claude/t29`.

| file | md5 |
|---|---|
| ge/gelist.c | 9192694d |
| ge/gelist.h | 0101dfaf |
| ge/xme.c | e4dc5e48 |
| ge/xme.h | 4b7bbdb6 |
| psp/xl_psp.c | 90ee3f3c |
| twin `m29/boot/xlive_chk.c` | 813323c2 |
| twin binary `x86/boot` | 28915af8 |

- Recipe: `gl29_apply.py` (317bfd38, retested on fresh copies: 7/7 files equal), `relink29.sh`, `tw29.sh`, `em29.sh`.
- relink29.sh preprocesses ge_gelist, ge_gelive and ge_xconv from t29 and relabels their line markers to `/home/claude/main/ge/`, so the gcda3 profiles still match. ge_gelive.o and ge_xconv.o come out byte-equal to gl23's [C].
- EBOOT `0580695e45de68d1409d32ef504e557e`, titled "MGS2 harness gl29 shadow depth test".
- Overlays:

| overlay | md5 |
|---|---|
| d00t | 1f489ef4 |
| d01t | 4125aad4 |
| init | 1649c401 |
| n_title | d4a6c43c |
| r_title | 142aeaac |
| r_tnk0 | f31fa759 |
| w00a | 8c15924a |
| w01a | 6f825b7f |

  They differ from gl28's because the main image moved [I]. With t28, the same recipe gives gl28's EBOOT ee11e43f.
- gelist stays ME-safe: the same undefined symbols as gl23 (me_memcmp, me_memset), .me_bss still 12 B, nothing static [C].

## Schedule (final, walk B2 -> B1 after E5)
`29552:29553:W,29652:29653:X,29752:29753:Y,29852:29853:B1,29952:29953:B2,30052:30053:B4,30152:30153:B8,30252:30253:Y,30352:30353:X,30452:30453:W,30550:30649:X,30650:30749:B1,30850:30949:X,30950:31049:B1,31150:31249:X,31250:31349:B1`

The plan's original cfg is `cfg_emu/mgs2_gl29_preE5.cfg` (86e4890b).

## Gates

### E1: keys absent equals gl28 [C]
- 685/685 hashes and 17,664/17,664 digests equal gl28's twin. The digests equal out_g22b.
- Every twin run today differs from hashref_g22b in 177 lines from vb 20,200. The old gl19 twin binary, rerun today, gives the same 685 lines, so the difference is not gl29's.
- Cause: UNKNOWN. The emulator runs match the reference (0 lines differ).

### E2 / E2b
- The tex-79 decals pair 1,020/1,020 [C].
- The only fallback is "vertex set": decals 0668245a 469/863, 30850609 463/523, 4f168d72 43/48.
- Largest list plus aux: 342,536 B overall (gl28's); 156,976 B in arm frames.
- Per arm:
  - X: 152 frames, 1,434 targets, 1,175 paired, 259 vertex-set fallbacks, largest copy 2,144 B.
  - Y: 2 frames, 30/34 paired.
  - B2: 137 frames, 1,223 biased.
  - W: 32 batches in 2 frames.

### E3: emulator E-A and E-A2
- 0 hash differences.
- 916/916 digests equal the twin, arm counts included.
- 478/478 shot lines say "last list = shot frame: yes" (213 in ME turns). No GAVE UP.

### E4: E-B (no arms)
- Bit-equal to gl28's emulator frames: 16/16 shots, 9/9 live frames.
- O frames of E-A against E-B: 133 of 155 equal. 12 frames after each arm window differ by at most 5 levels, a carry-over from the motion trail [I].
- Frame 24784 differs in every pair of emulator runs since gl26 (emulator noise).

### E5: the arms on the correct renderer
- **X and Y kept.** The changes are the sub-pixel shift of the shadows: on-screen vertex shift at most 0.24 px, median 0.13 [C model]; mean change 0.5 luma.
  - 88-96% of the pixels that change by 2+ luma lie on the shadows' soft edges.
  - X over 152 frames: 1,793 px change by 6+ luma; 103 px by over 8 levels, at most 6 in a frame; none outside the decals.
- **B bleeds:** a 1-2 px dark line along the foot of rails, beams and kerbs, never on Snake's boots in the frames checked.
  - Wait frames, px changed by 6+ luma: B1 17, B2 47, B4 76, B8 165.
  - Walk B2: 428 px over 8 levels across 136 frames, which breaks the margin, so the walk uses B1.
  - Walk B1: 166 px over 8 levels, 3,988 at 6+ luma.

  B1 by window, px over 6 luma / most in a frame / px over 8 levels:

  | window | px over 6 luma | most in a frame | px over 8 levels |
  |---|---|---|---|
  | 30650-30749 | 433 | 24 | 0 |
  | 30950-31049 | 3,505 | 165 | 139 |
  | 31250-31349 | 33 | 14 | 27 |

- **W** is a diagnostic: 10,202 px change by 6+ luma in 2 frames.

### E6: FAILED
- Every deck frame ends with full-screen sprites that test and write the farthest depth: f30500 batches b881-b886, f31046 b793-b798 (65535 at 30500, 61707 at 31046) [C].
- So the depth dumps hold 2-7 constant values.
- Fixes, not built: (a) stall the dump frame's list before those sprites, read, release; (b) turn off their depth write in dump frames only (must be proved bit-exact).

### E7: span-rule predictions
The predictions are lost px, mean / max per window [C model]. The rule over-predicts 2-3x at 30500 (82,437 predicted, 34,099 measured), so read these as upper bounds.

| window | arm | mean | max |
|---|---|---|---|
| 30498-30549 | O | 83,574 | 91,200 |
| 30550-30649 | X | 91,721 | 95,080 |
| 30650-30749 | B | 75,734 | 95,090 |
| 30750-30849 | O | 38,093 | 64,584 |
| 30850-30949 | X | 10,783 | 61,927 |
| 30950-31049 | B | 10,876 | 70,918 |
| 31050-31149 | O | 25,979 | 41,415 |
| 31150-31249 | X | 24,126 | 42,466 |
| 31250-31349 | B | 24,418 | 45,159 |

### E8: visible darkening of the smaller decals
- b800, b803, b805 and b806 visibly darken (median 6.9-12.6 luma). b797 and b801 do not (2.9-3.0).
- The tex-79 decals have medians of 5.4-19.7 luma.

### E9: build gates
- xme_rest_chk, xme_me_chk, mesec_chk and me_call_chk all pass. psp.sh exits with rc 0.
- No new register writes.

### Bias step size [C model]
One bias step is 64 GE units where the depth map ends near Z24 200k (29852-30722), and 0.5 units where it reaches 16.77M (31046, 31262).

## After the build (thread lead)
- **MGS2_GL_ZDUMP is turned off in deliver/mgs2.cfg** (commented, with the reason; the code stays). The dumps would have held no scene depth, cost a pause at every shot, and the mirror read was the only new kind of hardware access. Final cfg: `cfg_emu/mgs2_gl29_final.cfg`.
- README: the zbuf lines are removed and the size is now about 35 MB.
- Emulator run `emu_fin` with the final cfg: see psp_harness.md "Build gl29".

## Files
- `deliver/`: the 13 delivery files.
- `psp/`: the build output.
- `live_ref/`: 187 MB from E-A2, indexed by `live_ref.txt`.
- `rd29.py`: the log reader. It defaults to x86/out_e2b.log and live_ref.
- `x86/`: twin outputs out_e1, out_e2 and out_e2b.
- `ger/`, `cfg_emu/`, `emu_ea/`, `emu_eb/`, `emu_ea2/`.
- Scratch: scratchpad `g29/`.

## UNKNOWNs
- Why today's twin runs differ from hashref_g22b from vb 20,200 (the old binary does too).
- The real GE's depth rule. gl29 cannot measure it directly, but the R and live tables score the arms.
