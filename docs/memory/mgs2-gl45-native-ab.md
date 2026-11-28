---
name: mgs2-gl45-native-ab
description: 2026-11-27 gl45 study, batch 1: seven hand-native maths functions against the translation, exact on the twin; what native buys (about 0.5 ms a step) and what is not done
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

Not done: an integer/pointer-walk function (candidate P14b008, the HZX trap walk), the indoor profile, CalcObjsCollisionWithInvM. PSP toolchain for counts: the pspdev tarball from the PC Sources folder unpacked at /home/claude/pspsdk/pspdev (psp-gcc 15.2.0).

Related: [[mgs2-gl44-rain-stubs]], [[vita-port-study]], [[mgs2-status-now]], [[mgs2-speed-work]].
