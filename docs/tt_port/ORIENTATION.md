# TT port: orientation (read before reusing it)

Source: `TT_PSP_port_handoff.md` (copied here) and `TT_port_source.zip` (unpacked to `src/`), both uploaded by Ffm on 2026-10-17. The Two Towers (PS2 SLUS-20578) to PSP-3000 port reached build v19. Ffm now wants this knowledge as the base for a **new port of a different PS2 game** (not named yet).

Everything below was checked against the files in `src/` unless marked "handoff only".

## 1. Where the TT port stopped
- v19 was sent to Ffm and has not been run. Expected (handoff only): CPU update ≈29 ms (from ≈36), GE ≈13 ms lower, ≈23–25 fps.
- To resume TT you would need Ffm's v19 `game.txt` from two runs (`config_gebench`, then normal play without a cfg), plus whether the ground or walls look different from v17.
- Decisions still pending from Ffm: make `fpurz` (PS2 rounding) the default; re-enable the 20 safe HW counter words; offer a 365 MHz overclock through the CFW's clock call.

## 2. Reusable for any PS2-to-PSP static recompile port
These parts are the actual asset for a new game. Most tools are written against SLUS_205.78 and hard-coded paths, so each needs re-targeting, but the methods carry over.

**Recompiler and code-generation passes** (`src/model/`)
- `recomp.py` (+ `recomp_jt.py`, `tailfix.py`): R5900 to C, one C function per guest function, global `R[]/RH[]/F[]`. It reads the ELF through `SLUS` env var but assumes a load base of 0x100000 and a file offset of 0x80 (`W()`): change both for a new ELF.
- `narrow.py` (exact 64-to-32-bit narrowing of EE compiler idioms), `regcache.py` (GPRs as C locals), `qsr.py` (shared save/restore helpers), `layout.py` (hot layout from a profile). These are post-passes on generated C and contain almost no game addresses.
- `vurecomp.py` / `vuemit.py` / `vuvfpu.py` / `vudis.py`: VU0 micro-program AOT and COP2 macro emission; `demo_src/vu0.h` is the VU float model (denormal flush, Inf/NaN clamp to ±Fmax, per-lane flags).
- `yo_set.py` / `gen_pl.py` / `gen_pl_post.py` are the drivers: the **method** (recompile set = call closure of level boot + virtual methods + script wrappers; presentation layer stubbed) is reusable, the address lists are TT-only.
- `src/emu.py`: a Python R5900 interpreter used as a test oracle.
- Build flags that mattered: guest memory base pinned in `$s7` (`-ffixed-23`, `GUEST_PINREG`), `GUEST_NOMASK`, `-fwrapv -fno-strict-aliasing -ffp-contract=off` (`src/psp_game/Makefile`).

**FPU rules** (handoff §3.3; self-test in `demo_src/fpu_test.h`, `fpu_expect.h`, `fpu_gen.c`)
- R5900 rounds toward zero, DAZ + flush, no Inf/NaN. The Allegrex with FS=1 has no DAZ on inputs; that one difference is why x86 (d1a59100) and hardware (3650c540) hashes differ.
- Round toward zero works on the Allegrex via FCR31 RM=1.

**GE renderer techniques** (`demo_src/ge_*.h`, `psp_game/main.c`)
- Skinned figures with GE hardware skinning, int16 positions (`ge_model.h`); median-cut T8 + box-filter mipmaps (`ge_tex.h`); fog from the depth buffer read as T16 through VRAM mirror 0x04200000 (`ge_fog.h`); `GU_TEXTURE_SLOPE` per texture to mimic PS2 Q-based LOD.
- Display list built with GU_SEND and kicked at frame end without sync, so the GE draws frame N while the CPU runs N+1.
- 16-bit level vertices: the GE reads 16-bit UVs as **unsigned**; signed UVs caused the v15 "LOD bug".

