# gl32 plan: native ship meshes always + the ship's vertex cache in turns, 2026-11-20 23:55 UTC

Lead plan for the MGS2 port thread. gl32 is BUILT and GATED by an agent; the lead delivers it after gl31's last check (the 55 PSP captures).

## Why (gl31 PSP run 1, psp_harness.md "Build gl31") [C]
- Exact (62/62 hashes, 916/916 picture digests). Native step 1, deck-waiting stretch 29,600-30,500: ME turns -2.8 ms a step, main turns -3.5 (model -6.0 / -3.9 a step).
- The ME is the limit there: walks 73-76 ms a step, list build ("frame ends and lists") 22.5-23.3, records 4.7, idle 1.1; the main CPU waits for fills 17-19.
- Native time (merge runs: nat_p04_group = front end + the same converters, ge/xlme.c lw_nat) is 23.4-24.1 ms a step on the PSP's ME for ~74 calls / ~300 groups a step (the emulator shows 1/3.08 of that in every turn: its ME thread is faster). With lanes the same meshes cost ~27 ms a step. So the ship's cost is the per-frame conversion of its vertices, not the walk.
- Design answer: native_renderer_design.md 4.4 (vertex cache) and table 6 step 4. Ship meshes are mostly static: convert once, reuse.

## What gl32 is
- Tree /home/claude/t32 (= t31, checked identical 23:50, no symlinks), build /home/claude/main/build_gl32 (new). build_gl32s/cal is another task's leftover: leave it.
- Native ship meshes in EVERY turn (no builder arm). Arms by turns (100 vb from 24,001, like MGS2_NAT_CYCLE): A = native without cache (gl31's arm 3), B = native with the vertex cache. Both draw the ship.
- Everything else as gl31's cfg (FLIPWAIT_FROM=0, DZ X whole run, ME_PIC 4, live captures).

## Order of work
1. Model split [C model] (PspProf on the emulator, rain/waiting window): where native time goes per group (front end, converter core, per-vertex scans: depth range, off-screen cut, guard band, copies); same for the ME's list build (gelist.c and the dz pairing) at 29,600-30,500. Report both; no PSP timers unless the model cannot attribute it (then at most a few per frame).
2. x86 census over the whole route (both routes the twin runs): per drawn ship-mesh call per frame, hash of each stream (positions, UVs, colours, normals) and of the group descriptor; how many keys, bytes at GE format, how often each stream changes (colour buffers rewritten by the game: design 4.4), first and last frame of each key. This fixes the key, the invalidation rule and the size.
3. Design the cache for EXACT output first: the GE batches must equal today's (gelist.c and the X/XS shadow pairing read the base batch's vertices; census must keep 0 fallbacks in 29,490-31,400). Whatever depends on the frame's matrix (depth range, off-screen cut, guard-band class) is recomputed or proven equal; if exact identity costs most of the gain, measure both ways and report: the lead decides (a picture-rule change gets a DEVIATIONS entry).
4. Memory: where the cache lives (main RAM: the GE cannot read eDRAM). Today's figures: frame arenas (gl31 emulator log nat turn lines: arena most per turn), outside-heap blocks (PSP gl31: after the groups' arrays 116 KB largest, 216 KB total), heap low 156 KB (emulator). Size the cache from the census; give headroom figures.
5. Invalidation hooks (object creation 0x129128, colour buffer free 0x12cf28, stage load) and an x86 CHECK MODE that converts every cached mesh again each frame and compares: 0 differences on both routes before any PSP build.
6. Build gl32 behind its switch (cfg key, default off in the code; on in gl32's cfg by turns), then gates as gl31's (gl31_plan.md G1-G7): twin hashes and digests, read hash, emulator 62/62 + digests = twin, pictures vs gl31's emulator, ME gates (xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk; no new syscalls or register writes), memory, reader rd32.py (rd31 + the cache's counters per turn: hits, misses, bytes, invalidations, time), reproducible build. Model [C model] of A vs B per ME and main turn in the waiting and rain windows.
7. Deliverables in build_gl32/deliver (EBOOT, 8 overlays, mgs2.cfg, hashref.txt, pad.txt, README.txt draft, MANIFEST.md5) as gl31's. Not delivered by the agent.

## Limits
No PSP access, nothing on the owner's PC, no game data or pictures outside the container. Read-only: everything outside t32 and main/build_gl32 (and attic/gl32 for the report). Delete only inside those, by literal path after looking. Stop processes only by PID. python3 -I. Disk: 6.0 GB free at the start; keep new use under 2.5 GB; delete emulator game folders after checking. Reports as .txt (build_gl32/report.txt, copy to attic/gl32/report.txt). Every figure marked [C], [C model], [I] or UNKNOWN.
