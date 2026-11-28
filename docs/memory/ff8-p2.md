---
name: ff8-p2
description: FF8 play build p2 DELIVERED 2026-11-17 15:49 (p1 + quick start at the Squaresoft logo + interlaced pictures shown by field = fix for the flash at the FF8 logo fade); p1's PSP session read
metadata:
  type: project
---

**p2 = THE ONE BUILD OUT** (Ffm asked for both changes): `FF8\Builds\FF8T_p2\FF8T`, 5 files to copy over the PSP's FF8T (EBOOT 144fc480, ff8.cfg 74b723e6, README_p2.txt, qs_a8952fa7.st, qs_a8952fa7.ok); logs to `FF8\LOGS\p2`. Container psp/rel_p2 (frozen, never make there), sources /mnt/project-files/ff8/src_play_p2, notes stage5_notes.md "2026-11-17 15:49".

**Ffm's decisions behind it (2026-11-17):** card 14:52 "A. Quick start, logo speed later"; 14:56 / 14:57 no Sony and PS1 logos, start at the Squaresoft logo "like a true psp port game"; 14:53 the fade from the white FF8 logo flashed on the PSP.

**Quick start:** cfg `quick 2100` (black gap 2066-2117 before Squaresoft). Files qs_<stamp>.st/.ok, stamp = build date+time+title, so EVERY rebuild needs new files made with that EBOOT in the emulator (release cfg + `quitat 2200`). State hash at 2100 = cc3262c38aff97f0 on the PC reference and the PSP build alike. Without the files a start runs the whole start-up once and makes them. NEVER loaded on a real PSP yet: Ffm's p2 run tells.

**Flash cause [C reference + Mednafen; that it is what Ffm saw: I]:** title screens are 640x480 interlaced; in fades the hidden field is darkened twice (120 / 0 at the end), a TV shows only the once-darkened one; p1 mixed both with the GE's filter (PPSSPP weighs them evenly, the real GE's weights are UNKNOWN). **Fix `ilace 1`** (main_ff8.c `il_show`): a 640x480 RAM buffer takes each frame the lines of parity (display y + gpu.c `field`) & 1 from the exact VRAM, the others stay from the frame before; `field` read at RT_GFIELD (rt_syms.h, 5 lines now). Emulator: fade even 192 -> 0 over 121 frames; still screens 0 px different. PSP cost UNKNOWN: read "[pic] show" in the title windows of p2's log (p1: 9-14 ms).

**p1's PSP session (read 2026-11-17 15:45, psp/psp_p1) [C]:** 68,335 frames, HOME in a fight. Save written twice (frames 51,456 / 51,840); the card file is whole: loads in the PC reference and with p2 in PPSSPP (Squall LV 7, 0:11, B-Garden Hall). Loading on the PSP itself still untried. Speeds per 600 frames: title pictures 69-82 ms; main menu 16.7 (full speed); rooms 18-36, most 25-35; save screen 38-40; frames 58,800-64,800 63-80 ms with ~2 M interpreted blocks a window (which screen: UNKNOWN); FIGHT 227 / 574 / 660 ms a frame = 1.5 fps. interp.txt: 11,724 addresses, 158 M blocks; pages 800cf/800d0 = 115 M (the fight [I]).

**Corrections / facts:** X during "Published by" does NOT skip; X during the Squaresoft logo or the credits brings the menu ~200 frames later. card_blank had a wrong byte (0x8A for 0x89), fixed in p2. The corridor after the second movie needs Down held (not a hang). scr lines must be in rising order (16 at most).

**p2 RAN (Ffm 2026-11-17 18:52): "p2 is ok the flash is fixed, and i could continue the save game".** Files read 19:05 (psp/psp_p2) [C]: quick start loaded both times ("started from the stored state at frame 2101", card put in after the load); title with ilace: "[pic] show" 9.8-14.5 ms a frame (p1 9-14), whole frame 71-85; load screens 20.3 ms with 178,382 interpreted blocks; hall after loading 31-37 ms; 341,527 interpreted in all (1,200 addresses); old logs ARE renamed to *_N.txt on the PSP; "main stack 1024 KB at 0b9c0000" (large memory is real for FF8). The session replays exactly in the emulator with p2's EBOOT (FF8QB: release cfg + qs files + card 0d48a931 + `padsim padlog_p2.txt` + `quitat 20533`: every [pad] line = the PSP's).

**Next:** p3 is out: [[ff8-p3]]. See [[ff8-p1]], [[ff8-harness-layout]], [[ff8-translate-rounds]].
