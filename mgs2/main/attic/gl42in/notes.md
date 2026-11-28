# gl42in: the indoor measurement on gl42's EBOOT (2026-11-27)

Ffm 14:25: "want to start measuring stuff inside, like we were doing in the beginning?"  Lead decision: yes, with gl42 as it is on the
stick; only mgs2.cfg and hashref.txt change.  Last indoor PSP figure before this: gl20 (2026-11-19) 89.58 ms a step, main turns [C].

Facts found [C]:
- The test route (pad_w01af, 7bc72ace) already goes indoors (stage w01a loads after vb 31,400; timed 31,800-37,800); every run since
  gl25 stops at vb 31,400.  hashref lines already reach 37,800.
- vblanks 37800 with deck 24000: the step record (g_steps, 56 B a step) grows 414 -> 773 KB, the texture table's 128 KB at vb 536 is
  refused, EVERY texture refused, hashes still equal (emu in1: 124/124, refused 7,296,322).  Fix: deck 31400 (358 KB; emu in2: refused
  0, room 193 KB at the table, 159 KB worst).
- The digit cycles (BOUND, NGE, F1, DIG, XP) do NOT wrap: past their list a turn runs the defaults (kind 0, NGE off, F1 0, XP 0, every
  turn an ME turn).  The lists hold 128 turns at most (BD_CYC_N, boot/nat_stub.h:212).  OPT/OPT2/HALF word cycles wrap.
- Indoor schedule (tools/sched42in.py): t74-t77 K-A, t78-t127 X Y X Y X (X = A2ARANA2AR, Y = A2ARANA2A7), digests + XP 7 in t89 and
  t119 (GL_DIG 29490:36000), t128-t137 defaults ('D').
- Twin arm 'in' (build_gl42/tw, 221 s): against hashref_gl42 26 of 685 lines differ, all in people-native indoor turns (vb 32,000 on);
  hashref_gl42in.txt 5d6b129f.  Emulator in3 (731 s): 124/124 hashes, textures refused 0, no room 0, heap 159 KB worst, schedule check
  none, problem counters none, XP check differing 0 (1,720 plans); k0cmp42 vs the twin: indoor digest turns 100 frames, content differs 0
  (deck 61, 0).  Emulator inputs = the delivered files (cfg 5b0fddb0, hashref 5d6b129f).
- Delivery: zip MGS2T_gl42in_files.zip (0c4977b8): mgs2.cfg, hashref.txt, rd42in.py 7b990600, README_gl42in.txt, MANIFEST.md5 (18 files).
- Emulator timings are not PSP timings.  PSP run: UNKNOWN until read.

## PSP r1, 2026-11-27 16:29:45 UTC: froze, then the PSP off, at the door indoors; the log is LOST
- Ffm 17:44: "the game crashed when entering interior zone, image froze, psp went off, fresh psp restart".
- Fetched to LOGS\gl42in (55 files, md5-equal to the stick) [C]: log.txt (264 lines, 5cc0ea72), battery.txt and crash.txt are from a
  SECOND start of gl42in (15 s: battery 71% / 4092 mV, 'power-switch' + 'suspending' at t 14 s, vb -1), which overwrote the stopped run's
  files.  From the stopped run only pictures: shots to shot_31262.bmp, 25 live shots 30,512-31,287 (md5s all differ from gl42 r2's:
  UNKNOWN whether the pictures differ to the eye; live copies are timing-dependent).  So r1 reached >= vb 31,287; the emulator loads
  stage w01a at vb ~31,559 (turn t75, a main turn).
- The second start's boot lines: newlib's heap ends at 0b99bfff (gl42 r2: 0bd9bfff), headroom 7456 KB (gl42 r2: 11552): 4 MB less.
  UNKNOWN why (a start from the XMB instead of pclink.prx? [I]); says nothing certain about r1, which was launched as always.
- Cause of the stop: UNKNOWN.  Two candidates: the no-record power cut on battery (stops at vb 29,561-31,040 in gl29, gl29x, gl42 r1),
  or a fault at the stage change (no build since gl20 passed vb 31,400 on the PSP).  Next: r2, the same files, wall charger in, logs
  to LOGS\gl42in\r2; after a stop only PSPLink is started.
- Harness lesson: a start must never overwrite a log without its end line (keep it as log_prev / a numbered folder).  Owed, next build.
- Ffm 17:47-17:50: the second short start was their mistake (by hand); r1 froze "after snake open the door in the deck to go inside";
  "the psp went off by itself after the image freeze" (the same signature as the earlier no-record stops: freeze, then off).
  Charger in or not during r1: not answered yet.  r2 asked for by Ffm 17:49 ("run gl42in", logs LOGS\gl42in\r2).
