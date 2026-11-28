# mainline: gl18me's build in one tree

Made 2026-11-18 (UTC) after the container reset. It replaces the chain of private trees (sb17, sb18, sbg0, sbg18, me18c, me18e, v31, psp/vp22)
that was lost with the container. It is one source tree that rebuilds **gl18me** (the build delivered 2026-11-18 20:12, EBOOT 5139d6f2)
from the shared project folder and the game inputs alone. The build has **no PGO**: the profiles were lost (see "Still owed").

Generated headers, objects, VU0 images, logs and the game inputs are game data. They live only in the container (`gen/`, `build/`) and are
never copied here.

## Layout

| Folder | What it holds | Where it comes from |
|---|---|---|
| `runtime/` | the runtime headers | shared `runtime/`, with `ee_io.h` (3323976e) and `host_time.h` (4d23aae5) from the sb2..sb18 chain (`tools/dev/sbN/*.diff`, sb17's `patch_*.py`, `sb18/sb18.diff`) |
| `rt/` | the ME guards' runtime headers, ahead of `runtime/` in the PSP runtime unit | me18e's `rt/` (`ee_io.h` 436c659e rebuilt from sb18 + me18b + me18c; `sif.h`, `iop_host.h`, `kernel.h` from the backup) |
| `boot/` | the runtime unit and the generated units' frame | shared `boot/`, with `boot.c` = sb18's (d903d06d, the PSP), `boot_x86.c` = sbg0's (4435c7b9: rain/g0's boot.c + h3_boot + cs_boot, the x86 twin), `xlive_chk.c` (sb18, 758eb191), `native_w00a.c`/`.h` = `tools/dev/sb18/nat` (536fb887 / fe4aba81) |
| `boot_g0/` | `gen.c`, `gen_stage.c`, guarded `gen_pre.h` (5605558c) for the five units with the 12 G0 functions | me18c/me18e's `boot/` |
| `ge/` | the picture code (49 files, ME-safe) | me18e's `ge/` (backup; md5s = me18e's `src_md5_all.txt`) |
| `psp/` | PSP harness: `main_psp.c` (4d-5, 3023310d), `xl_psp.c` (8b88e07d), `xme_me.c`/`.h`, `me_sec.ld`; shared `Makefile`, `ovl.ld`, `ovl_hdr.c`, `spr_state.ld`, `psp_ctx.S`, `hwprobe.c`, `ucontext.h`, `gl1_glue.c`, `v26me_bat.c`, `exc3b.c`/`.S`, `me/hle_me_mgs2.*` | me18e (backup) and the shared `psp/` |
| `cfg/mgs2_gl18me.cfg` | gl18me's delivered cfg (9f46ad61) | backup |
| `cfg/mgs2_gl19.cfg`, `mgs2_gl19fb.cfg`, `hashref_gl19*.txt`; `recomp/cov/cov_gl19.txt` | gl19's cfgs, their references and the coverage extension (section gl19) | new |
| `cfg/mgs2_gl20.cfg` (gl19fb's settings, gl20 header), `mgs2_gl20pgo.cfg` + `hashref_gl20pgo.txt` (the PGO training cfg and its twin reference) | gl20 (section gl20 PGO) | new |
| `tools/` | `patch_g0.py` (me18's, with the two header folders as arguments), `patch_half.py` (gl19), `xme_me_chk.py`, `mesec_chk.py`, `cmp_ref.py` (sb18/gl18), `refcmp.py` (new: a log against a hashref); gl20: `pgo_lost.py` (new), `layout_sim.c`, `layout_funcs.py` (copies of the shared tools), `layout_order.py` (the shared one + the ME-era objects) | |
| `scripts/` | `env.sh`, `hdr.sh`, `x86.sh`, `vu.sh`, `twin.sh`, `psp.sh`, `emu.sh`; gl20: `pgo.sh`, `pgochk.sh` | new |
| `build_all.sh` | all steps in order | new |
| `tools/ppsspp_1204_mgs2.diff` | the headless PPSSPP 1.20.4 patch (1d3478ad) | new (see step 0) |

Used in place, never copied: `third_party/ps2rc` (IOP runtime, audio, `hle/psp`), `iopgen/` (the IOP translation: generated from the
game's modules), `recomp/`, `tools/mk_headers.sh`, `tools/stages_subset.py`, `tools/dev/rain/g0/*_w00a.diff` (they hold lines of the
generated code), `play/pad_w01af.txt`, `prof/vu_spec_v22.txt`, `prof/vu_vfpu_list_v22.txt`, `prof/gs_mask_v1.txt`, `prof/hashref_x86_in_gl18me.txt`.

## Inputs

In `scripts/env.sh`. You can override each one from the environment.

- `SL` is SLES_503.83 (e3f4c0df).
- `STG` is the 123 stages from `tools/extract_stage.sh`.
- `PAK` is gl18's mgs2disc.pak (189ca53f). The x86 twin and the PSP both read it; neither needed any disc file outside it.
- `PADF` is `play/pad_w01af.txt`. It is gl18's pad.txt (7bc72ace).
- `PSPDEV` is the pspdev toolchain (psp-gcc 15.2.0).
- `PPSSPP` is the patched headless 1.20.4.

## Steps

`build_all.sh` runs steps 1-5, then step 6 when `EMU=1`. It never deletes anything: `build/psp` and `gen/vudump` must not exist when it starts.

0. **PPSSPP** (once, outside build_all):
   - Extract `Sources/ppsspp-1.20.4.tar.xz` and apply `tools/ppsspp_1204_mgs2.diff`. It contains:
     - PspProf, from TT's `tt_port/src/ppsspp_patches.diff` and `tools/ppsspp_mgs2.diff`, ported to 1.20.4;
     - the FPU add/sub rule (FS zero operand, FCR31 to the host);
     - `PPSSPP_VFPU_RN=1` (VFPU add/sub/mul/div round to nearest with FTZ/DAZ; the JIT falls back for them). This one was rebuilt from `tools/ppsspp_master_vfpu_rn.diff` and speed_notes.md's description, because no 1.20.4 copy of it was kept.
   - Packages: libgl1-mesa-dev libegl1-mesa-dev libsdl2-dev libglu1-mesa-dev.
   - Configure: `cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEADLESS=ON -DUSING_QT_UI=OFF -DLIBRETRO=OFF -DUNITTEST=OFF -DUSE_DISCORD=OFF -DUSE_FFMPEG=OFF`. FFmpeg is off because the tarball's bundled FFmpeg libraries are empty and the route skips the movies.
   - Build: `ninja PPSSPPHeadless`.
1. **hdr** (`scripts/hdr.sh`):
   - `tools/mk_headers.sh` with v31's options (`RC_SOUND=1 RC_XUNIT=1 RC_SPECIAL=1 RC_POLL_R= RC_POLL_F= RC_RETLIVE=1 RC_CONDCLEAN=1 RC_WPW=wpd_all6.json MM_BASE=1 COV=recomp/cov/cov_x31.txt`) writes `gen/WP`.
   - `gen/wpl/gen_s_w00a.h` is v31/WPL's marked w00a with the rain's chain h3 → nbs → cs → h3r → g0r applied.
   - `tools/patch_g0.py` writes `gen/wg`.
   - Every file's md5 is asserted.
2. **x86** (`scripts/x86.sh obj`): the x86 game units (`tools/dev/x86_ref.sh` flags) and the IOP objects.
3. **vu** (`scripts/x86.sh dump`, `scripts/vu.sh`):
   - An x86 build with every VU0 micro program interpreted (`-DVU_PROFILE`, `MGS2_VU_PROF`) runs route i and saves each image it meets.
   - `recomp/vupipe.py` then compiles `prof/vu_spec_v22.txt`'s entries, with VFPU forms on `prof/vu_vfpu_list_v22.txt`'s 18 and `VP_MEM=all`, into `gen/vp22/gen_vu.h`.
4. **twin** (`scripts/x86.sh twin`, `scripts/twin.sh`):
   - The build is the x86 twin as `tools/dev/rain/g0/mk.sh`: `boot_x86.c` with `-DMGS2_XLIVE`, the VU0 header, and `ge/` with `-DGU_SENT -DXL_CENSUS`.
   - The run is route i to vb 37,800 as gl18me's reference twin r18a. It uses gl17's rain settings, `MGS2_OPT2=fffff9df`, the dense hashes 15,700-16,010 with vuh, and `MGS2_HASH_REF`.
   - `tools/refcmp.py` then compares the run against `prof/hashref_x86_in_gl18me.txt`.
5. **psp** (`scripts/psp.sh`): gl18me's objects and link (`tools/dev/me18/mkpsp18g.sh` on gl18's base, `psp/Makefile` via `rebuild_use.sh`/`mk_psp.sh`, gl18's genf.txt), with these differences:
   - No profile and no code-order files. `LAYOUT=1`'s `-ffunction-sections` is kept.
   - The five G0 units are compiled from `gen/wg` through `boot_g0/`.
   - `boot_rt.o` gets `rt/` ahead and `-DME_GUARD_RT`.
   - The picture files are built ME-safe (renamed calls and sections). `xme_me_chk.py` checks the ME side, and `mesec_chk.py` checks the ELF.
   - The catcher units use `psp/gl3.sh d`'s lines.
   - `make` runs on the shared Makefile through a shadow `MG` folder: psp, boot, runtime and ge come from this tree; third_party, iopgen and tools from the project folder.
6. **emu** (`scripts/emu.sh`): the EBOOT on the patched PPSSPP.
   - Settings: `PPSSPP_VFPU_RN=1`, software graphics, no profiler, gl18me's cfg and hashref.
   - The emulator has no ME. In `MGS2_ME_PIC=4` the ME's loop runs as the main CPU's "me_pic" thread (me_step4_notes.md 4c).
   - `refcmp.py --partial` then checks the log's 434 hash lines and 311 vuh lines.

## gl19: MGS2_HALF, the deck's cosmetic actors at half rate (2026-11-18)

A game-side cut, the same on the x86 twin and the PSP (new hash references). Off by default: with `MGS2_HALF` unset the twin gives
gl18me's 685 of 685 lines and a byte-equal hashref.

- **The switch.** `MGS2_HALF=<hex mask>` for the whole run, or `MGS2_HALF_CYCLE=<hex>,<hex>,...` in the turns of `MGS2_OPT_CYCLE`
  (like `MGS2_OPT2_CYCLE`); in the cfg on the PSP, in the environment on x86. Bits: 1 the ropes (ropemain, ropemain2) and 4 the sea
  (wave2, wave) act on even game steps only, 2 the lamps (fade_lgt, c_light, bonbori_yw, bonbori_red) on odd steps only, 8 the
  trail effect (afterimage) never. The steps are counted by the scheduler's own passes (`g_half_step`), never by real time.
- **Code.**
  - `tools/patch_half.py` changes two lines of the actor scheduler P13f140 in `gen_elf.h`: its entry counts the passes, and the act
    call at 0x13f1ac asks `rt_half_skip(act)` first while `g_half` is set. `hdr.sh` makes `gen/wh` (from WP, x86: 012150a6) and
    `gen/wgh` (from wg, PSP: 7706dd45).
  - `boot/boot.c` and `boot/boot_x86.c`: `g_half`, `g_half_step`, a table of 18 act addresses (d01t, w00a and w01a images), each checked
    against the actor's file name at +0x28, `rt_half_skip`, and the switch in `opt_window`.
  - `psp/main_psp.c`: `NCFG` 52 and names for steps.csv's cfg 36-51 (36 + the mask). This also fixes a bug: with 35 entries,
    `w_cfg[35]` was `w_deck`, so gl18me's log printed "deck steps: 12000" for 6,000 steps (the means were right).
  - `env.sh`: `B=` builds and runs in another folder. `emu.sh`: `CFG=`, `REF=`, `TW=` for another cfg, hashref and twin log.
- **Coverage.** With the sea cut, the route enters one function not in `cov_x31.txt`: w01a's 2b2b28 (inside, vb 37,431).
  `recomp/cov/cov_gl19.txt` lists it; `hdr.sh` joins it to `cov_x31.txt` for WP. Of WP's ten files only `gen_s_w01a.h` changes
  (d9e3170e -> eb55ccb6: that one body in place of its `rt_missing` line), and wg's copy (81abca76 -> 1681cd09). A WP made before
  gl19 fails the w01a check: run `FORCE=1 scripts/hdr.sh`.
- **cfg.** `cfg/mgs2_gl19.cfg`: gl18me's, plus turns of 100 vblanks from vb 24,000 with the cut off, off, on (mask f), on. Two turns
  each, so that each setting meets both picture makers (`MGS2_ME_PIC_TURN=100`). Its reference is `cfg/hashref_gl19.txt` (the twin
  with the same settings). `cfg/mgs2_gl19fb.cfg` (off, off, f, f, off, off, b, b; b = without the sea) and `cfg/hashref_gl19fb.txt`
  are a variant.
- **Checks [C].** Twin with the gl19 cfg: reaches vb 37,800, equal to gl18me's reference up to vb 24,200 and different from 24,300
  (the first cut turn). Emulator with each cfg: 434 of 434 hash lines, 150 of 150 frame digests and 311 of 311 vuh lines equal to
  the twin's.

## gl20 PGO: fresh profiles, a code order and the ME-side timers (2026-11-19)

gl19's game code and settings with PGO and a code order, plus the lead's ME-side timers. Built in `build_gl20/` (not delivered).
Figures: [C] measured or read, [C model] PspProf's cost model (ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000).

- **Scripts.** `psp.sh`: `PSPD=`, `PGO=gen` (`PGO_ELF=1|rt|game`, `PGO_STAGE=w00a|w01a`), `PGO=use GCDA=<folder>`, `ORDER=<folder>`
  (layout_*.ld), `RELINK=<build>` (relink its objects); the make shadow folder now has one path for every build (`MGS=`, default
  `build_mg`), see below. The no-PGO path is unchanged: before the ME timers, a no-PGO build in another folder gave gl19's EBOOT
  bf14ff6a byte for byte, and the new shadow path is in no object (every object make compiles equal to the old shadow's).
  `pgo.sh train | trace | model | order | new` (each emulator run in its own `$B/ms/<tag>`; `order` takes `IN=`, `OPT=`, `JS=`).
  `pgochk.sh` compiles each profiled object again with warnings on (-w and -Wno-missing-profile out) and, inside `gcno/`, with
  -fprofile-generate -ftest-coverage for the names of the profile ids; `tools/pgo_lost.py` lists what ran without a profile.
  `emu.sh` takes `EMU_HOME=`.
- **Profiles [C].** Route i, training cfg `mgs2_gl20pgo.cfg` (gl19fb + cut b in every turn from vb 24,000, no shots; reference
  `hashref_gl20pgo.txt` from twin pgb). The instrumented ELF does not fit the emulator's memory whole, so: gen.o from a run with
  `MGS2_ME_PIC=0 MGS2_GL_TEXKB=1024` (the picture's memory only; game state equal), w00a (pgo 29,100-31,550) and w01a (31,600-37,790)
  each in its own run, the runtime set (boot_rt, gen_dispatch, the ME-safe picture files but ge_me_rt and ge_xme_me, ge_fxthin,
  xl_psp) in a run with the delivered settings (rt3, the last, with the ME timers in). Every training run: all its hash lines equal.
- **Lost profiles [C].** Before: GCC found no profile for gen_dispatch's static jalr_find, guest_call, stage_call, gen_mm (silent
  under -Wno-missing-profile): a static function's profile id hashes its source path, and gen_dispatch.c was compiled through a
  shadow folder inside each build folder (-ffile-prefix-map / -fprofile-prefix-map do not change it). Fixed by the one shadow path.
  The ME timers change the control flow of 8 profiled functions (each added call splits a block for -fprofile-arcs), so the runtime
  set was trained again with them in. After: 0 drops, 0 missing; untrained but run only the trail's 4 w00a functions (afterimage,
  never run under cut b): 0.062% of the windows' instructions, about 0.07 ms a step in all [C model]; left as they are.
- **Code order [C model].** layout_sim on two 8-window traces (deck, inside), one main-ELF order for both routes. FF8's fa.c
  (fully associative LRU, 256 lines) gave 5.21 M train misses; the search went below that (a good 2-way placement beats FA-LRU), so
  FF8's lopt was not tried. Round 1 (u's objects): 6.97 M -> 4.56 M. The final objects differ (ME timers, gen_dispatch's profile), so
  round 2 traced them again and searched from round 1's order: 5.21 M -> 4.69 M train, 1.69 M -> 1.53 M held-out windows;
  `layout_sim -new` on the final link equals the search exactly.
- **Final build [C]** (`build_gl20/psp`): EBOOT.PBP 0c6ea78f (4,269,820 bytes), ELF text 4,260,340; overlays d00t e6d5ce0e, d01t
  7e3632a8, init 72288cc9, n_title 2b58f401, r_title 16cb5146, r_tnk0 dd8a57ed, w00a bc89e7b2 (4,401,856, under OVL_SIZE), w01a
  5a60dc4f. Title "MGS2 harness mainline gl20". cfg `mgs2_gl20.cfg` (gl19fb's settings), hashref `hashref_gl19fb.txt` (af1c02eb).
  ME timer sources: ge/xme.h b24a360b, ge/xme.c 264e0f6b, psp/xme_me.h 89c78296, psp/xme_me.c 89334c30, psp/xl_psp.c 449d8317.
- **Checks [C].** Emulator (emu.sh, that cfg): 434/434 hash lines, 150/150 frame digests and 311/311 vuh lines equal to gl19's
  twin; 190 "me turn vb" lines and the "me turns (gl20)" line; lowest headroom about 2,380 KB. mesec_chk PASS, xme_me_chk PASS (136
  instructions). The x86 twin compiles none of the changed files and takes no PGO, so it is gl19's.
- **Model [C model], ms a step, main-CPU picture turns (29,910-29,990 / 30,910-30,990 / 33,510-33,590 = the harness windows "deck
  waiting", "rain", "inside"):** gl19 173.2 / 156.3 / 112.5 -> gl20 140.5 / 128.7 / 91.4 (-19% / -18% / -19%; I-misses -57% / -55% /
  -51%, instructions -4%). For scale: gl18me's PSP run measured 168.55 / 162.52 / 105.33 there (another build). PspProf's windows run
  the interpreter, so the state after the first window is each build's own drift of the same scene (README of PspProf). In the ME's
  turns the emulator runs the ME's loop on the main CPU, so those windows (gl19 201.8 / 198.1 / 126.6 -> gl20 169.0 / 165.8 / 105.1)
  overstate a PSP step. Only a PSP run settles the speed.

## Checks passed (2026-11-18)

- **Headers [C].** v31's w00a regenerated with the marker, and the five rain diffs then applied with every md5 that `tools/dev/rain/g0/README.md` gives (15140a0f, ed441b44, 600b4378, 2e1047d2, 923514f4). No md5 of the other v31 headers was ever recorded, but the next check covers them.
- **PSP game unit [C].** `gen_s_d00t.o` compiled without a profile from `gen/wg` through `boot_g0/` with gl18's genf flags gives **ee9a1d4e, gl18me's own object byte for byte**. gl18me's d00t had no profile, so this checks the headers, `patch_g0.py`, the guarded `gen_pre.h`, the toolchain and the flags together.
- **x86 twin [C].** Route i to vb 37,800 (159 s): **685 of 685 lines of `prof/hashref_x86_in_gl18me.txt` equal**: ee and vfb on all of them, and vuh on the 307 dense lines. There were no "split:" lines. The hashref written from the run is byte-equal to the reference (md5 50de900f).
- **PSP build [C].** EBOOT and 8 overlays built, `xme_me_chk` PASS, `mesec_chk` PASS, all four `--wrap` symbols and the catcher present.
- **PSP build output.**
  - EBOOT.PBP 9f2ed223 (4,235,776 bytes). ELF text 4,226,296 bytes (gl18me's was 4,247,304 with PGO).
  - Overlays: d00t e44ab3e5, d01t f5196adb, init 8ebc440e, n_title 573617a9, r_title c84bc7a4, r_tnk0 92e90975, w00a 7f5e04b9 (4,285,216 bytes, under OVL_SIZE 0x460000), w01a d404718f.
  - The title is "MGS2 harness mainline gl18me".
- **Emulator [C].** Patched PPSSPP 1.20.4, route i to vb 37,800 with gl18me's cfg (450 s):
  - **434 of 434 hash lines** equal `prof/hashref_x86_in_gl18me.txt` (ee and vfb, plus the 307 vuh fields). There was no "split: first" line.
  - **150 of 150 frame digests** in the cfg's window (31,561-31,959) equal the x86 twin's, line for line (150 distinct).
  - **311 of 311 vuh lines** equal the twin's.
  - Mode 4 on the emulator matched hwme5e's run: 378 turns, 189 hand-overs (SYNC), 111,834 records walked from RAM by the main CPU.
  - Lowest heap headroom was about 2,412 KB at vb 23,677 (hwme5e 2,396) [C emulator]. Only a PSP log gives the PSP's figure.
- **Reproducible [C].** `build_all.sh` was run on a fresh copy of the sources in another folder, everything from the inputs (headers too), in 908 s:
  - every asserted md5 equal and the same `gen_vu.h` (feb948ce);
  - the twin's hashref byte-equal to the reference again;
  - **the same EBOOT (9f2ed223) and all 8 overlays, byte for byte**. The build does not depend on the folder it runs in.

## Differences from gl18me (all exact for route i)

- **No PGO and no code order.** Only code placement and the compiler's choices change; the hashes say the state is the same. The EBOOT therefore differs from 5139d6f2, and its speed on the PSP is not gl18me's: it is UNKNOWN until measured, likely slower [I].
- **The VU0 header is vp22 less three images ("vp22i", feb948ce).**
  - The three images are fe0033d5:0,2, b641370c:0,2,6 and de078118:0. Route i never runs them (the interpreted dump run met 23 of vp22's 26 images and 1 more that vp22 leaves to the interpreter).
  - speed_notes.md found them on route d (the deck tutorial), whose pack (`/home/claude/psp5/mgs2disc.pak`) is lost.
  - They stay on the interpreter, which is the exact model. To compile them, run `vu.sh` on a route d dump with `STRICT=1`.
  - vp22's `VP_KEEP=vp9` was not used either (vp9 is lost). That only moved 5 functions into a tail header for the old PGO profile.
- **x86 IOP objects** use boot.c's documented line (`-O1 -w -I runtime -I $RT -fwrapv -fno-strict-aliasing`). The exact flags of the lost v23/xg3 objects are UNKNOWN. The twin's hashes (iop too: the run's own hash lines) match.
- **`runtime/vu0.h` changed after gl18me** (2026-11-08 18:15): E1's `VU_RZ_ALL`. Every change is behind `#if defined(VU_RZ_ALL)`, which this build does not define.

## Still owed

- **A PGO pass:** done in gl20 (section gl20 PGO); its PSP run is owed. What it was:
  - fresh profiles: ELF + w00a on the deck, w01a on route i, as gl18's `pgo_gl.sh`;
  - then the code order (`layout_sim`, FF8's placement);
  - then a PSP run to measure.
  
  gl18me's own profiles (`sbg18/pg/gcda`) are lost.
- The three route-d VU0 images (needs a route-d pack or disc folder).
- A real-PSP run of this EBOOT. It has not been delivered: gl18me is the build that is out, and this one is gl18me without PGO, so it is likely slower [I].
- The 16 shots in the cfg are written by the emulator run, but no reference shots survived the reset, so they were not compared. The frame digests above cover the window 31,561-31,959.
- The PPSSPP binary lives in the container at `/home/claude/ppsspp/src/ppsspp-1.20.4/build/PPSSPPHeadless`. Old scripts expect `/home/claude/ppsspp_src/...` (`tools/dev/prof_run.sh`); `scripts/emu.sh` uses `$PPSSPP`.
- The generated headers' md5s copied into the port's notes for the next reset (today they are only in `scripts/hdr.sh`, which asserts them).

## Timings (container, 4 cores, 2026-11-18) [C]

| Step | Time |
|---|---|
| build_all.sh from scratch | 908 s |
| 1 hdr (mk_headers.sh J=3: 273 s) | 274 s |
| 2 x86 objects | 90 s |
| 3 vu: dump build 2 s, interpreted route i 162 s, vupipe about 1 s | 165 s |
| 4 twin: build 27 s, run to vb 37,800 + compare 157 s | 184 s |
| 5 psp: objects 180 s (w00a 112 s the longest), make and link 15 s | 195 s |
| 6 emu (EMU=1): route i on PPSSPP 1.20.4, software graphics | 450 s |
| PPSSPP 1.20.4 headless build (cmake + ninja -j3, once) | about 9 min |

## gl21-gl28 (2026-11-19/20)

The psp/ harness files, scripts/psp.sh, tools/me_call_chk.py, tools/xme_rest_chk.py and cfg/mgs2_gl28*.cfg are now gl28's (2026-11-20).
Each build's diff, apply script and notes are in `attic/<build>/` (ring_gl21r, gefast_gl22, ge2_gl23, two_gl24, lchk_gl24d, two_gl25, gl27, gl28);
`attic/gl28/README.md` says what gl28 adds (ME rest, recorder, the black-marks diagnostic) and its checks. Per-run results: ../psp_harness.md.

## 2026-11-28: sources refreshed to gl43's final tree

Until today this folder still held the 2026-11-18 sources; the later builds' trees lived in the container only. Now:
- `boot/ ge/ nge/ psp/ rt/ runtime/` = gl43's final tree (the container's `/home/claude/t43`, 119 files; the generated
  `gen41/gen_vu.h` is not copied: `scripts/vu.sh` makes it again). New since 2026-11-18: the Media Engine picture path, the native
  renderer (`nge/`, `ge/nat*.h`, `boot/nat_stub.h`, `nge_stub.h`, `np6_stub.h`), the rain's logic cut B (`boot/native_b43.*`).
- `tools/ scripts/ cfg/` = the container's `/home/claude/main` copies.
- `attic/gl38` to `attic/gl43`: each has a `scripts/` folder with that build's own scripts, patches and cfg files
  (trainings, order, gates, packaging), as they stood in `/home/claude/main/build_<name>/`.
After each build from now on: copy the build's tree and scripts here, then sync the repository.
