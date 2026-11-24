# gl42 BUILD round: state (newest block last)

## 2026-11-24 01:50 UTC, block B1 (tree made; twin built; arms running)
- Brief (lead via the coordinator, ~01:35 UTC): C2 on in every turn (DEVIATIONS.md B7); gl41 ships as planned; step 2 becomes gl42, the
  renderer checkpoint build, prepared now, NOT delivered.  Plan: gl42_plan.md.
- Tree /home/claude/t42 = cp -a t41 (01:38) + step 2 by a 3-way merge (gl41's overlap hunks kept, the people's copy after gl41's ship
  copy) + the reuse hash probe; t41's main_psp.c (MAIN_KB 128, 01:45) taken at 01:46 (scratchpad st2/rebase42.sh; snapshot st2/t41_at_copy.md5).
- Twin build_gl42/tw (tw42.sh): built 01:47, gen_nat.o with P1214a0, P12e8b8, P12e790 weak + _orig.
- Scripts: tw42.sh, tw_run42.sh, runs42a.sh, pkn42.sh, em42.sh, tools/sched42.py, tools/mkcfg42.py, m/scripts (gl41's, B=build_gl42,
  psp.sh with the two new weakenings); cfg/mgs2_gl42.cfg (48 keys), cfg/mgs2_gl42pgo.cfg.
- Running: twin arm sch (01:48), then off41, noff, nchk, lw7, pgo one at a time.
- Disk: my old scratch (step 0/1 twins, older studies) deleted after a look: 8.6 GB free at 01:40.

