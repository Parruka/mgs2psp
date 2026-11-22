MGS2 harness gl10 lighter rain (MGS2T_gl10) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl9 (whose run went to the end, exact) with three changes. The draws of Snake, the crew and the guards are now held
and converted together at the end of each transfer (step c, renderer_notes 11.29); five small exact trims in the picture code
(11.28); and the lighter rain you chose: of the big sheets of speckled drops close to the camera and of the splash at Snake's feet,
1 strip in 2 is drawn (11.30). The picture code's profile and code order are made again, since every code change needs them.
Nothing the game sees changes: on the PC this build gives the same state hashes as gl9 over the whole route, and the same
pictures except the rain's drop sheets and the foot splash.

Why: how much faster it is on the PSP. The model says about 6.5% less time a step on the aft deck and 5% less inside (gl9 on your
PSP: deck 169.3 ms, inside 104.0; gl10 should give about 158 and 99) [I], and about the same in the codec calls. The batching and
four of the trims take turns with everything on, 100 vblanks each (mgs2.cfg); everything is on in every other window, the same
windows as in gl9's run, so one run times each part and compares with gl9 on the same scenes. The four codec speed-ups are always
on now (gl6 to gl9 timed them).

What you will see: the same as gl9 (the logos, title and menus; a black opening apart from some text; the codec screen about
minutes 15 to 20 and again about 30 to 33; a plain grey picture inside a viewfinder between the two calls; Snake on the aft deck in
the rain, with half the drop sheets and half the splash; then indoors: the corridors, the guard in the hallway, the alert and the
evasion). gl9 took 52.1 minutes; gl10 should take about 50 [I].

The rain setting: two lines in mgs2.cfg, MGS2_GL_RAIN=2 (the drop sheets) and MGS2_GL_SPLASH=2 (the foot splash). 1 draws all of
them as the PS2 does, 3 draws 1 strip in 3, up to 8. Only the picture changes; the game runs the same with any setting.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl10 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl10/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack, pad and
   hash reference are gl9's. The gl9 folder can stay or go.
3. Power: the OSTENT battery or the charger, as always (topping it up over USB is fine).
4. Start "MGS2 harness gl10 lighter rain" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the 16 shot_*.bmp files (copy them to the PC, for example into LOGS\gl10, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
