MGS2 harness gl9 people lane (MGS2T_gl9) - personal use only, do not share (it contains code built from your MGS2 disc).

What it is: gl8 (whose run went to the end, exact) with the second lane of the native picture work. Snake, the crew and the
guards are now read straight from the game's own data and converted a group at a time, instead of one draw call at a time from
the PS2's vector unit memory (renderer_notes 11.24); the rooms' meshes already were in gl8. The picture code's profile and code
order are made again for both lanes, since every code change needs them (11.25). Nothing the game sees changes: on the PC this
build gives the same state hashes, the same frame checks and the same 13 shots as gl8, byte for byte, over the whole route.

Why: how much faster it is on the PSP, and a check of the cost model refitted on gl8's run. The model now says about 5% less
time a step on the aft deck and inside (deck 172.7 to 164.7 ms, inside 107.0 to 102.2), and about the same in the codec calls,
whose faces have no lane yet. The route and every setting are gl8's (the four codec speed-ups still take turns), so each
100-vblank window compares directly with the same window of gl8's run and gl7's.

What you will see: the same as gl8 (the logos, title and menus; a black opening apart from some text; the codec screen about
minutes 15 to 20 and again about 30 to 33; a plain grey picture inside a viewfinder between the two calls; Snake on the aft deck in
the rain; then indoors: the corridors, the guard in the hallway, the alert and the evasion), a little quicker. gl8 took 53.4
minutes; gl9 should take about 52.6 [I].

1. Back up your memory stick first if you have not since the last run.
2. Copy the folder MGS2T_gl9 from your PC to the memory stick as ms0:/PSP/GAME/MGS2T_gl9/ (EBOOT.PBP, the eight ovl_*.bin files,
   SLES_503.83, mgs2disc.pak, pad.txt, mgs2.cfg, hashref.txt and the memcard0 folder; all checked on your PC). The pack, pad and
   hash reference are gl8's. The gl8 folder can stay or go.
3. Power: the charger, or the OSTENT battery (topping it up over USB is fine). Please not the black "battery pack" (gh-168).
   Please tell me which one you used.
4. Start "MGS2 harness gl9 people lane" from the Game menu. At the crash catcher's question press O (or wait 15 seconds): no
   self-test is needed.
5. Let it run to the end. Do not put the PSP to sleep. HOME quits at any time.
6. If it stops: please note whether the PSP switched itself off and at about which minute, and photograph the screen if a red
   record shows. Then send log.txt, battery.txt and crash.txt (and steps.csv if it is there).
7. At the end the harness's text comes back with "Press X to exit". Please send back log.txt, battery.txt, crash.txt, steps.csv and
   the 13 shot_*.bmp files (copy them to the PC, for example into LOGS\gl9, before a new build is copied over that folder).

As before: the catcher lives only in RAM and is given back before every exit; nothing is written to flash and no power or clock
setting is changed. No hardware register writes, no new module; the Media Engine is not started.
