# gl39 build: state (kept current by the build agent; newest block first)

## 2026-11-23 00:16 UTC: twin gate arms done, emulator no-PGO run done, profile trained, use build + traces running
- Twin on the final sources (build_gl39/tw2, runs39c.sh): p0 (gl38's keys) / m5off / p1 (F1 everywhere, re-use off) / sch (gl39 cfg) /
  sck / ckr (F1CHK + re-use 1,1,0,0: 138,338 groups compared, 0 differing) / ckp (F1CHK + re-use all: 350,174, 0 differing): all
  685/685 = c9ddb593.  Digest lines: p0 = gl38's sch2 (3b1aa042), p1 = gl38's plain arm (36c2ba23).  runs39d.sh (F1's check as gl38
  ran it: REUSE=1 RRULE=0 RCHK=1 F1CHK=1; arms f1c, f1s) running.
- No-PGO PSP build n39 + emulator run n1 (cfg/mgs2_gl39.cfg before MGS2_LOAD_US was added): 62/62 hashes, 266 digests = the twin's sch
  arm in every digest-on turn (K0 166, K7 50, K8 50), schedule check none, heap headroom at its lowest about 152 KB.
- Reader: rd/rd39pc.tpl.py + rd/mkpc39.py (tested on emu_n1: rd/rd39pc_n1.out).  cfg now has MGS2_LOAD_US=100000 (a load entry a
  picture = 2 vblanks).  Test cfgs in cfg_emu/ (m5off, f1off, digon, f1chk).
- Profile: pgo_a39.sh done 00:14, five training runs 0 differing (124/124, 124/124, 124/124, 63/63, 124/124); gcda/ 21 files.
- Running: rt39a.sh (use build u, pgochk, traces t0 t1 t2 with cfg/mgs2_gl39pgo.cfg).
Next: rt39b.sh (merge, seed from gl38's order_s4 with the alias rule, search 40,000 moves, rain windows x4) -> rl39.sh psp ->
rt_new39.sh (real = simulated) -> rl39.sh psp_b + cmp (relink byte-equal) -> model39.sh psp a b (levers, DHIST guard) -> fin
(package pk_f39, emulator f2 + f3 (cfg_emu/mgs2_gl39_f1chk.cfg), k0cmp39, g4 (133 names), reader) -> deliver + README + report.
Optional: NOH39=1 build (psp.sh hook) for the hot block's own model figure.
Disk: 2.5 GB free after deleting build_gl38/model_o1, model_s2 and gl39's instrumented builds / training folders.

## 2026-11-22 23:30 UTC (container clock): code done in /home/claude/t39, twin proven, first PSP build running
Code (all in /home/claude/t39; edit scripts kept in build_gl39/edits/ed1-4.py):
- ge/bound.h kind 8 (BD_ALL|BD_ME: nothing drawn, ME's turn); boot/nat_stub.h MGS2_DIG_CYCLE (rt_dig_off_at), MGS2_LOAD_CYCLE (rt_load_at).
- psp/xl_psp.c: digest-off tests (gl_list, hook), kind 8 stays the ME's (mq_bd_turn want), XE_LOAD posted in K8 turns (only when the
  loop caught up), probes p9_* (service calls by host part, 1 in 8 timed with MGS2_PROBE9; GE polls; posts; whole-cache write-backs
  timed), a 'gl39 probe' line per turn after the 'gl38 bound' line.  psp/main_psp.c host_top(); cfg key table 48 -> 64.
- ge/xme.h/.c XE_LOAD=8, xme_load (1 arithmetic, 2 cached reads of 2 MB of .text from _ftext, 3 uncached writes of a 256-byte block;
  heartbeat each block; no syscall, no register).
- boot/xlive_chk.c twin digest test; ge/xconv.c + gelive.c form B's face census (MGS2_FACE_CEN, twin only).
- runtime/kernel.h M5 behind OPT2_M5 = bit 18 (set in fffff9df; off = fffbf9df).
- ge/hot39.h H39 -> .bss.h39 -> .me_bss.h39 (psp.sh objcopy; psp/me_sec.ld first in .me_bss); tools/mesec_chk39.py allows it.
Twin (build_gl39/tw, runs39a.sh, gl38's keys): p0 685/685 and its 17,664 frame digest lines = gl38's sch2 arm (md5 3b1aa042);
  m5off 685/685; fc (face census) 685/685.
Form B census [C twin, 26,200-28,700]: chain 10: 75,350 P07 calls, 74,528 seen in the picture before; streams equal 74,528, header
  equal 74,528, ALL inputs equal 0: the 8 bone slots (VU 32..63) and slot 2 (VU 8..11) differ in every call.  B's gain is 0.
Schedule: build_gl39/tools/sched39.py (keys | table); cfg build_gl39/cfg/mgs2_gl39.cfg (tools/mkcfg39.py), 41 keys.
Scripts: tw39.sh, tw_run39.sh, runs39a.sh; link farm build_gl39/m (scripts copied from build_gl38/m, re-pointed).
Running: no-PGO PSP build build_gl39/n39 (compile check only: main_psp.c was edited while it ran, rebuild before any gate).
Next: twin arms with the gl39 cfg keys (sch, + F1CHK); em39.sh; emulator run of n-build; PGO (pgo_a39.sh from pgo_a38.sh); traces;
order; relink x2; model + DHIST guard; gates; package; report.

## 2026-11-22 23:0x UTC: build ordered by the coordinator, starting
Order (coordinator's message): build gl39, package only (build_gl39/deliver), NOT delivered. Levers M1 (box cull) and M3 (walk; dropped
if its census says the walk feeds more than VU1 memory) each with their own PSP turns; M2+M4+M5+M6 one arm; one all-on / all-off pair;
M7 placement trained on digest-off K0 and K7; F1 default-on re-use 0 with 2 F1-off turns; data-placement fix + DHIST guard;
MGS2_DIG_CYCLE; coupling probes (remainder split counters, K7 + ME-load kind on our own memory only: no new system call or import);
B's twin census of the codec faces. Gates as gl38's + DHIST guard. README, MANIFEST, rd39pc.py, report with a draft harness entry.
Tree: /home/claude/t39 (copy of t38). Build folder: /home/claude/main/build_gl39. Plan: attic/gl39/gl39_plan.md.
Done so far: plan; chain 10 pictures (build_gl38/emu_c10k0, emu_c10k5: keep); DHIST measurement (dh_main_turn.txt).
Next: copy the tree and gl38's scripts; then code in this order: DIG_CYCLE, F1 cfg, data fix, probes, M5, M2, M6, M1, M4, M3 census.

## 2026-11-22 ~23:40 UTC: scope after reading the code (findings that change the plan's lever table)
- M3 DROPPED (coordinator's rule): the hot loop in io_dma_start is the one walk itself (7,001 DMA tags + 11,730 VIF words a step,
  74 instr a tag); it feeds the translator's records, not VU1 memory. [C code runtime/ee_io.h vif1_chain_fast + model counts]
- M4 DROPPED: its "VFPU-rounded form" is round-to-nearest; the owner's choice of 2026-11-08 17:46 is "Everything exact" (all VU0
  math toward zero, macro groups and box cull included; NOTES.md line 72). Needs the owner, not a build.
- M1: the generated box cull is 384 scalar + 421 VFPU instr a call; scalar-only hand work saves <= ~90 instr = <= 0.45 model ms
  (not 1.8). Deeper cuts need vscl / masked vmov forms whose equality on hardware is unproven. Not built; a start-up hardware
  probe is the candidate (if time).
- M2: vuc_dispatch is generated (gen_vu.h); a per-site cache needs the recompiler re-run. Not built.
- M6: nat_dbc0's misses are sequential reads of the game's vertices (inherent). Dropped.
- M5: only k_take_interrupts (loops of 26 tests -> count-trailing-zeros walk, same order): ~0.3 model ms. Built behind a switch.
- My DHIST note said "gl38's new counters": WRONG, those symbols exist in gl36 too (0 misses there); it is collisions only.
So gl39 = measurement + placement build: MGS2_DIG_CYCLE, kind 8 (nothing drawn, ME's turn) + MGS2_LOAD_CYCLE (ME load flavours),
probe counters, F1 on, hot-data block, M5, M7 retrain on play windows, B's face census. Files touched so far: none (t39 = t38).

## 00:52 UTC 2026-11-23 (build agent): the trace step found two problems; the placement is on hold until they are understood
- Use build u: built (rc 0), pgochk clean. Traces of u with the training cfg (build_gl39/cfg/mgs2_gl39pgo.cfg):
  - t2 (16,101 30,901 31,001): 62 hash lines, 0 differ.
  - t0 (16,001 17,501 26,901 27,101 29,601 29,701 30,201 30,601): 61 hash lines, 2 DIFFER: vb 30,000 ee 2bf42fc32a4b1e59 (ref
    64892c084c8807d7) and vb 30,500. The same build and cfg in t2 are equal there, the no-PGO build n39 on the emulator (run n1, delivery
    cfg) is 62/62 and the twin is 685/685 in every arm. So: a state difference that shows only with profile windows at 29,601 / 29,701
    (digest-off K0 turn t56, K7 turn t57; then K8 turns t58, t59). Cause UNKNOWN. gl38's same trace (other kinds there) had 0 differ.
  - t1 (31,901 ... 37,151): its windows hold 0.8 to 1.5 thousand million instructions each (gl38: about 50 million); the trace file
    filled the disk (0 MB free at 00:47). I stopped the run (my PIDs 16796, 16888) and deleted lay/t1.it (literal path). Nothing else
    was being written then (t0, t2 finished 00:30). Cause UNKNOWN (a spin after the schedule's end?).
- Running now (00:51): four short trace runs of u to locate it: x1 (29,601 29,701 30,001), x2 (29,601 30,001), x3 (29,701 30,001),
  x4 (30,001 only, with a reference changed at 30,000 so that it writes the EE RAM there: the good copy to diff against).
  Outputs build_gl39/trace_x[1-4].out, game folders build_gl39/ms/x[1-4] (delete by literal path after copying split.30000).
- Written and waiting: g4_39.sh, fin39.sh (final emulator gates), rt39b.sh (order search), rl39.sh, rt_new39.sh, model39.sh.
- Disk 1.9 GB free.

## 01:12 UTC (build agent): the hash difference is NOT settled; findings so far
- Short trace runs of the use build u (training cfg; profile windows of 5 vblanks = the emulator's interpreter instead of its JIT):
  x1 (29,601 29,701 30,001) equal; x1b (the same again) equal; x3 (29,701 30,001) equal; x4 (30,001) equal; x2h (29,601 30,001, hashes
  every 100) equal; x2 (29,601 30,001) DIFFERS at 30,000 (ee 2fc3818d29a2165d); x2b (the same again) DIFFERS, the same value; x2c
  (the same again) equal. So three identical runs give two outcomes: it depends on the HOST (the emulator's timing on this machine),
  3 of 7 runs with a window at 29,601 differ, 0 of 3 without one.
- What differs in the EE RAM at 30,000 (x2 against x4, build_gl39/xd/split_x[24].30000, container only): 6,704 bytes in three blocks,
  01479f10-0147c16a (two chains of about 12 points: directions and positions) and its matrices in the two frame buffers
  (01b21670.., 01d21670..). Nothing else: no counter, no other object. The per-turn counts (calls, groups) are equal in every run.
  [I] a chain simulation (two ribbons) whose floats differ; cause UNKNOWN.
- Running (01:10): the twin with hashes at every vblank 29,590-30,010 (tw2 arm dn) and three emulator model-mode runs d1 d2 d3
  (window 29,601-29,606, the same dense hashes) to find the first vblank that differs.
- Not done: order search, relink, model, final gates, package. The package is NOT ready.

## 02:38 UTC 2026-11-23 (build agent, after the limit's reset): the hash difference is SETTLED: the profiling emulator, not gl39
- Cause [C, code read + counted]: the patched PPSSPP's interpreter (the profile windows run in it) computed mul.s / div.s / sqrt.s /
  cvt.s.w in the HOST's rounding mode, which the local patch set only at a slice's start and at ctc1 (add.s / sub.s set it per
  operation). After a thread switch inside a slice the host mode stayed the previous thread's: a thread with the kernel's default
  FCR31 00000e00 (idle, battery, diag, callback threads) leaves round-to-nearest for the game's thread (FCR31 01000001).
- Proof: a new emulator binary build/PPSSPPHeadless_fcr (the original build/PPSSPPHeadless md5 50406ec1... is untouched; the patch is
  attic/gl39/tools/ppsspp_pspprof_fcr_2026-11-23.diff) computes the four operations in the thread's FCR31 mode and counts.
  Window 29,601-29,606 of the use build u: 1,385,128 such operations, 30,002 in another host mode, 9,359 with another result, at
  game code addresses (P125aa8, P166090, P16b158.., S_w00a_P3215a8, S_w00a_P33b5a8).
  With the old results kept (PSPPROF_FPU_HOSTMODE=1): 4 of 4 runs differ at 30,000 (ee 2fc3818d29a2165d, the value of x2 / x2b).
  With the fix: 4 of 4 runs equal (60/60). The JIT-only runs never differed (n1 62/62, five training runs, the twin 685/685).
- Earlier probes for the record: gl38's build + cfg, old emulator, 4 of 4 equal (g1-g4); u with K8 -> K7 2 of 2 equal; u with hashes
  every 100 from 29,500 3 of 3 equal; dense hashes 3 of 3 equal. Why the old emulator's runs were sometimes equal: UNKNOWN (the
  counts differ a little from run to run: the emulator's timeline is not the same in every run).
- t1's oversized windows: NOT reproduced with one window at 31,901 (38 million instructions, hashes 65/65). [I] the cause was the
  cfg: gl38 traced t1 with "deck 31800" (cfg/mgs2_gl38trc_t1.cfg); gl39's t1 had the training cfg's "deck 24000". New
  cfg/mgs2_gl39trc_t1.cfg = the training cfg with deck 31800.
- Running (02:37): t0, t1 (new cfg), t2 again, all on PPSSPPHeadless_fcr (PPF in the environment; pgo.sh not edited), and g5 (gl38's
  build + cfg with the old results kept, to count the same operations there).
- Next: rt39b.sh (order search), rl39.sh, rt_new39.sh, model39.sh (PPF = the fixed binary; k38x is the check that the model's
  counts are those of gl38's notes), g4_39.sh, fin39.sh, package.

## 04:22 UTC 2026-11-23 (build agent): order searched, first final build gated, DATA GUARD FAILED on it, padded relink is the new final
- Traces on the fixed emulator: t0 61/61, t1 117/117 (cfg/mgs2_gl39trc_t1.cfg: deck 31800; windows 37-52 million instructions), t2 62/62.
- Order: rt39b.sh (40,000 moves) + rt39b2.sh (20,000 moves with the corrected weight file: rt39b.sh's wf1.txt named windows 5..10,
  they count from 0, so 29,601 had weight 1 in stage 1). I-misses simulated, seed -> final: deck training windows 4,785,837 ->
  2,920,194, test 329,224 -> 220,430; w01a 3,508,483 -> 2,271,823, test 435,484 -> 274,950. Relink: real = simulated in every window;
  a second relink byte-equal.
- First final build (now build_gl39/psp_np, EBOOT 329e2cfd; its gates in *_np files): emulator f2 62/62, f3 (F1 check) 62/62 with
  474,212 groups compared 0 differing, digests by kind = the twin's (K0 166, K7 50, K8 50), G4 133/133 names (g4_39.sh had compared
  the build with itself: fixed, run again), reader EQUAL. Model (model_np, tools/score39.py): the emulator check holds (gl38's build
  now against gl38's own model runs: within 0.5 ms), but
  THE DATA GUARD FAILED: same cfg, ME turn 29,810: gl39 - gl38 +4.75 ms a step (D-misses +15,739, I-misses +3,360), static
  D-misses 110,174 against gl38's 98,968 and gl36's 96,558; K7 of the same turn (t67): +1.11 (D +4,094).
  Cause [C by experiment]: where the data lies against the cache's 8 KB ways: .rodata moved by 0x3c0 and the picture tables by
  0xa40 (mod 0x2000) against gl38. Relinks with padding (no compile, same code order), delivery cfg, against psp_np:
    ME tables back on gl38's sets (ME_PAD 0x15c0):            K0 -1.24 / -1.13, K7 +0.04
    + .rodata back at gl38's offset (RO_PAD 0x1c40) = "pd":   K0 -2.09 / -2.00, K7 -0.56
    + .data too (DA_PAD 0x1be0):                              K0 -2.25 / -2.18, K7 -0.73   (22 KB of heap; .data alone does nothing)
    .rodata + .data only:                                     K0 +0.27 / +0.16, K7 -0.70
- New final build build_gl39/psp = pd (EBOOT e2d681b5; padld/me_sec_pad.ld + padld/ro_pad.ld through MESEC / XLD in m/scripts/psp.sh,
  rl39p.sh; 13.5 KB of memory: the break 16 KB above psp_np's [to be read from f2]). Its gates are running: fin39.sh, model39.sh a b.
- tools: score39.py (hw is the DIGEST's hash, not a wait: the plan's prof39.py label was wrong), fdiff39.py, padcmp.py.

## 05:06 UTC 2026-11-23 (build agent): final build is now the THIRD padded relink (EBOOT 52eb4246); M5 is NOT in the PSP EBOOT
- M5 IS NOT IN THE PSP BUILD (my mistake, found 04:52 from the model: k_take_interrupts is instruction for instruction gl38's).
  The M5 block went into runtime/kernel.h, the PC twin's file; the PSP's boot_rt.o is compiled with rt/ ahead of runtime/ and
  rt/kernel.h is gl38's, unchanged. So MGS2_OPT2's bit 18 does nothing on the PSP [C code: no other reader], the twin's m5off arm
  proved the twin's code only, and "M5 off - on" model lines are two runs of the same code (= the model's run-to-run spread:
  K7 +0.19 / +0.44 ms a step, K0 -0.06 main). Not rebuilt: adding it changes inlining in 17 runtime functions (+17.7 KB of
  boot_rt.o text in a side compile, build_gl39/u5), i.e. a new profile, traces and order search (3-4 hours) for about 0.2 ms [I].
  Words corrected (edits/ed5_m5_inert.py): the cfg's comment 6 (no key changed; cfg md5 now 4263dd11), tools/mkcfg39.py (makes the cfg
  byte for byte), the reader template (turns no longer split or labelled by M5; a NOTE line says it).
- The first padded build (NOBITS pad, EBOOT e2d681b5) had a changed ELF: 5 program headers, the text segment RWE with MemSiz > FileSiz
  (the emulator took it; a risk on the PSP's loader). padld/ro_pad2.ld (a READONLY section with contents) gives gl38's structure
  (3 LOADs: R E with FileSiz = MemSiz, RW, RW bss): relink "pf", EBOOT 459e41ab, the same loaded bytes and symbols as e2d681b5
  (flat image cmp); its gates all passed (fin39_pf.out, f2/f3 62/62, digests by kind, G4, reader) and real = simulated.
- Model of pf against gl38's FINAL build at the same turns and kinds (new cfg_emu/mgs2_gl38_y.cfg, run k38y; score39_pf.out):
  K0 digest-off F1 off: main +0.39/+0.42, ME +0.93/+0.78; K7 +0.36..+0.52; the plan's DHIST line (static D-misses <= gl36's + 2 %
  in the ME turn) still FAILED: 101,992 against gl36's 96,558 (+5.6 %), gl38's 98,968.
- Cause of the rest [C, link map + DHIST by symbol]: the hot block took 0x210 bytes out of ge_xlate.o's .me_bss, which moved its later
  tables (own, VM, gr ...) and every table behind them (ge_xconv.o's 331 KB) by -0x210 / -0x240 against gl38's cache sets.
  padld/me_sec_pad2.ld: XL_PAD 0x210 in front of ge_xlate.o's .me_bss. Relink "px" (576 bytes more memory; every code symbol at pf's
  address; real = simulated; second relink byte-equal 10 of 10 files), EBOOT 52eb42460054cd3a3600d82026fb33cb, against pf [C model]:
  K0 -0.94 / -1.01 / -0.94 (rain wait), walk K0 -0.37, K0 F1 off -1.93, K7 0.00, guard cfg ME turn -1.71, main-drawing turn +0.26.
  Static D-misses in the guard's ME turn 98,135: under gl38's 98,968, gl36's + 1.6 % -> the DHIST line PASSES there.
- NOW the final build: build_gl39/psp = px (52eb4246). Running since 05:05: fin39.sh (f2, f3, digests by kind, G4, reader) and
  model39.sh psp a b (PPF = PPSSPPHeadless_fcr). Kept: psp_pf (459e41ab) + *_pf outputs, model_pf, psp_np (329e2cfd) + *_np outputs,
  score39_np.out, dh_np_d38.out / dh_np_d36.out. Deleted (literal paths): psp_nb, model_np, n39, pk_* of old candidates, the merged
  traces lay/t0n.it + t1n.it (rt_new39 was run on px first), picture folders of old emulator runs.
- A plain recompile of boot_rt.o with the use build's command is byte-equal to u/boot_rt.o [C]: the tree is what u was built from.
- Next: read fin39.out; score (tools/score39.py with the k38y lines, tools/dh39.py guard); M5's own model figure from the side
  build u5/p5 (model only, never delivered); README, MANIFEST, deliver/, attic copies, report.txt, final entry here.

## 05:50 UTC 2026-11-23 (build agent): DONE: gl39 packaged (NOT delivered), report written
- Package: /home/claude/main/build_gl39/deliver (18 MB, 15 files): EBOOT 52eb42460054cd3a3600d82026fb33cb, cfg 4263dd11, reader
  rd39pc.py b7966785, README b20a69ba, MANIFEST.md5 91fdaacf. Copies of README / MANIFEST / reader + report.txt in attic/gl39/;
  tools, scripts, pad linker scripts and result tables in attic/gl39/{tools,scripts,padld,results}.
- Final gates (fin39.out, 05:16): f2 62/62, f3 62/62 with 474,212 groups compared 0 differing, digests 266/266 = the twin's sch
  arm by kind (K0 166, K7 50, K8 50), G4 133/133 names, 3 LOAD segments as gl38's, reader EQUAL / EQUAL / schedule check none,
  heap low point reads 64 KB (break 0be66000). K0 lists = the plain run's in 403 of 405 frames with digests on in every turn.
- Final model (score39.out, dh39_guard.out; model39.sh a b 05:34, all runs hash-equal): digest on - off ME +22.7 (walk +17.7);
  F1 off - on ME +1.5 (walk +0.2); K8 no load - K7 +0.45; gl39 - gl38 same turn: K0 digest-off F1 off main +0.35 / +0.41, ME
  -0.07 / -0.02; K7 +0.39..+0.47. DHIST line: ME turn static 98,122 = gl36 + 1.6 % (PASS), K7 gl38 + 1.0-1.2 %.
- M5's own figure from a side build with the block in the PSP's kernel file (model only, deleted): 0.11 ms a step, hashes 59/59.
- Expected PSP rain-wait step [I]: about 76 ms (digest-off K0, F1 on), main-CPU limited.
- OPEN for the lead: (1) M5 is not in the EBOOT: ship as a measurement build or order a rebuild step; (2) the fixed profiling
  emulator binary (PPSSPPHeadless_fcr) needs a word to the owner; (3) the main CPU is +0.4 ms a step over gl38's on the model;
  (4) deliver only after gl38's log is read (it is: gl38_psp_read.md) and after checking the package.
