---
name: mgs2-consents
description: Every consent Ffm gave for the MGS2 port (visual trail, VFPU, clock model, FPU denormals, ME audio, register writes, downgrades), with dates and limits
metadata:
  type: feedback
  modified: 2026-10-27T20:18:40.576Z
---

- Visual (2026-10-18 23:54, again 2026-10-19 20:14): motion blur / motion trail may be removed if too heavy on the PSP and no fix. Only that.
- VFPU (2026-10-20 00:39, card): the vector unit may do VU0 vector math though its last bit rounds differently; VFPU forms need an Inf/NaN check + exact fallback (speed_notes §22); PPSSPP needs PPSSPP_VFPU_RN=1. Scalar exact comes first; shortcuts proposed with numbers.
- CLOCK "A" (2026-10-21 00:14): the PS2 renderer takes no game-clock time ([[mgs2-pipeline-clock]]).
- FPU denormals (2026-10-23 12:49, card): keep the PSP rule; each new route or area also gets an x86 PS2-rule check run (speed_notes §40).
- ME AUDIO (2026-10-23 18:28, card A): the sound mix runs on the Media Engine via one kernel-mode call through ARK-4's KUBridge (no clock change, nothing to flash, sleep held off); each ME build is described before it ships; ME render offload stays dropped ([[mgs2-me-audio]]).
- Register writes OKed: 0xBC100044, 0xBC300008, 0xBC300000 (2026-10-26 20:43 "Build it"); 0xBC100050 = 7 and VME 0xBCC00010/70/30/40 (2026-10-27 00:33 "All of Sony's"). Anything else needs a new OK.
- DOWNGRADES (2026-10-24 11:45): allowed in principle; each one is shown with numbers, pictures and the best alternative and needs Ffm's OK before it ships. Ffm 2026-10-28 14:07 (project chat, on the 25 fps outlook): if it is that heavy they will reduce and cut heavy effects, "but always let me know the options first"; plan a costed list of the deck's weather-effect cuts when the time comes. Ffm 2026-10-31 18:34 (on the roadmap's logic step, weather + ropes ~half the deck's logic): "wen we get there big downgrade if possible and need, we will see how it goes": a BIG cut of the weather/ropes logic is open in principle; still exact native form first, then the costed options (numbers + pictures) for their pick. A logic cut changes the game state: x86 and PSP must still match each other with the same cut.
- A new kernel module is told to Ffm before it ships. sceGeEdramSetSize needs Ffm's OK.
- C1 TEST (2026-11-02 12:51, card "Test in gl14"): the GE skins, lights and maps the codec faces (stagec_notes 1.9) in TURNS in gl14 (on/off on the codec route, PSP shots of both); a test, not yet the default: making it the default needs Ffm's word after gl14's run ([[mgs2-stagec]]).
- HEAVY SCENES RULE (Ffm card 2026-11-07 21:17, "Cut cosmetic effects only"): to reach 25 fps where the original logic can't, weather and effect actors may be lightened in heavy scenes, each logged in mgs2/DEVIATIONS.md with pictures; gameplay logic (AI, physics, timers, input) stays original; no half-rate logic. Each cut still gets Ffm's OK with shots and numbers first.
- EFFECT CUTS NARROWED (Ffm card 2026-11-08 00:09, "Drawing-side cuts only"): guards and effect actors share one stage seed ([[mgs2-game-random]]), so heavy-scene effect cuts must leave state hashes unchanged (draw-side filters, exact skips). No game-side thinning of effect/weather actors; H2 out. Supersedes the 2026-10-31 "big downgrade of weather/ropes logic" opening unless Ffm reopens it.
- E1 NARROWED (Ffm card 2026-11-08 01:54, "Game logic exact"): game logic's VU0 macro-op groups round toward zero as on PS2 (~+2.3-3.2 PSP ms deck [I]); draw pipeline + box cull stay VFPU round-to-nearest, conditional on a PC poison proof that their results never reach game state. Supersedes the 2026-11-07 21:22 "Keep it" for logic. If the proof fails, back to Ffm.
- E1 RESOLVED BY CHOICE (Ffm card 2026-11-08 17:46, "Everything exact"): ALL VU0 math (macro-op groups + box-cull micro programs) rounds toward zero as on PS2, ~+8.6-9.7 PSP ms rainy deck, +3.9-4.5 inside [I]. Supersedes 2026-11-07 21:22 "Keep it" and 2026-11-08 01:54 "Game logic exact". x86 reference becomes the MGS2_VFPU_RZ=1 twin. Later exact work: box-cull leftovers computed only when read (up to ~4.7 ms [I]).
