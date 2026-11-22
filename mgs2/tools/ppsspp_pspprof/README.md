# PspProf, current copy (2026-10-20, v18 prep)

The container's PPSSPP 1.20.4 profiler files as they stand (`Core/MIPS/PspProf.h`, `PspProf.cpp`): TT's PspProf plus the MGS2 local
changes (PSPPROF_SWITCH, PSPPROF_ITRACE, PSPPROF_ITRACE_ALL, PSPPROF_ITRACE_MARK, PSPPROF_DHIST, PSPPROF_DACC, PSPPROF_DWATCH, PSPPROF_BUCKET,
PSPPROF_BUCKET_ONLY, PSPPROF_DSKIP, cache ops). `tools/ppsspp_mgs2.diff` holds the hooks in
MIPS.cpp / MIPSTables.cpp / MIPSInt.cpp; copy these two files over the PPSSPP tree and rebuild with `ninja PPSSPPHeadless` in build/.

Per-actor costs: build the harness with `-DGEN_ACTOR_PROF` (boot/dispatch.h; e.g. `make ... PGOX="-DGEN_ACTOR_PROF -Wno-coverage-mismatch"`),
then run with `PSPPROF_BUCKET=<g_emuactor address from psp-nm>`; the dump gets `<out>.buckets` (act instructions imiss dmiss).
`PSPPROF_BUCKET_ONLY=<act>` counts only that actor (its own per-function profile through tools/psp_prof.py).

Data accesses by address: `PSPPROF_DACC=1` writes `<out>.dacc`, the counted loads and stores per 64-byte line of 0x08000000-0x0bffffff
(`PSPPROF_DHIST=1`: the misses per 1 KB). Map the lines to symbols with psp-nm (the guest register file R / RH / F, VU0 state, host stacks).

Code-layout traces (tools/layout_sim.c): `PSPPROF_ITRACE=<file>` writes the counted fetches as runs (pc, length) of the first profiled
window; with `PSPPROF_ITRACE_ALL=1` every window (each starts with the record {0xffffffff, 0}; the file is flushed at each dump), with
`PSPPROF_ITRACE_MARK=1` also the paused fetches (flag 2: draw pipeline, GS walk) with bit 31 of the length set. mgs2.cfg takes up to 8
`prof A B` lines; a run of 8 five-step windows per route is the v17 layout input.

Caution: a profiled window runs PPSSPP's interpreter. On the deck route its game state left the JIT's (and x86's) after vb 24000 in
v17's 8-window trace run (ee hash at vb 24500 / 25000; the same build without windows matches): a timing-only artifact of the
emulator, not of the build.

Cache ops and data placement (v18): `cache` (opcode 0x2f) on a main-RAM address now acts on the D-cache model: 0x18 / 0x1c (create
dirty exclusive) allocate the line with no miss charged (counted in `cde`); 0x1e / 0x1f (fill) fetch it ahead (a miss there counts in
`pref`, not in dmiss: its cost is a hardware question, hwprobe.c probe 12); 0x19 / 0x1b (hit invalidate, hit writeback invalidate) drop
it (`cinv`). The dump header has `cde pref cinv dskip`. `PSPPROF_DSKIP=<lo hex>:<hi hex>[,...]` (up to 8 ranges): data accesses there
bypass the D-cache model, i.e. what moving that data to the PSP scratchpad would do (a scratchpad load costs a cache hit, v8 probe);
counted in `dskip`. `PSPPROF_DHIST=64` writes the misses per 64-byte line instead of per 1 KB (`tools/dev/dmap.py -g 64` maps them).
Accesses outside 0x08000000-0x0bffffff (the scratchpad at 0x00010000, the uncached mirror) are not in the D-cache model.

Written lines (2026-10-23, for MGS2GE v6, audit H2): the D-cache model is a write-back, write-allocate cache. A store (sb sh swl sw swr sc
swc1 sv.s svl.q / svr.q sv.q) marks its line written; a miss that replaces a written line writes it back first, as does cache 0x1b
on a written line, and 0x18 / 0x1c leave the line written. The dump header adds `dst` (the D-misses that were stores, also in dmiss)
and `dwb` (written lines written back). With `imiss` and `dmiss` that is every 64-byte transfer the CPU makes on the bus per window.
Write-backs are not charged in the cost model (tools/dev/prof_ms.sh); the hardware's 317 ns store miss against 172 ns for a load
(v8 probe) suggests they cost [I].
