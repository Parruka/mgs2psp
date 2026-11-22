# MGS2 native renderer step 1 (gl30n) build report, 2026-11-20

Written by the native step 1 build agent (tree /home/claude/t30n, build /home/claude/main/build_gl30n); saved by the thread lead unchanged in substance. Not delivered: one build out at a time, and step 1 is to be merged with gl30 (gl31) before a PSP run.

gl30n (native step 1, ship meshes from records) is built and reproducible; four gates pass outright and gate 3 passes except for one literal failure that needs your call.

That failure: the two opposite-phase emulator runs do not have equal frame digests in 53 of 916 pictures. The cause is found (HUD depth values that are written but never tested), and each run equals its own twin in all 916. One other thing first: I edited a file under `/home/claude/main` by accident for 24 minutes; it is restored (details under "Hard-limit incident").

All figures are [C] from the container (twin, read-hash twin, PPSSPP 1.20.4 headless) unless marked. Nothing here is a PSP measurement.

## Files

Everything is in `/home/claude/main/build_gl30n`; the tree is `/home/claude/t30n`.

| File | md5 |
|---|---|
| `deliver/EBOOT.PBP` | 8d72094de271090ecd39c464867af139 |
| `deliver/mgs2.cfg` (= `cfg/mgs2_gl30n.cfg`) | 27271255c4cac876c3854c068b5f0251 |
| `deliver/hashref.txt` (= `tw/x86/hashref_c1.txt`) | 6678ae6209d63bf37b2c1b98c92ef180 |
| ovl_d00t.bin | 04ec39b2969574e189f276b68b78223c |
| ovl_d01t.bin | 1908c0c263bab1bb81fbf76ddb2874d3 |
| ovl_init.bin | 536db373dbcb8487c3e4bb25ae166694 |
| ovl_n_title.bin | 42b4984fcca848326e68359c3bdc9651 |
| ovl_r_title.bin | 0032ce12b56db12426d677b58b248084 |
| ovl_r_tnk0.bin | c4f34efcd8eb1c10e5a62824144a8fde |
| ovl_w00a.bin | c4aaa2de7ac1910b988b48279a2bff74 |
| ovl_w01a.bin | 73123e17fae48961e61fb7e7aa19976c |

- **Proposed cfg:** gl29x's file (11f04b6f) plus six comment lines and one key at the end, `MGS2_NAT_CYCLE=0,0,3,3,4,4`.
- **Hash reference:** it is not gl29x's 72e0ca4c; the PSP must be given this one.
- **Also in `deliver/`:** `MANIFEST.md5` and a `README.txt` draft for you to finish.
- **Opposite-phase test cfg:** `cfg/mgs2_gl30n_opp.cfg` (94be2c9e, `3,3,0,0,4,4`), with twin reference `tw/x86/hashref_c2.txt` (7f4a67ba).
- **Rebuild check:** `rl30.sh` into a second folder gave the same EBOOT and the same eight overlays.

## Build recipe

Scripts are in `build_gl30n`:
- `rl30.sh [ROOT]`: PSP relink as `relink29.sh`, from gl29's stage objects. `CHECK=1` first proves the relabel recipe on main's own sources (boot_rt.o b280cb03, ge_xlate.o 4ddac055).
  - Recompiled: ge_xme.o, ge_xme_me.o, ge_xlme.o, xl_psp.o, ge_xlate.o, boot_rt.o.
  - gen.o is not recompiled: `psp-objcopy` makes `P1214a0` weak and adds `P1214a0_orig`; the strong wrapper is in `boot/nat_stub.h`.
  - Relabelled objects must go through `-E -fdirectives-only`, then `-fpreprocessed -fdirectives-only`. Plain `-E` does not reproduce gl23's boot_rt.o.
- `tw30.sh`, `tw_run.sh`, `tw_part.sh`: the twin and its runs. `xt30.sh`, `rh30.sh`: the read-hash twin and its runs.
- `em30.sh TAG CFG REF`: emulator run. `emcmp.py`, `seccmp.py`, `ramdiff2.py`: comparisons.

## Switches

