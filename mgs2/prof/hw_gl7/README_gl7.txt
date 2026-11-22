MGS2 harness gl7 codec background (MGS2T_gl7) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl6 (whose run went to the end, exact) with the codec screen's background fixed, on gl4's indoor route. On your PSP the
codec screen had a teal wash: the game draws its background from two 16-bit noise images and a black glow, which the picture code
did not draw as the PS2 does. Now it does: near black with a faint noise, as on the PS2 (side by side in your PC folder,
SHOTS\codec_background_fix). Also, the log's data totals are counted in 64 bits (gl6's printed a negative number). The game's code
is gl6's; the game's state hashes must stay the PC's. Crash catcher 3b as in gl6; the Media Engine is off.

Why: three answers from one run.
1. The background on your PSP: the first three shots are the frames of SHOTS\codec_background_fix.
2. gl6's four codec speed-ups on the deck and inside, where three of them also work (every texture lookup): from vblank 1,000 to
   the end, all four on and all four off take turns every 100 vblanks.
3. The fix's own cost: the two story codec calls are the same game in gl6's run and in this one, and gl6's run had all four on and
   all four off in the same windows, so those windows give it directly. The PC's cost model says about 2.6 ms more a codec step,
   less on the PSP, whose cache misses cost less than the model's [I].

What you will see: the same as gl6, but on gl4's route and with the new codec background. The logos, title and menus; a black
opening apart from some text; the codec screen about minutes 15 to 24 and again about 34 to 39, with the faces, the text and the
near-black background; a plain grey picture inside a viewfinder between the two calls; from about minute 39 Snake on the aft deck
in the rain; from about minute 47 indoors: Snake in the corridors, the guard in the hallway, the alert and the evasion. Still
missing: the radar's map, the viewfinder's picture, and a few surfaces close to the camera show as flat colour; in the codec box a
faint line near its bottom edge (task #84). gl4b took 53 minutes on this route; gl7 should take about an hour [I], as the codec
faces are drawn now and half of the time the four speed-ups are off.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl7 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl7/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack, pad and
   hash reference are gl4's (the indoor route), not gl6's. The gl6 folder can stay or go.
3. Power: the charger, or the OSTENT battery (topping it up over USB is fine). Please not the black "battery pack" (gh-168).
   Please tell me which one you used.
4. Start "MGS2 harness gl7 codec background" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the 13 shot_*.bmp files (copy them to the PC, for example into LOGS\gl7, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