## 2026-11-24 02:02 UTC, block B2 (twin arms sch, off41, noff read)
- sch [C twin]: 685 hash lines, = gl38 to vb 24,000, then its own (people native, as step 2's kc) -> cfg/hashref_gl42.txt (0cbcb5b9);
  digest lines c957ed16cf8d x 14,203, content 2b4664d2e396 x 14,203; people: 27,436 calls, 167,832 groups referenced, no room 0.
- off41 [C twin]: gl42's code with gl41's keys and OPT2 fffff9df = gl41's tw2 sch arm exactly: hashref_gl38 byte for byte, digest lines
  a9d11ad8eed6 x 14,303, content 073dd0e5adc7 x 14,303.
- noff [C twin]: = hashref_gl38 byte for byte; content b399ce4d8acc x 14,203 (the C2-on reference).
- sch vs noff content: equal in every picture to vb 36,771; 14 differ at vb 36,773-36,887 (w01a, PAST the delivery run's end 31,400 and
  past the NGE cycle, where nothing is native): the frame arena after step 2's carve (1236 KB) is too small there: 'no room 18' (971 KB
  data against noff's 988 KB).  Step 2 had flagged it (33 KB to spare at 36,755-36,907); gl41's additions took the rest.  No picture
  of the delivery run (to 31,400) has 'no room' > 0 in any arm [C twin].  Not changed for gl42 (the run stops at 31,400); for a run past
  the cycle: MGS2_NGE=2 there (step 2's advice) or a smaller carve.
- Scripts written while the arms run: pgo_a42.sh (sequential: build, its training, next build; nice 19), rt42a.sh (u, pgochk, traces
  t0-t3 with gl42's windows: K-A, K-N2, K-N2R, K-N1, K7 in deck, rain and walk; model mu), rt42b.sh (24-window merge, gl41's caps rule,
  seed SEEDORD = gl41's final order when it exists), layout_sim41 compiled from gl41's source; cfg/mgs2_gl42trc_t1.cfg (deck 31800).

## 2026-11-24 02:14 UTC, block B3 (all twin arms read: the twin gates pass)
- nchk (MGS2_NGE=2 whole run, check mode) [C twin]: ship check 1,629,828 groups compared, every check counter 0; people 1,243,894 compared,
  every counter 0; content b399ce4d8acc and drawn2 f230d5465a12 = noff in 14,203/14,203 pictures; people no-room 3 (drawn today's way).
- lw7 (nchk + the PSP's late walk + XP check) [C twin]: XP check 215,193 plans compared, DIFFERING 0; late walk plans failed 0, dropped 0;
  content and drawn2 = noff; hashref_lw7 = hashref_nchk = step 2's hashref_g384 (e892826c) byte for byte.
- pgo (the training keys) [C twin]: = hashref_gl42 to vb 31,700, own lines from 31,800 (people native past t73) -> cfg/hashref_gl42pgo.txt
  (43d4948c); content = noff but the K-N2R turns past t73 (chains 2-4 only, by design) and 6 'no room' pictures at 36,871-36,887.
- runs42a.sh's REF for nchk/lw7/pgo was a relative path (refcmp could not open it): post42.sh ran the refcmp after, absolute paths.
- Running: pkn42.sh (no-PGO build n42 + pk_n42), then em42.sh n1 (the loop apart).

## 2026-11-24 02:33 UTC, block B4 (no-PGO build n42 and its emulator gate PASS; PGO started)
- n42 (pkn42.sh, 02:12-02:19) [C]: mesec_chk PASS, me_call_chk PASS, xme_rest_chk PASS; gen.o's P1214a0, P12e8b8 (1 R_MIPS_26 + 1 R_MIPS_32)
  and P12e790 (2 + 1) weak with <name>_orig; 3 LOAD segments; k_take_m5 present; imports 134/134 names = gl41's n41, 0 differences
  (gl40 133 + gl41's sceKernelGetThreadStackFreeSize; boot_rt.o now references sceKernelGetSystemTimeLow, the hash probe, which the ELF
  already imported); stub sections the same sizes.  Image: _end 0b237580, +28.4 KB over n41 (text +23.4 KB, bss +4.4 KB).  EBOOT 626e6a47.
- emu_n1 (pk_n42 = the delivery cfg, PPSSPP 1.20.4, the loop apart, 567 s) [C emu]: 62/62 hashes = hashref_gl42; 0 'no thread' lines;
  digest pictures 61 (t55, t73: K-N2): content = the twin's sch in 61/61 (list/frame differ by design); growth at vb 24,007 (in time);
  XP check 720 plans DIFFERING 0; problem counters none; textures refused 0.
  HEAP: the route's worst moment 203 KB above the break (vb 536, the texture table's event); picture heap's lowest room + top 223 KB;
  malloc can still give 208 KB at the end; me_pic stack deepest 5,196 of 16,384 B.  (No-PGO; the PGO image will take more: gl41 +72 KB.)
- Reader rd42pc: SCHEDULE CHECK none; t11, t13, t17 (vb 25,100/25,300/25,700, 'deck (first play)') have no people records because no
  people are in that scene (K-A neighbours have none either): mkpc42 now writes the emulator's no-people turns into the reader as expected.
- tools/k0cmp42.py (sched42's kinds; N, 2, R native).  em42.sh takes REF= (h0 run: hashref_gl38).  cfg_emu/mgs2_gl42_{xpchk,h1,h0}.cfg.
- t41 checked 02:31: unchanged since the 01:46 snapshot (attic/gl41/state.md last block B9 01:45).  PGO (pgo_a42.sh) started 02:32.

## 2026-11-24 02:53 UTC, block B5 (PGO: the first training)
- g_game built 02:41; train_game (02:41-02:50, 587 s): rc 1 against hashref_gl42pgo (75 of 124 lines differ from vb 24,500) BECAUSE the
  training's memory-cut lines (MGS2_ME_PIC=0, MGS2_GL_TEXKB=1024, as gl41) leave no mode-4 loop: 'the carve after the opening needs mode
  4's loop and two frame memories: none ... the people stay the builder's'.  Against hashref_gl38 the same 124 lines: 0 differ [C emu]:
  the game ran exactly, people never native.  So gen.o's profile counts the people's builder (P12e8b8_orig / P12e790_orig) in every
  turn, as gl41's game training counted the ship's builder; the rtb and pic trainings keep the ME picture (the people native there).
  The w00a / w01a trainings (ME_PIC=0) will match hashref_gl38 too, not gl42pgo.
- Disk: my step-1/step-2 build outputs and emulator folders deleted after a look (st1g/t, st2/t and the merge files kept): 8.9 GB free.
- Written while the PGO runs: rl42.sh, rt_new42.sh, padld/ (gl41's), tools/padfit42.py (pads that put .rodata, the .me_bss groups and
  the image end on gl40's final offsets mod 0x2000), model42.sh + tools/score42.py (gl42's pairs; the reuse hash as its own column),
  fin42.sh (f2, f3, fh1, fh0 one at a time), g4_42.sh, README_draft.txt.

## 2026-11-24 04:03 UTC, block B6 (PGO done; gl41's B11 change rebased; use build and traces running)
- pgo_a42.sh 02:32-03:58 [C emu]: 5 trainings, every one exact: rtb and pic 124/124 = hashref_gl42pgo (the people and the ship native:
  pic 64,561 people calls, 393,450 groups referenced, 90,309 native ship calls; me_pic deepest 6,744 B instrumented); game 124/124,
  w00a 63/63, w01a 124/124 = hashref_gl38 (MGS2_ME_PIC=0 memory cut: no carve, people the builder's, B5).  21 files in gcda/
  (gcda/md5.txt).  Logs gzipped in old/pgo/; instrumented folders, game folders and pg/ deleted after a look.
- REBASE: t41's psp/xl_psp.c changed (gl41 B11: the me_pic stack line without sceKernelGetThreadStackFreeSize; same line count).  The two
  hunks (mq_thread's first line, the end's stack line) applied to t42's xl_psp.c (t42 had gl41's old lines there unchanged); t42 vs
  t41 now differ only by step 2's hunks; snapshot renewed (st2/t41_at_copy.md5, the 01:46 one kept beside it).  The gcda were trained
  on the old two functions (-Wno-error=coverage-mismatch: those two lose their profile, as gl41's u3).
- Running: rt42a.sh 04:02 (u from gcda, pgochk, traces t0 t1 t2 t3, model mu), then rt42b.sh.

## 2026-11-24 05:12 UTC, block B7 (use build u, traces, guard model)
- u (PGO=use, gcda/) built 04:12; pgochk rc 0: profile lines lost only in xl_psp.o's mq_thread / mq_end (the rebased cold functions).
- Traces [C emu]: t0 61/61, t1 117/117 = hashref_gl42pgo; t3 (gl38's guard cfg, no native people) 55/55 = hashref_gl38 (rt42a.sh
  compared it with gl42pgo and said rc 1: 6 lines from vb 24,500, the people the builder's there, by design).  Model mu (guard windows,
  DHIST 64) 55/55 = hashref_gl38; I-misses 4,944,840 / 2,432,896 / 183,059 / 4,958,699 (gl41's mu 4,876,553 / ...).
- t2 MISSED 3 WINDOWS: its list was not in ascending order and pgo.sh trace stops the run 10 vblanks after the last window given
  (30,711), so 30,801 / 30,901 / 31,101 were never traced (5 windows; rt42b.sh's merge stopped on that).  Fixed: t2 again with the
  windows sorted (rt42a_t2.sh; rt42a.sh's line corrected), rt42b.sh's merge mapped to the time order.  Running 05:11: t2, then rt42b.sh
  (seed: gl41's chosen s2 order, old/seed/gl41_order_s2.txt md5 d1f23753).

## 2026-11-24 06:45 UTC, block B8 (order s2 chosen; final link; static gates)
- t2 again (sorted, 05:11-05:15) [C emu]: 8 windows, 62/62 = hashref_gl42pgo.  rt42b.sh (05:15-06:29; seed gl41's s2: 2,064 of 2,068
  kept, 4 gone, 31 newly hot appended = step 2's code): s1 every guard window under its cap (-3.7 to -15.3 %); s2 (rain-weighted)
  rain/walk windows 5..14 -3.9 to -8.3 % I-misses against s1, guard windows +0.1..+4.7 % against s1 but every one under its cap
  (-3.9 to -13.1 %); test windows -0.6 % in all.  CHOSEN: s2 (as gl41: the rain is main-bound).
- Pads: padfit42's '.rodata' line measures the section's start, which new constants before the hot tables move; by symbols (as gl41's
  pads were set) RO_PAD 0x559 puts .rodata's names at gl41's final offsets (80 names +0, the 129 KB table -8 B; .data +24/+32 B).
  ME_PRE 0x1200, ME_PAD 0x1580, XL_PAD 0x210 (h39 block, xl group, the rest at gl41's = gl40's offsets: 0 / 0 / 0 for most names),
  END_PAD 0x19c0 (_end 0b24f900: heap block at gl40/gl41's offset 0x1900; 0x19f0 first gave 0b24f930, relinked).  Pads in the heap's
  way 17,232 B (gl41 16,312).  The image ends 32 KB higher than gl41's (0b247900).
- Final build psp (U=u, ORD=ord/s2; EBOOT bb85fb95): relinked twice byte-equal in all 10 files; rt_new42 [C sim]: 0 hot functions
  missing or resized, I-misses 5,920,866 / 2,301,158 = the search's.  3 LOADs.  G4 [C]: 133 / 133 sce names against gl41's final,
  0 differences, stub sections equal (boot_rt.o's new reference: sceKernelGetSystemTimeLow, the hash probe, already imported);
  xme_me / xme_rest / mesec / me_call PASS; k_take_m5 at 08a5f04c, 3 call sites.  Text +24,585 B against gl41.
- Trial links psp_t, psp_b deleted after a look.  Running 06:40: fin42.sh psp f (f2, f3, fh1, fh0 one at a time).

## 2026-11-24 07:10 UTC, block B9 (final build's emulator gates: all PASS)
- fin42.sh psp f (pk_f42) [C emu]: f2 / f3 / fh1 / fh0 62/62 each (fh0 = hashref_gl38: nothing native).  f2, f3 against the twin's sch
  arm: K-N2 61 frames (t55, t73) content 0 differ (list 60 by design).  fh1 against fh0 (digests in every frame from 24,001): K-A 1,762
  and K7 150 frames list/frame 0 differ; K-N1 300, K-N2 761, K-N2R 549 frames content 0 differ (lists 100 / 570 / 499 by design).
  f3 XP check: 114,008 plans, 0 DIFFERING (1,968 by result only).  Textures refused 0 in all four; 0 'no thread' lines (loop apart);
  me_pic deepest 5,312 of 16,384 B.  People totals = n42's (27,436 calls; 94 groups today's path; reuse hash checks 27,212).
- HEAP [C emu]: newlib's heap ends 0be9bfff (as gl41); heap block 0b24f900.  THE ROUTE's worst: 103 KB above the break at vb 536 (the
  texture table; + 35 KB free top = 139 KB; gl41 135 + 34 = 170): GATE >= 32 KB PASS.  ME heap events' lowest room + top 126 KB (vb
  23,676; gl41 158).  The END'S REPORT takes the break 68 KB above the route's high point (0be93000: 35 KB of room; gl41's end report
  stayed below its route high), malloc at the end 96 KB in 2 blocks (gl41 144 in 1); the same in fh0 (nothing native), so the image's
  +32 KB, not step 2.  Reader rd/rd42pc_f.py (4ef0c0b4) on f2: HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- Running 07:06: model42.sh psp a c (ka42, kb42, gd42, kc42), then score42 against gl41's final (X41=pc).

## 2026-11-24 07:55 UTC, block B10 (FINAL: gl42 packaged in build_gl42/deliver; NOT delivered)
- Model of the final build [C model, every run's hash lines 0 differ; score42_final.txt here, gl41 = its final X=pc windows]:
  guard 59.36 / 58.12 / 24.21 / 60.55, I-misses -2.4 / -10.2 / -8.1 / -3.5 % of gl36: PASS (gl42 - gl41 +0.64 / +0.23 / -0.36 /
  +0.29).  Rain wait main-bound: K-N2 - K-A t59-t58 / t59-t60 main -1.79 / -1.80 (hash +0.95), ME -5.38 / -4.98; K-N2R - K-A main
  -9.63 / -9.90, ME -26.81 / -27.61; K-N1 - K-A main +0.47, ME -2.56; K-N2 - K-N1 main -2.26, ME -2.82; K-A - K7 main +16.75 /
  +16.92.  Heavier: K-N2R t67 - t66 main -11.20, ME -24.33 (hash +1.32); t65 - t66 -6.92 / -11.66 (content).  Walk: K-N2 -6.82 /
  -9.55 main, -13.77 / -10.71 ME; K-N2R -15.31 / -10.48, -22.96 / -20.87.  Steady deck (ME-bound): K-N2 - K-A main +0.33 / -0.01,
  ME +0.26 / -0.17; K-N2R main -5.44 / -5.04, ME -22.58 / -22.19; hash 0.05.  gl42 - gl41 K-A: rain t60 -0.07 / +0.18, t58 +0.35 /
  +0.19, t66 +0.32 / +0.60; deck t30/t32/t34 main +0.07/+0.26/+0.03, ME -0.40/-0.39/-0.41; K7 +0.56.
- Reader: the HASHES line's wording fixed ('from vb 24,100'); rd/rd42pc_f.py 1db49abe, on f2 EQUAL / EQUAL / EQUAL / none.
- deliver/ (15 files): EBOOT bb85fb95, mgs2.cfg 0f77271b, hashref.txt 0cbcb5b9, pad.txt 7bc72ace, rd42pc.py 1db49abe, README.txt
  1ab3dca9, MANIFEST.md5 a9537e3a (SLES_503.83 e3f4c0df, mgs2disc.pak 189ca53f as gl41's).  README copy and score42_final.txt here.
- gl41's source checked again 07:52: t41 = the snapshot (gl41 B12 final, EBOOT 44272485): no rebase needed.
- Cleanup (literal paths, after a look): psp_t, psp_b, n42, pk_n42, emu_n1 (log gzipped in build_gl42/old/), the trace and model
  game folders.  Kept: lay/ (1.2 GB, the traces behind the order), u, psp, pk_f42 (= deliver), emu_f2/f3/fh1/fh0, model/.  Disk 6.7 GB.
