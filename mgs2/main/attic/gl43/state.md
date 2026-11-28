# gl43 BUILD round: state (newest block last)

## 2026-11-24 14:06 UTC, block B0 (started)
- Brief (lead, 2026-11-24 ~14:0x): gl43 = gl42 + Ffm's rain option "B" on the game side (keep rain.c's drop sheets; splashes, ripples,
  far sheets may go; the guards' random numbers unchanged).  Base tree /home/claude/t42 (read only), build dir /home/claude/main/build_gl43,
  notes here.  Do not deliver; do not edit DEVIATIONS.md / NOTES.md / roadmap.md / psp_harness.md / memory.
- Read: rain_options_2026-11-21.md s4-s6, hd_study_2026-11-23.md s1.3-1.4, rain_study.md, gl42 plan/state, gl42_psp_read.md.

## 2026-11-24 14:31 UTC, block B1 (PAUSED by the coordinator: owner out of tokens; nothing running, no shared file half-written)
- Correction to B0: the near drop sheets are d_splash_camera's (+0x40, ge/fxthin.c kind 0), not rain.c's; rain.c draws the streaks
  (already hidden by H3R).  So d_splash_camera, rain.c, line_sph, body_sph, charaspread, rain_gas_pers stay as they are (not in B).
- FORM CHOSEN: the exact form (rain_options s5.3 / hd_study s1.3), not DICE+NP: DICE alone moves the guards' numbers on the aft deck
  (rain_options s5.2).  Exact = each shadowed call advances the seed 0x1fc954 by the translation's draw count and adds its polls
  (x 64) only when NAT_NOW() + P*64 < k_poll_at (else the translation runs), so the seed chain and interrupt positions stay the PS2's
  and B can switch per turn.  [design; not yet proven]
- B content (OPT2 bit 21 = 2097152, "OPT2_B43"; OPT2_DEFAULT must exclude it; base word ffdff9ff, B turns fffff9ff):
  floor update 0x318418 (word0 -= 1; translate if word4 == 0), floor spawn 0x317e88 (1 draw: life 8+(r&7) at slot +0/+4, then 96 more
  draws by a jump; replicate its 3 qword copies to *(0x1f85c8/cc/d0)), fall update 0x319600 (float life -1.0, clamp 0), fall dead
  0x319078 (polls only + R3 -1, R5 a1<<10, R6 (a2<<2)+RD32(actor+76)); fall spawn 0x3190c8 stays translated.  Prims hidden in B turns
  (B43_HWR keeps +164 | 0x3000) at floor 0x318808, fall 0x319aec, ripple 0x3321f4/0x332214/0x332494/0x3324a4/0x3324b4.  Draw side:
  MGS2_BOUND_CYCLE 4 (BD_RAIN) + MGS2_GL_RAIN_NEAR=1 in B turns.  Slot = actor+176+a1*1040; prim = RD32(actor+80) (floor) / +76 (fall);
  prim +184/+188 position double buffer, +192/+196 vertex buffers (idx = the act's R6).
- Clock: floor update polls = 15 (loop) + 5 qword copies of 64 qw (3 in: dst 0x70000000/0x70002000/0x70001000; 2 out: to
  RD32(prim+184+idx*4)+a1*1024 and RD32(prim+192+idx*4)+a1*1024) computed with native_w00a.c nat_qplan (include native_b43.c after
  native_w00a.c); P125f18 with r6=1 has no poll.  Also call P125a78(0x1df990) in the shadow (matrix word 0x1b99f0 left as the
  translation leaves it).  Check mode (x86 B43_CHECK) must confirm every P per call.
- Done [C file]: /home/claude/t43 = cp -a t42 (t42 md5 snapshot: scratchpad g43/t42_at_copy.md5, 118 files); t43/boot/native_b43.h
  written (macros B43_ON, B43_HWR, B43_WRAP, b43_pre/b43_post; enum FLSP/FLUP/FAUP/FADE).  Nothing else changed in t43 yet.
- Not done: native_b43.c; tools/b43_apply.py (gen/wg + gen/wpl -> /home/claude/main/gen43/{wg,wpl}, md5-checked, wrappers + B43_HWR +
  includes); host_time.h OPT2_B43; boot_x86.c seed log at rt_half_skip (called before every act while g_half; MGS2_HALF_CYCLE b in every
  turn) + counters; boot.c counters; xl_psp.c "b43" turn line; build_gl43 scripts (tw43.sh = build_gl42/tw42.sh + gen_s_w00a.o compiled
  "gcc -O1 -g0 -w -I/home/claude/t43/runtime -I/home/claude/t43/boot -fwrapv -fno-strict-aliasing -DGEN_MM_FAST -DGUEST_SPR0
  -DSTAGE_GEN=\"/home/claude/main/gen43/wpl/gen_s_w00a.h\" -c /home/claude/t43/boot/gen_stage.c", other units from build_gl40/x19/x86);
  sched43.py / mkcfg43.py / runs43a.sh; all proofs, PSP builds, PGO, package, notes.
- Proof plan: arm off (B bit never set) = gl42's sch twin (hashref_gl42) byte for byte; arm chk (B43_CHECK, translate + compare
  0 differences); arm bon (B from vb 29,000 every turn) and arm sch43 (B in turns): seed log per act (vb, act, seed at entry, k_time)
  equal to arm off in every line incl. the guards; IOP, route (stage loads), vfb and ee after the deck; EE dumps (MGS2_EE_DUMP) classified;
  read hash (GEN_DG_TRACE twin, g_rh_off set around the floor/fall acts in both arms) if time allows.
