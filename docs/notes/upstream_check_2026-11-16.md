# Upstream check, 2026-11-16 (read only; nothing changed, installed or rebuilt)

Method: `git ls-remote` and blobless clones through the container's proxy (the GitHub API is blocked), logs read 2026-11-16 ~20:40 UTC.
[I] = inferred.

| What | Ours | Upstream now | Matters? | Recommendation |
|---|---|---|---|---|
| pspdev toolchain (Sources\pspdev-ubuntu-latest-x86_64.tar.gz) | downloaded 2026-10-18, psp-gcc 15.2.0; = release v20261001 [I: by date] | release v20261101 is tagged on the same pspdev commit (99dee46, 2026-09-28); gcc branch allegrex-v15.2.0 unchanged since 2026-05-11; pspsdk +34 commits since Oct 1 (kernel stubs and headers: sceUsbAcc/UsbGps, loadcore structs, sceKernelCancelSema, lflash_fatfmt; tools: PBP offset checks; "no more beta" README) | No. Same compiler; no user-mode or speed change | Stay. An update would re-baseline every measurement |
| PPSSPP release | 1.20.4 | 1.20.4 is still the newest tag | No | Stay |
| PPSSPP master (exact reference) | 83e616ea (2026-10-28) + our VFPU-RN and FPU-FS patches | 819 commits later (c437463, 2026-11-16). Media Engine emulation (in master since 2026-10-21) now ON by default (16427de, 2026-11-12); interpreter: branches in delay slots as on hardware (8695803); sceMpegAvcCsc partial uploads; KIRK, lag sync, sceSas reverb. Files our patches touch changed: FPU 6 commits, VFPU 2 | Possibly: a master with ME emulation could run gl18me and FF8's Sony JPEG path closer to the PSP. UNKNOWN until tried | Keep 83e616ea as THE reference. Optional: a second, newer master side by side only to try ME emulation; patches may need rework; it never replaces the reference without Ffm's OK |
| ARK-4 (PSP-Archive/ARK-4) | Ffm's install | last commit 2026-08-02 (README); newest tag v1778786111 = 2026-05-14 | No | Stay on ARK-4 |
| roengstrom/ff8-decomp | read 2026-11-10 (70.6%) and 2026-11-16 (movie.c) | +25 commits 2026-11-10..15: DrawMovieMask and its type, func_800A2AF8, func_800A06F0, FieldPause, menumain2, fe_object1b commands, MCH model records | Yes for FF8 movies (m3 hand-over) [I] | FF8 thread: read the movie-mask commits (ae91ddd, a2230aa, af6b402) before the new hand-over |
| SandstoneDev/psp-developer (skill; our copy mgs2/third_party/psp-developer = 49326ad, 2026-10-20) | 49326ad | d27e89a (2026-10-30): new references/recompiled-port-lessons.md | Some lines yes: (1) a CFW clock setting can override scePowerSetClockFrequency while the getters still say 333 (they ran 31 builds at 443 MHz); our harnesses only log the getters; (2) hot code in eDRAM +6-8% on a translated game, eDRAM D-miss 44 vs 74 cycles (their numbers); (3) GE 15.9 ms with textures in RAM vs 5.7 in VRAM; (4) PRX over PSPLink | Refresh our copy (Ffm's OK, it is a project file); later, a fixed-loop timing line in the logs proves the clock [our FF8 model matching the PSP within ~2% already suggests 333 [I]] |
| mcidclan/psp-media-engine-mist | read 2026-10-29 | +2 commits 2026-11-12 (vme-poc sleep/suspend) | Only if MIST is taken up | Note |
| mcidclan/psp-media-engine-libs, ajitmohapatr/ps2-recomp-Agent-SKILL | cited in notes | no longer public (clone asks for a login) | Citations only | Note |
| custom-core, reload, me-feed-graphics-engine, vfpu-docs, psprecomp | | no change since our reads | No | |
| uofw | | +23 (Power Service, usbcam, linkfile, $k1 include) | No | |
| PS2RC | | 0.2.0 / 0.2.1 (menus, settings) | No | |
| sa1-psp | | +140 (devkit, texture pool, frame skip, ME loader proof, VFPU matrix copies) | Ideas only | Optional read |
| BullyPSP-PS2R2C | | README only | No | |
| ARK-5 | not used | +46 | No (we stay on ARK-4) | |

Claude skills: the account has no PSP or recompiler skill; the only PSP skill we use is psp-developer (above), not installed in this container (copy from mgs2/third_party when needed).

## Done after Ffm's "go" (2026-11-16 20:39)

- mgs2/third_party/psp-developer refreshed 49326ad -> d27e89a (2 files: SKILL.md, references/recompiled-port-lessons.md; the other 29 files were already identical; SOURCE.md updated). Checked: the copy equals upstream HEAD file for file.
- Clock proof line: handed to the FF8 thread through the coordinator (harness-only, next FF8 build).

## Skill search (2026-11-16; Claude's skill and plugin catalog + GitHub; nothing installed)

Catalog: no PSP, PlayStation, MIPS or recompiler skill. One generic community plugin "Reverse Engineering" (EncidXVII), contents not readable before install: not recommended.

| Found | What it is | Licence | Fits us? | Verdict |
|---|---|---|---|---|
| dmang-dev/mcp-ppsspp (7f132ab, 2026-11-13) | MCP server over PPSSPP's WebSocket debugger: memory and register reads, breakpoints, single step, screenshots, pad input (18 tools). Our headless reference build 83e616ea has `--debugger` / `--debugger-run` [C: headless/Headless.cpp] | MIT | Yes, for emulator-only faults (m3's purple hand-over) without a rebuild per question | USE as protocol reference for a small script of ours in the container; no install needed |
| mcpads/emucap (da7a2e7, 2026-10-18) | One MCP interface over patched emulators: Mednafen (PS1), PCSX2, PPSSPP and others: checkpoints, input replay, memory watch, breakpoints | GPL-2.0-or-later | Partly: a debugger on the real PS1 game (Mednafen) or on PCSX2. Our own exact reference emulators already trace | LATER, only if a question needs a second opinion from Mednafen or PCSX2; needs patched emulator builds |
| mitsuhiko/agent-stuff skills/ghidra (0865c84) and trailofbits/skills-curated ghidra-headless (6d05be4) | Skill + scripts to run Ghidra headless: decompile functions, strings, call graphs | Apache-2.0 / CC BY-SA 4.0 | Sometimes: reading game functions no decompile covers (MGS2 SLES-50383; FF8's missing ~29%). Needs Ghidra + Java (~0.5 GB [I]) in the container each time | OPTIONAL, install when a reading task comes up |
| bgyss/switchrecomp (7 static-recomp-* skills) | Short checklists: scope plan, input replay, A/V compare, perf profile, regression triage | none stated | No: our rules already do all of it, stricter | SKIP |
| itsgrimetime/melee-decomp, marijnvdwerf/agent-skills (N64) | Byte-matching decompilation skills (PowerPC, N64 IDO) | - | No: not our method or CPU | SKIP |
| hkmodd/ps2-recomp-Agent-SKILL (a1cafc2, 2026-03-25) | PS2Recomp PC pipeline skill (the one web_research_2.md line 167 already judged "do not install" under another account name) | has a LICENSE | No PSP content | SKIP (earlier verdict stands) |
| vgrichina/re-skill | Reverse-engineering session scaffold (REVERSE.md, port to web) | has a LICENSE | No | SKIP |
| Waffle565/god-of-war-native-port | Not a skill: PS2 God of War PC port on PS2Recomp, with a work log | none stated | Peer project only | Note |

Own skill: nothing public packages OUR method (charter, exactness rule, build cadence, the 15-file delivery, hash test, cost model). A skill of our own made from memory and NOTES.md would give every new thread those rules from its first minute: offered to Ffm, not made.
