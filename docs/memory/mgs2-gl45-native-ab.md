---
name: mgs2-gl45-native-ab
description: 2026-11-27/28 gl45 study: hand-native code against the translation (7 maths leaves, then the pointer walk P14b008); where the translation's extra PSP slots come from; what a generator change could buy
metadata:
  type: project
---

Asked by the MGS2 port lead of the Vita study thread (2026-11-27 20:07): A/B of hand-written native C against the recompiled code for hot game-logic functions. Batch 1 delivered 2026-11-27 23:40 in mgs2/main/attic/gl45_native/ (gl45_native.md, native45.h/.c, n45_apply.py, psp_path.py, build/run scripts). Study tree /home/claude/main (container), scratch /home/claude/n45.

Forms (bits of g_n45; x86 MGS2_N45=<hex>): 0 MT_QuatNormalize 0x11cd68, 1 sceVu0MulMatrix 0x165d78, 2 MT_QuatToMat 0x11c828, 3 _sceVu0ecossin 0x166090, 4-6 sceVu0RotMatrixZ/X/Y 0x166108/0x1661b0/0x166258 merged with their ecossin call.

Results:
- Exact [C twin]: check mode (each call both ways, all registers, VU0 state, time, declared RAM; whole RAM for 50 calls a form): 1.5 to 5.8 million calls a form, none different. All seven on: seed/time log, 685 hash lines (EE, IOP, game state, vfb), 37,831 picture lines equal the reference.
- PSP instruction slots on the hot path, translated -> native [C psp-gcc static]: 90 -> 77, 286 -> 141, 253 -> 170, 247 -> 205, 636 -> 487. About 1.4x on the set.
- PSP estimate [I, no PSP anchor]: -0.55 ms a step in the rain wait (of 34.4 logic), -0.43 indoors (of 17.4). The translation of FPU/VU0 leaves is already near-native; native removes VF staging through memory, register-space tests and entry/exit work only.
- x86 ticks are NOT a proxy for VU0 forms (the x86 arm is the VFPU reference emulation, slower by construction).
- The translator inlines small leaves inside the ELF unit (MulMatrix 31 copies): a hook on the function serves real calls only (the stage units').
- PSP asm strings assemble with psp-gcc but have never run on a PSP or emulator.

Batch 2 (asked by the lead 23:38, delivered 2026-11-28 00:25; section 7 of gl45_native.md): the pointer walk P14b008, rain wait, 26.5 calls a step.
- Translated: about 5,550 PSP slots a call for 3,537 guest instructions (1.57x); 96% is one loop, 30 slots an entry (guest's own 17, address forming 9, clock/poll 4). Extra over the guest's work: address forming 61%, clock 27%; 64-bit halves + write-backs + entry/exit + call glue under 3% here but most of the once-a-call part (226 slots for ~65 guest instructions) [C psp-gcc + C twin weights].
- Address slots an entry: 4 I/O-region test (GEN_FAST), 4 base adds (gen_hb's addu is asm, never merged or hoisted), 1 mask.
- Generator levers [I]: drop the I/O test where proven (-4), host pointers for invariant bases (-3), clock in a register (-2): 30 -> ~21, about 1.4x on loop code.
- Hand-written loop (bit 7, N45_HZX_FF at the loop head of a second copy of the translated text; polls batched by N45_FREE): 6 slots an entry, exact (1,526 whole-RAM compared calls equal; whole route with all 8 forms equal). ~1,390 slots a call, 4x, about -0.55 ms a step [I]. But 11 of the 24 slots saved an entry are the GUEST'S own code (stack-held loop state): a rewrite, not a translation fix.
- Flatness [C twin x86, logic + draw prep mixed]: 910 translated functions ran; top 10 = 38%, top 50 = 74%, top 100 = 88%, top 200 = 95%.
- psp-gcc -g changes the code slightly (1,169 vs 1,158 instructions): class tables come from the debug object, cross-checked by hand on the shipped one.
- Tools: n45_bc.py (counters), n45_gcount.py, psp_flow.py + flow_spec.txt.

Not done: the indoor profile, CalcObjsCollisionWithInvM, any PSP run of these forms. PSP toolchain for counts: the pspdev tarball from the PC Sources folder unpacked at /home/claude/pspsdk/pspdev (psp-gcc 15.2.0).

Related: [[mgs2-gl44-rain-stubs]], [[vita-port-study]], [[mgs2-status-now]], [[mgs2-speed-work]].