- `MGS2_NAT=<n>` sets one arm for the whole run; `MGS2_NAT_CYCLE=<a>,<b>,...` uses MGS2_OPT_CYCLE's turns (100 vblanks from vb 24,001).
- Arms: 0 = builder (today's path), 3 = native, 4 = stubbed and not drawn. Arms 1 and 2 are x86 shadow only.
- `MGS2_NAT_ONE=1` forces one shared array (test).
- With no NAT key the wrapper costs one test and the original builder runs.

## Gate 1: game state — pass for the proposed schedule and the PSP run's length

- **Read hash, base against the proposed schedule, final tree:** hash and read count are equal in all 31,399 vblanks (the PSP run ends at 31,400). The base equals the design's base in all vblanks.
- **Opposite phase (3,3,0,0,4,4):** counts equal everywhere; values differ in 2 vblanks, 25,944 and 25,974. These are the design's known pair (w lanes only, reader d01t 0x329668).
- **Whole-run arm 3:** counts equal; values differ in 3,784 opening vblanks plus the same 2 (design V1: 3,785 + 2). This is not the proposed configuration.
- **ee hash:** the cycle run differs from the builder run at 136 of 685 checkpoints, first at 24,300; iop differs at 0. The design's rule holds: twin = emulator for the same key (below).
- **Where RAM differs** (dumps at 13 checkpoints), and what a masked hash would leave out (`cfg/gs_mask_nat2.txt`):
  - 0x000000–0x004000: the scratchpad (the builders' work area);
  - 0x1b0900–0x1b4900: the game thread's stack (dead frames);
  - 0x1b8b30–0x1b9348: the two chain lists (packet addresses);
  - 0x1e5080–0x1e5084 and 0x1e50a0–0x1e50a4: packet control words;
  - 0x1b00000–0x1f00000: packet memory.
- **Masked hash, proposed schedule:** equal at all 314 checkpoints to vb 31,400, and at 681 of 684 to 37,800.
  - The 3 that differ (36,900–37,100, in w01a) are 2 words at 0x14fe444 and 0x14fe4e4.
  - They sit in the y lane of a 32-byte record array whose neighbours hold floats in that lane, so I read them as a leftover lane [I].
- **A fixed mask is not a general rule.** With the opposite phase the masked hash differs at 119 checkpoints from 26,000: 918 words, all w lanes, in heap 0x141–0x146. Whole-run arm 3 differs at 621. The rule that stays honest is "twin, emulator and PSP print the same hashes for the same key".
- **Open, beyond the PSP run (w01a, vb 31,400–37,800):** read counts are equal in all 6,399 vblanks, but values differ in 1,191, only in stub turns plus the first vblank after.
  - Three sampled vblanks show w lanes (readers S_w01a_P2e8200 and GLDU) and the z,w lanes of scratchpad 0x70003ec0 read 63 times by S_w01a_P3aaef8, whose last writer is itself.
  - That is the same class as the design's w00a 0x3bebf0. The whole-route classification is not done; do it before any build runs w01a with the stub on.

## Gate 2: shadow — pass

Route i + w01a to vb 37,800, 17,664 pictures, final tree.
- **Arm 0:** 685 of 685 hash lines equal `hashref_g22b`.
- **Arm 1 (records against packets):** 7,167,004 records matched, 0 differing words. 6,672 calls fell back to the builder because the array was full; all are in the opening and none from 24,001 with the cycle.
- **Arm 2 (builder runs, records drawn):** 0 differing lines of any kind (list, frame, content, natb, natq, natz) in all three paths:
  - the twin's own walk;
  - the PSP direct walk on x86 (`MGS2_NAT_LW=1`);
  - the PSP plan + copy + walk on x86 (`MGS2_NAT_LW=2`).
  - 7,167,004 groups drawn (core 7,165,614, fed word by word 1,390), dropped 0, plans failed 0, late 0, data outside the copy 0.
- **Arm 3, whole run:** 92 pictures (vb 30,854–31,082) differ from the builder run in list/frame; natb differs in 0 and natq in 0.
  - The differing values are Z of HUD vertices that are written but never tested.
  - They come from stale packet memory the game's HUD code leaves unwritten, so they depend on history.
- **Cycle with `MGS2_NAT_LW=2`:** all 17,664 pictures equal the cycle run through the twin walk.
- **Fallback tests:**
  - `MGS2_NAT_ONE=1`: pictures and hashes equal the cycle run.
  - `MGS2_NAT_CAP=1500`: 655 calls ran the builder; arm-3 pictures still equal the builder run's.
  - `MGS2_NAT_NOMEM=1`: hashes equal the cycle run; nothing drawn in arm-3 turns, as designed.

## Gate 3: emulator — pass, except the literal phase-equality item

Final EBOOT, software graphics, `PPSSPP_VFPU_RN=1`. The cycle run used the exact deliverables (EBOOT, cfg, hashref md5s above).
- **Key absent (e0):** all 62 hash lines and all 916 picture lines equal gl29x's emulator run (`build_gl29/emu_x2`).
  - 51 of 52 saved pictures are byte-identical to gl29x's.
  - The 52nd, shot_24784, differs between every pair of gl29's own emulator runs too.
- **Proposed cfg (e1):** runs to its end in 471 s; 62 of 62 hash lines and 916 of 916 picture lines equal the twin's.
- **Opposite phase (e2):** 62 of 62 and 916 of 916 equal its twin.
- **e1 against e2: 863 equal, 53 differ.** This is the literal failure of "digests equal between opposite phases".
  - All 53 are in vb 30,854–31,000, where one run is arm 3 and the other arm 0, inside the 92-picture HUD set above.
  - On the twin, natq (never-tested Z zeroed) differs in 0 of 17,664 between the two phases.
  - Colour output is the same [I, from the digest's definition]; I did not verify that depth is cleared before the next picture tests it.
- **Pixel check on saved emulator pictures:** all 11 native-turn pictures are byte-identical to the off run (shots 24244, 30824, 30924 and 8 live captures).
- **Arm C leaves traces in the next turn:**
  - The first one to three pictures after an arm C turn differ in list/frame. The code carries the previous picture's depth range into the next one (`gl_end`), which I take as the cause [I].
  - Saved pictures keep a fading remainder: 12 vblanks after, 81,583–118,621 pixels differ by up to 14 levels; 37 after, 83–688 pixels by 1–2; 62 after, none. I take this to be the game's frame feedback [I].
  - e1 and e2 are byte-identical in these, so native after C equals builder after C.
- **Counters, e1:** early fills 0, dropped 0, fallbacks 0, plans undone 0.

## Gate 4: ME gates — pass

- xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk all PASS; psp.sh rc 0.
- No new register writes and no new system calls beyond ones gl29's xl_psp.o already imports. The two memory blocks are asked for on the main CPU.

## Gate 5: memory (emulator; not the PSP's)

| | gl29x | gl30n |
|---|---|---|
| Heap headroom at its lowest | 188 KB | 156 KB |
| Outside the heap, largest / total, before the arrays | 224 / 432 KB | 224 / 432 KB |
| Outside the heap, after two 108 KB blocks | — | 116 / 216 KB |

- ELF text grew by 20.8 KB.
- If the PSP gives one block, both buffers share it; with none, native turns draw no ship. The log says which.

## What the PSP run should show

- **Hashes:** 62 lines equal to `hashref.txt` (6678ae62).
- **Start-up line:** "native step 1: the groups' arrays: ... made, an array each ... that memory before: largest block X KB, total Y KB; after: ...".
- **End line:** "native step 1 totals: ...".
- **One line per 100-vblank turn from 24,000:**
  `nat turn vb A-B <me|main> turn arm N (...; cycle turn K): calls recorded, groups, pictures with records; snapshot bytes a picture: mean, most; stubbed with no record; calls with no group; the builder run instead (dropped records) N: array full, key order, near the buffer's end, other; recorded with no array; groups left untaken; transfers planned with groups, groups taken by walks from RAM, plans undone, early fills for an array's reuse; the picture's side: records' groups copied, merge runs, groups to the core, fed word by word, dropped, drawn at a walk's end, run lanes stopped at a key, the groups' data in the copy / read from RAM / partly; native time X ms (ME turns only); hold: the main CPU waited for fills X ms, for copies X ms; frame arena, most: ...; copy arena, most in a step: ...`
- **Expected in native turns:** dropped 0, fallbacks 0, early fills 0. On the emulator the rain walk had about 610 groups and 29 KB of records a picture.
- **Turn offset:** the first picture of a turn belongs to the arm before. ME/main picture turns are (vb/100)&1, one vblank off from the nat turns.
- **Reading speed:** compare the harness's existing per-turn times across arms A, B and C, in ME turns and main turns separately. The nat line adds the merge time on the ME and the fill waits.
- **Shots:** 24,246, 30,826 and 30,926 fall in native turns; 30,502, 30,548, 31,048 and 31,114 fall in arm C (no ship); 30,646 is 43 vblanks after arm C.

## Decision for you: the owner needs warning about arm C

In arm C the ship (deck, walls, rails, crates) is missing for 200 of every 600 vblanks from 24,001, with a short fade after. The brief asked for this arm, but it is a visible change in a test run, so the owner should be told before it ships. The README draft says so.

## Expected gains (none measured)

- **Design [I]:** main CPU −3 to −7 ms, ME −6 to −14 ms a rain step.
- **Main CPU, mine:** about the builder's 3.8 ms a step [C model], plus a little from about 8 quadwords less packet data a group against 48 B of record [I].
- **ME, arm B:** likely small, because the same converter core runs on the same data [I].
- **Arm C:** gives the upper bound: ship meshes cost 8.95 ms a step on the ME [C, gl26 split], plus the walk's share (UNKNOWN).

## UNKNOWN until the PSP log

- Memory outside the heap after two blocks, and heap headroom (expect about 32 KB less than gl29x [I]).
- The real hold time and the real ME time.
- Whether early fills stay 0.
- Whether the HUD depth differences show at all.

## Deviations from design 6.1

- No separate snapshot phase: groups ride on the transfer records and are planned as extents; the hold is the existing fill wait.
- One 48-byte record a group (no 32-byte call record, no block or channel copy).
- An array of 2,304 groups per packet buffer, in two system-memory blocks. One shared array caused an early fill at every ME-turn picture on the emulator; it is kept only as the fallback.
- gen.o is not recompiled (weak symbol + wrapper). `xlate.c` gains one function (`xl_p04_core`); `xconv.c` is untouched.
- When the array is full, a key is out of order or the buffer is near its end, the builder runs instead of emulating its skip.
- Arms follow MGS2_OPT_CYCLE's turns. Merge time is measured only on the ME. Native drawing needs `MGS2_ME_PIC=4`.
- The ME-to-VRAM probe is left out (not cheap and safe).
- Arm A behaves as gl29x but is not instruction-identical: rt_dg_call 131→148 instructions, xm_plan 590→737, xm_walk 2201→3054, xme_entry 864→1090. All `.text.hot` lanes are unchanged. The functions with mismatched profiles are the same set as t29's.

## Merge with gl30 (dz_pair)

- Step 1 does not touch `ge/gelist.c` or `gelist.h`: t30n's copies are byte-identical to t29's, and ge_gelist.o is gl29's object.
- My changes against t29:
  - `ge/`: lw_lanes.h, xlate.c/.h, xlme.c/.h, xme.c/.h; new natp04.h and natsnap.h.
  - `psp/`: xl_psp.c.
  - `boot/` (against main/boot): boot.c, boot_x86.c, xlive_chk.c; new nat_stub.h and nat_x86.c.
  - `runtime/ee_io.h`: twin only.
- The one overlap with t30 is `psp/xl_psp.c`. A `diff3` dry run merged it with no conflict; it is not built or tested.
- A merged build needs the twin, the emulator comparisons and the hash reference made again.

## Hard-limit incident

- `t30n/runtime` was a symlink to `/home/claude/main/runtime` (as in t28 and t29), so my edit of `ee_io.h` at 18:32 changed main's file. I noticed at about 18:55.
- I replaced the symlink with a real folder and restored main's file from `/mnt/project-files/mgs2/main/runtime/ee_io.h` (md5 3323976e, the recorded value).
- Its mtime is set to 2026-11-18 20:48:00.763; the original sub-second value is unknown.
- During the window every added line was under `#ifdef NAT_HOOK` or a macro that is constant 0 without it. A recompile of boot_rt.o from main gave b280cb03 in that time.
- Nothing else under main outside `build_gl30n` was changed by me.

## Other notes

- Bare `scripts/twin.sh` starts the cycle at vb 1000, not 24,000; `tw_run.sh` passes the PSP cfg's schedule.
- `em30.sh` only ends by itself when steps.csv appears (a full run).
- Emulator pictures are kept in `emu_e0`, `emu_e1`, `emu_e2` under `pic/`. My stick roots and all RAM dumps are deleted; `build_gl30n` is 1.3 GB.
- The container restarted at about 21:10; the files survived, and the runs in flight were restarted and finished.

