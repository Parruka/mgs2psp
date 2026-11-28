# gl45 state (pipeline cuts F / P / S on a copy of gl44's tree; agent build, NOT delivered)

## 0 2026-11-28 06:47 UTC: set-up
- Task (lead): gl45 = gl44's tree + cut 1 F (scratchpad-DMA runtime fast path, STRICT), cut 2 P (people path: old builder 0x12e8b8
  returns at once on unshown steps in native-people turns; cheaper reuse check; MGS2_NGE_HPROBE=0), cut 3 S (unshown-step skips of
  0x120860 / 0x13bf78 / 0x12d910, E5 standard), optional g_n45.  Each behind its own switch.  Package in build_gl45/deliver, no delivery.
- Read: the method skill, memory (status-now, gl45-plan, rules, working-facts, mainline), pipeline_anatomy_2026-11-27.md, attic/gl44/state.md,
  build_gl44's scripts.
- Nothing built yet.  Nothing running in the background.
- Next: cp -a /home/claude/t44 /home/claude/t45; build_gl45 with gl44's scripts renamed 45; read the code of the three cuts.

## 1 2026-11-28 07:05 UTC: tree, switches, first code, the twin built
- /home/claude/t45 = cp -a t44 (md5s of t44 in build_gl45/t44_final.md5).  build_gl45/m = links to t45 + scripts copied from build_gl44/m
  (env.sh B=build_gl45, pgo.sh / pgo2.sh cfg + hashref names gl45, psp.sh TITLE "MGS2 harness gl45"; w00a still gen44/wg and gen44/wpl:
  gl45 changes no stage header).
- SWITCHES (lead asked: a free MGS2_OPT2 bit each; runtime/host_time.h): bit 23 OPT2_F45 (cut 1 F), bit 24 OPT2_P45A (people builder
  skip on reduced calls), bit 25 OPT2_P45B (cheaper reuse hash), bit 26 OPT2_S45_SM (0x120860), bit 27 OPT2_S45_SEA (0x13bf78), bit 28
  OPT2_S45_PP (0x12d910); all out of OPT2_DEFAULT.  TRAP: gl44's words (ff9ff9ff ...) have bits 23-31 set = every cut ON in gl45;
  gl45's words clear bits 23-28: base e01ff9ff (deck), e03ff9ff (B), e07ff9ff (B+V).
