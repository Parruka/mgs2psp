MGS2 harness gl5 codec faces (MGS2T_gl5) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl4c with the codec faces. The game draws the faces in a codec call with its own small vector program (VU1 program
7); gl5 has the converter for that program, so Otacon's and Snake's faces are now drawn on the PSP, and the text box shows the
words instead of the plain grey block. The route is gl4c's (the same pad.txt, pack and hash references), but it ends at vblank
40,000 instead of 60,000: four Otacon calls instead of fifteen, because each call now costs more. The game's code is gl4c's;
only the picture code changed. Crash catcher 3b as in gl4c; the Media Engine is off; the game's state hashes must stay the PC's.

Why: two things only the PSP can tell. First, whether the faces come out exact on the PSP: during the first Otacon call the
harness takes a digest of each frame's drawing (205 frames), which must equal the PC's. Second, what the faces really cost on the
PSP: the cost model says they nearly double the time of a codec frame, and the PSP's answer sets the order of the exact speed-ups
planned for Friday.

What you will see (from the emulator): the logos, title and menus; a black opening apart from some text, as in gl4c; the codec
screen of the first story call about minutes 16 to 23, now with Otacon's face on the left, Snake's on the right and the words in
the text box; a plain grey picture inside a viewfinder about minute 31; the second story call about minutes 34 to 39, with faces
and words; Snake on the aft deck in the rain. At about minute 44 the codec opens: the frequency 141.12 in the middle; in each call
the two faces talk and the words appear in the box; between calls "PRESS SELECT BUTTON TO EXIT" at the bottom. The background
behind the faces and the box is teal instead of near black: a known gap (the game's glow images are not drawn yet), not a fault of
this run; a faint thin line under the words (also faint between calls) is being checked against the PS2's picture. The faces
make the calls slower than in gl4c, so the end should come about an hour in (58 to 66 minutes) [I].

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl5 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl5/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack is gl4c's,
   so the gl4c folder can stay or go.
3. Power: the charger, or the OSTENT battery charged with the cable out. Please not the black "battery pack" (gh-168): every stop
   came on it. Please tell me which one you used.
4. Start "MGS2 harness gl5 codec faces" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end, about an hour [I]; on a battery it uses about a third of a charge [I]. Do not put the PSP to sleep. HOME
   quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files (copy them to the PC, for example into LOGS\gl5, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
