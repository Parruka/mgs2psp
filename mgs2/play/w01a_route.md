# Indoor free-play route: Deck-A crew's quarters (w01a), facts

Task #33. Probe: the x86 fork server in the container (/home/claude/play2, ephemeral: serve3.sh, scen2.py, guard.py, gdev.py, actors.py,
sheet2.py). Pad codes: U 1000, R 2000, D 4000, L 8000, X 0040, O 0020, SQ 0080, TR 0010, R1 0008. Controls are camera-relative; in
both areas U = -z, D = +z, L = -x, R = +x. Yaw: short at +0x22 of the position block, 4096 a turn, 0 = +z, 1024 = +x, 2048 = -z.

## Getting in (2026-10-25)
- From free play's aft deck (`pad_free.txt`, then this pad from vb 30501), unseen, pad only: `w01a_entry_pad.txt` (copy of
  play2/run/pad_q30b.txt): L 200, U 154, L 40, U 78, R 52, U 38, L 3, U 2, then triangle taps every 4 vb from 31082 to 31320 at
  the hatch (the action zone snaps Snake to it; the hatch opens after about 100 vb of mashing). Works with every start shifted by
  -2..+2 vb (3 of 3 checks). Door A on the deck stays shut.
- Snake stands inside at (-8750, 972, -5750), yaw 2048, from about vb 31800 (fork point of the probe).

## Layout and the guard (coordinates x, z)
- Port hallway x about -5500, z -6700 .. 7551 (no guard). Aft corridor z about 7000, x -6551 .. 6551. Starboard hallway x about
  6000, z -6500 .. 7000.
- One guard (watcher.c) walks the starboard hallway at x 6000-6005 between z -6030 (turns to yaw 1024 and waits) and 3895 (faces
  yaw 0 and waits); one loop is about 900 vb, unchanged by Snake until Snake comes near (draft ia1: first change vb 36000).
- Actors in w01a (EE addresses, the same in every fork from vb 31800): raiden.c 0x13d6c70 (position block 0x13d6cb0), watcher.c
  0x1420d90 (0x1420dd0), command.c 0x1420830, wccomm.c 0x1420880, radar.c 0x13d4c80, lockerd.c 0x14eb440, bgmanage.c 0x1478a90,
  attacker.c x2 at (+-500, 1135, -13000). 159 actors at vb 31900.
- Walkthrough (reference only): the guard's beat ends at the locker room on Normal; lockers hold items and can hide Snake; the
  sliding door at the far end leads to the lounge (w01b, a cutscene). The route must stay in w01a.

## Draft ia1 (vb 31800-36500)
- Stand to 32300; run R 52, D 150, U 150, D 150 in the port hallway; X, crawl U 360, X; six O taps; SQ 120; R1 600 with L, R, U, D
  looks; then D 200, R 250, U 300, D 300, L 300, R 300, U 300 (34250-36200).
- All parts worked in the port hallway. The last U run went up the starboard hallway into the guard at its bow end (vb about
  36100): he noticed Snake, blood on the wall (blood.c, blood_part1.c, 3 vibrate.c appear), Snake's life bar shows and Snake lies on
  the floor at (6351, 193, -6519) from vb 36200; the guard walks back aft. **Not an alert:** the radar stays on in every frame and
  the ELF's alert words below keep their normal values (dumps at vb 35900, 36150, 36300, 36500).

## Alert state (found 2026-10-25 on the deck, ELF globals so the same in every stage)
- Known alert: the old deck session tour2 (play/batch4.txt, fork vb 30500 of pad_free: L 600, U 600, R 600, D 600 from 30510).
  Frames: "It's the enemy! I need help!" at vb 31303, then a red ALERT / RADAR NO USE box in place of the radar from 31603 to the
  end (33103); Snake's life falls.
