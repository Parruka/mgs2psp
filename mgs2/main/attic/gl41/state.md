# gl41 BUILD round: state (build agent; newest block last)

## 2026-11-23 22:00 UTC, block B1 (started; tree made)  [time corrected in B2: written ~22:00]
- Brief (lead, 2026-11-23 ~21:50 UTC): gl41 on gl40's final tree in build_gl41 + attic/gl41 (plan, state, report); NOT delivered (the
  lead reads gl40's PSP log first).  Content: (1) native renderer step 1 (ship native, MGS2_NGE; attic/ge_native/step1 t40_step1.diff
  with the late-walk fix, 256 KB a region, static); (2) gl41s levers P1, P3 + probes (MGS2_OPT2 bits 19/20; E6 dropped; gen_vu.h by
  gl41_genvu.py); (3) HEAP FIX: (a) texture table 128 KB from the start, (b) newlib's free room logged at each ME heap event and at
  the texture table's allocation, (c) step 1's ~9.2 KB scratch out of .bss into its own block outside newlib's heap + the cooker
  built for size; keep ME_PRE / END_PAD unless the heap gate still fails.  HEAP GATE: emulator, delivered package: 0 textures refused,
  every list/frame digest equal to the same turns with MGS2_NGE=0, >= 32 KB free at the worst moment (new log lines).
  Turns: K-A (all drawn, today's path), K-N1 (ship native), K7; P1, P3 on/off in their own K-A turns; same-scene pairs in the rain
  wait and the walk.  Build: one PGO training with NGE turns, one new order (guard as gl40 + a rain-weighted stage), pads retuned.
  Gates as gl40 + NGE check mode 0 differences 24,000-37,800 + the heap gate.
- Read: attic/ge_native/step1_results.md (all), attic/gl41s/results.md + state.md, patches, scripts.
- Tree /home/claude/t41 = copy of /home/claude/t40 (gl40 final) + t40_step1.diff (8 files, clean) + nge/, ge/nge.h, boot/nge_stub.h:
  byte-equal to the step-1 agent's checked tree (scratchpad st1g/t: diff -r empty) [C].  Then gl41s patches gl41_1_p1, gl41_2_p3,
  gl41_4_probes in series (no failed hunk; the probes patch at offset -70, E6 left out) [C].
- gen_vu.h: python3 -I tools/gl41_genvu.py main/gen/vp22/gen_vu.h (feb948ce) -> /home/claude/t41/gen41/gen_vu.h a00691cd (= gl41s's
  expected output) [C]: P3 8 checks left out, the three box-cull texts proven one, vuc_fns 56 entries.
- Next: code (heap fix a/b/c, the per-turn OPT cycle to 128 turns, PSP content digest line), plan file, twin.

## 2026-11-23 22:50 UTC, block B2 (code done; twin arms 6 of 8 passed; after the usage pause 22:28-22:40: checked, cut steps rerun)
- Code in /home/claude/t41 [C]:
  - heap fix (a): ge/texcache.c TK_CAP0 = 128 KB of entries at the table's first entry (no 64 -> 128 KB realloc); refused growths
    counted (TkStats.tab_fail); xl_psp.c's texture line: "textures refused for want of memory: N (the pool, the table)".
  - heap fix (b): psp/main_psp.c --wrap=_sbrk (the break's highest point and its vblank), heap_top_find (newlib's heap end, by
    bisection with the real sbrk under __malloc_lock), host_heap_room(); "memory (gl41)" lines at the start and the end;
    psp/xl_psp.c: every ME picture heap event logs newlib's room / free top / free; "picture heap (gl41): ... the lowest room + top".
  - heap fix (c): nge/nge_cook.c scratch (nc_v, nc_fl, nc_runs) and boot/nge_stub.h nge_bkt now pointers into one block
    "nge_scratch" (9,220 B + align: NGE_NBKT*2 + NC_SCR) from the host's allocator (PSP: sceKernelAllocPartitionMemory, outside
    newlib's heap; x86: posix_memalign); nge_cook.c built with #pragma GCC optimize("Os") on the PSP only.
  - boot.c / boot_x86.c OPT_NCFG 128 (MGS2_OPT_CYCLE / _OPT2_CYCLE / _HALF_CYCLE words); xl_psp.c "picture: vb N content X" in
    each digest picture; ge/gelive.c gl_digest_c now compiled on the PSP too (it was x86 only: the first n41 link failed on it,
    22:23; x86 code unchanged, so the twin was not rebuilt); runtime/vu_l41.h probe lines "L41 probe a/b/c".
- Scripts / tools (build_gl41): tools/sched41.py (turns), mkcfg41.py, k0cmp41.py; cfg/mgs2_gl41.cfg (46 MGS2 keys),
  mgs2_gl41pgo.cfg (no shots, NGE_CYCLE 128 turns), cfg_emu/mgs2_gl41_xpchk.cfg, _h1.cfg (digests 24,000-31,400 every turn),
  _h0.cfg (h1 with NGE_CYCLE all 0); tw41.sh, tw_run41.sh, runs41a.sh, em41.sh, pgo_a41.sh; layout_sim41 (layout_sim40 + -pin
  name:ref:off, the P3 copy's rule; -new on gl40's final gives layout_sim40's figures exactly).  Plan: attic/gl41/gl41_plan.md.
- TWIN [C twin, build_gl41/tw (x19 objects + t41, gen41 gen_vu.h), vs hashref_gl38 c9ddb593]: off40, off, p1, p3, sch, noff:
  685/685 each, hashref byte-equal each.  off40 (gl40's keys, every gl41 switch off) frames b8a2ae4fb016 x 16,903 = gl40's sch arm:
  gl41's code with its switches off (texture table 128 KB from the start included) draws gl40's picture.
- PAUSE: nchk and lw7 were cut mid-run (22:28) and n41's link had failed (22:23, gl_digest_c): half logs deleted, n41 folder looked
  at and deleted (literal paths); nchk + lw7 rerunning (runs41a.sh nchk lw7, 22:47) and n41 rebuilding (22:47).

## 2026-11-23 23:10 UTC, block B3 (twin 8/8 passed; first emulator runs found three PSP-only faults; fixed, n41 rebuilding)
- TWIN gate [C twin]: nchk and lw7 reruns passed (685/685, hashref byte-equal); nchk (MGS2_NGE=1 all run, check mode, GX_SEM=2):
  1,626,909 groups compared, 0 differences; lw7 also XP check 219,731 plans, 0 differing.  All 8 arms passed.
- n41 (first build, EBOOT 000bae52..) emulator n1 (package cfg) and nh1 (h1 cfg): 62/62 hashes each, 0 textures refused [C emu]; but:
  (1) "me picture loop: no thread (semaphore .., thread 80020190)": the emulator's me_pic thread (64 KB stack) did not fit in partition 2
      (27 KB largest block left after native step 1's tables + the gl41 scratch): the main CPU ran every entry itself, no ME turns, so the
      emulator and the cost model lost the main/ME split.  Fix: the thread's stack 16 KB (the real ME's own stack size; mecheck's deepest
      chain 5,120 B) and its stack's deepest use logged at the end.  Hardware: the ME starts without a thread (unchanged path).
  (2) the PSP printed no "picture: vb N content" lines: the ME loop's path (xme.c results) had none (only the old 4b-2 path).  Fix: xm_list
      computes gl_digest_c with the frame digest; XmeRes carries it (#ifdef __psp__); xl_psp.c prints it with each list line.
  (3) "memory (gl41): the break's highest point at vb 0, 15 KB of room": that was the log's own headroom probe (heap_headroom's
      bisection mallocs to within 16 KB of the end), not the game.  Fix: the probes are left out of the --wrap=_sbrk peak.
  Also: a line naming the heap event that gave the texture table and newlib's room then (n1: vb 536, +128 KB, room 103 + top 33 KB).
  x86 objects recompiled after these edits are byte-equal to the twin's (ge_texcache, ge_xlme, ge_gelive, xlive_chk, boot_rt):
  the twin's 8 arms stand.  Old n1/nh1 logs kept gzipped in build_gl41/old/.
- n1 heap figures so far [C emu, pre-fix build]: ME heap events' lowest room + top 125 KB (vb 23,674); textures refused 0.
- Now: n41 rebuild + package (pkn41.sh, 23:08), then emulator n1, nh1, nh0, then PGO.