- Draft turn table t50-t73: A B A B A 2* A 2 C 2 A B A 7 D 7 A B | walk A B A B A 2* (B = K-B today's path + B + bound 4;
  C = K-N2B; D = K7B; * digests t55, t73); deck t0-t49 as gl42.
- RESUME: read this block, then write t43/boot/native_b43.c (scratchpad bodies f_*.c can be re-extracted with
  python3 -I <scratchpad>/g43/xfn.py /home/claude/main/gen/wpl/gen_s_w00a.h <addr>), then the apply script.  Disk 6.7 GB free.

## 2026-11-27 13:37 UTC, block B2 (resumed by the coordinator; owner back)
- Container kept everything: t43 (= t42 + boot/native_b43.h only), main/gen/wg w00a eac6413f, wpl 923514f4, x19 units, toolchain,
  emulator, game files; df 6.7 GB free.  Continuing from B1's RESUME line.

## 2026-11-27 14:25 UTC, block B3 (twin proof so far)
- Built [C file]: t43/boot/native_b43.{h,c}; tools/b43_apply.py (gen43/wg w00a 4fa185cf, gen43/wpl 8713add5); host_time.h OPT2_B43
  (OPT2_DEFAULT excludes it); boot.c/boot_x86.c g_b43_n + "b43 turn" lines; boot_x86.c b43_seedlog (MGS2_B43_SEEDLOG); tw43.sh,
  sched43.py, mkcfg43.py (cfg/mgs2_gl43.cfg 49 keys, MGS2_GL_RAIN_NEAR=1 new), runs43a.sh, pkn43.sh, em43.sh, m/scripts.
- [C twin] off arm (B never on) = hashref_gl42 byte for byte (685/685), digests c957ed16cf8d, content 2b4664d2e396 = gl42's.
- [C twin] chk arm: every B-turn call translated and compared with the shadow: floor spawn 964, floor update 11,036, fall update 9,560,
  fall dead 45,704 compared, 0 different (time, seed, lives, copies); 0 calls fell back (poll due / guard).
- [C twin] sch arm: shadowed 964 / 11,036 / 9,560 / 45,704, 0 fell back; bon arm (B every turn from t50): 2,784 / 31,876 / 22,362 / -.
- [C twin] seed log identical in off, chk, sch, bon (and offd/schd): sv b27073b998d1 (12,699 lines: every act's seeds 0x1fc954,
  0x1fbd74, 0x1ff554 + game time at entry, vb 28,900-37,800), sd 99a5a9c90b85 (65,070 seed changes incl. the guards: watcher.c 187,
  attacker.c 221), st f2e204bbc21e.  => the guards' draws and the route are the reference's.
