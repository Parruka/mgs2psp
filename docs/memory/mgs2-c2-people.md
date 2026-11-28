---
name: mgs2-c2-people
description: MGS2 Stage C step 2 (C2): the people (P06) skinned, lit and mapped by the GE: mapping (b), sb12 checked exact, master pictures, gl15 (C2 in turns) read 2026-11-02: exact, no CPU saving, Ffm 23:22 keep C2 off by default; the motion-trail effect on shots
metadata:
  type: project
  modified: 2026-11-02T19:13:10.525Z
---

- Design + results: mgs2/stagec_notes.md step 2 (2.1-2.9), task #140. Tree /home/claude/sb12 (ephemeral) = sb11 + tools/dev/sb12/sb12.diff (checked: rebuilds byte for byte); scripts and README there.
- KEY FACT: the PS2 does not renormalise P06's blended normals (length down to 0.16), the GE divides by the length, so C1's mapping would brighten joints up to 90 levels. MAPPING (b): emissive = PS2 ambient; global ambient (0,0,0,255); MATUPD 3; vertex colour (d,d,d,a), d = round(255 x PS2 normal length); pass 3 TEXMAPMODE 1 | 3<<8; bone 0 = 32768 I, bone 1 = (32768 B3, t); 16-bit weights w*8, 32768 - w*8. Vertices 20 B (0x15e) / 24 B skinned (0x455e); GerC2 164 B a call (116 unskinned).
- No exact VFPU loop: the converter rounds toward zero (FCR31 0x01000001), the VFPU only to nearest.
- CHECKED 2026-11-02 [C]: x86 3 routes, all four C1/C2 settings: hashes equal, per-frame draws and depth ranges equal, all 2.07 M P06 calls take C2, 0 fallbacks; off settings = sb11. Emulator (master, indoor, sb12m/d): every frame's list+frame digests = x86 in both settings, no split. gl15 twin x2 with 9 turns = x1 runs in every window.
- Frame data: -0.6..-1.4% per route but +131 KB in frames whose people draw pass 1 only (36,899 indoor); largest frames do not grow.
- PICTURES (master, indoor): up to 3.8% of pixels differ, 99.3% of differing pixels by 1 level, edge pixels up to 15; dark frames equal. Six comparisons in Ffm's Desktop\mg2 port\SHOTS\c2_people (19:05).
- Card to Ffm 19:06: Ffm CHOSE "Test in gl15" (19:12). gl15 = C1 on by default + C2 in 4 of 9 turns: DELIVERED 2026-11-02 21:50 to Desktop\mg2 port\MGS2T_gl15 (15 md5s checked on the PC; psp_harness.md "Build gl15"); waiting for Ffm's run, then C2 by default or not. [[mgs2-stagec]]
- MOTION TRAIL [C]: the game's trail (sprites sampling the frame before; gelist.c cut 2, 16-px columns) carries the old turn's picture into a new turn, about half each frame (2,024 px -> 36 at 6 frames -> 0 by 20 frames: tools/dev/sb12/gl15/trail.sh). Shots soon after a turn change show it (gl15 master 31,112: 36 px); compare PSP shots with the same build's master (same turns), never with an always-on reference. Lists are right in every frame.
- PSP RUN (gl15, 2026-11-02 23:01) [C]: the PSP GE draws C2 exactly as the master (three C2 shots byte-equal; the three trail frames differ only in the rows 256-271 band, #68). Speed: NO CPU saving (deck +0.7/+0.8 ms a step, inside +0.79 within noise; model +0.9/+0.3: instructions -0.4/-0.5 lost to I-misses +1.2/+0.9); GE wait +0.06 deck / +0.28 inside with the overlap off. Card 23:10: C2 by default? FFM CHOSE "Keep it off" (23:22): C2 off by default in gl16 and after; retest it after the ME picture work.
