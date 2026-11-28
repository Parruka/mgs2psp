# DEVIATIONS.md entry draft for option B (gl43) — for the lead to place; not in DEVIATIONS.md

Suggested place: section G (the lead's game-side cuts), as G-next. Text:

- **G?. The aft deck's splashes and ripples without their pictures, the logic kept to the PS2's random numbers (Ffm's rain option "B", 2026-11-22 13:03/13:05; `OPT2_B43`, MGS2_OPT2 bit 21 = 2097152; t43/boot/native_b43.{h,c}, build_gl43/tools/b43_apply.py; gl43 tests it in its own turns, attic/gl43).**
  - **What (game side):** while the bit is set, four of the deck's rain functions run as a short native form in place of their translation. These are the floor splash's update and spawn (0x318418, 0x317e88) and the falling splash's update and dead step (0x319600, 0x319078); the falling splash's spawn (0x3190c8) stays translated. Each form keeps only what other code can see:
    - the stage seed 0x1fc954, advanced by exactly the translation's draw count (the floor spawn draws 1 + 96);
    - the slots' lives and counters (word 0, word 4, the float life with its clamp at 0);
    - the floor spawn's three 16-byte copies into the splash work vectors (*(0x1f85c8/cc/d0));
    - the matrix call P125a78;
    - the game time: the translation's interrupt polls × 64 added to the clock.

    A form runs only when no interrupt is due inside it (NOW + P × 64 < k_poll_at). Otherwise, or when a guard holds (word 4 = 0, actor+76 = 0), the translation runs. The interrupts therefore land on the same game instructions as on the PS2.
  - **What (picture side):** the floor splash prims (0x318808), falling splash prims (0x319aec) and ripple rings (0x3321f4, 0x332214, 0x332494, 0x3324a4, 0x3324b4) are hidden (their +164 word gets bits 12-13). In the same turns MGS2_BOUND_CYCLE 4 (BD_RAIN) leaves out the rain's other effect calls: the far drop sheets and the foot splash. MGS2_GL_RAIN_NEAR=1 keeps the nearest of the 4 drop-sheet calls (d_splash_camera's last). The streaks are H3R's and stay as they are.
  - **Why this form:** this is the exact form (rain_options s5.3), not DICE + NP. DICE alone changes the guards' random numbers on the aft deck (rain_options s5.2). In the exact form every draw and every poll is the translation's, so the guards' draws, the route and the stage loads are the reference's. B can also switch on and off per turn.
  - **Impact [C twin, x86 twin of gl43, route i to vb 37,800]:**
    - **Seed and route.** The seed log is identical with B on in its turns (sch), B on in every turn from vb 29,000 (bon), and B never on (off). It covers every act's seeds 0x1fc954 / 0x1fbd74 / 0x1ff554 and the game time at entry, 12,699 lines (md5 b27073b998d1). It also covers 65,070 seed changes with the drawing act, including the guards (watcher.c 187, attacker.c 221; md5 99a5a9c90b85).
    - **IOP and pictures after the deck.** IOP and frame-buffer hashes are equal at all 684 points. Pictures after the deck (3,072 content and digest lines) are equal.
    - **Check mode.** In check mode every B call ran translated and was compared with the form's prediction: floor spawn 964, floor update 11,036, fall update 9,560, fall dead 45,704. 0 differed in time, seed, lives or copies.
    - **Read hash.** Every load the game makes outside the draw pipeline, hashed per vblank, is equal in 37,795 of 37,799 vblanks. The 4 others are t69 (vb 30,928/929/932/933): 16 loads of an 8-byte unaligned copy (GLDU) at scratchpad 0x70003f88. Its low word is freshly written (S_w00a_P2e59b0); its high word 0x70003f8c is a leftover from the translated splash code (0 with B). No later load sees a different value.
    - **State hash.** The EE state hash differs from vb 29,100 by design: the slots' particles, the prims' vertex and position buffers, the actors' own blocks and the packet areas. gl43's hashref is its own.
  - **Impact [C emu]:** the PSP build on the emulator gives 62/62 hashes = gl43's hashref. Its B-turn counters equal the twin's; the B digest turn (t73) has content = the twin's.
  - **What you see in a B turn:** the deck's rain has only its streaks and the nearest drop sheet. The splashes on the floor, the falling splashes, the ripple rings, the far drop sheets and the foot splash are MISSING ON PURPOSE. Right after a B turn, the splashes that started during it may stay hidden until they end [I]. On the emulator, against a run without B, the first 6 to 16 pictures after each B turn differ (up to about 30 vblanks), and one at vb 31,062; then the pictures are equal [C emu, fh1/fh0]. In the twin's t55, two turns after a B turn, the picture content is equal and only the list digests differ: leftover vertex bytes of splash slots that are not shown.
  - **Undo:** clear bit 21 of MGS2_OPT2 / the MGS2_OPT2_CYCLE words (ffdff9ff = B off). Also drop bound kind 4 from MGS2_BOUND_CYCLE and MGS2_GL_RAIN_NEAR.
  - **Status:** gl43 = a measurement build (B in turns t51, t53, t57, t59, t62, t65, t69, t72, t73). Not on by default.