- Code so far (all in t45): boot/f45_spr.h (new; F: f45_st32 + f45_spr_start; k_dmac_raise and the poll kick KEPT: natives read
  k_poll_at, so skipping the kick is not exact; so the anatomy's 0.26 of the poll is not taken), boot/s45_stub.h (new; rt_s45),
  boot/dg_lite.h + boot/gen_pre.h + boot_g0/gen_pre.h (FN_ENTER: GEN_S45), boot/np6_stub.h (P12e8b8's wrapper: np6_lite_skip),
  boot/nge_stub.h (n6_csum2, n6_cs2[], n6_same's third argument), boot/boot.c + boot_x86.c (rt_st32's hook, the includes, the
  "g45 turn" line), runtime/host_time.h.
- P part b DECISION: NOT the anatomy's "full hash for a quarter of the meshes a picture" (a mesh rewritten in place could be shown stale
  for up to 4 pictures: a picture risk the route cannot prove away); instead the SAME words every picture through a cheaper sum pair
  (about -45% of the hash's instructions [I]; its cache misses stay).
- Twin: build_gl45/tw45.sh (gen.o compiled from t45: cut 3's entry test is in the game code's three functions) -> tw/x86/boot, rc 0.
- RUNNING 07:04: `bash runs45a.sh off f pa pb` (pid 11338; out runs45a_1.out; logs tw/x86/out_<arm>.log.gz, g1_<arm>.out): gl44's table
  with the new bits clear (off: must give hashref_gl44) / F / PA / PB on everywhere.
- Next: arms sm sea pp; read the results; the read-hash twin (rh45.sh) for PA and the S leaves; PSP no-PGO build; disk clean-up before PGO.

## 2 2026-11-28 07:12 UTC: screening arms on the twin (cuts on from the boot, gl44's table), disk, the read-hash twin started
- Arms of runs45a.sh (build_gl45/tw/x86: out_<arm>.log.gz, seed_, act_, hashref_<arm>.txt; tools/cmp45.py A B) [C twin], route i to vb 37,800:
  - off: hashref = hashref_gl44 BYTE FOR BYTE; digests cfe341989b1d (14,203), content 5a77ede14160, seed sv 5b898b0ae2fa sd 1c5de4be2c5b
    st 6adcba9a61e1, act 5f14feead8e5 = gl44's s44.  (every new switch off = gl44: PASS on the twin.)
  - f (F from the boot): everything equal to off, hashref byte-equal.  STRICT holds on the twin.
  - pb: everything equal to off (reused 38,095, cooked 224, re-cooked 0 in both).
  - pa: ee differs at 133 of 685 lines (from 24,200), gs 102, iop 0, vfb 0; xlive 37,800 / digests / content 0 differ; seed and act logs
    identical.  Read hash owed.
  - sm (StoreMatrixObjs, from the boot): ee and gs differ at 503 lines (from vb 8,800), iop 0, vfb 0; pictures 0 differ (37,800 xlive,
    14,203 digest, 14,203 content lines); seed and act logs identical.  Read hash owed.
  - sea (0x13bf78, plain return at entry): ee and gs differ at 149 lines (from vb 23,000), iop 0, vfb 0; pictures 0 differ; seed and
    act logs identical.  Read hash owed (the anatomy's UNKNOWN).
  - pp (DG_WritePrimPacks 0x12d910) and all: the run STOPS at vb 3,599 ("missing function 15d9a0", a function outside the build): the
    caller 0x121d54 copies the leaf's 10 quadwords from scratchpad 0x70000290 into the packet chain whether or not the leaf wrote them,
    so a skipped leaf puts leftover bytes into a chain the game sends.  The stub's registers were right (r2 = 10, r6, r7 as the code).
    DROPPED: not exact as a leaf skip (the cure is the leaf's own work), and its model gain was 0.22 ms [I], at the drop line.
    Switch bit 28 stays defined but nothing sets it; rt_s45's k 2 and the FN_ENTER test for 0x12d910 to be taken out before the build.
- Disk: deleted after a look (old builds' instruction traces, not on the keep list): build_gl41/lay/{t0,t0n,t1,t1n,t2,t3}.it and
  build_gl42/lay/{t0n,t1,t1n}.it: 2.8 -> 5.3 GB free.
- RUNNING 07:11: rh45.sh > rh45.out (the read-hash twin build, build_gl45/rh/x86; ~2 min).
  Next: read-hash arms off / pa / sm / sea (runs45a.sh with TW=rh G2=rh EXTRA="MGS2_READ_HASH=1 MGS2_READ_SKIP=.. MGS2_READ_G0=.."),
  rh_cmp.py, then MGS2_READ_LOG on differing vblanks; take pp out; port the tools (sched45, mkcfg45, ...); the final table.

## 3 2026-11-28 07:46 UTC: read hash of the screening arms; pp out of the code; the turn table; the final-table arms
- Read-hash twin (rh45.sh -> build_gl45/rh/x86; EXTRA keys in rh/extra.txt: MGS2_READ_HASH=1, READ_SKIP of rl_skip.py, READ_G0 of
  S_w00a_P333380), cuts on FROM THE BOOT, gl44's table, vblanks 1-37,799 [C twin] (tools: /mnt/project-files/mgs2/tools/dev/rh_cmp.py,
  rl_cmp.py):
  - off vs pa: 0 vblanks differ.  PA: the game reads nothing different anywhere.  E5 PASS without a single differing load.
  - off vs sm: 14 vblanks differ (8844, 24283, 24697, and 11 in 37,498-37,572, after the PSP's end), read counts equal in all.
    MGS2_READ_LOG of ALL 14 (rl/x86, rl/rlcmp_off_sm.txt): 361 loads differ, address never: 252 + 104 + 4 by the stages' quadword copy
    routine (S_d00t_P323b58, S_w01a_P3a3f90, S_d01t_P329668: 50 lines of RD64B / WR64B, the same routine as w00a's 0x3b7c88) in a
    lane the writer left over (w, or y after a 4-byte x), 1 by 0x125f18 (16-byte load of a vector whose x, y, z it uses with the
    constant 1 for w).  The known leftover class.
  - off vs sea: 1,234 vblanks differ (106 in 23,256-23,9xx, 3 in t0-t6, every step of t50-t73), read counts equal in all.  READ_LOG of
    8 of them (23256, 23262, 24283, 24697, 29071, 29073, 30001, 31317; rl/rlcmp_off_sea.txt): 331 loads differ, address never: 326 by
    the quadword copies (w lanes), 5 (one a rain step) by S_w00a_P331160 at 0x331698: the VU0 clip flags it has just stored
    (cfc2 vi18 after its three VCLIPs at 0x331674-7c), of which it tests 0x3330 (the three newest judgements); the differing bit 22 is
    in the fourth, oldest group, left by whoever clipped before (the sea's builder or not).  The anatomy's UNKNOWN is settled for the
    route: no flag or LOD the sea's builder writes is read by game code outside the pipeline with another value; "flag 0x1b4818" of
    the memory note is that stack slot.  A second pass over more sea vblanks: with the final-table arms (below).
- pp taken out of the code (host_time.h: bit 28 unused, still excluded from the default; dg_lite.h, s45_stub.h, the g45 turn line).
  Dev key on the twin: MGS2_S45_POISON=1 (the registers the skipped function could write get a marker at each return).
- THE TURN TABLE (tools/sched45.py; `sched45.py table | count | pairs`): 
  deck  A2A2a2f2p22s2a2f2p27 2f2a2s2p2a2f2s2a2p27 2a2f2s2p27   (A K-A t0 t2; 7 K7 t19 t39 t49; 2 K-N2; a ALL 6; f 5; p 5; s 4)
  rain  cLcFcPc C cLcScFcLc c | cLcLc L   (c K-N2BV; C K-N2B anchor t57; L ALL t51 t59 t65 + walk t69 t71 + t73; F t53 t63; P t55; S t61;
        digests t67 (base) and t73 (ALL))
  Asked 21 deck lever turns, 20 fit (5 anchors, a lever between two base turns; f's t21 and p's t47 are change-of-scene turns); asked 9
  rain-wait lever turns, 7 fit (one anchor + one digest base turn; the digest base turn moved t55 -> t67: with t55 only 6 fit).
  Words: e01ff9ff / e03ff9ff (B) / e07ff9ff (B+V) + F 00800000, P 03000000, S 0c000000, ALL 0f800000.
- Tools ported: sched45.py, mkcfg45.py (from gl44's cfg 1cf3d930; MGS2_NGE_HPROBE=0; 50 keys), mkemu45.py, turnchk45.py (g45 lines).
  cfg/mgs2_gl45.cfg generated (to be regenerated at the end).  runs45b.sh: arms on gl45's table (never / sch / every / boot / pgo / pgo0).
- Final-table arms on the twin (runs45b_1.out; tw/x86/out_{never,sch,every}.log.gz) [C twin]: never / sch / every: digests 1adfa4a3caae
  (14,203), content 31c2b3ac9a6f, xlive 37,800 lines 0 differ, seed logs and act log IDENTICAL, iop 0 and vfb 0 of 685 lines differ;
  ee differs never vs sch at 124 lines (from 24,500), vs every 138.  never vs hashref_gl44: 133 lines differ from vb 24,600 (the deck's
  kinds differ from gl44's table: expected).
- SURPRISE [C twin counters]: on the steady deck (t11-t49) the pipeline has NO reduced calls (every step is shown): StoreMatrixObjs
  returns 0, sea 0, people-builder returns 0 there.  P-a and S act only in the rain (t50-t73: ~1,000 SM, 100 sea, 3,100 builder returns
  a turn) and a little in t0-t10 (SM ~290, sea 100).  So the deck's 's' turns measure nothing by construction and 'p' only part b.
- Poison arm 1 (every register that is caller-saved, also those the functions never write): pictures / seeds equal but vfb differs at
  117 lines: not a fair test (hand-written callers keep values in registers the callee does not write).  Poison arm 2 (only the
  functions' own write-back lists) RUNNING 07:45: `G2=g2p EXTRA=MGS2_S45_POISON=1 bash runs45b.sh evp=0f800000` > runs45b_p.out.
- RUNNING 07:45: `TW=rh G2=rh2 EXTRA=$(cat rh/extra.txt) bash runs45b.sh never sch every` > runs45b_rh.out (read hash, the final table).
  Next: compare (rh_cmp.py), read logs of a spread of vblanks; pkn45.sh (PSP no-PGO) + emulator gate; pgo arms; PGO chain.

## 4 2026-11-28 08:10 UTC: poison proof, no-PGO build and its emulator gate PASS, PGO trainings running
- Poison arm 2 [C twin] (evp: the table with ALL in every word + MGS2_S45_POISON=1: at each of 134,520 StoreMatrixObjs and 3,196 sea
  returns every register the function's translation writes back and does not restore gets a marker): hashref BYTE FOR BYTE = the every
  arm's (ee, iop, gs, vfb), xlive / digests / content 0 differ, seed and act logs identical.  So no caller uses a register the two
  functions leave: a plain return at entry needs no exit state.  (Poison 1, all caller-saved registers, was not a fair test.)
- Twin pgo / pgo0 arms -> cfg/hashref_gl45pgo.txt (eca1e99e), hashref_gl45pgo0.txt (97bce8ff); cfg/hashref_gl45.txt = the sch arm
  (49004765); cfg/mgs2_gl45.cfg 17bd37cf (50 keys; to be regenerated only if the header text changes), mgs2_gl45pgo.cfg,
  mgs2_gl45trc_t1.cfg (deck 31800), cfg_emu/mgs2_m37_rc0_g45off.cfg (the guard cfg, words e01ff9df), cfg_emu/mgs2_gl45_{xpchk,h1,h0}.cfg.
- n45 (pkn45.sh, no PGO; EBOOT e7c3644e) [C]: mesec_chk, me_call_chk, xme_rest_chk, xme_me_chk PASS; imports 134/134 = gl44's final, 0
  differences, stub sections equal.
- gates45a.sh on pk_n45 (emu_n1, PPSSPP 1.20.4, 581 s) [C emu]: 62/62 = hashref_gl45; digest turns t67 (50 frames) and t73 (11):
  content = the twin's sch in 61/61; XP check 720 compared, DIFFERING 0; turnchk45 PASS (74 words, 29 g45 lines; F stores 2,474,785,
  transfers 359,270, to the general path 3; P returned 14,881; S StoreMatrixObjs 7,499, sea 700); textures refused 0.
  HEAP: worst vb 536: 179 KB above the break + 35 KB free top = 215 KB (gate >= 32 KB PASS, no-PGO); lowest room + top 203 KB.
  rdbbox.py OK; rd/mkpc45.py + rd45pc.tpl.py on emu_n1: HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- Read hash on the FINAL TABLE (rh/x86 never / sch / every; rh/rhcmp_*.txt, rh/bad_*.pkl) [C twin]: read counts equal in all 37,799
  vblanks; hashes differ never vs sch in 1,542 vblanks: every step of every turn with P, S or ALL on (49-50 a turn, the deck too: on
  the deck each step has ONE reduced pipeline call, so 1 builder return and 1 StoreMatrixObjs return a step), 1 in the turn after, 0 in
  F-only turns; never vs every 5,928.  More than the screening showed (PA alone: 0 vblanks with gl44's table).  Read log of 27 of them
  (rl2/x86 never / sch) done 08:08; the compare is running (rl2/rlcmp_never_sch.txt).  NOT YET CLASSIFIED: the E5 verdict of P and S
  waits for it.
- Disk: deleted (after a look: check-build scratch of old builds) pgochk_u of build_gl36, build_gl36/pre_cull, gl39, gl40, gl41, gl42:
  5.0 -> 5.8 GB free.
- RUNNING since 07:56: pgo_a45.sh > pgo_a45.out (5 instrumented builds + trainings; g_game done 08:04) and chain45.sh > chain45.out
  (waits for it, then rt45a.sh, rt45b.sh).  Scripts ported: pkn45, em45, g4_45, gates45a (new), pgo_a45, chain45, rt45a, rt45b, rl45,
  fl45, rt_new45, fin45 (no rot test), model45 (new batches r1-r3 rain, d1-d4 deck, g).  Still to port: score45.py, pack45.sh, README.

## 5 2026-11-28 08:25 UTC: the read hash on the final table redone (block 4's 1,542 were a tool mistake), PGO training 1 of 5 done
- Block 4's "1,542 differing vblanks" was MY TOOL MISTAKE, not the cuts: rh/extra.txt (MGS2_READ_SKIP / MGS2_READ_G0, host offsets from
  rt_dgt_rd) had been computed for the first read-hash binary; rh45.sh's relink (after the pp leaf was removed) moved the code by 0x169,
  so the renderer's frame code (P123e80's load of the packet cursor 0x1e50a0) was hashed.  27 of 37 sampled differing loads were that.
  RULE: recompute rh/extra.txt with tools/dev/rl_skip.py after EVERY relink of rh/x86/boot (current: 10d286:..., G0 110321f:110533a).
  The stale run's files are kept as rh/*_staleskip.*.
- Read hash redone with the right ranges (runs45b_rh3.out; rh/rhcmp_never_{sch,every}.txt, rh/bad_*.pkl) [C twin]: read counts equal in
  all 37,799 vblanks.  never vs sch: 317 vblanks differ: 49 in each rain turn with S or ALL (t51, t59, t61, t65, t69, t71), 1 in the
  turn after each, 8 in t73, 9 in 37,506-37,572 (the walk after the table); 0 in every deck turn, 0 in the F turns, 0 in the P turn t55.
  never vs every: 1,139: every rain step t50-t73 (50 a turn), 1 each in t0, t2, t6, and 11 in 37,498-37,572.  These are the screening's
  two classes by their places (the sea builder's clip-flag stack slot in the rain steps; StoreMatrixObjs's leftover lanes late in the
  walk); to be PROVED load by load:
- RUNNING 08:24: `TW=rl2 G2=rl3 EXTRA="$(cat rl2/extra.txt)" bash runs45b.sh never sch every` > runs45b_rl3.out (MGS2_READ_LOG of 52
  vblanks in 32 ranges, the list in rl2/extra.txt).  Then rl_cmp.py never vs sch and never vs every with rl2/x86/boot; classify each load.
- PGO: train_game done 08:15 (rc 1 as gl44's: pgo.sh compares with hashref_gl45pgo, the ME_PIC=0 training equals hashref_gl45pgo0 in
  124/124 lines, 0 differ).  pgo_a45.sh goes on (rtb, pic, w00a, w01a), chain45.sh waits for it.
- RUNNING 08:10: `X=n sh model45.sh /home/claude/main/build_gl45/n45 r1 r2 r3` > model45_n.out (early cost model on the no-PGO build:
  to test tools/score45.py and to see the levers' size; the final figures come from the PGO build).
- pack45.sh fixed (rd45pc names).  rl45.sh, fl45.sh, rt_new45.sh, fin45.sh read through: as gl44's.
- Next: classify the read log; test score45.py on model/nr*; write README.txt while the chain runs; then fl45.sh s2, fin45.sh, model45.sh
  on psp, score45, pack45, drafts, mirror.

## 6 2026-11-28 08:28 UTC: THE TABLE CHANGED ONCE (t51 and t53 swapped): cfg and hashref are being remade
- The early model (no-PGO build, batch nr1) showed t50's window at 55 ms a step against 94 in t52: the rain BEGINS during t50, so t50 is
  no base neighbour and t51's pair has one neighbour only.  The first table had ALL in t51.  Changed tools/sched45.py: RAIN is now
  'cFcLcPc' 'C' 'cLcScFcLc' 'c' 'cLcLc' 'L': F in t51 (one neighbour, t52), ALL in t53, t59, t65 (three pairs with two neighbours), P t55,
  S t61, F t63.  NOBASE = (50,) in sched45.py; pairs(), score45.py and rd45pc.tpl.py leave t50 out as a neighbour.
- The PGO training's cfg keeps the FIRST table (sched45.RAIN_PGO; pgokeys md5 unchanged ef08781f; cfg/mgs2_gl45pgo.cfg, trc_t1 and the
  two pgo hashrefs are NOT remade: the trainings already ran with them).
- Remade: cfg/mgs2_gl45.cfg 943fa6f6 (first table's kept as cfg/mgs2_gl45_tab1.cfg 17bd37cf, hashref_gl45_tab1.txt 49004765),
  cfg_emu/mgs2_gl45_{xpchk,h1,h0}.cfg.  The never and every arms do not depend on the swap (never clears, every sets the bits in all words).
- RUNNING 08:26: `bash runs45b.sh sch` > runs45b_2.out (tw, the new table): its tw/x86/hashref_sch.txt becomes cfg/hashref_gl45.txt
  (must be copied BEFORE rt45a.sh's model or fin45.sh use it; pkn's emu_n1 gate was on the first table: the final package's gate redoes it).
- The read-log pass rl3 (never, every still running; its sch arm of the first table stopped by PID 21359).  When it ends: rerun sch in rl2
  with the new table and the same MGS2_READ_LOG list (`TW=rl2 G2=rl4 EXTRA="$(cat rl2/extra.txt)" bash runs45b.sh sch`), then compare
  never vs sch (rh lines and rl lines) and never vs every.
- The early model's nr1 (t50-t57) ran with the first table (t51 ALL, t53 F): read it with that in mind; nr2 started before the swap too.

## 7 2026-11-28 08:50 UTC: the E5 read-hash proof on the FINAL table (second form) is in; trainings 2 of 5 done
- cfg/hashref_gl45.txt = the twin's sch arm on the final table: md5 298f4eab (runs45b_2.out; differs from the first table's in 18 lines
  from vb 29,200; from hashref_gl44 in 134 lines from vb 24,500).  never vs sch (tools/cmp45.py): ee differs at 124 of 685 lines, the
  masked hash 'gs' at 109, iop 0; xlive 37,800 / digests 14,203 / content 14,203 lines 0 differ; seed log (sv, sd, st) and act log
  IDENTICAL.  NOTE: the hash line's 'vfb' field is the count of VFPU group fallbacks (MGS2_VFPU_STAT), NOT a frame-buffer hash (gl44's
  README called it that); it is equal in all arms; the picture proof is the digests and content lines.
- Read hash, final table (rl2/x86 out_{never,every,sch}_p1.log.gz: the hash lines and 49 logged vblanks; rh/rhcmp_never_sch.txt,
  rl2/rlvb_never_{sch,every}.txt by tools/rlvb45.py) [C twin]: read counts equal in all 37,799 vblanks in all three arms.
  never vs sch: 317 vblanks differ: 49 in each of t53, t59, t61, t65, t69, t71 (the rain turns with S), 1 in the turn after each, 8 in
  t73, 9 in 37,506-37,572 (the cycle wraps after t73: t135 = t61's kind, S, in the tanker stage); 0 in the deck, in F turns, in P's t55.
  never vs every: 1,139 (every rain step t50-t73, 1 each in t0, t2, t6, 11 in 37,498-37,572).
  Load by load in the 49 logged vblanks (7.65 M loads an arm), never vs every: 358 loads differ, the address never:
    - 32: one load a rain step, S_w00a_P331160's reload of its own stack slot 0x1b4818 = VU0's clip flags (cfc2 vi18 after its three
      VCLIPs), of which it tests & 0x3330 (the three newest judgements); the differing bit 22 belongs to the oldest judgement, the last
      VCLIP of the sea's builder (not run when skipped).  No other load of that slot.
    - 272 (vb 24,014, 24,283, 24,697; stage d01t): w lanes of scratchpad quadwords, read only by the stage's quadword copy
      (S_d01t_P329668); writers S_d01t_P2aca20 / P2a55c8 / P2b0ba8 (the same in both arms).
    - 54 (37,498-37,572; stage w01a): bytes 20-23 of 32-byte scratchpad records that S_w01a_P350100 builds with 16-bit stores at +0..+18
      and +24..+30 (it never writes +20..+23: they keep what the scratchpad held; StoreMatrixObjs uses the scratchpad as its work area),
      read only by the stage's quadword copy (S_w01a_P3a3f90).
  never vs sch: 46 loads differ in 16 of the 49 vblanks: 12 of the first class, 34 of the third.  Nothing else.
- RUNNING 08:46: `TW=rl2 G2=rl5 EXTRA="$(cat rl2/extra.txt)" bash runs45b.sh never sch` > runs45b_rl5.out: a second sample (t53's and
  t54's vblanks, more rain steps, 37,548-37,564); then tools/rlvb45.py on rl2/x86/out_{never,sch}.log.gz.
- Early model, no-PGO build (model/nr1, nr2; nr1 and nr2 ran on the first table) [C model, no PGO]: ALL t59 -9.55 ms a step main (game
  -1.90, runtime -7.41), t51 (one neighbour) -9.63; F -2.31, -3.19; P -1.86; S -4.39.  Base K-N2BV about 90.5 main.
- PGO: train_rtb rc 0 08:34, g_pic 08:42; then pic's training, w00a, w01a (about 09:15), chain45.sh -> rt45a.sh (about 40 min) ->
  rt45b.sh (about 65 min in gl44).  Then by hand: fl45.sh s2, fin45.sh, model45.sh, score45, README, pack45, drafts, mirror.

## 8 2026-11-28 08:58 UTC: read-log second sample done (E5 for P and S: PASS on the twin); drafts started
- Second read-log sample (rl2/x86/out_{never,sch}_p2.log.gz, rl2/rlvb_never_sch_p2.txt; list rl2/extra_p2.txt): 37 vblanks, 4.55 M loads
  an arm, 66 differing loads in 21 vblanks: 16 the clip-flag slot (one a rain step, t53, t54's one, t59, t61, t65, t69, t71, t73), 50 the
  unwritten bytes 20-23 of S_w01a_P350100's records (37,548-37,564).  Both passes, never vs sch: 86 vblanks logged (12.2 M loads an
  arm), 37 of the 317 differing vblanks, 112 differing loads, 28 of class 1 and 84 of class 3; never vs every (pass 1 only): 41 of
  1,139, 358 loads.  E5 VERDICT [C twin]: P and S pass (counts equal everywhere; every differing value a leftover: three classes).
  The clip-flag code checked in gen44/wpl/gen_s_w00a.h: 0x331674-7c three VCLIPs, 0x331690 cfc2 vi18 -> r8, stored to sp+84, reloaded,
  `& 0x3330`; r8 not read again before 0x3318e8 (the same pattern with sp+88).
- Note for the lead: gl45's table fills the people mesh arena in the walk: at vb 31,083-31,087 (t70) 3 people meshes found no room
  (nge people totals: no room 3; gl44: 0) and went today's path for those frames.  It is the TABLE (people native in every turn), not
  the cuts: the never arm has the same 3.  Content of the digest turns still equals the twin's.
- t45/boot/s45_stub.h: one comment reworded in place (no line added: "the sound state's hash" instead of "sound and frame-buffer
  hashes"), done while no compile ran.
- Drafts with @@tokens@@ for the final figures: build_gl45/README.tpl.txt, docs/deviations_draft.tpl.md; tools/mktable45.py (README's
  table), tools/rlvb45.py (per-vblank read-log compare).
- PGO: train_pic rc 0 08:54; w00a and w01a to go.  Early model nr3 done 08:51 (model45_n.out).

## 9 2026-11-28 09:33 UTC: CONTAINER RESTART at 09:30 (every background job gone); the chain resumed from pgochk
- Before the restart, all finished and on disk: PGO trainings (pgo_a45.out: "pgo_a45.sh: done 09:22:24"; train_rtb rc 0, train_pic rc 0;
  game, w00a, w01a against hashref_gl45pgo0: 0 differ in 124 / 63 / 124 lines; their rc 1 is pgo.sh's compare with hashref_gl45pgo, as
  gl44's); gcda 20 files; chain45.out "ok=1 09:22:31"; the use build u (u.out "psp.sh: done 267 s", rt45a.out "u rc 0 09:26:58").
  The restart cut rt45a.sh inside pgochk (pgochk_u partial).
- No-PGO gate ON THE FINAL TABLE finished 09:16 (pk_n45b = n45 + the final cfg 943fa6f6 + hashref 298f4eab; gates45a_n2.out, emu_n2)
  [C emu]: 62/62 hashes = hashref_gl45; content of t67 (50 frames) and t73 (11) = the twin's; XP check 720 compared, DIFFERING 0;
  turnchk45 PASS (74 words, 29 g45 lines; F stores 2,476,798, transfers 359,581, to the general path 3; P returned 14,759; S
  StoreMatrixObjs 7,410, sea 700); heap vb 536: 179 + 35 = 215 KB; textures refused 0; imports 134 / 134, 0 differences; rdbbox OK;
  reader rd/rd45pc_n2.py: HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none, GL45 LEVERS section prints.
- RUNNING 09:32: `sh chain45r.sh >> chain45.out` = rt45a2.sh (rt45a.sh from pgochk on: pgochk, traces t0-t3, model mu; appends to
  rt45a.out), its gates, then rt45b.sh > rt45b.out.  If it is cut again: look at rt45a.out for the last "rc" line; a finished trace
  leaves lay/t<n>.it (rt45a2.sh refuses to start when one exists: move finished ones aside and edit a copy to skip them).
- Coordinator's addition (09:30): put the people-mesh-arena line (3 meshes with no room at vb 31,083-31,087, t70) in README's "what the
  run should show"; say in the hand-back whether enlarging the arena fits the heap gate; no change without a proof on the twin.
- Drafts so far: build_gl45/README.tpl.txt, docs/deviations_draft.tpl.md, docs/harness_draft.tpl.md (tokens @@..@@ for final figures);
  tools/mirror45.py written (main/tools; dry run: 238 files, 79 to copy).

## 10 2026-11-28 09:38 UTC: the mesh arena question (coordinator's addition) measured on the twin; NOT changed
- Where the 3 "no room" come from: boot/nge_stub.h's regions (MGS2_NGE_KB=256 until vb 24,000, MGS2_NGE_KB2=384 after: the cooked
  meshes of ship and people) are carved from the two FRAME MEMORIES' arenas (1,696 KB each; 1,236 KB left for a frame's data after the
  carve and the people's 76 KB block), NOT from newlib's heap: the heap gate (vb 536) does not move with their size.  gl42's note at
  NGE_KB2_P6 already said: 384 KB gives 3 people no-room on the whole route with the people native, 416 KB none but leaves the frame
  arena 1 KB short of spare at vb 36,755-36,907.
- Twin run [C twin, gl45's table, cuts never on, MGS2_NGE_KB2=416; `EXTRA=MGS2_NGE_KB2=416 G2=g3 bash runs45b.sh kb416=0`,
  runs45b_kb416.out, tw/x86/out_kb416.log.gz]: people no room 3 -> 0, re-cooked after eviction 32 -> 0, evicted 829 -> 689; hash file
  = the never arm's byte for byte, seed and act logs identical, xlive lines equal; content lines equal in every frame up to vb 31,400
  (the PSP run's end) BUT 56 frames differ at vb 36,755-36,907 (the frame arena runs out there, as gl42's note says).  So 416 KB is
  exact for the measurement run to 31,400 and NOT for the whole route.  Frame arena data in the PSP run [C emu, emu_n2]: at most 414 KB
  a turn (t3) of the 1,236 KB.  gl45 keeps 384 KB (no change without a full proof); for the hand-back.
- Chain: rt45a2.sh in pgochk since 09:32.

## 11 2026-11-28 09:50 UTC: pgochk PASS; traces running; the rest of the chain queued
- pgochk rc 0 09:42 (pgochk_u.out: every object "0 profile lines" = no profile dropped; objects equal to the build's).
- RUNNING: chain45r.sh (traces t0 since 09:42, then t1, t2, t3, model mu, its gates, rt45b.sh: about 11:15 by gl44's times), and
  WAITING behind it `sh chain45c.sh > chain45c.out` (new): gate rt45b rc 0 + s2's 8 "bind ... under" lines -> fl45.sh s2 > fl45.out ->
  gate (10 of 10 files byte-equal, ME checks, imports 0 differences) -> fin45.sh > fin45.out (pk_f45; emulator f2, f3, fh1, fh0) with
  two model streams beside it (model45.sh psp: r1 r2 r3 g > model45_fa.out; d1 d2 d3 d4 > model45_fb.out).
  If cut: rerun what the out files show unfinished (fl45.sh refuses when psp / psp_t / psp_b exist; fin45.sh when pk_f45 exists;
  model45.sh a batch when ms/<tag> exists: delete the unfinished ones by literal path first).
- Then by hand: read fl45.out (pads, rt_new45: real addresses = the search's, guard windows under caps), fin45.out (62/62 x 3 and h0 =
  hashref_gl38, content, XP check 0, turnchk, heap gate figure), tools/score45.py > score45_final.txt, README.txt from README.tpl.txt
  (@@TABLE@@ by tools/mktable45.py, @@MEMORY@@, @@SHOW@@, @@MODEL@@, @@HASHREF8@@ 298f4eab, @@EMUHASH@@), pack45.sh, docs/*.tpl.md ->
  attic/gl45/deviations_draft.md, harness_draft.md, tools/mirror45.py, the hand-back.

## 12 2026-11-28 09:55 UTC: strict arms on gl45's own table; t0 traced
- [C twin] `G2=g4 bash runs45b.sh fall=00800000 pball=02000000` (runs45b_fpb.out; the table with F, or P's part b, in EVERY word): both
  hash files = the never arm's BYTE FOR BYTE (tw/x86/hashref_{fall,pball}.txt), xlive / digest / content 0 differ, seed and act logs
  identical.  So F's STRICT standard holds on gl45's own table too (the screening had it on gl44's table, F on from the boot).
- RUNNING 09:53: `TW=rh G2=rh5 EXTRA="$(cat rh/extra.txt)" bash runs45b.sh pall=03000000` > runs45b_rh5.out (P, a + b, in every word,
  read-hash build): compare its rh lines with rh/x86/out_never.log.gz (expected 0 differing vblanks).
- Chain: t0 rc 0 09:49; t1 running.

## 13 2026-11-28 10:12 UTC: P's read hash on gl45's own table; waiting for the traces
- [C twin] P (a + b) in EVERY word of gl45's table, read-hash build (runs45b_rh5.out, rh/rhcmp_never_pall.txt): 0 of 37,799 vblanks
  differ from the never arm (counts and values); 62,026 builder calls returned over the route (145,736 reduced calls run: arm not 3).
- Note: the copy plan's "M5 walks" a turn fall from about 40,000-43,000 to about 34,500 in the S and ALL rain turns (not in F or P
  turns) [C emu, emu_n2]: part of S's runtime gain; plans (600 a turn) and pictures unchanged.
- Chain: t0 09:49, t1 about 10:02; t2, t3, mu to go, then rt45b.sh; chain45c.sh waits (fl45, fin45, model).

## 14 2026-11-28 10:33 UTC: traces and their gates done; the order search runs
- rt45a.out: t0 rc 0 09:49, t1 10:00, t2 10:08, t3 10:14, mu 10:21; chain45.out "rt45a gates ok 10:21:54" (traces t0, t1, t2 against
  hashref_gl45pgo and t3, mu against hashref_gl38: 0 differ each).  rt45b.sh started 10:22 (merge done: lay/t0n.it, t1n.it; caps in
  rt45b.out); order_s1 then order_s2 (gl44: 35 + 29 min).  chain45c.sh waits behind it (block 11).
