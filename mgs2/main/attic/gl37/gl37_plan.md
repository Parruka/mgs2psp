# gl37 plan: cull always on + re-use only when it pays (in turns) + a first cut of the ship group's fixed work (in turns) + a retrain with a binding guard, 2026-11-22 12:45 UTC

Lead plan for the MGS2 port thread. gl36's PSP run (mgs2/psp_harness.md "Build gl36", read 12:15; prof/hw_gl36/natsplit_summary.md) was exact (62/62, 916/916 incl. cull-on turns, 52/52 pictures = gl35's). What it says [C, PSP]:
- rain wait (29,200-30,400), ME turns, both levers on: 85.1 ms a step (gl35 88.6). Cull -2.67 (model -4.77). Re-use alone -1.6 ME a step = -5.4 us of 58.6 a native ship group (model -3.1).
- rain walk (30,800-31,400): main-limited; re-use LOSES +0.8..+1.6 ms a step: it replays only 7-27% of the groups there (87% in the wait), and recording + look-ups cost ~5-7 us a group
  (register +2.0/+1.1, A+D +1.4/+1.2, call's rest +1.0; positions not cut). Replay counts per turn are exact, so the emulator's are the PSP's (build_gl36/emu_nB/log.txt "nat reuse vb").
- base (keep 1 + boot fixes + retrain) +0.2 on the PSP (model +4.0: the ME's own D-cache; the data-placement half of the model does not carry over). d01t deck main turns: cull -6.7. Steady deck 40.0 = gl35.
- rain wait ME a step: walks 58.6 = people (chain 2) 18.9 + ship (chain 3) 17.4 + sorted prims/meshes (chain 6) 15.7; ME total ~83; main logic 42.7 + waits 9.1.
- native ship group, plain rain turn, us a timed group (each stage holds one ~0.46 us clock read): FIXED walk+pointers 5.6, register 9.2, A+D packet 6.4, host call+GS state 5.5,
  texture 8.8, way back 2.5, drawn areas 1.9, depth range 1.5, walk's end 1.1, into the frame 0.9 (~43); PER VERTEX transform 4.2, positions 4.0, attributes 1.8, strips 1.7, GE batch 4.4 (~16).
- PSP/model ratio for ME-side savings ~0.5 (cull 0.55, re-use 0.5): score levers on the model, expect half.

## What gl37 is
1. Tree /home/claude/t37 = t36 (the delivered gl36: check t36 against t36_files_C.md5 and the package's EBOOT c4d797f0 rebuilds). Build folder /home/claude/main/build_gl37; notes attic/gl37.
2. CULL ALWAYS ON (no cull cycle): PSP-proven exact (cull-on turns' digests = twin) and paying everywhere it ran.
3. RE-USE ONLY WHEN IT PAYS, in turns (MGS2_NAT_REUSE_CYCLE=1,1,0,0 as gl36: on-turns use the new rule, off-turns plain): record and look up only while recent pictures replayed well
   (rule, threshold and how it notices the camera settling again are yours, from the emulator counts; per run or per picture). Exact by construction: replays are unchanged, only
   whether to try; prove it with MGS2_NAT_RCHK=1 on the emulator (0 differing). Target [C model]: walk loss ~0, wait gain kept.
4. THE LEVER, in turns (own key, e.g. MGS2_NAT_FIX_CYCLE=1,1,1,1,0,0,0,0 = on when t%8<4): cut the native ship group's FIXED work (~43 of ~58 us). Look first where the fixed us go
   (texture 8.8 is the largest stage nothing has touched; gl35's texture keep COST +0.8: read attic/gl35/keep_design.txt and find why), then register 9.2, A+D 6.4, host call+GS state 5.5,
   walk+pointers 5.6. Write a short design note (attic/gl37/fix_design.md: per stage what it does, what is skippable exactly, model estimate) BEFORE building. Gate preference as gl36:
   (a) GE words identical to the plain path in every picture, checked mode on the twin and the emulator with 0 differing; (b) only if (a) is impossible, pictures identical, and say so.
   Target: -30% of the fixed work or better (~-13 us a group, ~-4 ms a rain ME step [C model], ~-2 on the PSP). If the design says it cannot reach -15%, report that and stop before building.
5. RETRAIN PGO + code order with all arms (new hot code). The order search must guard the deck and the opening as well as the rain (gl36's guard did not bind: its start order was
   13-23% worse in the model): no window [C model] slower than gl36 by more than 0.5 ms a step, or report why.
6. cfg otherwise = gl36's delivered cfg (MGS2_OPT_CYCLE must stay, or every cycle is ignored; in place everywhere; FLIPWAIT_FROM=0; DZ X whole run; live captures; short run to 31,400).

## Scoring before delivery [C model + emulator timers]
gl36's windows and turns; each lever (on - off, same ELF, matched arms) and the base (t37 plain against gl36's plain turns on the same vblanks). Per-stage us a group for both arms from
the emulator's nat split timers. Report the rain wait, the rain walk, the d01t deck, the steady deck and the opening, model and expected PSP (x0.5 for ME-side).

## Gates
As gl36's: twin 685/685 in every arm and order; all frames equal between arms in every column; read hash; emulator 62/62 + 916/916 = twin; pictures vs gl36's emulator pictures;
nat problem counters 0; the levers' check modes 0 differing; boot heap low point not below gl36's (64 KB at vb 23,677; vb 0 boots); same system calls (NIDs) as gl36, clock 333,
no new hardware writes. Package build_gl37/deliver with README, MANIFEST, rd37pc.py (stdlib reader: split by both arms the way attic/gl36/split36pc.py does, plus the nat split
stages, and a per-turn re-use replay line), and a draft "Build gl37" harness entry in attic/gl37/report.txt. Do NOT deliver: the lead verifies and delivers.

## Limits
No PSP, nothing on the owner's PC; game data and binaries stay in the container. Read-only outside /home/claude/t37, /home/claude/main/build_gl37 and attic/gl37 (t36, t37d,
build_gl36 read-only). Disk: ~4.6 GB free; stay under ~3 GB, delete your own emulator picture folders after their checks (literal paths, after looking). Kill only your own
processes by PID. python3 -I. Every figure marked [C], [C model], [I] or UNKNOWN.
