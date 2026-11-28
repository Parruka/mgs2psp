---
name: ff8-fx-families
description: FF8 module FAMILIES (overlay files past the 64-module limit; f1, f2 summons, f2b dev builds 2026-11-18) and the scripted magic test session fx1 (Quezacotl, Blizzard, Shiva); costs and picture findings
metadata:
  type: project
  modified: 2026-11-18T00:01:47.429Z
---

Why: p4 ([[ff8-p4]]) still interprets effect code: GF summons at 0x80190000 (28 files), spells at 0x801A0000 (311), 0x801B0000 (4); 11.6 MB MIPS. rt_codemap is u64: at most 64 modules ([[ff8-disc-modules]]).

**f1 = family refactor, DONE in dev (psp/vy, EBOOT 54bcd0da, not delivered):** the 7 modules at 0x80098000 are ONE family (RtModule ovl=2); each member's labels + code words live in its overlay file, main memory keeps fingerprints. _end 0x0a7804d8 = 1.25 MB below p4 (rodata reserve dR 0x7c000). Checks [C emulator]: route 80/80 aae53db0; p3 session 53/53; p1 session byte-identical, card 0d48a931; fx1 = p4's. Heap largest new block: p1 session 11,264 KB, p3 13,568, fx1 14,080 (PSP = emu - 5,120 KB). Backups *.p4. Next: menus (4.8 MB in main memory) as families; the stick reads 16 MB/s [C p4] (a 0.7 MB menu file ~45 ms).

**f2 / f2b (dev, 2026-11-18; notes stage5_notes.md "2026-11-18 02:35"):** the 28 GF summon files (0x80190000, manifest ovl/disc_fx_summons.txt, disc_mods_f2.txt, gen tree recomp/gendkf2) = family 1 with its own area rt_ovl_area1 (1.75 MB); link with scratchpad p1/l6q.sh (l6.sh: d_mag* -> area1). Family ranges overlap: rt_lookup tries every family. f2b fix: rt_codepage bit 1+f only while family f's loaded member has a code word in the page (f2 slowed plain fights 3.8 ms). f2b = EBOOT 9826e905, kept with ELF + 35 ovl files in psp/keep_f2b; exact on route, p3, p1 sessions and fx1. Model ms a frame: Quezacotl 153/240/120 -> 83/123/86, Shiva 131/132 -> 108/112, Blizzard 95 -> 97, fight 50.6 -> 51.3. Left: an invalid member is re-verified after each write into it (Blizzard +2 ms).

**fx1 test session (no play needed, Ffm's rule 2026-11-17 21:06):** container /home/claude/ff8/psp/run_dk/fx1 (fx1.pad = game frames, pad_fx1.txt = padsim file, cfg_head.txt, out_p4.txt = 136 hashes = REFERENCE, interp_p4.txt). Start: stload fight65800.st (p1 session's state at frame 65800, world map), padshift 12256, quitat 79400. Script: menu (CIRCLE 0x2000 opens it; X 0x4000 confirm; TRIANGLE 0x1000 cancel), junction Quezacotl+Shiva to Quistis, commands Magic/GF/Draw; fight 1 (Bite Bugs) Quistis summons Quezacotl 69620; fight 2 (beach) Draw > Cast Blizzard 75880, Shiva 77150. Battle menus ignore presses for ~50 frames after a list opens: press again later. RAM sits at state-file offset 560 (8-byte header + 552; PSP-written state); GF table 0x80077378, 68 bytes each, owned flag bit0 at +0x61.
Folders: FF8MG = p4, FF8FX = f2b (fx1 cfg). Tools: scratchpad jn/jn.sh, jn/sheet.py.

**fx1 findings [C emulator]:** 10.28 M blocks on the slow path: 10.14 M at 0x80190000 (the two summons), 69 K stage code, 69 K at 0x801A0000 (Blizzard). Cost model on p4 [C model] per frame: fight 50.6 ms, Quezacotl 120-240, Blizzard 95, Shiva 131; slow path 15-85 ms of it, exact drawing (lz_run/lz_walk/PS1 GPU, read-back effects) 40-55 in every effect window: code translation alone leaves summons ~70-150 [I]. Profile: PSPPROF_SWITCH=1 PSPPROF_FLAG=0a240b00 (p4) PSPPROF_OUT=..., cfg prof A B; prof/ in run_dk/fx1. Notes stage5_notes.md entry 2026-11-18 00:40 (md5 3dbea020 after it).
PICTURE: summon shots up to 44% px differ, up to 17% by 2+ steps (over the margin even under Ffm's A). Per-primitive check: every primitive is only ONE step off (T fl22 textured additive dithered, G-S-D sm1/sm2), but the summon stacks many see-through layers on the same pixels, so the steps add up. Idea [I, untested]: GE dither matrix -8 at the PS1's negative dither positions, no GE dither elsewhere, gives the PS1 sum exactly except zero source channels and saturation; else CPU-draw see-through dithered prims in effects.

Size [I]: overlay file ~20x MIPS; effects ~230 MB of files.
