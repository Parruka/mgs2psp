# gl40 planning: state (kept by the planning agent; newest block last)

## 2026-11-23 (UTC), block 1: reading done for item 1, first finding
PLAN ONLY round: nothing built, nothing packaged, no file outside attic/gl40 changed.
Read: prof/hw_gl39/gl39_psp_read.md, psp_harness.md "Build gl38" / "Build gl39" (+ the 4 rules), attic/gl39/state.md + report.txt,
h1_notes.md, attic/gl39/prof_rain_k0_minus_k7.txt, the gl39 tree /home/claude/t39 (psp/xl_psp.c, rt/ee_io.h, ge/xlme.c, ge/bound.h,
psp/main_psp.c), build_gl39 (model39.sh, tools/score39.py, emu_f2/log.txt), build_gl38 (model/, prof/hw_gl38/log.txt).
FINDING 1 [C code + C model + C PSP gl38 run 1]: the "unknown ~8 ms of logic in drawing turns" is mostly a filing error of the
model's readers (prof39.py, score39.py), not unknown work:
 - they file every function of a ge_* object under "ME", but the copy plan (ge/xlme.c xm_plan, xc_merge, xc_add) is run by the MAIN
   CPU at each recorded transfer's end (psp/xl_psp.c xlive_chk_end -> mq_chk_end -> mq_plan), outside every timed part = "logic":
   5.58 model ms a PSP step in gl39's play-form turn (t56 - t57; 5.59 in t62 - t64);
 - they merge functions by NAME: "gif_qw +2.57 (boot_rt)" is ge/xlate.c's own static gif_qw (the translator's, on the ME), not the
   runtime's.
 Corrected main-CPU drawing work, gl39 t56 - t57 [C model]: plan 5.58 + runtime/harness 1.90 + game 0.34 = 7.8 ms a step.
 gl38's PSP run by kind agrees: K0 - K7 logic +7.0..+7.4 [C PSP] against model main +5.3..+6.1; K6 +0.8 against +0.1.
Scratch tools (container only): scratchpad gl40/sides.py, kinds38.py, callers.py, who.py.
Next: items 2 (H1), 3 (placement), 4 (M5), 5, 6; then gl40_plan.md.

## 2026-11-23 14:30 UTC, block 2 (second planning agent; continues block 1, PLAN ONLY)
- Re-read block 1 and the five sources; block 1's item-1 split reproduced from build_gl39/model/ka39.1 - ka39.2 with sides.py: main-pic
  5.58 (xc_merge 2.57, xm_plan 1.98, xc_add 1.04), main-rt 1.90, game 0.34 [C model]. Units checked: score39/sides divide a window by
  its 80 vblanks; mq_frame runs 0.5 a step (40 pictures a window), and t57 K7 model 68.28 vs PSP K7 67.06: these "ms a step" are
  already per PSP step (no halving for them) [C]. Halving applies only if a script divides by pictures (none found yet).
