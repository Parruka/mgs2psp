MGS2 harness gl4c-p0 codec test route, picture off in the calls (MGS2T_gl4c, new mgs2.cfg only) - personal use only, do not share
(it contains code built from your MGS2 disc).

What it is: exactly gl4c (the same EBOOT, pack and route) with one change in mgs2.cfg: from vblank 30,500, just before the codec opens
(about minute 38), the live picture is off. The game, the sound mix and the translator still run as before and the state hashes must
still be the PC's, but no frame is built, no texture is converted, the GE draws nothing and no new frame is shown.

Why: [FILL AT DELIVERY: the result of gl4c run 2 on the charger and what this run tells apart]

What you will see: up to about minute 38 the same as gl4c. From then on the screen keeps the last frame before the codec, to the end:
Snake on the aft deck in the rain, the empty radar box at the top right and the pistol (16/46) at the bottom right. The codec still
opens and Otacon is still called 15 times, but you will not see it; the picture does not change for the rest of the run, which is
expected. The end comes at about the same time as gl4c's, about 70 minutes in [I] (the picture took only ~2 ms of a ~66 ms step
in the calls).

1. Back up your memory stick first if you have not since the last run.
2. Copy only the new mgs2.cfg from the PC folder MGS2T_gl4c_p0 over ms0:/PSP/GAME/MGS2T_gl4c/mgs2.cfg (every other file stays as it
   is on the memory stick). Send the old run's files first if they are still only on the memory stick.
3. [FILL AT DELIVERY: battery or charger, as the result calls for]
4. Start "MGS2 harness gl4c codec test route" from the Game menu. At the crash catcher's question press O (or wait 15 seconds).
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time. If you can, glance at the screen from minute 38; if
   it stops, say whether the screen froze first or went black at once.
6. If it stops: note about which minute, photograph the screen if a red record shows, and send log.txt, battery.txt and crash.txt.
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt and steps.csv
   (the shot files will all show the same frame; they are not needed).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
