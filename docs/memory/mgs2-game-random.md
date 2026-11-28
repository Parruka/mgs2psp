---
name: mgs2-game-random
description: MGS2 stage random seeds shared by guards and effect actors, SIG_BasicShoot, clock actor fields; why effect cuts must keep every seed step
metadata:
  type: project
  modified: 2026-11-08T03:05:17.947Z
---

**Stage seeds (route i census 2026-11-07, prof/hr2_route_i/seed_users_route_i.txt):** w01a 0x1fbd74, w00a 0x1fc954 (inside each stage image's data). Inlined step x = x*0x5d588b65+1 in 274 w01a functions; w01a 0x3a4328 = srand(a0). One word per stage, shared by guards (attacker.c, watcher.c, eneequip.c, usp.c, command.c; KR_RandS 0x22a8d0 / KR_RandU 0x22a878 in w01a) and cosmetic actors (deck: item_box 987,648 steps, body_sph 731,692, d_splash_floor 306,229, chim_smo 293,776, charaspread 120,504, d_splash_camera, d_splash_fall, ripple, rain.c; w01a: cloth/hair, charaspread, eyeanim, steam_smoke, win_rain, puddle).
**Why:** any change in how often an effect actor runs or how many particles draw changes every later guard value. **How to apply:** effect cuts must keep each step (exact skips, draw-side filters); game-side effect thinning is a gameplay deviation. FFM CHOSE "Drawing-side cuts only" (card 2026-11-08 00:09): no game-side effect thinning, H2 out.

**SIG_BasicShoot** (w01a 0x2b4238 = w00a 0x2b9478; act chain to "attacker.c", w01a act 0x29d948 init 0x29e260, w00a act 0x2a2b88 init 0x2a34a0): period 4/2/1 from owner+0xf2c bits 0x8/0x10 (owner obj+16); steps the seed when 0x18db64 % period == 0 unless bit 15 of 0x1ed040; calls ENE_ShootBullet (w01a 0x251ed8 = w00a 0x25b420) when bits 8-9 of the new seed are 0; obj+672 counts shots.

**Guard fire chain (w01a, #174, 2026-11-08):** sight test 0x2527e8 once a vb on guard work g: +392 dist, +396 direction to target, +398 result (0 no, 1 seen, 3 in cone but line blocked), +404 near = range, +408 far = 1.5 x range, head yaw +576 (stored after the test by 0x255538, so the test uses last vb's), half view +580 = 512 (45 deg on a 4096 circle), pitch <= 512, line test 0x24fb98. SIG_RandShoot 0x2b4318 runs each vb in the shooting action (flag word +852 set by SetMoveMode 0x2b33b8): needs +672 < +676 and +398 != 0, body yaw (*(g+4))+42 within 90 deg of +396; dist > 6000 -> SIG_SetRandFvec 0x2b4058 (3 seed steps) then SIG_BasicShoot. Route i: first shot 37,498 when head-to-Snake fell to 510; 0x18db64 checked = start-of-vb value + 1.

**Clock actor** (w00a act 0x2d16b0 init 0x2d2468; w01a act 0x2d09c8 init 0x2d1780): +72 last vb, +76 day length (4,320,000), +80 elapsed in day, +84..+91 console-clock copy, +92 year, +96 month, +100 day, +104 hour, +108 minute, +112 second, +116 vb for seconds. On route i only its own act reads the date fields (hour/minute/second only). 0x1cc8b8 (console clock copy) written at boot and by unaligned stores at stage loads only; the act re-syncs when bytes 1,2,3,5,6,7 differ. Twin RTC fixed 2026-10-18 12:00:00.

Tools (container only): scratchpad dis/actors.py (named actors via GV_SetNamedActor 0x13f378), callers.py, xref.py; boot_lc.c census in /home/claude/lcen3 (MGS2_LCEN_R/_CLK/_W). Related: [[mgs2-h1]], [[mgs2-halfrate-study]].