- Item 3 model runs that block 1 had started (exp40, mod40x.sh, gl38's guard cfg mgs2_m37_rc0, windows 16110 21110 23110 27510)
  finished 14:18, 55/55 hash lines equal each. gl39 (c0) minus gl36, ms a step [C model]: 16110 +1.80 (I +0.61, D +1.16),
  21110 +1.45 (I +0.95, D +0.41), 23110 +0.14, 27510 +1.57 (I +0.59, D +0.87). c1 (ME_PAD 12c0, RO_PAD 1800) and c2 (ME_PAD 1040,
  RO_PAD 1580): I the same, D +0.48..+0.87: no pad pair repairs every window. DHIST 16110: dynamic +2,959 a step (heap 0b4xxxxx
  +1,478, 0bfxxxxx +1,130), static +1,546 (St, F, glf, C1m, H, lw_mem, g_io_ee/seen).
- M5 side compiles (exp40/m5 v0/va/vb, block 1): v0 (= rt/kernel.h) is NOT byte-equal to build_gl39/u/boot_rt.o (+17.3 KB text)
  although the command and gcda are u's: [I] the header found at another path changes GCC's line checksum of every kernel.h
  function, so their profiles are dropped; va against v0 changes only k_take_interrupts (352 -> 552 B). To settle with
  -fprofile-prefix-map or a same-path copy.
- Running next: an instrumented copy of the x86 twin (scratchpad gl40b/tw, MGS2_NAT_LW=2) to count the copy plan's ranges, runs and
  duplicates per transfer, and to check candidate cheaper merges against xm_plan's extents (twin only; nothing built for the PSP).

## 2026-11-23 14:50 UTC, block 3 (findings so far)
- ITEM 1 data [C twin]: an instrumented copy of gl39's twin (scratchpad gl40b/tw: ge/xlme.c + xs_stat.inc, MGS2_NAT_LW=2 runs the
  PSP's xm_plan on every lane transfer; runs lw2, lw2b: 685/685 hashes = hashref_gl38 each). Rain wait vb 29,600, a step: 6 plans,
  6,441 xc_add calls (PSP model 6,282), ra 37 ranges -> 22 extents, rb 2,175 ranges in 934 ascending runs -> 362 extents; the
  bottom-up sort moves 24,876 elements a step (PSP model ~22,300: same order). Natural merge sort -14% only (runs ~2.3 long).
  Joining a range with any of the last 8 (not only the last) leaves rb 764; exact duplicates (equal k0 and k1) are 780 of 2,175;
  same start, other length 0. A candidate pipeline (natural merge of the same ranges) gave extents EQUAL to xm_plan's in all
  198,799 plans of the route [C twin]: the plan's output is the union of the ranges, so any union-keeping change is exact by
  construction and checkable plan by plan.
- So the copy plan stays on the main CPU and gets cheaper (moving the merge to the ME needs the line marks made from 2,400 raw
  ranges at each transfer's end and the arena's room decisions moved: a protocol change for ~2.2 model ms; not chosen).
- M5 (item 4) [C side compiles]: boot_rt.o compiled with gl39's command in another folder is byte-equal to u's (pz); with
  kernel.h (unchanged) found at another path it is +17.3 KB (v0); -fprofile-prefix-map does not change that (pv0 = v0). So the
  gl39 report's "+17.7 KB, 17 functions re-inlined" was the header's path, not M5. M5 alone (va vs v0) changes only
  k_take_interrupts (352 -> 552 B). In place in rt/kernel.h it still shifts the line numbers of the functions below it, which
  drops their stale profile: harmless in a build that trains a new profile (gl40 must anyway).
- Lead note 14:4x (hd_study sections 1, 2, 4) read: dice cut = later build; H1 questions for the libdg source to list in the plan;
  lamps' line test: HZX_OnlineHazardCheck (P14fe30) has getters (HZX_GetOnlineHazard*, libhzx/onlinex) [names], so a skipped test
  may change a module state others read; the lamp alphas are RAM: hash moves either way -> not hash-exact; ~0.3 [I]: not gl40.
- Running: twin lw2c (lookback 16/32 and a 1 KB page-hash join, counted only).

## 2026-11-23 15:15 UTC, block 4 (plan written: attic/gl40/gl40_plan.md)
- Twin lw2c (rain wait 29,600, a step) [C twin]: rb left after a join with any of the last 16 / 32: 639 / 528; a 1 KB page-hash join:
  678 left, 6,850 moves, 5,043 probes. Twin k8c: the exact candidate (join with any of the last 8, then sort and join) gave extents
  equal to xm_plan's in all 198,799 plans; 685/685 hashes; sort moves after it 6,655 (raw 24,876). Chosen: XP = look-back K 8 +
  a ping-pong merge (xc_merge copies its scratch back with me_memcpy after each pass: ~0.65-0.9 model ms a step hidden in "shared").
- Item 3, c3 (gl39 + END_PAD 0x1a00 after .me_bss: heap block 0x0b239900, gl36's offset) [C model, 55/55 hashes]: 16,110 60.47,
  21,110 60.60, 23,110 24.40, 27,510 61.65 (gl39 61.04 / 60.25 / 24.41 / 62.18; gl36 59.24 / 58.81 / 24.27 / 60.61): helps two
  windows, hurts 21,110; not the fix alone. Files: exp40/end_pad.ld, exp40/c3, exp40/model/xc3*, scratchpad gl40/rl40y.sh.
- Lead notes 14:5x and 14:55 (hd_study section 7.1) folded into the plan's H1 section.
- Plan copied to attic/gl40/gl40_plan.md (md5 61b7fe0e). Nothing built for the PSP, nothing packaged.

# gl40 BUILD round (build agent; newest block last)

## 2026-11-23 ~15:30 UTC, block B1: build round started
- Read: gl40_plan.md (md5 61b7fe0e), state blocks 1-4, psp_harness.md "Build gl39" + its 4 rules, attic/gl39/state.md + report.txt.
- Scope (lead): XP (MGS2_XP_CYCLE bits 1/2, digit 7 check), MGS2_PROBE10 every turn, M5 as noinline k_take_m5 in rt/kernel.h + per-turn
  counter, retrain with the four main-drawing guard windows + binding guard (I-misses <= gl36 + 2 % each), END_PAD/RO_PAD search.
  NOT: H1, dice cut, lamps, anything that moves a state hash.
- Plan of folders: tree /home/claude/t40 (copy of t39), build folder /home/claude/main/build_gl40 (m/ link farm -> t40, scripts copied
  from build_gl39 and re-pointed). Disk at start 3.7 GB free.
- Next: code (step 7.1).

## 2026-11-23 15:20 UTC, block B2 (code done, twin and first PSP compile running; B1's "~15:30" was ~15:00)
- Tree /home/claude/t40 (copy of t39; edit scripts build_gl40/edits/ed1..ed6): ge/xlme.h XmCopy + XP fields (xp, counters, check
  results, clk, probe times); ge/xlme.c: xc_merge ping-pong (XP bit 1), xc_add look-back of 8 on the miss path (bit 2), xm_plan split
  into xc_ranges + xc_plan, check mode (bit 4: XP's plan kept, gl39's ranges made again and compared extent by extent, every result
  restored); rt/kernel.h AND runtime/kernel.h: noinline k_take_m5 + g_k_m5 counter; boot/nat_stub.h: MGS2_XP_CYCLE / MGS2_XP_ALL /
  MGS2_XP_CHECK, rt_xp_at(vb); psp/xl_psp.c: MGS2_PROBE10 (gl40 plan line per turn, gl40 XP totals at the end); boot/xlive_chk.c:
  the twin's LW path sets XP per turn and prints "nat lw: gl40 XP totals".
- FINDING (container): the lead's 15:05 delete of build_gl19* took build_gl19/x86, which tw39.sh links (O=build_gl19/x86).  Rebuilt
  gl19's x86 objects into build_gl40/x19 (scripts/x86.sh obj); check [C]: gl39's tw2 objects relinked with x19 give a boot
  byte-equal to build_gl39/tw2/x86/boot.  build_gl40/tw40.sh links against build_gl40/x19/x86.
- Twin built (build_gl40/tw/x86, rc 0; k_take_m5, rt_xp_at present).  Arms running since 15:10 (runs40a.sh): sch, p0 (gl39 keys),
  m5off, lw0/lw3/lw7/lws (MGS2_NAT_LW=2: every lane transfer through the PSP's plan; lw7 = check in every plan).
- No-PGO PSP compile n40 running since 15:11 (m/scripts/psp.sh, PSPD=build_gl40/n40).
- Cfgs: cfg/mgs2_gl40.cfg (gl39's + gl40 keys, header notes), mgs2_gl40pgo.cfg (no shots), mgs2_gl40trc_t1.cfg (deck 31800),
  cfg_emu/mgs2_gl40_xpchk.cfg (+MGS2_XP_CHECK=1).  Schedule tools/sched40.py.
- Placement tools written: layout_sim40 (layout_sim38 + "-bind k": a -wf cap becomes a binding ceiling, k x 1e8 / cap per miss
  over it); rt40a.sh (use build, traces t0/t1/t2 with gl40's kinds, t3 = the guard cfg's four main-drawing windows, two 5-vb windows
  each, model of u on the four 80-vb windows); rt40b.sh (caps = u's trace-window misses x 1.02 x gl36/u model ratio [I proxy],
  seed gl39's final order, rain x4, guard x2 + caps); rl40.sh (relink with gl39's pads, END_PAD optional).

## 2026-11-23 15:40 UTC, block B3 (twin gate passed; n40 built; PGO training running)
- TWIN [C twin, build_gl40/tw, hashref_gl38 c9ddb593]: sch, p0, m5off, lw0, lw3, lw7: 685/685 each, hashref byte-equal each.
  p0 (gl39's keys, new code) digests = build_gl39/tw2 sch arm's: 17,014 lines, md5 8be40bf85beb both.
  lw0 (gl39's plan in every transfer), lw3 (XP both parts in every plan), lw7 (XP + check in every plan): digests 16,903 lines,
  b8a2ae4fb016 in all three = the sch arm's.  lw7 XP CHECK over the whole route: 201,799 plans compared, DIFFERING 0, by result 0.
  XP's counts over the route: rb ranges into the merge 65.59 M -> 25.53 M (-61 %), ra 1.54 M -> 1.19 M, sort moves 752.0 M ->
  262.4 M (-65 %), copied back 752.0 M -> 0; extents 12,793,791 both.  lws (gl40's cycle with LW) still running.
- n40 (no-PGO PSP compile, EBOOT 669ccaf8): psp.sh rc 0, mesec_chk PASS, me_call_chk PASS, xme_rest_chk PASS; G4 against gl39's
  psp: 133/133 sce names, stub sections equal.  M5 IN THE PSP ELF [C psp-nm/objdump]: k_take_m5 at 088adf2c (two clz walks,
  jal k_intc.part.0 / k_dmac.part.0), k_take_interrupts tests g_opt2 bit 18 (ext v1,0x12) and tail-calls `j k_take_m5`;
  g_k_m5 counter; emulator log shows M5 walks a turn (1,300-2,100 a turn from vb 600).
- Running: emulator n1 (n40 + delivery cfg, em40.sh), pgo_a40.sh (5 instrumented builds + training runs, PPSSPPHeadless_fcr).
- Written: rt40a.sh, rt40b.sh, layout_sim40, rl40.sh (+padld/), model40.sh, fin40.sh, g4_40.sh, tools/k0cmp40.py, symshift40.py
  (data symbols' shift mod 0x2000 between two ELFs, weighted by DHIST), rd/rd40pc.tpl.py + mkpc40.py (reader: XP and M5 by turn,
  PLAN PROBE table, XP check counter).

## 2026-11-23 16:35 UTC, block B4 (emulator n1 passed; PGO trained; use build u built; traces running)
- Twin lws (gl40's own XP cycle with LW 2): 685/685, digests = sch's, XP check 1,200 plans (t55, t68) 0 differing.
- EMULATOR n1 (n40 no-PGO + cfg/mgs2_gl40.cfg, PPSSPP 1.20.4) [C emulator]: 62/62 hashes; digests 155 frames (K0: t54's last 10 vb,
  t55, t65, t68) = the twin's sch arm, 0 differ; "gl40 XP totals": XP check 1,200 plans compared, DIFFERING 0; copies not made
  (arena) 388 = gl39's f2 (388): not new.  SCHEDULE CHECK none (reader test on the partial log); M5 walks a turn 0 in t1, ~27,000-
  41,000 in deck turns (t0 shows 455: the cycle's switch falls a step after the turn line's boundary; the reader allows <= 20 a step).
  Heap: low point vb 23,677, headroom ~156 KB (no pads in n40; image end 77 KB below gl39's padded build).
- PGO (pgo_a40.sh, 15:24-16:26): five instrumented builds rc 0; training runs game/rtb/pic/w01a 124/124 hash lines 0 differ, w00a
  63/63 0 differ; 20 gcda (build_gl40/gcda/md5.txt).  Instrumented build folders deleted after (literal paths).
- Use build u (16:32, rc 0, mesec_chk / me_call_chk / xme_rest_chk PASS): text 4,457,004 (gl39 u 4,450,388, +6.6 KB); boot_rt.o
  +1,660 B only (the profile trained through the same path: no "+17 KB" header-path effect), k_take_m5 300 B, k_take_interrupts
  324 B (gl39 492); ge_xlme.o +2.1 KB.
- Running: rt40a.sh traces t0/t1/t2 (training cfg), then t3 (guard cfg) + model of u on the four guard windows.

## 2026-11-23 17:45 UTC, block B5 (after the pause 16:55-17:40: checked, half-written step redone)
- Verified before trusting: traces t0 (61/61 hash lines, 0 differ, 8 windows), t1 (117/117, 8 windows), t2 (62/62, 4 windows) complete
  (16:47-16:49); pgochk_u clean (every compared object equal to the build's).
- HALF-WRITTEN: rt40a.sh's step 3/4 were cut at 16:55: t3 had 6 of 8 windows, the model mu 3 of 4.  Set aside in build_gl40/x_int/
  (not used); game folders ms/t0..t3, mu looked at and deleted (literal paths).  Rerun as rt40a2.sh (started 17:43).
- Next: rt40b.sh (merge, caps, seed, the order search with -bind), rl40.sh, rt_new, pads, model40.sh, fin40.sh, package.

## 2026-11-23 18:50 UTC, block B6 (order searched with the binding guard; relinked; real = simulated; pad candidates modelling)
- rt40a2.sh (17:43-17:51): t3 8 windows 55/55 hash lines 0 differ; model of u (guard cfg) 55/55: I-misses a window 16,110 5,158,843
  / 21,110 2,882,644 / 23,110 175,404 / 27,510 5,082,468 (gl36 3,073,622 / 1,896,980 / 172,720 / 3,067,354).
- rt40b.sh (17:51-18:44, layout_sim40 -norm -guard 10 -bind 20, 40,000 moves; rain windows x4, guard windows x2 with caps):
  caps (u's 5-vb window x 1.02 x gl36/u) 199,585 225,188 142,822 136,986 10,412 10,628 163,131 163,150; ALL 8 UNDER at the end
  (by 4.5-10.9 %) [C sim].  Seed (gl39's order on gl40's code) -> result: deck route training windows 8,766,483 -> 6,741,749 (both
  routes), rain windows -22..-28 %, test windows 627,103 -> 483,894.  Seed: 2,012 of gl39's 2,013 names kept, 1 alias replaced
  (vupv_8baafbc3_0 -> vupv_660a267e_0), 38 newly hot appended (k_take_m5, k_intc, k_dmac.part.0, mq_chk_end_do, ...).
- Relink build_gl40/psp (gl39's pad values): psp.sh rc 0, mesec/me_call/xme_rest PASS; rt_new40.sh: real = simulated in every window
  of both routes (0 hot functions missing or resized); second relink psp_b byte-equal 10 of 10 files (deleted after).
- readelf -l [C]: 3 LOADs as gl39 (R E FileSiz = MemSiz 0x441e80 (gl39 0x4404a0), RW 0x21e4/0x2294, RW bss 0x25f0140); with gl39's
  pads the bss segment starts 0x2000 higher and .rodata/.data are -0x620 off gl39's way offset, .me_bss +0x300.
- Data shifts against gl39 (tools/symshift40.py, weighted by gl39's DHIST in the guard and rain windows) [C]: the main .bss after g_mem
  +0x88 / +0x80 (boot_rt's new XP table xp_cyc 128 B + 2 words: g_spu, k_hd, sif_calls, g_io_seen, g_io_ee) and xl_psp's +0xd8..+0x300
  (XmCopy +0x58, the probe's 0x200): NO pad can move these (a code change would); .me_bss +0x300 (the .bss growth); .rodata -0x620.
- Pad candidates (all mesec PASS, 3 LOADs): pd = gl39's values (ME 0x15c0 XL 0x210 RO 0x1c40); pa = RO 0x260 ME 0x12c0 (rodata,
  data, every heavy .me_bss table at gl39's offset, the small h39 block +0x300, image end = gl39's exactly); pb = RO 0x260 ME 0x15c0
  + ME_PRE 0x1d00 (padld/me_sec_pad3.ld: a NOBITS .me_pre before .me_bss; all of .me_bss at gl39's; +8 KB); pc = pa + END_PAD 0x1a00
  (heap at gl36's offset); pe = pa with RO 0x1080 (.rodata/.data at gl36's offset); pf = pb + END_PAD 0x1a00.  Modelling the four
  guard windows on each (model40.sh g, started 18:48).

## 2026-11-23 19:20 UTC, block B7 (pad candidates on the four guard windows; THE CODE GUARD HOLDS)
- Guard cfg (gl38's mgs2_m37_rc0), 4 windows, ms a PSP step [C model, PPSSPPHeadless_fcr, each run 55/55 hash lines]; gl36 / gl39:
  16,110 59.24 / 61.04; 21,110 58.81 / 60.25; 23,110 24.27 / 24.41; 27,510 60.61 / 62.18.
  I-misses a step, every candidate (same code order): 37,842 / 21,774 / 2,073 / 37,442 against gl36's 38,420 / 23,712 / 2,159 /
  38,342 and gl39's 41,413 / 28,381 / 2,149 / 41,221: UNDER gl36 in all four (-1.5 / -8.2 / -4.0 / -2.3 %): the binding guard held.
  Candidates, sum of the four minus gl39 / minus gl36 (worst window minus gl39): pd (gl39's values) -2.03 / +2.92 (-0.15);
  pa -2.61 / +2.34; pb -3.10 / +1.85; pc (pa + END_PAD 0x1a00) -4.02 / +0.94 (-0.12); pe (rodata at gl36's) -2.23 / +2.73;
  pf (pb + END_PAD 0x1a00: all .me_bss at gl39's offsets, heap block at gl36's address 0b23b900) -4.21 / +0.75 (-0.12).
  Per window pf: 59.51 / 59.04 / 24.29 / 60.83 (pc: 59.72 / 58.81 / 24.29 / 61.04).  What is left over gl36 is data (D-misses).
- Rain check of pc and pf (delivery cfg, ka40's 8 windows) running since 19:19; psp_pa/pb/pe deleted.

## 2026-11-23 19:41 UTC, block B8 (placement chosen: pf; final build relinked; emulator gates running)
- Rain check pc vs pf (delivery cfg, 8 windows, 61/61 each) [C model]: pf better in every K0 window by 0.42-0.77 ms a step, K7 equal.
  pf: t55 178.83, t56 XP0 151.99, t57 XP3 149.21, t58 K7 68.53, t59 XP1 149.78, t60 XP3 149.23, t61 XP0 152.28, t64 K7 69.38.
- FINDING: against gl39 at the same turn the rain windows are a little slower: t56 K0 XP0 +0.75 (I-misses +3,879 a step, D +1,350;
  main -0.16 with the plan 13.81 vs 13.92, ME +0.91, game +0.31), t64 K7 +0.50 (main +0.50) [C model].  The extra I-misses are spread
  over many functions (+16.9k / -13.0k), not in the new code; pads cannot fix it (it is code order: the guard windows got the room).
  Not chased further (no second search stage: scope).  XP's own gain is larger: XP3 turns -2.8..-3.1 under the XP0 turns beside them.
- Chosen: pf (order s1 + RO_PAD 0x260 ME_PAD 0x15c0 XL_PAD 0x210 ME_PRE 0x1d00 END_PAD 0x1a00).  psp (gl39 pads) -> psp_pd; relinked
  into build_gl40/psp: byte-equal to psp_pf in 110 of 111 files (make.log differs: times).  EBOOT 530f80825757d3519e341bfd6e2aa748.
- readelf -l [C]: 3 LOADs; R E 0x4404a0 = gl39's exactly; RW 0x2204/0x22b4 (gl39 0x21c4/0x2274); bss 0x25f3840 (gl39 0x25efe40,
  +0x3a00 = .bss +0x300, .me_pre 0x1d00, END_PAD 0x1a00).  Hot data mod 0x2000 against gl39 (symshift40, gl39's rain DHIST) [C]:
  g_mem/.bss head +0, .me_bss +0, .rodata +0, .data +0; left: .bss +0x88 (27.2k D-misses a step in gl39's map: vif0_buf, g_spu,
  g_io_seen, ...) and +0x80 (13.7k), the new XP table; .me_data +0x40 (1.0k).  Code symbols identical to psp_pd (so rt_new holds).
- k_take_m5 in the final ELF [C]: 08a5a980, 0x12c bytes, 2 clz; k_take_interrupts 088a0b20 'j k_take_m5' after ext v0,v0,0x12,0x1.
- Running: fin40.sh psp f (pk_f40, em f2 delivery cfg + f3 XP check every plan), rt_new40.sh on psp.

## 2026-11-23 19:55 UTC, block B9 (the final build's gates: emulator, ELF, G4, rt_new)
- rt_new40.sh on the final psp [C sim]: 0 hot functions missing or resized on both routes; training windows 4,389,708 + 2,352,041 =
  6,741,749 I-misses = the search's own figure (real = simulated); test windows 195,214 / 288,680.
- fin40.sh psp f (19:40-19:49): package pk_f40 (EBOOT 530f8082, mgs2.cfg fbd15627, hashref c9ddb593, pad 7bc72ace).
  f2 (the package as delivered) [C emulator]: 62/62 hash lines equal hashref, 0 differ; 155 digest frames (29,490-30,898) equal to the
  twin's same-schedule arm (0 differ) and to the no-PGO build's run n1 (0 differ); reader rd40pc_f.py on f2: HASHES EQUAL, DIGESTS
  EQUAL, SCHEDULE CHECK none, END line present (4,497 log lines).  XP totals: plans 100,604, arena failures 388 (= gl39's and n1's),
  check (t55, t68) 1,200 plans 0 differing.  M5 walks 3,607,139 (off turns 0-4.5 a step, on turns 15-450).
  f3 (XP check in every plan, cfg_emu/mgs2_gl40_xpchk.cfg) [C emulator]: 62/62 hashes; digests = twin; XP check: plans compared
  100,604, DIFFERING 0; 1,970 of them compared by result only (the plan ran out of extent/arena room; the host plans it again with
  room and that plan is compared in full).  The twin (lw7): 201,799 plans compared in full, 0 differing.
- Heap [C emulator]: the image ends at 0b23b840 (gl39 0b237e40, +14.5 KB); heap block 0b23b900 = gl36's address.  Low point: the
  break's highest 0be6a000 at vb 23,677, headroom about 64 KB at its lowest (gl39: 64, gl36: 64); at the end malloc can still give
  64 KB in 1 block (as gl39).  'ME load entries not posted' 2 (gl39's emulator run: 1).
- G4 [C]: 133/133 sce names, 0 differences; stub sections the same sizes; no object with a new sce reference; xme_me / xme_rest /
  mesec / me_call PASS.
- Running: model40.sh psp k b (kb40, dn40); ka40 and gd40 are psp_pf's runs (byte-equal build).

## 2026-11-23 20:04 UTC, block B10 (model of the final build; package made; DONE for the build agent)
- model40.sh psp k b (kb40 19:40-19:54, dn40 -20:02; each 62/62 resp. 60/60 hash lines, 0 differ); ka40/gd40 = psp_pf's runs
  (byte-equal).  score40.py -> build_gl40/score40_final.txt (copy: attic/gl40/score40_final.txt).
- XP [C model, the plan's functions, main CPU]: XP3 - XP0 -2.01 / -2.02 / -1.99 (t57-t56, t60-t61, t63-t61), heavier t66-t67 -3.07
  (merge -1.8, copy-back -0.9, xc_add +0.7); XP1 - XP0 -0.88 (copy-back).  Walk t70-t69 confounded by content.  NOTE: score40 files
  me_memcpy under ME, but xc_merge's copy-back on the main CPU calls it; its -0.9 is main-CPU time.
- M5 [C model]: k_take_interrupts + k_take_m5 0.28 a step on, 0.60 off: -0.32 in every kind of turn (plan ~-0.11).
- gl40 - gl39 same turn [C model]: t56 K0 XP0 main -0.16 ME +0.91; t69 walk main -0.02 ME +1.14; K7 t64 main +0.50.
  dn40 (gl38's second guard cfg): -0.63 / -0.97 vs gl39.
- Package build_gl40/deliver (15 files, MANIFEST.md5 with SLES/pak lines as gl39); rd40pc.py run on f2: EQUAL/EQUAL/none.
  report.txt and README.txt copied to attic/gl40.
- Cleanup: psp_pa..pf, x_int deleted (literal paths); kept u, psp, n40, pk_n40, pk_f40, emu_*, model/, lay/ (869 MB traces).
