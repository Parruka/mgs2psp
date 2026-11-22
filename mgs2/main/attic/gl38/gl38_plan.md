# gl38 plan: a measurement build. Each kind of drawing left out in its own turns (bounds), new timers, F1 in turns, on top of gl37's parts; 2026-11-22 13:45 UTC

Lead plan for the MGS2 port thread. Why: mgs2/plan_20fps_2026-11-22.md (read it, and its Erratum at the end) shows that 20 fps in the rain is out of reach of every lever we
can cost, and that about ten of its figures are [I] or UNKNOWN. gl38 turns them into measured bounds on the PSP before we spend more builds. gl37 (main/attic/gl37/report.txt,
fix_design.md) stopped at its design checkpoint: the fixed-work lever reaches only -12..-13.5%; its re-use rule and cull-always-on are gated. gl37 is not shipped alone: it is gl38's base.

UNITS: the cost model's "ms a step" is ms a picture (2 PSP steps); PSP logs' "ms a step" is a vblank. Halve model figures before comparing with the PSP, and say which unit you use.

## What gl38 is
1. Tree /home/claude/t38 = /home/claude/t37 (gl37: re-use rule MGS2_NAT_RRULE=1, MGS2_NAT_CULL=1 always). Build folder /home/claude/main/build_gl38; notes attic/gl38.
2. BOUND TURNS (the point of the build). A new cycle key (e.g. MGS2_BOUND_CYCLE) from vb 24,000 picks, per 100-vblank turn, one kind of drawing that is NOT drawn:
   K0 all drawn; K1 ship (native chain 3) off; K2 people (chain 2) off; K3 sorted prims/meshes (chain 6) except the rain off; K4 the rain off EXCEPT the drop-sheet (rain.c)
   lines nearest the camera (the owner's choice 2026-11-22 13:05; this is also a candidate default later, so make it its own clean switch and find which sheet calls are the near ones,
   from code or data; if the sheets cannot be told apart by distance, keep all sheets and say so); K5 the stage blur / post program (chain 10) off; K6 ship + people + chain 6 + rain all off
   (the "is 20 fps closed?" bound); K7 nothing drawn by the ME (the main CPU's logic with the ME resting: the coupling question).
   - Every kind off must keep the GAME STATE exact: hashes equal to the twin's c9ddb593 route reference in every arm (draw-side only). Prove it on the twin and the emulator. The pictures
     in K1-K7 turns are incomplete by design; the owner will be told before the run.
   - The rain wait (29,200-30,400) and walk (30,800-31,400) have few turns. Get at least 2 turns per kind in the rain wait where you can: you may make every deck turn an ME turn
     (drop the ME/main alternation from 24,000) if nothing else needs main turns, and you may order the cycle so the rain scenes get the kinds that matter (K0, K1, K2, K3, K4, K6, K7).
     Say exactly which vblanks carry which kind (a table in the README) so the reader can split them.
3. TIMERS: split the ME's list build (~17 ms a step [C]), records and hooks; time a people call (134 us [C]) by stage as nat split does for the ship. Keep gl36's nat split / me chains lines.
4. F1 (gl37 fix_design.md option A: skip the VU writes of groups a later group overwrites, for runs that are not replayed) behind its own switch, in turns among the K0 turns if the
   schedule allows, otherwise on in every turn with its twin/emulator check mode 0 differing. Exact by gl36's proof; prove it again in check mode.
5. RETRAIN PGO + code order with all arms (new code), order search guarded on every window (none slower than gl36 by more than 0.25 ms a PSP step [C model halved]).
6. cfg otherwise = gl37's (MGS2_OPT_CYCLE must stay or every cycle is ignored; in place everywhere; FLIPWAIT_FROM=0; DZ X whole run; live captures; short run to 31,400).

## Gates
As gl37's: twin 685/685 in every arm; frames equal between arms in every game-state column; read hash; emulator 62/62 + 916/916 = twin for the K0 turns and the expected
digests for K1-K7 turns (or: list digests are compared only in K0 turns, and say so); pictures of K0 turns vs gl36's emulator pictures; nat problem counters 0; check modes
0 differing; boot heap low point not below gl36's 64 KB; same system calls as gl36, clock 333, no new hardware writes, nothing near the flash. Package build_gl38/deliver with README
(the kind-by-vblank table, what the owner will see), MANIFEST, rd38pc.py (stdlib: per scene and processor, each kind's ms a PSP step against the K0 turns on the same build, plus
the new timers), and a draft "Build gl38" harness entry in attic/gl38/report.txt. Do NOT deliver: the lead verifies and delivers.

## Limits
No PSP, nothing on the owner's PC; game data and binaries stay in the container. Write only under /home/claude/t38, /home/claude/main/build_gl38 and attic/gl38. Disk ~4 GB free:
stay under ~3 GB; delete your own emulator picture folders after their checks (literal paths, after looking). Kill only your own processes by PID. python3 -I. Every figure marked
[C], [C model] (say the unit), [I] or UNKNOWN.
