MGS2 harness gl4c codec test route (MGS2T_gl4c) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl4b on a new route made to test the codec, with your SELECT idea. The opening is the same as before up to the aft deck
(about minute 38); then the harness presses SELECT, which opens the codec, and calls Otacon with O 15 times, one call after another,
to the end: about half an hour of codec, about four times as long as the two story calls together. The game's code is gl4b's plus
the code of the 279 game functions these calls run (no earlier route reached them, so they were not in the build); nothing else
changed. Crash catcher 3b as in gl4b; the Media Engine is off; the game's state hashes must stay the PC's.

Why: every switch-off so far came in a codec call (v24, gl1, gl3 and gl4: four stops, all inside the two story calls, while the
other runs passed them). If the codec is what sets it off, this route should stop somewhere in its half hour of calls [I]. If it
runs to the end on battery, the codec alone is not enough, and the next step looks elsewhere.

What you will see: up to the aft deck the same as gl4b: the logos, title and menus; a black opening apart from some text; the codec
screen about minutes 15 to 21 and again about 30 to 33; a plain grey picture inside a viewfinder about minute 28; Snake on the aft
deck in the rain from about minute 35. At about minute 38 the codec opens and stays open to the end: the frequency 141.12 in the
middle; during each call two dark windows where the faces go and a text box with a plain grey block (faces and text are not drawn
yet, as in the story calls); between calls "PRESS SELECT BUTTON TO EXIT" at the bottom. The screen changes little for about half an
hour; that is expected. The calls cost a little more per frame than the story calls, so the end should come about 70 minutes in [I].

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl4c from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl4c/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack is new (it
   holds the codec data the calls read), so please copy the whole folder. The gl4b folder can stay or go.
3. Charge the battery fully, then pull the USB cable out. Please run it on battery, like the runs that stopped, so that the route is
   the only change; if it stops, a second run on the charger will tell whether the power matters.
4. Start "MGS2 harness gl4c codec test route" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed, catcher 3b passed it in gl4b.
5. Let it run to the end, about 70 minutes [I]; it uses about half the battery. Do not put the PSP to sleep. HOME quits at any
   time. Optional but very useful: film the screen with a phone from minute 38 to the end. If it stops, the last seconds of the
   video show whether a red record, a frozen picture or an instant black screen came first, which the files cannot tell.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red record
   shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files (copy them to the PC, for example into LOGS\gl4c, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
