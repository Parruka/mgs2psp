# gl35 plan: gl34 + the fast front end everywhere + per-group fixed work cut for native ship groups (in turns) + a retrain that recovers gl34's loss, 2026-11-21 17:10 UTC

Lead plan for the MGS2 port thread. gl34's PSP run (psp_harness.md "Build gl34", read 17:05) was exact; the fast front end gave about -2 ms a step in the rain's ME turns [I]; the new PSP timers say [C]:
- a native ship group costs the ME 63-70 us; about 2/3 of it is FIXED per-group work: register 9.1, texture 8.2, walk + pointers 7.6, A+D packet 6.0, host call + GS state 6.0, drawn areas 4.6 us (fast arm, rain wait); the per-vertex stages (transform, positions, cut, attributes, strips) are ~12-17 us for ~18 vertices a group;
- the rain draws ~30,500 such groups a turn (~21 ms a step of ME time); people (chain 2) and sorted prims + meshes (chain 6) each cost the ME about as much as the ship.
So the lever now is the per-group fixed work, built so the people (gl36) can reuse it.

## What gl35 is
1. Tree /home/claude/t35 = t34. MGS2_NAT_FAST on in every turn (the gl34 lever, exact on the PSP). Timers (MGS2_NAT_SPLIT=1) stay on.
2. **The lever, in turns (its own key, e.g. MGS2_NAT_KEEP_CYCLE=0,0,1,1):** for consecutive native groups, keep what is equal instead of re-deriving it per group: texture / CLUT setup, register and A+D packet state, the host call's GS state, the drawn-areas bookkeeping; batch the groups of one mesh / material where the GE words stay identical. Start from the gl34 report's "left out" list (GS tracking for native groups 4.6 ms a picture, needs a design that shows the translator's state equal wherever it is read: write that design first, in attic/gl35/keep_design.txt, with the reads it relies on listed and checked on the twin). HARD GATE: the GE words identical to today's in every picture (as gl34). If a part can only be pixel-identical but not word-identical, keep it out of gl35 and say what it would give.
3. Target [C model + emulator timers]: native ship time a group in the rain wait -35% or more against gl34's fast arm (63 -> ~40 us), no loss elsewhere.
4. Retrain PGO + order on t35 with both arms, and try to recover gl34's retrain loss (gl34 report: text +23.9 KB, I-misses +5-8% before the rain, ent_index and lw_p07 out of line): include main-turn windows in the order search, pin the inlining that gl33 had where it pays, and score against gl33 AND gl34.
5. cfg otherwise = gl34's delivered cfg (in place everywhere, FLIPWAIT_FROM=0, DZ X whole run, live captures, short run to 31,400).

## Scoring before delivery [C model]
gl34's windows; the lever (keep - plain, same ELF) and the retrain (gl35 plain - gl34 fast, i.e. the delivered gl34's fast turns; and against gl33). Plus the emulator timers' per-stage us a group for both arms, so the PSP can check stage by stage. Delivered if the lever is ahead in the rain ME windows and the build is not behind gl34 elsewhere by more than run-to-run noise (say what that noise is).

## Gates
As gl34's (twin 685/685 in every arm and order with plain = keep in all 17,664 frames, read hash, emulator 62/62 + 916/916 = twin + pictures vs gl34's emulator, nat problem counters 0, the lever's own self-checks (state kept vs recomputed, checked mode on the twin and the emulator: 0 differing), ME gates, imports, heap headroom, reproducible). hashref should stay c9ddb593. Readers: build_gl35/rd35.py and a PC-side build_gl35/rd35pc.py (stdlib only, one file, reads LOGS\gl35 + LOGS\gl34, prints fingerprints, turn table vs gl34's same vblanks, means by arm, the split and chain lines by arm, battery, pictures vs gl34's).

## Limits
As gl34's plan: no PSP, nothing on the owner's PC, game data stays in the container; read-only outside t35, main/build_gl35 and attic/gl35; delete only there by literal path after looking, plus finished emulator game folders inside main/build_gl30, build_gl30n, build_gl32 if disk is short (look first, say what went). Stop processes by PID; python3 -I; no package installs. Reports as .txt (build_gl35/report.txt, copy attic/gl35/report.txt). Every figure [C], [C model], [I] or UNKNOWN.