- Peeked every 10 vb: **0x1bcacc** 2 in normal play, 1 from vb 31540 (radar off, the ALERT box), 6 at 32670-32830, 1 again from
  32840. **0x1e10b4** 0x00c00a60 in normal play, 0x00c03030 from 31360 (the guard's call); 0x000200xx for a few peeks at 30760-30790
  and 32700-32740 (short transitions, not understood). w00a stage words: 0x1f4170 0 -> 1 at 31360; 0x4193f8 0 -> 1 at 31360 -> 5 at
  31520; 0x1f9b00 0 -> 1 at 31880 -> 2 at 33170. In the ELF region only 8 words hold one value in all 9 non-alert dumps (deck and w01a)
  and another in both alert dumps: these two and 6 pointers (0x1d2220, 0x1d2224, 0x1ed344, 0x1ed454, 0x1ed5a4, 0x1ed6a4).
- **Correction (indoor route, below): 0x1bcacc is not an alert flag.** In w01a it is 1 from the area's entry to vb 32,320 (radar off
  while the area starts) and 1 at 36,910-37,440, but 2 while the frames show the ALERT box at 36,803 and again at 37,603-37,803.
  0x1e10b4 = 0x00c03030 from the guard's call (31,360 on the deck, 36,720 here) is the earliest sign seen, not proved either. **Frames
  are the check for an alert.**
- **The deck timing route (v25f's pad, used by every free-play build since) has no alert:** its last part (vb 34201-36500, "into
  the guards' view") shows the normal radar in 17 frames from 34203 to 36302, and 0x1bcacc = 2 at vb 34700 and 35000. Snake walks
  into the wall under the stairs while the guards patrol. The timings stand; the label "alert" for that part and its model window
  (35501-35600) was wrong (the mix reused tour2's directions from a different start point and never checked the frames).

## The timing route (2026-10-25; pad `pad_w01af.txt`, made by `w01a/ig.py` route() with F = 31800)
- `pad_w01af.txt` (md5 7bc72aceb89b192298c635ef52b81b8f) = pad_free.txt to vb 30500, the entry pad (30501-31800), then the inside part
  (`w01a/pad_inside.txt`) from 32300. Timed steps planned for the PSP harness: vb 31,801-37,800 (6,000 vblanks, 2 minutes).
- Parts (vb): stand 31,800-32,300 (the radar comes on at 32,320); walk 32,300-34,110 (port hallway laps R 52, D 150, U 150, D 150,
  U 150, D 179 into the aft corridor, R 80 and L 80 along it, U 150, D 150, U 150, D 135; ends standing at about (-5480, 5408));
  crawl 34,110-34,510 (X, U 360, X); six punches 34,530-34,690; fire 34,720-34,840 (SQ 120); first person 34,900-35,500 (R1 600 with
  L, R, U, D looks); sneak 35,520-36,440 (D 110, R 130: down to the aft corridor and east; Snake waits at (3961, 6904), out of the
  guard's sight); alert 36,440-37,800 (R 60, U 16 round the corner into the starboard hallway; the guard sees Snake and calls it in;
  D 20, L 260 back west along the aft corridor; U / D laps of 150 from 37,106).
- Frames (ig1c, every 200 vb from 36,403): normal radar at 36,403 and 36,603; ALERT / RADAR NO USE and "A backup unit is on its way to
  Deck-A, crew's quarters." at 36,803; ALERT at 37,003; EVASION at 37,203 and 37,403; ALERT again at 37,603 ("Send additional
  personnel.") and 37,803 ("Search team to Deck-A, port side", a backup soldier next to Snake; Snake alive, life bar part full).
- Survival (earlier probes): standing in the guard's fire kills Snake in about 700 vb; running away survives about 1,750-2,150 vb.
- Shifted by -2 and +2 vb (ig2-ig5: the walk's second half, and the alert entry): 0x1e10b4 = 0x00c03030 at 36,720 and 0x1bcacc = 1
  at 36,910 in all five; ig2 (entry 2 vb early) differs after 37,440 (0x1bcacc stays 1 to the end).
- Coverage: the route's functions joined v26's list as `recomp/cov/cov_x30.txt` (+41 ELF, +91 w00a, +557 w01a); build v30 (psp/v30.sh).
