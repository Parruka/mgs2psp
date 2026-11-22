MGS2 harness gl6 codec speed-ups (MGS2T_gl6) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl5 with four exact speed-ups of the picture work in the codec calls, made on the PC while gl5 waited for its run. The
texture map remembers an area's identity block by block, answers the "is this a render target?" question from its flags alone,
keeps a palette's identity with the load that filled it, and the faces' V4-8 vertex data is unpacked directly. On your PSP gl5 took
132 ms a codec step, as the cost model said (132.5); the model says these four bring it to 115.6 ms (13% less) [I until the PSP
says]. The pictures and the game state must be exactly gl5's: the game's code is gl5's, only the picture code and the log's names
changed. Same route, pack, pad and hash references as gl5. Crash catcher 3b as in gl5; the Media Engine is off.

Why: the PSP's own answer for each speed-up. From vblank 1,000 to the end six settings take turns every 100 vblanks: all four
on, each one off in turn, and all four off (as gl5). Three of the four help every texture lookup, so the opening and the deck are
timed too, not only the calls. One run then times each of them on the PSP, while the 205 frame digests of the first call and the
game's hashes must stay the PC's in every setting.

What you will see: the same as gl5. On the PC emulator that draws exactly what your PSP draws, gl6's pictures are gl5's byte for
byte (gl5's six shots from your PSP equal that emulator's byte for byte too). It runs about an hour, a little less than gl5's 63
minutes.

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl6 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl6/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack is gl5's,
   so the gl5 folder can stay or go.
3. Power: the charger, or the OSTENT battery (topping it up over USB, as in gl5's run, is fine). Please not the black "battery pack"
   (gh-168). Please tell me which one you used.
4. Start "MGS2 harness gl6 codec speed-ups" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the six shot_*.bmp files (copy them to the PC, for example into LOGS\gl6, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
