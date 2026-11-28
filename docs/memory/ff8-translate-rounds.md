---
name: ff8-translate-rounds
description: FF8 2026-11-17: Ffm's p1 session (fight, menu, save screen) translated in 3 feedback rounds, exact; replay method, round procedure, results, dev build psp/vx, what p3 needs
metadata:
  type: project
  modified: 2026-11-17T18:08:10.163Z
---

Notes: ff8/stage5_notes.md "2026-11-17 17:00" (round 1) and "18:15" (rounds 2-3); tools in ff8/src_round_k. Follows [[ff8-p2]]; uses [[ff8-layout]], [[ff8-harness-layout]].

**Replay method [C]:** a PSP padlog replays EXACTLY in PPSSPP: cfg `quick 0`, `padsim padlog_p1.txt`, `quitat 68335`. Proof of a good replay: final card1.mcd md5 0d48a931 (= Ffm's) and state hash at 67,000 = 3484680ab942cb1d. THE FOLDER MUST NOT HOLD card1.mcd from an earlier run (the title's cursor then starts on Continue and the replay goes elsewhere: two void runs). A new emulator folder needs its link in /root/.ppsspp/PSP/GAME (else PPSSPP exits silently). Folders: FF8P2 (interpreter build), FF8K0 (plain new build, profile windows), FF8K1 (collector, snap/ pre-filled with fb/snap/*.bin). One replay = 6-7 min.

**A round:** collector replay (psp/vy, cfg `seeds F` / `snapdir snap`) -> clean seed lines to recomp/fb/seeds_kN.txt + keep_kN.txt, new snapshots to fb/snap -> scratchpad p1/round.sh (generate --covloose, _cov twin, fresh fb/cov_d1_start.txt, strict generation, PC twin psxrck must give 80 hashes + per-frame counts + 0 interpreted, PSP form genk) -> p1/psp_round.sh (dev build psp/vx: vw's runtime objects, link script by layout/mkx.py in two passes, everything behind the game code moved by multiples of 0x2000) -> replay.

**Fixes (keep):** gen.py `sh<n>` name (s8/s16 clash), coverage carried over when function borders move, `--covloose`; rt.c seed_note collector: a seed at a word the valid module does not know goes to it only if the block equals the module's snapshot file, else this run's snapshots, else a new one (answer cache keyed by address AND module). recomp/rt.c.p2 = release runtime source (vv's PGO objects match it); builds use vw's rt.o, only the collector folder compiles the new rt.c. By hand: fb/snap/s4k_058613_76b6550e.txt limits that module to 0x801E2000-0x801E5000 (the menu's loading loop runs while pages above are half-written).

**Result [C emulator], dev build psp/vx EBOOT 75b0a7a7 (19.7 MB, 3 LOAD, image end 0x0a1b13d4):** scripted route 80/80 on the PC twin and the PSP form; Ffm's session exact; interpreted blocks 158 M -> 5,608. Cost model [C model], old -> new: fight 709 / 663 -> 36.1 / 47.1 ms a frame (model check: old 663.4 vs p1 on the PSP 660.5); menu 70.6 -> 23-28; save screen 14.0; fight start 110.9. Fight now = 35-46% of PS1 speed: exact triangles (draw_tri_fast) 13-17 ms, fight code 10, runtime 4-5: the next lever.

**Memory [C from MGS2's PSP log gl17 run 2, same PSP]:** user memory reaches ~0x0BC00000 (52 MB); FF8 with this code uses ~33.5 MB. The next FF8 PSP build should print its own heap room.

**COVERAGE RULE (Ffm 2026-11-17 21:06/21:07): "you want me to play the full game, get all fight etc so you can get the code? if so, i dont think thats going to happen".** Coverage must come from my side: per-function code validity (p3's new fight reused the translated code but its 1 MB snapshot differed elsewhere) and translating the disc's code files statically (decompile's file list). Ffm's normal play logs are a bonus, never a requirement. Told Ffm 21:12. [[ff8-p3]]

**Open:** 64-module limit (rt_codemap 64-bit mask; 32 modules now); s4k_013402 / s4k_027981 carry the same code twice (merge equal modules); general page-wise module validity not built. **ROUND 4 (19:10):** the p2 session (loading a save) on rounds 1-3's code interpreted only 4,455 blocks; 198 seed lines -> fb/seeds_k4.txt; twin 80/80, 0 interpreted; genk = rounds 1-4 with the fight wait hook. **SHIPPED in p3** (2026-11-17 19:19): [[ff8-p3]].
