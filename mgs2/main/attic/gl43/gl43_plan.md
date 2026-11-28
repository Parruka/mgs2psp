# gl43 plan (build agent; brief from the lead 2026-11-24 ~14:0x, resumed 2026-11-27 13:33 UTC)

gl43 = gl42 + Ffm's rain option "B" (consent 2026-11-22 13:03/13:05) on the game side, in its own 100-vblank turns.  Tree
/home/claude/t43 (= t42 + the files below), build /home/claude/main/build_gl43, notes here (state.md is the running log).

## 1. Form: the exact form (rain_options s5.3, hd_study s1.3), not DICE+NP
DICE alone moves the guards' numbers on the aft deck (rain_options s5.2), so DICE+NP cannot keep the PS2's numbers.  The exact form
does: each shadowed call advances the shared seed (0x1fc954) by exactly the draws the call would make and adds exactly its polls
(x 64 cycles) to the game clock, and only when no poll of the call would reach k_poll_at (else the translation runs), so every
later user of the seed (the guards first) and every interrupt sees the PS2's values at the PS2's times.  So B can switch per turn.

## 2. What B does (OPT2_B43 = g_opt2 bit 21, 2097152; off by default and outside the B turns)
- Shadows (boot/native_b43.c): d_splash_floor's slot spawn 0x317e88 (97 draws: life 8 + (r & 7) at slot +0/+4; its three qword copies
  into the splash work vectors) and slot update 0x318418 (life word - 1; its matrix copy P125a78(0x1df990)); d_splash_fall's slot update
  0x319600 (float life - 1.0, not below 0) and dead slot 0x319078 (time only).  d_splash_fall's spawn 0x3190c8 stays translated.
- Hidden (B43_HWR keeps +164's hide bits 0x3000 at the acts' show stores): the floor's prim (0x318808), the fall's (0x319aec), ripple's
  three (0x3321f4 0x332214 0x332494 0x3324a4 0x3324b4; ripple's logic runs whole).
- Draw side in the B turns: MGS2_BOUND_CYCLE 4 (BD_RAIN) with MGS2_GL_RAIN_NEAR=1: the rain's effect calls left out but the nearest of
  each picture's 4 drop-sheet calls (d_splash_camera's: the near sheet stays).
- Not touched: d_splash_camera (its logic whole: the sheets), rain.c (streaks already hidden by H3R), line_sph, body_sph, charaspread
  (whole: route), rain_gas_pers, the sound manager's and d_splash_camera's read words (written as before: the acts' prologues run).

## 3. Files
t43/boot/native_b43.h, native_b43.c (new); t43/runtime/host_time.h (OPT2_B43, out of OPT2_DEFAULT); t43/boot/boot.c and boot_x86.c
(g_b43_n counters, the per-turn 'b43 turn' line, x86 MGS2_B43_SEEDLOG); build_gl43/tools/b43_apply.py (gen/{wg,wpl} -> gen43/{wg,wpl}),
sched43.py, mkcfg43.py; build_gl43/tw43.sh, tw_run43.sh, runs43a.sh, pkn43.sh, em43.sh, ...

## 4. Proof (x86 twin) and gates
Arms: off (gl42's keys without bit 21: must be gl42's hashref byte for byte), chk (MGS2_B43_CHECK=1: every call in the B turns translated
and compared with the shadow's prediction: time, seed, lives, copies), sch (the delivery keys: hashref_gl43), bon (B on in every turn
from t50).  Seed log (every act's seeds and time at its start, every seed change by act) equal between off and sch/bon; IOP and route
equal; EE dumps classified; after-deck hashes and pictures (vfb) equal.  PSP: no-PGO build, emulator 62/62 against hashref_gl43 with the
loop apart, digests = twin, heap >= 32 KB, textures refused 0, 3 LOADs; fresh PGO and code order; package in build_gl43/deliver.
