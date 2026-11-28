---
name: ff8-p5
description: FF8 play build p5 (built 2026-11-18 03:50, NEVER delivered: superseded by p6): GF summons from code files, world map lazy-drawing fix w2, gen.py cluster fix, GE check logs the worst 2+ step picture; files, checks, model figures, what is left
metadata:
  type: project
  modified: 2026-11-18T03:03:46.260Z
---

SUPERSEDED by [[ff8-p6]] (= p5 + four exact speed changes) before delivery. Follows [[ff8-p4]] (p4's log read 2026-11-18 01:40; no build out since). Notes: stage5_notes.md entry "2026-11-18 03:30" (md5 9ad53b08 after it). Sources /mnt/project-files/ff8/src_p5 (gpu6.c, ge5.c, rt_l.c f2b, gen.py, main_ff8.c, rt.c, rt.h, build scripts l6.sh l6q.sh lg.sh lge.sh, checks chk*.sh).

**Files [C]:** container psp/rel_p5 FROZEN. EBOOT a76eb4ad (18,855,068 B, "FF8 play p5", 3 LOAD, _end 0x0a9404d8, g_emuprof 0x0a09d3c0), ff8.cfg = p4's 9c719328, README_p5.txt 019efb30, qs_41bde84c.st 17f9b339 (= p4's state) + .ok 6bd005d1, ovl/ 35 files 48.2 MB (p4's 7 + d_mag006..d_mag330). md5 list rel_p5/md5_delivery.txt (40 lines). Deliver to FF8\Builds\FF8T_p5\FF8T when Ffm's PC is on; logs to FF8\LOGS\p5.

**What changed:** (1) summons = family 1 ([[ff8-fx-families]], f2b). (2) gen.py: blocks in another CLUSTER_CUTS band than their function go to the interpreter (f1/f2 trees had put spell-band words into stage cluster s3k_066728_6945277f~c1: p1 session 21,219 interpreted -> 343); tree recomp/gendkf3. (3) world map lazy drawing w2 ([[ff8-lazy-drawing]]): 8-bit palettes wrapping at x 1023 cut to the indices their texels hold (lz_pal8), a read region that is not the current one runs and is given up, cover map kept over the rounds of one frame, R->own skips needless box walks; cfg "env LZW2 n" (default 15). (4) ge5.c: [ge] line ends "2+ steps, worst picture X.XX%".

**Checks on the exact files [C emulator]:** route 80/80 aae53db0; p1 session 683 + card 0d48a931; p3 53 + card; p4 session 49 = p4's, lazy vs eager [vsum] equal at 49 points; fx1 132 = p4's. Heap largest new block: p1 9,216 KB, p3 / p4 session 11,520, fx1 12,032 (PSP = emu - 5,120).

**Model [C model]:** p4 session world map 120.7 -> 97.9, 109.0 -> 88.1 (p4's model was 7% over its PSP there: ~91 / ~82 [I]); other scenes 0-5% faster. fx1: fight 50.6 -> 50.4, Quezacotl 152.7/240.3/120.1 -> 81.3/120.3/84.8, Shiva 131.3/132.1 -> 92.6/94.9, Blizzard 94.9 -> 83.6. fx1 windows are 400/500/600/500/400/600/400 frames (prof_ms.py needs each count).

**Left (dev):** world map: tri_time 11.0 ms (PS1 draw-time model), lz_swap 8.6 (~80 patch swaps a frame, ~35 live patches), __pad_a_start 9.8 (I-misses); summons: exact drawing of stacked see-through layers; summon picture up to 17% px 2+ steps off in PPSSPP (dither idea untested); spells 0x801A0000 (311 files) and battle stage code still slow path.
