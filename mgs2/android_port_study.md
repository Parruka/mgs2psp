# Study: "METAL GEAR SOLID 2 Sons of Liberty for Android" (luisxl15)

2026-11-01. Repo github.com/luisxl15/METAL-GEAR-SOLID-2-Sons-of-Liberty-for-Android, commit de02d726 (2026-10-24), GPL-3.0.
Read: README, recomp/*.py, recomp/runtime/*.c/h, the Java shell's sources. Not read or checked out: the five screenshots in docs/capturas (game pictures) and the button PNGs. The repo carries no game data; it builds from the user's own SHIELD files.

## What it is

Not a PS2 port. It is a static recompile of the **NVIDIA SHIELD TV build of MGS2 HD** (`libMGS2HD.so`, ARM32, the HD Collection's Android release, package jp.konami.mgs2hd.shield, version 21.21.1219.GM) to ARM64, so the game runs on phones whose CPUs dropped 32-bit code. About 22 k game functions + 3 k C++ library functions, translated by Python (Capstone) to C, built with the NDK's clang. Runtime replaces the SHIELD system: a 4 GB guest memory window, libc bridges, pthreads, desktop OpenGL (the TransGaming D3D9->GL layer) over GLES 3.2, CPU DXT decode for Mali, OpenSL audio, a fake JNI for NVIDIA Forge, optional LSFG frame generation in Vulkan.

So the source binary, CPU, renderer and data formats all differ from ours (R5900 + VU + GS -> PSP Allegrex + GE). No PS2 engine names or symbols appear in the repo (grep for DG_/GV_/GM_/GCL_: none).

## Ideas, ranked by use to us

1. **A safety rule for skipping pictures under adaptive pacing** (`runtime/pulo.c`) [new to us; useful later, Roadmap "Next: pacing"]. Their comment confirms the HD game is still the PS2 design: one logic step per frame, no clock. Their frame skip keeps logic at 60/s and, when a frame ends late, runs the next frame fully (state, textures, buffers) but drops its draws. Rules: a draw is dropped only if its render target was drawn in each of the 3 previous frames ("fixed" targets: screen, scene, post-processing); a new or occasional target (a load-time texture, a shadow map drawn now and then) is always drawn, since skipping it would leave stale content for good; never two skipped frames in a row for lateness; after a hitch over 8 steps (a load) it does not try to catch up. Our current skip is the game's own reduced pipeline (every other step, renderer_notes 11.12), which is exact by construction. If we ever add a timing-driven skip on top (e.g. near real time, drop a shown picture when a step runs late), this per-target history rule is the guard we would need for the trail's display buffers and the font page halves (the render targets renderer_notes 11.12 names). Not useful before the step is near its time budget.

2. **Function-level random-state fuzz against the original code** (`fuzz_funcoes.py`, `diferencial.py`) [partly known]. They pick leaf functions (no calls, no jumps out, under 600 instructions), run each with random register states on both the original (Unicorn) and the recompiled C over the same memory window, and compare registers, flags and memory; plus an instruction-by-instruction compare. We already fuzz our converters (vc_p06, vc_df0, the unpack; renderer_notes) and check the game code by state hashes along routes; we do not fuzz the recompiled EE game functions themselves. That would catch generator faults in code no route reaches yet (later stages, before "playable w00a" grows into more areas). A reference would be our own R5900 interpreter path or PCSX2 (Unicorn has no R5900 MMI/128-bit). Low cost, low-medium value.

3. **Skipping register reloads after calls to ABI-following functions** (`medir_abi.py`, `gerar.py` CARREGA_ABI) [already measured]. Same idea as our register-locals question; research2_port_plan "Results" measured register traffic at calls at ~2% of game-code time, so no change.

4. **A 64 k-entry direct-mapped cache in front of indirect-call lookup** (`runtime/despacho.c`) [already have a form]. We have jalr_tab's 256-entry probe + binary search (speed_notes §v15). A larger table would cost D-cache on the PSP; not worth it.

Not useful to us: flag-result analysis (`medir_bandeiras.py`, ARM flags), LSFG frame generation (Vulkan), CPU S3TC, OBB extraction, the Java shell, thread pinning to big cores.

## Verdict

Little that is new. The one idea worth keeping is item 1, for the pacing work later; item 2 is a cheap extra check to consider before later stages. Licence GPL-3.0: we take ideas only, no code.
