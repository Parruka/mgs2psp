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

## 2026-11-23 23:40 UTC, block B4 (native step 1 wrong with the loop apart: found, fix built; coordinator agreed 23:35)
- n41b (thread 16 KB, EBOOT f7877e70): the thread starts ("as a thread of the main CPU", 1,015 ME-turn lines; deepest stack use 5,212
  of 16,384 B [C emu]); 62/62 hashes n1, nh1, nh0.  But with the loop apart [C emu]: NGN_W_ALL 61,814 of 62,101 native batches (stub
  WORLD != converter's), declined by count 31,068 (n41a, loop on the main CPU: both 0); content digest differs from nh0 (MGS2_NGE=0)
  in ~98 % of the frames of every K-N1 turn that draws the ship; shots 30500/30546/30722/30824 differ from nh0 by 46,599-115,390 px
  (max 48-79 levels), the misplaced pixels on the ship geometry only.  nh0 = twin off in every digest frame (161 of 161).
- Cause [C code + the counts]: the late walk read the stub's references (xc_nge_ref[b]) and transforms (xc_nge_xfa[b]) from the
  main CPU's packet buffers, refilled for a later picture before the walk; only the NatGrps were copied at REC.
- Fix (t41): ge/xme.c REC copies the record's references with its groups into the arena (room NGE_NAT_ROOM in xl_psp.c mq_plan and
  in the x86 mirror boot/xlive_chk.c, which also walks from its copy); the walk reads them (ge/nge.h nge_walk_r, XmPic.nat_rf);
  ge/xconv.c xc_nge_batch takes WORLD and pa/pb from the converter's own Dx; NgeRef.pad carries a 16-bit digest of the stub's
  transform (nge_xf_h16) so NGN_W_ALL still compares stub vs converter.  step1_results.md got a dated note (coordinator's request).
- Now: twin tw2 (all 8 arms, outputs g3_*), n41 rebuild (pkn41.sh; first link failed on a missing include, fixed), then emulator
  n1 / nh1 / nh0 with the loop apart.  Old logs: build_gl41/old/n41a_*, n41b_*.

## 2026-11-23 23:52 UTC, block B5 (container restart ~23:48: what was cut, rerun)
- Survived: n41 + pk_n41 of the fix (EBOOT 7f73502f, built 23:42; mesec_chk PASS, me_call_chk PASS), twin tw2 (built 23:38) and its
  off40 arm (685/685, hashref_off40 = hashref_gl38 byte for byte).
- Cut and deleted after a look (literal paths): emulator n1/nh1/nh0 (had reached vb ~24,100 only), twin off/p1/p3 half logs.
- Rerunning: twin arms one at a time, nice 19 (nchk lw7 sch noff off p1 p3, outputs g3_*); emulator nh1 + nh0 (the fix's gate), n1 next.

## 2026-11-24 00:08 UTC, block B6 (the step-1 fix PASSES on the emulator with the loop apart; PGO started)
- n41c (EBOOT 7f73502f) nh1 vs nh0 [C emu, loop apart: me_pic thread, ME turns]: 62/62 each; every list/frame digest equal in every
  non-N frame (A 1,662, P1-off 399, P3-off 400, K7 250 frames); N 811 frames: list differs in 410 (the CALLs), CONTENT EQUAL 811/811.
  NGN_W_ALL 0, declined by count 0, native batches 76,469 (= the loop-on-main-CPU run n41a).  Shots: the 4 N shots pixel-equal to
  MGS2_NGE=0 (were 46,599-115,390 px); shot_24784 120 px +-1 (known emulator noise).  me_pic deepest stack use 5,196 of 16,384 B.
- Twin tw2 [C twin]: off40 685/685 (hashref equal); nchk 685/685, 1,626,909 groups compared, 0 differences, W_ALL 0 (the digest).
- Heap log: the boot's 15 KB "peak" was heap_headroom's own probe: GCC had dropped its ++/-- of the (non-volatile) skip flag around
  malloc; flag now volatile, and the break's peak is split boot / ROUTE / end report.  Route figures from the fix build: ME heap
  events' lowest room + top 124 KB (vb 23,676); texture table at vb 536 with 103 + 32 = 136 KB; the break after vb 1 highest at
  the end (35 KB, the end report; now split out).  n41 rebuilt 00:06 (EBOOT 64155ac8): emulator n1 running; pgo_a41.sh started 00:06.

## 2026-11-24 00:30 UTC, block B7 (twin tw2 8/8 PASS on the fixed code; package run n1 heap figures; PGO running)
- TWIN tw2 [C twin, vs hashref_gl38 c9ddb593]: all 8 arms 685/685, hashref byte-equal.  off40 frames b8a2ae4fb016 x 16,903 (= gl40);
  off = p1 = p3 = noff digest lines ac521cae3604 x 14,303; sch a9d11ad8eed6 (N frames differ by design), content 073dd0e5adc7 x
  14,303 in every arm; nchk and lw7: digests a9d51dbb4a6c, content 073dd0e5adc7, drawn2 b75a4d7ff604 = noff, 1,626,909 groups
  compared 0 differences, W_ALL 0, lw7 XP check 219,731 plans 0 differing.  = the first twin's nchk/lw7 digests: the fix changes
  nothing on x86.
- n41d (EBOOT 64155ac8, package cfg) emulator n1 [C emu]: 62/62; the thread runs (1,014 ME-turn lines), stack deepest 5,196 B;
  W_ALL 0, declined by count 0, native batches 76,469; textures refused 0 (pool 0, table 0).  newlib's room: the break's highest
  point in THE ROUTE 0be62000 at vb 536 (the texture table's region), 103 KB above it; ME heap events' lowest room + top 124 KB
  (vb 23,676); the texture table's event 103 + 32 = 136 KB; boot 403 KB; the end's report 35 KB (after the route).
  HEAP GATE (worst moment of the route >= 32 KB): 103 KB on this build [C emu]; to be read again on the delivered package.
- PGO: pgo_a41.sh 00:06 (g_game built 00:15; the rest building; trainings run as each build ends).

## 2026-11-24 00:32 UTC, block B8 (while PGO runs)
- tools/score41.py written (score40's method; per turn main | ME | P = max(main, 0.8 ME) [I], K7 = main; pairs K-N1/P1-off/P3-off
  against K-A neighbours, gl41 - gl40 same turns, guard windows against gl36 + 2 %).
- Reader rd/rd41pc.tpl.py: prints the me_pic stack line in HEAP; SCHEDULE CHECK accepts one native picture in the turn line BEFORE a
  K-N1 turn (the stub's switch is the frame's, rt_nge_at; a turn's line is written at the next turn's first hook, after that frame's
  calls: the next K-N1 turn's first picture is counted one line early; content lines equal, so nothing is drawn wrong).  Tested on
  n41d's n1 log: hashes/digests/content EQUAL, SCHEDULE CHECK none.
- build_gl40/lay looked at and deleted (869 MB); its small text outputs (order_s1.txt = rt41b's seed, ls_*.out, run_t*.out, wf1)
  copied to build_gl41/old/gl40_lay_small/, rt41b.sh points there.  Disk 8.3 GB free.

## 2026-11-24 01:45 UTC, block B9 (PGO done; order search running; THE HEAP: the PGO image eats the gate's margin)
- PGO (pgo_a41.sh 00:06-00:57): 5 trainings, each hash line equal (124/124/124/63/124 of hashref_gl38, 0 differ); the picture training
  ran native step 1 (69,852 native calls, 409,027 native batches; me_pic stack deepest 6,744 B instrumented); 21 gcda (gcda/md5.txt).
  Instrumented folders, game folders and pg/ deleted after a look (literal paths); training logs gzipped in old/pgo/.
- rt41a.sh (01:02-01:27): use build u (EBOOT built 01:02), pgochk: every object 0 profile lines; traces t0-t3 (61/117/62/55 hash lines,
  0 differ); guard model mu (55/55): I-misses a step 60,957 / 33,995 / 2,185 / 60,478 (u has no order yet).
- rt41b.sh (01:28-): caps from gl36 + 2 %; seed = gl40's final order (12 .isra names made plain; k_take_interrupts is now
  k_take_interrupts.part.0 in the PGO build); P3's copy pinned on vupv_8baafbc3_0's sets + 0x80 (in the PGO build that name is a
  0x44-byte wrapper holding the P3 test, 'j vupv_96936c0f_0_p3' when bit 20 is set; the form's body is .part.0); s1 then s2.
- HEAP [C, ELF sizes; I for the room]: the PGO image is 72 KB above the no-PGO package build n41d (_end 0b2423c0 against 0b2303c0;
  text +68 KB), on which the heap gate read 103 KB.  newlib's heap top is fixed (PSP_HEAP_SIZE_KB(-1024)), so the room at the route's
  worst moment falls by the heap start's rise: u with no pads ~32 KB; with gl40's pads (ME_PRE 0x1d00, END_PAD 0x1a00, ME 0x15c0,
  XL 0x210) ~12 KB; without ME_PRE/END_PAD ~26 KB.  Against gl40: image +46.6 KB (P1/P3 in boot_rt +30.7 KB text +4.3 KB bss,
  xl_psp +2.4, the ge objects +5.6, the heap log +4) and the route's peak use +13 KB.
- DECISION: MAIN_KB 256 -> 128 (psp/main_psp.c; the main thread's own part, guest thread 1's stack; use 2.9 KB in every PSP and
  emulator run since gl16): newlib's heap = all free memory but 1 MB, so its top moves up 128 KB (to be read on the emulator: 'newlib's
  heap ends at').  ME_PRE and END_PAD kept.  u2 = u with main_psp.o recompiled (only sce_newlib_stack_kb_size 0x310 -> 0x290 KB and
  the pool check's constant differ); the relinks take U=u2.

## 2026-11-24 02:45 UTC, block B10 (MAIN_KB 128 confirmed; order s1 modelled; pads)
- Heap test hz (u2, no order, no pads; package cfg) [C emu]: 62/62; newlib's heap ends at 0be9bfff (was 0be7bfff: +128 KB, as
  expected from MAIN_KB 128); heap block 0b242400; the route's worst 159 KB (vb 536, the texture table: 159 above the break + 32 top);
  ME heap events' lowest room + top 180 KB; textures refused 0; me_pic deepest 5,188 B.  Logs old/hz_emu_log.txt.gz; hz, pk_hz,
  emu_hz, ms/em_hz deleted after a look.
- Order s1 (ls_s1.out): every guard window under its cap (-9 to -22 %).  lay_one41.sh wrote its layout files to ord/e1.
- Pads [C, symshift40 against gl40's final, gl40's DHIST]: pa = RO_PAD 0xb3c (.rodata +0 vs gl40; .data/.me_data +0x1c0..0x1e0),
  ME_PRE 0x340 (h39 block +0), ME_PAD 0x15c0 (xl group glf/xmp +0), XL_PAD 0x210 (VM/own/UD/gr +0; Fx/Dx/lw_mem groups +0x78..+0xc0
  from ge objects' growth), END_PAD 0x1a00 (heap block 0b247900, gl40's offset 0x1900).  pb = pa with RO_PAD 0x960 (.data/.me_data
  /.sbss +0, .rodata -0x1dc).  Not movable by a pad: g_mem +0x680 (boot_rt's .bss before it grew: P1/P3's tables) and the .bss
  groups -0x8c0..-0xd20 / +0x650.  With RO_PAD >= 0xec the .bss moves up one 8 KB step (same sets; image +8 KB).
- s1 + pa (q_s1a) [C model, 55/61/62 hash lines 0 differ]: guard 58.78 / 57.95 / 24.44 / 60.18 (gl40 59.51 / 59.04 / 24.29 / 60.83;
  gl36 59.24 / 58.81 / 24.27 / 60.61); I-misses 36,040 / 19,266 / 1,976 / 35,604 = -6.2 / -18.7 / -8.5 / -7.1 % of gl36: PASS.
  Rain: main-bound in every drawn window (main ~85, 0.8 ME ~52).  gl41 - gl40 same turn: t60 K-A main -0.29 ME +1.54; t66 main
  -0.83 ME +0.97; K7 t64 main -0.48.  K-N1 - K-A rain wait: ME -2.2 / -2.6, main -0.05 / +0.66.  P1 off - on: main +1.08 / +1.49.
  P3 off - on: main +0.15 / +0.50.
- Running: rt41b.sh's stage s2 (slowed by the model runs), pb's guard windows.

## 2026-11-24 03:50 UTC, block B11 (order s2 chosen; final build 1 gates; one new import found and removed)
- s2 + pa (q_s2a, RO_PAD 0xb88 for s2's text) [C model, every run's hash lines 0 differ]: guard 58.74 / 57.87 / 24.57 / 60.09, I-misses
  36,396 / 19,318 / 2,002 / 36,020 = -5.3 / -18.5 / -7.3 / -6.1 % of gl36: PASS; s2 - s1: -0.04 / -0.07 / +0.13 / -0.10.  Rain (16
  windows): s2 better in every one, -1.0 to -2.0 ms a step both CPUs (sum -23.4; I-misses -6..-10k a step).  pb (RO_PAD for .data)
  against pa on s1's guard: +0.10 / +0.05 / -0.00 / +0.03 (worse): pa kept.  CHOSEN: s2 + pa.
- Final build 1 (psp, EBOOT be50907b): relinked twice (psp_b) byte-equal in all 10 files, = q_s2a; rt_new41 [C sim]: 0 hot functions
  missing or resized; training-window I-misses 4,579,365 + 2,360,614 = the search's figures.  3 LOADs; M5: rt_syscall tests bit 18
  and calls k_take_m5 or k_take_interrupts.part.0 (PGO split the walk), 3 call sites.  Heap block 0b247900 (gl40's offset 0x1900).
- fin41.sh psp f [C emu]: f2, f3, fh1, fh0 62/62 each.  f2/f3 against twin tw2 sch: K-A 55 frames 0 differ, N 61 frames content
  0 differ (list differs by design 60).  fh1 against fh0 (digests in every frame from 24,001): A 1,662 / P1-off 399 / P3-off 400 /
  K7 250 frames list/frame 0 differ; N 811 frames content 0 differ (list 410 by design).  f3: XP check 118,546 plans, 0 DIFFERING
  (1,968 by result only).  Textures refused 0 (pool 0, table 0) in all four.  HEAP: newlib's heap ends 0be9bfff; the route's worst
  135 KB (vb 536, the texture table: 135 above the break + 34 top = 170 KB); ME heap events' lowest room + top 158 KB (vb 23,676);
  boot 439 KB; at the end malloc can give 144 KB in 1 block.  me_pic deepest stack use 5,188 of 16,384 B.  Reader on f2: HASHES /
  DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- G4 FAIL: 134 imports against gl40's 133: sceKernelGetThreadStackFreeSize, from gl41's me_pic stack line (xl_psp.c).  Fixed: the
  thread marks its own stack's bottom (0xff, up to 512 B below its frame) with sceKernelReferThreadStatus + sceKernelGetThreadId
  (both imported already) and the end counts the marks after the thread ends; same line count (no profile line shifts).  Rebuilding
  the use build as u3 (u2's objects must stay equal but xl_psp.o), then relink, G4, fin41 again (tag g), the model again.

## 2026-11-24 04:40 UTC, block B12 (FINAL: fixed build 44272485 passes every gate; package in build_gl41/deliver; NOT delivered)
- Final build (psp, U=u3, pads RO_PAD 0xaa8 ME_PRE 0x340 ME_PAD 0x15c0 XL_PAD 0x210 END_PAD 0x1a00; EBOOT 44272485): relinked twice
  byte-equal; rt_new [C sim] 0 hot functions missing or resized.  G4 [C]: 133 / 133 sce names, 0 differences, stub sections equal;
  mesec / me_call / xme_rest / xme_me PASS; 3 LOADs; k_take_m5 at 08a67a0c, 3 call sites.
- fin41.sh psp g [C emu, pk_g41]: g2 / g3 / gh1 / gh0 62/62 each.  g2, g3 vs twin sch: K-A 55 frames 0 differ, N 61 content 0
  differ.  gh1 vs gh0: A 1,662 / P1-off 399 / P3-off 400 / K7 250 frames list/frame 0 differ; N 811 content 0 differ.  g3 XP check
  118,546 plans, 0 DIFFERING.  Textures refused 0 in all four.  HEAP: route worst 135 KB (vb 536, the texture table: 135 + 34 top
  = 170 KB); ME heap events' lowest 158 KB; end 144 KB in 1 block.  me_pic deepest stack use 5,276 of 16,384 B (marks method).
  Shots gh1 vs gh0: 15 of 16 equal (the 4 K-N1 shots too); shot_24784 (P3-off turn) 265 bytes +-4: the known noise.
  Reader rd41pc_g.py (955b6667) on g2: HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- Model X=pc of the final build [C model, every run's hash lines 0 differ]: guard 58.72 / 57.89 / 24.57 / 60.26, I-misses -5.5 /
  -18.6 / -6.9 / -5.9 % of gl36: PASS.  Rain wait main-bound: K-N1 - K-A ME -3.10 / -2.71, main +0.29 / -0.14; P1 off main +1.37 /
  +0.94; P3 off main +0.03 / +0.41; gl41 - gl40 t60 main -0.84 ME +0.23, t66 -1.11 / -0.51, K7 -0.86.  Steady deck ME-bound, every
  kind within ~0.4 of K-A; the steady deck's K-N1 turns draw no ship mesh (0 native calls on the emulator).
- deliver/ (15 files, MANIFEST.md5; README 53064e88).  attic: report.txt, README.txt, score41_final.txt.
- Cleanup (literal paths, after a look): psp_v1, pk_f41, pk_n41, u2, emu_f* (logs gzipped in build_gl41/old/), the model's and the
  emulator's game folders.  Kept: lay/ (1.7 GB), u, u3, psp, pk_g41, emu_g*.
