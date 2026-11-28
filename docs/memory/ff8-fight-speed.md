---
name: ff8-fight-speed
description: FF8 fight speed (2026-11-17): lever 1 (wait-loop skip) is in p3; lever 2 (fight drawn by the GE, exact drawing owed and dropped) BUILT AND PROVEN in the emulator, fight 28/41.5 -> 23.2/35.2 ms a frame in the model; a renderer bug fixed; what is left; fast fight tests
metadata:
  type: project
  modified: 2026-11-17T20:10:21.021Z
---

Notes: ff8/stage5_notes.md "2026-11-17 20:15" (lever 2), "19:10" (lever 1). Sources: /mnt/project-files/ff8/src_l2 (gpu3.c, ge3.c, l2.sh, l2_full.sh). Follows [[ff8-p3]]; background [[ff8-lazy-drawing]], [[ff8-ge-renderer]], [[ff8-layout]].

**Lever 1** (exact skip of the fight's spin wait loop, rt_idle_spin): shipped in p3.

**Lever 2, dev build psp/vy EBOOT fede5796, NOT delivered (p3 is out; ships only in the next generation after p3's log).** Why the fight drew everything on the CPU [C]: (1) one command sets the drawing area to the whole VRAM = a region holding every texture; (2) small copies (HUD strips, a 2x1 self-copy) ran everything owed; (3) the fight draws ALL with "set mask bit" and the GE renderer refused masked drawing, so the picture came from the exact VRAM; (4) idle regions left from the fight's start hold its textures. Changes: gpu3.c (a) no region for an area >= 640 wide or >= 480 high, (b) small boxes made exact alone + MARK records, (c) an idle region is given up when a texture lies in it; ge3.c (d) GE draws set-mask primitives, (e) BUG FIX: right half of an 8-bit page was read one row up in triangles (the GE wraps u at width 128; in every build so far, small in rooms), (f) the PS1's cut texture slopes on the GE (cfg `geslope 1`).

**Proof [C emulator]:** Ffm's whole p1 session = the reference's 683 hashes + card 0d48a931; fight pictures (check on every picture, 65,801-68,335): 0.7% of pixels differ on average, worst picture 1.3%, 99% by one step (inside Ffm's 2-3%); p2 session old vs new: title / menus / load identical, hall slightly closer. Scripted route 80/80 + end line.

**Model [C model], p1 session windows, ms a frame:** fight 23.16 / 35.19 (p3 28.11 / 41.47); menu +0.3 to +0.8 (dev-build layout effects). Left in the 35.19: lz_walk 4.4 + lz_run 2.2 (clipped runs for boxes / peeks), tri_time 3.3 (exact PS1 draw time), fight code 1.75, clut_fill 1.6 (a palette per flat colour), runtime 5.5. Next levers (not built): a stored box per owed record, closed-form draw time, GE multiply instead of palette variants. 100% in fights is NOT promised.

**UNKNOWN on the PSP:** GE coordinates down to about -1,000 (fight polygons far off the picture); the real GE's time for ~370 magnified triangles a frame; the real picture difference. Read all three from the first PSP build with this, check on every Nth picture.

**Dev method:** l2.sh <gpu src> [ge src] compiles the dev copies UNDER ref/gpu.c and psp/ge_ff8.c with vv's profile and restores both (md5 26a6dc43 / 6872dd31), removes grown functions' slot lines, moves the four fixed addresses behind the code up 0x6000, links psp/vy, copies the EBOOT to FF8QA. l2_full.sh: full p1 session (hash 100), then the profile windows. main_ff8.c has new dev keys `geslope`, `geuvb U V`.

**FAST FIGHT TESTS [C]:** FF8QA: `stload fight67000.st` or `fight65800.st` (before the fight starts) + `padsim padlog_p1.txt` + `padshift 12256` + card1.mcd 0d48a931 + `quitat 68335` + `hash 100`; reference hashes psp/run_l1h/keep/out_ref.txt. LZSTAT lines print every 500 frames in log.txt; a state load forgets regions, so "at once" problems only show in the full replay. `gedump A B` = per-primitive check with texel "?" lines; `shot F`.
