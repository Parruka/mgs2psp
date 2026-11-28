---
name: ff8-port
description: FF8 PS1-to-PSP native port workstream (started 2026-11-10): Ffm's disc release, where the files are, goals, plan stages, open questions
metadata:
  type: project
  modified: 2026-11-10T15:48:04.656Z
---

- Started 2026-11-10 by Ffm: "Port Final Fantasy 8 from ps1 to psp native, with better textures and more visual upgrades". Separate from MGS2 (MGS2's STOP ALL does not cover it; never touch mgs2/ files, threads or builds: read and copy only). Same method: static recompile + new GE renderer; PSP-3000 ARK-4; personal use only. MGS2 rules ([[mgs2-rules]]) apply here too.
- Thread "Port Final Fantasy VIII to PSP natively"; work in /mnt/project-files/ff8/ (feasibility.md = study + plan).
- Ffm's discs (MOVED by Ffm 2026-11-19 10:27, old folder medievel gone): PC Desktop\mg2 port\FF8\CDs: FF8 USA Disc 1-4 .bin/.cue (+ zips; SLUS-00892/00908/00909/00910) and SCPH7001.BIN (disc 1 md5 50d5b62e and BIOS 1e68c231 re-checked there 2026-11-19); the GOG Remastered files are now in FF8\FINAL FANTASY VIII - REMASTERED, movies in FF8\Movies. The exe is identical on all four discs (md5 7221fbc7..., load 0x80010000, entry 0x80011534). Bin md5s in feasibility.md §1; redump match UNKNOWN.
- ONE DISC (Ffm 2026-11-11 11:25, "keep the full game in 1 cd", no answer wanted yet): stage 6 goal = all four discs as one install, no swaps; idea [I] in feasibility.md stage 6 (deduplicated sector pack, automatic swap through the game's own code, timing in DEVIATIONS).
- POPS already runs FF8; the port's value is upgrades U1-U11 (feasibility.md §5; U11 fast disc offered), each with shots + Ffm's OK and a cfg switch.
- Code rule (Ffm 2026-11-11 01:31): the CC0 decompile roengstrom/ff8-decomp (matches our disc; ff8/decomp_survey.md) may be used where useful, always verified by the per-frame hash + cycle test; replaces "own code only".
- Plan: 1 x86 reference; 2 recompiler (stage2_plan.md); 3 PSP harness; 4 GE renderer; 5 upgrades; 6 whole game.
- Screen shape: Ffm decides at stage 5 between side bars and backgrounds extended to fill widescreen (16:00); wish: full 480x272, no side bars if possible; show rooms both ways + zoom-and-crop first.
- Effort rule (Ffm 2026-11-10 15:51, corrects 15:45): pick medium, high or xhigh to fit each task (set_effort_level); max only when 100% needed.
- BIOS: Ffm's own SCPH7001.BIN (US v4.1, md5 1e68c231...) in PC FF8\CDs\ (was medievel\); the reference interpreter runs it low-level (no HLE kernel). Mednafen 1.29 (/usr/games/mednafen) runs it headless, sound via -sound.driver dummy (stage1_notes.md). GOG Remastered files now in FF8\FINAL FANTASY VIII - REMASTERED\ (was medievel\Final Fantasy VIII - Remastered [GOG]\).
- Container copies (ephemeral): /home/claude/ff8/{SLUS_008.92,disc1.bin (md5 50d5b62e = PC),bios/}; disc 1 came over in 4 parts _claude_tmp\ff8\d1bin.part0-3 on the PC (732 MB there; ask Ffm before deleting).
- Stages: 1 reference DONE 2026-11-10 (stage1_notes.md; ff8/ref/psxref runs Ffm's BIOS + disc 1 to free walking, deterministic; timing study timing_study.md; Ffm's card 18:18 "Match the real PS1": [[ff8-timing-calibration]], hashes_ref_d1_start.txt end 5b7cec928fadf580 at 40,000); 2 recompiler DONE 2026-11-10 23:10 ([[ff8-stage2]]); 3 PSP harness, b1 exact on the PSP, b2 out ([[ff8-stage3]]); 4 started 2026-11-11 ([[ff8-stage4]]). Later: battle coverage (pad script).
- Remastered mix (Ffm 17:49, "ok lets go" 17:51): study ff8/remaster_study.md DONE 18:20: PC data layout; battle/field models keep PS1 skeletons (+ animations mostly), meshes 2-5x; textures 3x; backgrounds = PS1 .mim; movies VP8 1280x896 15 fps; music: PS1 default + Remastered OGG option (Ffm 18:20). Port menu (Ffm 18:32, replaces 18:22 "music only"): U10 60 fps, U11 fast disc, U13 music, each if doable; every other upgrade on by default (still shots + OK first). Container: /home/claude/ff8/remaster (installer, game/, x/, tools/fsarc.py, bmodel.py).
- 60 fps (Ffm 2026-11-10 17:12 "we can get a steady 60 fps"): FF8 draws field 30, menus 60, movies 15 (measured in the reference); 60 fps added as upgrade U10 (in-between frames from the GE renderer, logic unchanged), shots + OK at stage 5.
