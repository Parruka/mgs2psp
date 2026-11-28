---
name: ff8-p3
description: FF8 play build p3 DELIVERED 2026-11-17 19:19 (p2 + translated fight / menu / save / load code + fight wait skip); RAN on the PSP, log read 2026-11-17 21:03: starts, quick start works, but a NEW fight (other module) ran interpreted at 440-600 ms a frame; round 5 from its padlog
metadata:
  type: project
  modified: 2026-11-17T21:04:29.063Z
---

Notes: ff8/stage5_notes.md "2026-11-17 19:19" (delivery), "19:10" (wait skip). Follows [[ff8-p2]], [[ff8-translate-rounds]], [[ff8-fight-speed]], [[ff8-title-speed]].

**Delivered:** `FF8\Builds\FF8T_p3\FF8T`, 5 files: EBOOT.PBP e90dc141 (19.8 MB), ff8.cfg 06adfcf7, README_p3.txt, qs_2dcf8acd.st / .ok. Container: psp/rel_p3 (frozen), dev folder psp/vz (NEVER make there), sources /mnt/project-files/ff8/src_play_p3. Checked in the emulator: route 80/80, Ffm's p1 session 683 hashes + card 0d48a931, p2 session 37 hashes. Model: fight 28 / 41 ms, menu 24-28, save 14, load 12.

**Ffm's words:** 19:44 asked what belongs in the PSP's FF8T folder (answered: keep EBOOT, ff8.cfg, qs_2dcf8acd.*, disc1.bin, scph7001.bin, intro_rm.ffv, mvd, card1.mcd/.new; the rest may go after copying logs). 19:49 credits still at 19% (expected; title speed promised for the next build). 21:01 "p3 in folder".

**p3's PSP run, read 2026-11-17 21:03 (container psp/psp_p3; log_1 = the session, log = a second start stopped at once) [C]:** 28,564 frames from the quick start (loaded fine both times), clock proof 330 MHz, card1.mcd unchanged 0d48a931 (no save this time), heap at the end 5.4 MB in use, largest free block 14.8 MB. Windows: title / credits 76-87 ms a frame (show 10-16); rooms 19-48 ms (most 27-35); 6,000-7,200 33 ms with ~3,500 interpreted blocks; FIGHT 15,600-22,200: 440-600 ms a frame = 372 M interpreted blocks, almost all the fight wait loop 0x800D073C / 0x800CF3FC (240 M) and 0x80098848 (8 M): the fight module in RAM was another snapshot than the one translated from p1, so neither its code nor the wait skip applied; 22,800-26,400 64 ms with ~35 k interpreted a window; 3,356 addresses in interp_1.txt. Model-vs-PSP for rooms / menus still to compare (replay of the padlog on rel_p3's EBOOT with prof windows).

**Round 5 (started 21:03, SET ASIDE 21:15 for [[ff8-disc-modules]]: the fight code comes from the disc files now; p3's padlog stays the test of that fight):** collector FF8K1 (EBOOT 6a3b9e03, cfg `stload qs_a8952fa7.st`, `padsim padlog_p3.txt`, `quitat 28564`, `seeds seeds_k5.txt`, card 0d48a931; old cfg = ff8.cfg.collect4). Then fb/seeds_k5 + snapshots, round.sh, PSP form; the SPIN hook covers a new fight snapshot only if its words match (gen.py SPIN_PARTS).

**Lesson:** a session that reaches a new fight brings a new fight module; every fight kind may need its own round until the generator covers modules by content, not by snapshot.
