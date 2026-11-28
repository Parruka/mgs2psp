---
name: ff8-p4
description: FF8 play build p4 (delivered 2026-11-17 22:42): disc code files + PSP code overlays (folder ovl) + whole exe + fight on the GE + title levers + disc-wait skip; files, checks, picture-margin card, what to read in its log
metadata:
  type: project
  modified: 2026-11-17T22:44:02.794Z
---

**p4 DELIVERED 2026-11-17 22:42** to FF8\Builds\FF8T_p4\FF8T (12 files md5-checked on the PC). THE ONE BUILD OUT; logs to FF8\LOGS\p4. Follows [[ff8-p3]], [[ff8-disc-modules]]. Notes: stage5_notes.md entry "2026-11-17 22:45" (md5 7a3ec419 after it).

**Files [C]:** EBOOT ac456780 (title "FF8 play p4", 3 LOAD, _end 0x0a8bf4d8 = +6.94 MB over p3, g_emuprof 0x0a240b00), ff8.cfg 9c719328 (= psp/ff8_p4.cfg: p3's keys + `gecheck 120`), README_p4.txt 1b1c0ae8, qs_688edf0c.st 17f9b339 (= p3's state byte for byte) + .ok 6bd005d1, folder ovl/ with 7 files (18.2 MB): d_battle 51f1fd68, d_battle_render 7079e32e, d_card 77481ad8, d_field d6ef5a76, d_field_init e45d5fc7, d_intro 2c4cc88f, d_world 6673bbb9. Container psp/rel_p4 FROZEN (never make there); sources /mnt/project-files/ff8/src_p4 (refreshed 22:45).

**Checks on the delivered files [C emulator]:** scripted route 80/80, out aae53db0 (use the TEST cfg = FF8QA's, no file movies: with the play cfg the second movie plays from mvd and skips 11,524 frames, so hashes move by design); Ffm's p1 session with the play cfg byte-identical (683), card 0d48a931; p3 session 53/53, 80,906 interpreted (battle stage code at 0x8013E0xx); p2 37/37 on dev e8bcf8dd.

**Memory [I]:** PSP = emulator - 5,120 KB [C]. Largest new heap block at the end: ~6.1 MB (p3-like session), ~3.8 MB (p1-like). Heap buffers fixed or freed.

**Picture [C emulator]:** p1 fight <=0.7% (lever 2's full check: avg 0.7, worst 1.3%); p3 fight avg 1.3%, worst checked 5.2%; world map up to 6.3% (p3's EBOOT: 24% with big steps = the texture-half bug, now fixed). All p4 excess is ONE colour step in see-through dithered primitives (GE blends the 8-bit source; the PS1 truncates the source to 5 bits first). CARD cmsg_01Tzc8YgQrnPXLpp31PGjWVPQsu4c8hezZdNCP1TqtgzTV ANSWERED by Ffm 2026-11-17 23:27: "A. Only bigger steps count" = one-step colour differences do NOT count in the 2-3% margin; only pixels off by 2+ steps (of 32) count. p4 stays, the GE keeps the fight. Open: the Quezacotl summon (scripted test, [[ff8-fx-families]]) has up to 17% of pixels off by 2+ steps (emulator, side by side look the same); cause to find before the effects build.

**Read in p4's log:** done, below.

**Next (not built):** stages (163) and effects (343) as overlay families (64-module limit), menus as overlays, smaller cw tables.

**p4 RAN, log READ 2026-11-18 01:10-01:40 (copied to container psp/psp_p4; PC FF8\LOGS\p4) [C PSP]:** clock 331 MHz; session from the quick start to frame 26,657 (title, card load, field, world map 12,683, fight 14,946-20,344, world map, field); EXACT: replayed in the emulator (folder FF8R4 = p4 EBOOT + stload qs_a8952fa7.st + card 0d48a931 + padsim padlog_p4.txt, quitat 26657): game prints = the PSP's, same 32 interpreted blocks (80032dd0 x31), card unchanged; reference run_dk/out_p4session_p4eboot.txt (49 hashes), cfg run_dk/ff8_p4session.cfg. Code files read at 16 MB/s (d_field 4.9 MB 308 ms, d_world 6.1 MB 377 ms, d_battle 6.0 MB 369 ms). Heap at the end: largest new block 6,144 KB (emulator 11,264: PSP = emu - 5,120 again). ms a frame: title 80-85 (p3 87), field 21-37, world map 112-124 and 102 (= p3's 112: exact drawing runs everything there, LZSTAT dropped 0%, "texture written" ~1.85 M words / 500 frames), fight 38-44 (model 23.7/35.0 was p1/p3's fights). GE check: 2+ step pixels avg <= 0.52% a checked picture (fight), world map 0.40%, field <= 0.06%; the REAL GE has 2x PPSSPP's differing pixels in the world map and 3-200x the 2+ ones (PPSSPP is not the PSP for picture checks). Harness logs only window sums: next build should log the per-picture max of 2+ step pixels.