- [C twin] iop, vfb equal at all 684 hash points (sch, bon); ee/gs differ from vb 29,100 (B turns' RAM) -> hashref_gl43 (58cdec28).
- [C twin] pictures: content and digest lines after the deck (vb > 31,400: 3,072 each) equal off's; in the deck, content differs only
  in B digest frames (sch: t73's 10; bon: t55's 50 + t73's 10); t55 in sch (no B, 2 turns after t53 B) has equal content but 50
  differing list digests (same word / PRIM counts: leftover vertex bytes of not-shown splash slots).
- EE dumps off vs sch (vb 29,199 ... 36,699, 6 pairs) [C twin]: differing bytes in the 5 rain actors (floor 0x15943b0, fall 0x153d090,
  0x1565f10, 0x15755f0, 0x1584cd0) and their prim buffers (+184..+196), scratchpad work area, and "other": 0x0-0xffff (1,995 B at
  29,199), 0x1b40000-0x1b5ffff and 0x1d00000-0x1d5ffff (packet / heap areas), 0x1b8bf0 and 0x1b8ffc (2 B each); after the deck
  (32,199+) ~190-200 KB: the old actors' blocks + 0x1b4-0x1b5 / 0x1d3-0x1d5 + the 4 bytes at 0x1b8bf0/ffc.  Not classifiable by
  hand -> read-hash proof (rh43.sh: GEN_DG_TRACE twin of every unit, build_gl43/rh/x86) decides whether any game load sees them.

## 2026-11-27 14:44 UTC, block B4 (read hash; no-PGO build n43 and its emulator gate PASS; PGO started)
- READ-HASH twin (rh43.sh: every unit -DGEN_DG_TRACE, build_gl43/rh/x86; MGS2_READ_HASH=1, READ_SKIP = rl_skip's frame code, READ_G0 =
  S_w00a_P333380; the floor/fall acts' own loads out of the hash in both arms, native_b43.h B43_ACT_WRAP) [C twin]: off vs sch, every
  vblank 1-37,799: 37,795 equal, 4 differ (vb 30,928, 30,929, 30,932, 30,933, t69 K-B walk; same load counts).  So the game reads no
  differing value anywhere but there; after the deck (and in every other B turn) every load the game makes outside the draw pipeline
  sees the reference's bytes.  The 4: MGS2_READ_LOG runs going (rl_runs_off/sch.out).  Seed log of the trace build = the plain twin's.
  EE dumps deleted after use (dumps/, 12 files).
- n43 (pkn43.sh, fresh; the trial n43/pk_n43x deleted after a look) [C]: mesec_chk, me_call_chk, xme_rest_chk, xme_me_chk PASS; 3 LOAD
  segments; imports 133/133 = gl42's final, 0 differences, stub sections equal; EBOOT 4f92e835.
- emu_n1 (pk_n43 = delivery cfg, PPSSPP 1.20.4, loop apart, 551 s) [C emu]: 62/62 = hashref_gl43; digests 61 (t55 K-N2, t73 K-N2B):
  content = the twin's sch in 61/61 (t73's 11 B frames 0 differ); 23 b43 turn lines = the twin's (t73's comes after the run's end);
  0 'no thread'; XP check 720 DIFFERING 0; textures refused 0.  HEAP: route's worst vb 536, 203 KB above the break + 31 KB free top =
  234 KB (gate >= 32 KB PASS, no-PGO); picture heap lowest room + top 223 KB; malloc at the end 208 KB in 1 block; me_pic deepest 5,284 B.
- Reader rd/rd43pc.tpl.py + mkpc43.py (rd42pc + B kinds, B pairs, 'OPTION B by turn', schedule check of the b43 lines) on emu_n1:
  HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- Twin pgo arm [C twin]: = hashref_gl43 to vb 31,700, own lines from 31,800 -> cfg/hashref_gl43pgo.txt (191e1b82).
- Written: pgo_a43.sh, rt43a.sh, rt43b.sh (gl42's final order as seed), rl43.sh, rt_new43.sh, fin43.sh, g4_43.sh, pack43.sh, model43.sh,
  tools/{k0cmp43,mkemu43,score43}.py, cfg_emu/mgs2_gl43_{xpchk,h1,h0}.cfg (h0: NGE 0 and B off), cfg/mgs2_gl43trc_t1.cfg.
- Running 14:43: pgo_a43.sh (5 instrumented builds + trainings, one after another).

## 2026-11-27 16:07 UTC, block B5 (the 4 read-hash vblanks explained; PGO done; use build and traces running)
- The 4 vblanks [C twin, MGS2_READ_LOG both arms, 981,692 loads each]: 16 loads differ, all an 8-byte unaligned load (GLDU) of scratchpad
  0x70003f88 whose low word was just written (S_w00a_P2e59b0) and whose high word 0x70003f8c is a leftover: 0x3dbf2dad-like floats in
  off (left by the translated splash code), 0 in sch (B: that code did not run).  Every other load equal; no vblank after differs.
- Twin arm pgo0 (pgo keys, MGS2_NGE_CYCLE all 0) -> cfg/hashref_gl43pgo0.txt: the reference of the ME_PIC=0 trainings (gl42 used
  hashref_gl38; gl43's B turns change the state from 29,100).  Its seed log = off's again (b27073b998d1 / 99a5a9c90b85).
- pgo_a43.sh 14:43-16:05 [C emu]: 5 trainings, every one exact: rtb, pic 124/124 = hashref_gl43pgo; game 124/124, w00a 63/63, w01a
  124/124 = hashref_gl43pgo0.  21 files in gcda/ (gcda/md5.txt).  Logs gzipped in old/pgo/; instrumented folders, pg/ and the game folders
  (ms/game, pic, rtb, w00a, w01a, em_n1) deleted after a look.  Disk 6.1 GB.
- deviations_B_draft.md written here.  Running 16:05: rt43a.sh (u, pgochk, traces t0-t3, model mu), then rt43b.sh.

## 2026-11-27 17:19 UTC, block B6 (use build u, traces; guard cfg made B-off; order search running)
- u (PGO=use, gcda/) built 16:10; pgochk rc 0, no lost profile lines.  Traces [C emu]: t0 59/59, t1 117/117, t2 62/62 = hashref_gl43pgo.
- FOUND: gl38's guard cfg (build_gl38/cfg_emu/mgs2_m37_rc0.cfg) has MGS2_OPT2 / _CYCLE fffff9df, i.e. bit 21 set, which gl43 reads as
  OPT2_B43: the first t3 and model mu ran B ON (9 of 55 hashes differed from hashref_gl38 from vb 23,500; logs in old/guard_b_on).
  cfg_emu/mgs2_m37_rc0_b43off.cfg (the two words ffdff9df, marked 'gl43'); rt43a_t3.sh: t3 and mu again: 55/55 = hashref_gl38 each.
  rt43a.sh / model43.sh now use it.  NOTE for the README / lead: any older cfg whose OPT2 words carry bit 21 turns B on in gl43.
- Running 17:17: rt43b.sh (merge, caps, seed = gl42's final order s2, s1, s2).  Disk 4.9 GB before the merge.

## 2026-11-27 18:35 UTC, block B7 (order s2 chosen; final link; static gates)
- rt43b.sh 17:17-18:32 [C sim]: seed = gl42's final order s2 (2,095 kept, 0 gone, 28 newly hot appended: b43_pre, the _t wrappers, ...).
  s1: every guard window under its cap (-5.6 to -14.1 %); s2 (rain-weighted): rain/walk windows 5..14 -1.5 to -4.6 % against s1, guard
  windows -6.6 to -17.4 % of their caps (19/20 +4 % against s1, still under).  CHOSEN: s2 (as gl41/gl42).
- Pads (padfit42 against gl42's final ELF): RO_PAD 0x15d9, ME_PRE 0x1180, ME_PAD 0x1580, XL_PAD 0x210, END_PAD 0x19c0: .rodata, .bss head,
  every .me_bss group at gl42's offsets mod 0x2000 (.data +0xb0), image end 0b24f900 = gl42's.
- Final build psp (U=u, ORD=ord/s2; EBOOT 318caa49): linked twice byte-equal in all 10 files; rt_new43 [C sim]: 0 hot functions missing or
  resized, I-misses 5,679,670 / 2,281,161 = the search's.  3 LOADs.  G4 [C]: 133 / 133 sce names = gl42's final, stub sections equal;
  xme_me / xme_rest / mesec / me_call PASS.  Text +168 B against gl42's final.  Trial links psp_t, psp_b deleted after a look.
- Running 18:33: fin43.sh psp f (f2, f3, fh1, fh0 one at a time), then model43.sh psp a g.

## 2026-11-27 20:02 UTC, block B8 (FINAL: gl43 packaged in build_gl43/deliver; NOT delivered)
- fin43.sh psp f [C emu]: f2 / f3 / fh1 / fh0 62/62 each (fh0 = hashref_gl38); f2, f3 content = twin sch 61/61; XP check f3 114,608 / 0;
  textures refused 0; 0 'no thread'; heap worst vb 536 103 + 34 = 137 KB (PASS); ME heap lowest 126 KB; malloc end 96 KB in 2.
  fh1 vs fh0: B/K7B/K7 frames equal in list and content; K-A 38 / K-N2 15 content frames differ, all in the first 6-16 frames after a B
  turn (+1 at vb 31,062): the B turn's splashes still hidden (README says so).
- model43.sh psp a g [C model, all runs' hashes 0 differ]: score43_final.txt here.  K-B - K-A mean main -6.57 / ME -10.42 (7 pairs);
  K-N2B - K-N2 -5.35 / -8.90 (5); K7B - K7 -4.99 (2); gl43 - gl42 K-A walk -0.42 / -0.57; guard I-misses PASS.
- deliver/ (15 files): EBOOT 318caa49, mgs2.cfg 533aa271, hashref.txt 58cdec28, pad.txt 7bc72ace, rd43pc.py 24875760 (on f2: EQUAL x3,
  SCHEDULE none), README.txt 32ff6681, MANIFEST.md5 2bf39121.  gl43_results.md, README.txt copy, score43_final.txt here.
- Cleanup (literal paths, after a look): n43, pk_n43, psp_t, psp_b, dumps, the PGO folders, the game folders.  Kept: lay/ (1.2 GB, the
  traces behind the order), u, psp, pk_f43 (= deliver), emu_*, model/, tw/, rh/.  Disk 4.4 GB (was 6.7 at the resume).
