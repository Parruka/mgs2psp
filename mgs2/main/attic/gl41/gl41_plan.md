# gl41 plan (build agent, 2026-11-23 ~22:20 UTC; the lead's brief of ~21:50 UTC)

gl41 = gl40's final tree + native renderer step 1 (ship native, MGS2_NGE) + gl41s's levers P1 and P3 and probes + a heap fix.
Built in /home/claude/main/build_gl41 from /home/claude/t41; notes here (attic/gl41: this plan, state.md, report.txt, README copy).
NOT delivered: the lead reads gl40's PSP log first.  Same method and gates as gl40 (attic/gl40/gl40_plan.md).

## 1. Tree (/home/claude/t41) [C]
- t40 (gl40 final) + attic/ge_native/step1/t40_step1.diff (8 files, the late-walk fix in) + nge/, ge/nge.h, boot/nge_stub.h
  (= the step-1 agent's st1g/t byte for byte), then attic/gl41s/patches gl41_1_p1, gl41_2_p3, gl41_4_probes (E6 left out).
- gen41/gen_vu.h by attic/gl41s/tools/gl41_genvu.py from main/gen/vp22/gen_vu.h (feb948ce -> a00691cd, gl41s's figure).
- Probe log lines renamed "gl41 probe a/b/c" -> "L41 probe a/b/c" (step 1's design probes are N-P1..N-P5; the levers stay P1, P3).
- boot.c / boot_x86.c: MGS2_OPT_CYCLE / _OPT2_CYCLE / _HALF_CYCLE take up to 128 words (OPT_NCFG; gl40 16): a word for each of the
  74 turns, so a lever's turns can sit anywhere.
- psp/xl_psp.c: a "picture: vb N content X" line in each digest picture (gl_digest_c, as the twin prints it): with MGS2_NGE on, the
  list and frame digests differ by design (the CALLs); content equals the switch-off run.

## 2. Heap fix
(a) ge/texcache.c: the table is made at 128 KB (TK_CAP0) at its first entry, so the 64 -> 128 KB realloc at vb ~10,850 (newlib taking a
    fresh 128 KB at the top) never happens.  Entries are handed out in the same order either way (digests must equal gl40's: twin off40).
    A refused growth is now counted (tab_fail); the texture line adds "textures refused for want of memory: N (the pool, the table)".
(b) psp/main_psp.c: --wrap=_sbrk records the break's highest point (and its vblank) over the whole run; newlib's heap top is found once
    at the start (bisection with sbrk under the malloc lock); lines "memory (gl41): ..." at the start and the end give the room above
    the break at its highest point.  psp/xl_psp.c: every ME picture heap event (me_more / me_less / me_resize) logs newlib's room
    (above the break), the free top chunk and all free bytes; the lowest room + top over the events is printed ("picture heap (gl41)").
(c) Step 1's scratch (nc_v 6,144 B, nc_fl 512 B, nc_runs 516 B, nge_bkt 2,048 B) moved from .bss into one block "nge_scratch" from the
    host's allocator (the PSP: sceKernelAllocPartitionMemory, outside newlib's heap); nge_cook.c built with optimize("Os") on the PSP.
Keep ME_PRE and END_PAD unless the heap gate fails; if it fails, decide and say why in the report.

## 3. Turns (tools/sched41.py; 74 turns of 100 vblanks from 24,000, the run stops at 31,400 as gl40)
Kinds: A = K-A (all drawn, today's path, P1 and P3 on); N = K-N1 (ship native, MGS2_NGE_CYCLE 1); 1 = K-A with P1 off; 3 = K-A with P3
off; 7 = K7 (nothing drawn).  M5 on, F1 on, XP 3 everywhere (7 in the digest turns); no K5 / K8 / ME load turns.
- Deck t0-t49: blocks "A N A 1 A N A 3 A 7" (N, 1, 3 each between two A turns): A 25, N 10, P1-off 5, P3-off 5, K7 5.
- Rain t50-t73: A N A 1 A | [A dig] [N dig] | 3 A N A 1 A 3 7 N A N | walk N A 1 A 3 | [N dig] (t73).
  Timed: A 9, N 5, P1-off 3, P3-off 3, K7 1; every lever turn has a K-A neighbour in its scene (rain wait and walk both).
- Digests: t55 (A), t56 (N), t73 (N) and 29,490-29,499.  Shots as gl40 (16); 30502/30548 (t65), 30724 (t67), 30826 (t68) land in N
  turns: the native picture on hardware against gl40's shots at the same vblanks.
- Probes: MGS2_PROBE41=3 (a, b at the first vblank), MGS2_PROBE41_CULL=5000 (c), MGS2_L41_LOG=1.  Risk: probe (a)'s `cache 0x1e`
  is UNKNOWN on hardware (gl41s); if the run stops at the start in probe a, MGS2_PROBE41=2 runs the rest.

## 4. Build
1. Twin (tw41.sh: x19 objects, gen41 gen_vu.h) and arms (runs41a.sh): off40 (gl40's keys, gl41 switches off: frames = gl40's sch
   b8a2ae4fb016 x 16,903), off, p1, p3, sch, noff, nchk (MGS2_NGE=1 whole run, MGS2_NGE_CHK=1, GX_SEM=2), lw7 (nchk + LW 2 + XP check).
2. No-PGO PSP build n41: gates mesec, me_call, xme_rest, 133 imports, k_take_m5; emulator run.
3. PGO: five instrumented builds, training on cfg/mgs2_gl41pgo.cfg (delivery minus shots; NGE_CYCLE extended to 128 turns, N every
   other turn after t73), use build u, pgochk.
4. Order: traces t0..t3 on u; stage 1 = gl40's guarded search (the four main-drawing windows at or under gl36's I-misses + 2 %);
   stage 2 rain-weighted (the rain-wait and walk windows weighted, the guard caps kept); P3's copy placed by gl41s's rule
   (`_l41_f` + 8 KB set offset); the .isra names P1 removed written plain.  Relink; rt_new real = simulated; a second relink equal.
5. Pads retuned (RO_PAD, ME_PAD, XL_PAD; ME_PRE / END_PAD kept unless the heap gate fails).
6. Final: readelf 3 LOADs, symshift, G4 133 imports, M5; emulator 62/62 on the package with digests = twin; XP check run; heap gate
   runs (package with digests everywhere, and the same with MGS2_NGE=0); model per turn kind.

## 5. Gates
Twin 685/685 in every arm; NGE check 0 differences vb 24,000-37,800 (content and drawn2 = off); XP check 0 differing; emulator 62/62
on the package; ELF 3 LOAD segments, 133 imports, M5 present; HEAP GATE (emulator, the package): 0 textures refused for want of
memory, every list/frame digest of K-A turns (and content of all turns) equal to the same run with MGS2_NGE=0, >= 32 KB room at the
worst moment (the break's highest point and the ME heap events, from the new lines).

## 6. Package
build_gl41/deliver: EBOOT.PBP, 8 overlays, mgs2.cfg, hashref.txt, pad.txt, rd41pc.py, README.txt (what changed, turn table, what the
run should show, logs to LOGS\gl41), MANIFEST.md5.  Not delivered.
