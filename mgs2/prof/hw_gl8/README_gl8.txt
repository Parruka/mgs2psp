MGS2 harness gl8 faster picture (MGS2T_gl8) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl7 (whose run went to the end, exact) with the picture code built the way the game's code already is. Until now the
picture code was compiled without a profile of its own run and placed wherever the linker put it, and most of its time on the PSP
goes to fetching its code again from memory. gl8 builds it from a profile of a PC run of this route and orders the whole program's
code so that what runs together does not push itself out of the PSP's 16 KB code cache. The rooms' meshes are also read straight
from the game's data (renderer_notes 11.21). Nothing the game sees changes: on the PC this build gives the same state hashes, the
same frame checks and the same 13 shots as gl7, byte for byte, over the whole route.

Why: one answer, how much faster it is on the PSP. The PC's cost model says about 17% less time a step on the aft deck and about
21% less inside against gl7, and about 18% less in the codec calls. The model has read savings of this kind well before (v26's
code order did on the PSP what it said), but this is the first time the picture code is ordered, so the PSP decides. The route and
every setting are gl7's, so each 100-vblank window compares directly with the same window of gl7's run.

What you will see: the same as gl7 (the logos, title and menus; a black opening apart from some text; the codec screen about
minutes 15 to 20 and again about 30 to 33; a plain grey picture inside a viewfinder between the two calls; Snake on the aft deck in
the rain; then indoors: the corridors, the guard in the hallway, the alert and the evasion), a little quicker. gl7 took 59.7
minutes; gl8 should take about 52 [I].

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl8 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl8/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack, pad and
   hash reference are gl7's. The gl7 folder can stay or go.
3. Power: the charger, or the OSTENT battery (topping it up over USB is fine). Please not the black "battery pack" (gh-168).
   Please tell me which one you used.
4. Start "MGS2 harness gl8 faster picture" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the 13 shot_*.bmp files (copy them to the PC, for example into LOGS\gl8, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
