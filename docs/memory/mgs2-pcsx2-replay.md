---
name: mgs2-pcsx2-replay
description: MGS2 logic-timing proof and PCSX2 replay test (2026-11-07/08, mgs2/logic_timing.md): findings, alignment method, splits found, Ffm's choices, test-kit location, next steps
metadata:
  type: project
---

- Request (Ffm 2026-11-07 21:03-21:04): prove logic is step-tied vs delta time with addresses; PCSX2 replay over a Tanker section. Notes: mgs2/logic_timing.md (all numbers); PCSX2 data prof/pcsx2_replay/; twin data prof/hr2_route_i/.
- RESULT [C]: no delta time. Step-tied: movement, camera, AI counters, animation. Vblank-driven: play time, in-game clock (deck stages, from console RTC), w01a seed stepping by SIG_BasicShoot (count parity), cutscene pacing (+ disc readiness). Link block placed by T1 at stage loads (anti_par). PCR0 reads dead. Rule proposed: virtual vblank count = 1 per step, never PSP real time.
- One stage seed shared by guards + effects ([[mgs2-game-random]]) -> FFM CHOSE "Drawing-side cuts only" (2026-11-08 00:09); H2 out.
- Replay method: PCSX2 portable test copy on Ffm's PC (RC session; own BIOS/NVM English, RTC 2026-10-18 12:00, TZ 0, volume 0, PINE 28011, MTVU off, patches off); .p2m2 route shifted per segment; align each stage by its FIRST GAME STEP (not pointer); PCSX2 reads pad 1 frame later (inputs -1); deterministic (pairs identical every time). Compare against the twin with VU0 toward zero (RZ).
- Test kit on Ffm's PC: Desktop\mg2 port\PROJECT\pcsx2_test\ (moved 2026-11-08 17:5x, 539 files 718.7 MB, SHA-256 verified; runners relative paths; ISO from Sources\). Temp copy deleted.
- Next: H9 input check + per-frame db68/6c P35,610-670 and Snake/seed P35,700-830; final pair; then send NOTES/DEVIATIONS text to MGS2 port (it owns those files). Never let the RC session explain results to Ffm.
- 18:0x route_s9 vs s8: all 70 w01a inputs exactly 9 frames apart -> recording is right; H9's split must come from elsewhere (w00a timing changed actor heap address 0x13d6c40). R9 per-frame run going.
- R9 (18:2x): no lost step; H9 vs G8 differs only by w00a 2nd-stay input timing; asked twin: first R9 diff frame + w00a inputs +-8 vb test.
- #182 (18:51): R9 leaves twin at 36,493: wall plane x=6542.0, PCSX2 exactly on it, twin 1 ulp short -> collision differs. Root = 1-ulp start at 31,707. U9 asked: PCSX2 step vector actor+0x50 at twin 31,707.
- B9 (19:1x): PCSX2 step x/z = 0 exactly at twin 31,706-707 (twin 6.5e-9 / 7.6e-6) -> difference in motion root delta math. Asked MGS2 port to trace ops.
- STOP ALL (Ffm 2026-11-08 19:31): everything on hold. Open when resumed: MGS2 port trace of root-motion delta (w01a 0x39fef0/0x3ab1f0) at twin 31,706-707 (PCSX2 step 0, twin tiny); then final PCSX2 pair; then NOTES/DEVIATIONS text to MGS2 port. RC idle, kit in PROJECT\pcsx2_test.
- RESUMED (Ffm 'continuie' in the logic thread, 2026-11-10 15:32): #183 asked again of MGS2 port (Ffm's message attached; covers my asks only). NOTES/DEVIATIONS draft in my scratchpad notes_dev_draft.md (E1 parity fix, E3 replay results, E4 arithmetic model, NOTES Q5/Q6 + rows); fill the #183 line, then send.
- STOPPED AGAIN (Ffm 'stop', 2026-11-10 15:38): #183 put on hold again at MGS2 port; nothing running; wait for Ffm's word.
