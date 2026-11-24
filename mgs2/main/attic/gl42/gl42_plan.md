# gl42 plan (2026-11-24 ~01:50 UTC; the lead's brief of ~01:35 UTC): the RENDERER CHECKPOINT build

gl42 = gl41's final source + native renderer step 2 (the people native, attic/ge_native/step2/t40_step2.diff minus the gl41-overlap hunks).
Prepared while gl41 finishes and runs; NOT delivered (one build out at a time: gl41's PSP log is read first).  Built in
/home/claude/main/build_gl42 from /home/claude/t42; notes here (this plan, state.md).  gl41's folders (t41, build_gl41, attic/gl41) are
read only.  Same method and gates as gl41 (attic/gl41/gl41_plan.md).  nice 19, one heavy job at a time (gl41 has the CPU first).

## 1. Tree (/home/claude/t42)
- cp -a /home/claude/t41 (01:38 UTC); t41's later changes taken by st2/rebase42.sh (a file t42 has as t41 had it is copied; others merged
  by hand): 01:46 psp/main_psp.c (gl41's MAIN_KB 128).  Checked again before the PGO (attic/gl41/state.md).
- Step 2 merged by a 3-way merge per file (base st1g/t = t40 + step 1, which t41 was made from; mine st2/t; theirs t41):
  gl41's versions taken for the overlap (texcache.c TK_CAP0, gelive.c's guard, dig_c in xlme/xme/xl_psp, the ship's references copied at
  REC with NGE_NAT_ROOM / nat_rf / nge_walk_r, WORLD from Dx with the 16-bit digest, the me_pic stack MQ_THR_STK, the scratch block);
  step 2's people copy placed after gl41's ship copy (ge/xme.h XME_POFF = noff + R16(NGE_NAT_ROOM), XME_P6ROFF; ge/xlme.h XME_NATBYTES(b,
  ng, pn); the x86 mirror boot/xlive_chk.c the same).
- New: the reuse content hash probe (boot/nge_stub.h n6_same: MGS2_NGE_HPROBE, default 1): each n6_csum at a reuse check timed on the
  main CPU (sceKernelGetSystemTimeLow, two reads a check); 'reuse hash checks' and 'reuse hash us' in each 'nge people turn' line.

## 2. gen.o
P12e8b8 and P12e790 weakened with <name>_orig beside P1214a0 (m/scripts/psp.sh; tw42.sh for the twin), each after its reference check
(P12e8b8 1 call + 1 table word, P12e790 2 calls + 1 table word; an instrumented gen.o adds HI16/LO16).

## 3. Config and turns (tools/sched42.py, tools/mkcfg42.py from gl41's delivery cfg)
C2 on in MGS2_OPT2 and every OPT2 word (fffff9ff: C2, P1, P3, M5), F1 on, XP 3 (7 in the digest turns), NGE_KB 256 / KB2 384, P6LAG 100,
MGS2_NGE_HPROBE 1; gl41's one-time probes PROBE41 / PROBE41_CULL out.  74 turns from vb 24,000 (the run stops at 31,400 as gl40/gl41):
- deck t0-t49: blocks X = A 2 A R A N A 2 A R, Y = A 2 A R A N A 2 A 7 (X Y X Y Y): K-A 25, K-N1 5, K-N2 10, K-N2R 7, K7 3.
- rain t50-t73: A 2 A R A 2* A N A 2 A R A 7 A 2 A R | walk A 2 A R A 2*: rain wait K-N2 3 pairs (+ t55 digest), K-N2R 3, K-N1 1, K7 1;
  walk K-N2 1 pair (+ t73 digest), K-N2R 1.  Every lever turn sits between two K-A turns.  Digests t55 and t73 (both K-N2).
The people change the state hash from vb ~24,200: hashref.txt is gl42's own (twin sch arm with the delivery keys).

## 4. Build
1. Twin (tw42.sh) and arms (runs42a.sh): sch (-> hashref_gl42), off41 (gl41's keys: = gl41's tw2 sch arm, hashref_gl38), noff
   (NGE 0: hashref_gl38, content/drawn2 reference with C2), nchk (MGS2_NGE=2, check mode: 0 differences, content = noff), lw7 (nchk +
   the PSP's late walk + XP check), pgo (the training keys: hashref_gl42pgo).
2. No-PGO PSP build n42 (pkn42.sh): gates mesec, me_call, xme_rest, imports; emulator (em42.sh) with the loop apart.
3. PGO as gl41 (five instrumented builds, trainings on cfg/mgs2_gl42pgo.cfg: K-N2 / K-N2R in the counts), use build u, pgochk.
4. Order as gl41 (layout_sim41, guard + rain-weighted stage), relink, pads retuned.
5. Final: 3 LOADs, imports, M5; emulator 62/62 with the loop apart and digests = twin; heap gate >= 32 KB at the route's worst moment.

## 5. Package
build_gl42/deliver, gl40/gl41's 15 files: EBOOT.PBP, 8 overlays, mgs2.cfg, hashref.txt, pad.txt, rd42pc.py, README.txt, MANIFEST.md5.
