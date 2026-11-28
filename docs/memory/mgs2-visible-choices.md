---
name: mgs2-visible-choices
description: Ffm's words on visible MGS2 changes: rain-off consent (keep the near rain lines), deferred trail and shadow wishes, rain dice; check before any visible cut or look change
metadata:
  type: feedback
  modified: 2026-11-22T18:15:44.680Z
---
- RAIN OFF OK (Ffm 2026-11-22 13:03): "If we can remove the rain in the deck and let just the close camera sprite be im ok with that if we gain fps". 13:05 "B" = keep the rain lines nearest the camera (the near drop sheets are d_splash_camera's, not rain.c's: gl43 agent 2026-11-24; all sheets ~1.1 ms ME a picture [C model]); splashes, ripples and far sheets may go. Gain small (rain drawing ~3-4 ME, rain logic ~5 main a PSP step; I first said 9.5, corrected 13:42). gl38 turn K4 measures it; make it a default if it pays, logged in DEVIATIONS.md and told to Ffm.
  **Why:** Ffm prefers fps over rain detail on the deck. **How to apply:** a game-side rain cut keeps the guards' dice unchanged (Ffm's "separate dice" idea, 2026-11-21 21:54) unless Ffm says otherwise; an exact dice form needs a new hashref.
- LOOK WISHES, NOT NOW (Ffm 2026-11-20 23:20-23:26): shadows "a bit lighter and softer"; "reduce the motion blur for half" (= trail blend 25%, DEVIATIONS B2). "leave the trail and shadows for wen the game is running better, so i can also see better": do them once the deck runs faster, as cfg keys (calibrate on r29cap/).
- Test turns that leave drawing out (gl38 K1-K7) show incomplete pictures by design: tell Ffm before the run.