**Performance findings on PSP-3000 hardware** (handoff §6, measured by Ffm's runs)
- The recompiled CPU code is the wall, and it is memory-bound: ≈125 cycles per cache miss on average, code ≈3.6× the PS2 size.
- The GE's main-RAM traffic stole ≈15 ms of CPU per frame. Vertex data moved to VRAM made the level cost the CPU nothing. **Where data lives matters, not its size.** Pixels cost nothing, so lower resolution was dropped.
- The Media Engine lost to cache flushes for the skeleton pass.
- Still-unbuilt CPU levers: register-argument calling convention (largest), return-value demand, dead save/restore removal, cold marking.

**Hardware safety lessons** (`psp_game/main.c`, handoff §6.4)
- Reading word +0x50 of the counter block 0xBC400000 kills the thread and switched the PSP off. `HWP_N` is 20 (words 0x00–0x4C) and `g_hwp_on` is never set (main.c:419–435).
- Exit order: HOME sets `g_quit`, the loop finishes the GE list, `sceGuTerm`, exception handler handed back, then `sceKernelExitGame`; bench waits for X (main.c:461–471, 619–623). Never auto-exit.
- Overclock only through the CFW clock call; writing the PLL yourself switched a console off (SandstoneDev). main.c:481 calls `scePowerSetClockFrequency(333, 333, 166)`, the stock maximum, so no overclock is active.

**Method and diagnostics**
- Deterministic bench: no input, fixed dt, 900 frames, state hash printed (`chk fr 900 hash`). Every codegen change must keep x86, emulator and hardware hashes identical.
- One change per measurement; gebench mode cycling for GE questions; input record/replay (`input.rec`, TTREC1); boot-step hashes; exception trap via kubridge; watchdog; heap log.
- Patched PPSSPP (`src/ppsspp_patches.diff` on hrydgard commit 66dc8972 in `src/ppsspp_base.txt`): Allegrex-accurate FS mode and a `PspProf` cache/miss profiler. The emulator is for "does it run/render/crash" only; take numbers from hardware.

## 3. Two Towers specific (does not carry over)
- File formats: BFA/BFF container, PSWB world geometry, ClPbi textures, DBDB tables, YO22 models, 'hier' skeleton, 'aniz' animation, 'prps' props, sky dome, PLV4 (handoff §3.1, §5). The DBDB column hash and the PSMT8 unswizzle are PS2-generic but the containers are EA/Stormfront-specific.
- Every EE address in handoff §3.2 (frame loop, services, AI, player), gp = 0x3892f0, and the Lua 4 state snapshot.
- `demo_src/` game glue (`yo.h`, `player.h`, `co.h`, `pj.h`, `brain.h`, `animmach.h`, `lod.h`, AI headers, `*_ids.h`), `docs/ai_semantics.md`, `docs/logic_semantics.md`, `docs/level1_video_timeline.md`.
- Ffm's approved TT gameplay levers (LOD rate, 4 elves, 2 followers, Kirk/Spock every 3rd frame, orc cap 10, T8 textures) apply only to TT. A new game needs its own consent for each.

## 4. What is in `src/` and what is missing
**Present:** `model/` (recompiler, passes, exporters), `demo_src/` (runtime headers + many `test_*.c`), `psp_game/` (v19 `main.c`, Makefile, scripts, `exc.S`), `psp_me/`, `psp_bench/`, `psp_micro/`, `viewer/` (v6–v8), `docs/`, `convert_level.py`, `add_strips.py`, `verify_plv.py`, `research_summary.md`, PPSSPP patch + base commit. The root also holds older copies of workbench scripts (`recomp.py` 11 KB vs `model/recomp.py` 20 KB; `lift.py`, `rtl.py`, etc.) and parsers from `/home/claude/work` (`emu.py`, `lsc.py`, `yo22.py`, `skel2.py`...). `demo_src/main.c` + `main_part.c` are the retired TTDemo, not the game.

**Game-derived files inside the zip (personal use only, never publish):** `model/isul1.lgc` (exported level logic, LGC1), and ~40 compiled x86 test binaries in `demo_src/test_*` (no extension) that may embed game data. `docs/*` are notes about the game.

**Missing, must be rebuilt from Ffm's disc and a new container:**
- `demo/` entirely: `gen_pl.h` (13 MB), `gen_co.h`, `gen_wc.h`, `gen_pr.h`, `gen_ow.h`, `gen_me.h`, `gen_vu.h`, `gen_stub.h`, `gen_dual.h`, `gen_hacts.h`, plus `/tmp/gen_pr_tr.h` (needed by `pathroom_tr.h`).
- Runtime data: `exe_main.bin`, `bff_isul1.bin`, `pj_guest.bin`, `pl_guest.bin`, `wc_isul1.bin`, `brain_*.bin`, `anim_*.bin`, `00isul1.bfa`, `level.plv` / `level_v15.plv`.
- Workbench: `SLUS_205.78`, `/tmp/all.dis` (llvm-objdump with symbols, `model/dsm.sh`), `gen_pl_prerc.h`, VU micro-memory dumps (from a `-DVU_DUMP` build), exported JSON.
- Python modules imported but absent: `ids`, `relset` (used by `model/export_abin.py`, `pipeline.py`, `symx.py`).
- Toolchain: pspdev with psp-gcc 15.2.0; patched PPSSPP build.
- Scripts hard-code `/home/claude/proj/TwoTowers_PSP_viewer`, `/home/claude/work` and `/mnt/user-data/uploads`; expect to fix paths.

## 5. Handoff vs code: discrepancies found
- `model/gen_pl.py` docstring calls `gen_pl.h` "the hero (ClPlayer) recompile set"; the handoff calls it the whole recompiled game. The docstring is stale; `yo.h` builds the full set into the same file name (inferred from `yo_set.py` and `gen_pl.py` both writing it).
- `gen_dual.h` and `gen_hacts.h` are required by `guest.h` / `hero_acts.h` but the handoff does not list them.
- The handoff lists `demo/` files that `main.c` reads (`exe_main.bin`, `pj_guest.bin`, `pl_guest.bin`, `bff_isul1.bin`, `00isul1.bfa`, `level.plv`): matches main.c. `wc_isul1.bin` is read only by the `test_*.c` harnesses; `yo.h`, `main.c` and `bench_yo.c` never name it, which fits the handoff (the game runs the original collision code on data from the BFF).
- No other contradictions found in the cfg keys (main.c:484–486 matches handoff §4.9), HW counter state, exit order, or Makefile flags.

## 6. For the new game, first questions
- Which game and disc ID; the ELF layout (segments, gp) replaces the 0x100000/0x80 assumptions.
- Whether its engine has a script VM, VU0 micro programs, and a display-list boundary like TT's, which decides how much of the recompile-set method transfers.
