# gl44 state (block A: harness only, H1 unfinished logs kept, H2 1-s black box; block B: the Vita rain list, after A's gates)

## A0 2026-11-27 20:10 UTC: set-up
- Task (lead via coordinator): H1 rotate log/battery/crash/steps/bbox to *_prev1/_prev2 at start; H2 MGS2_BBOX=1 black box bbox.txt
  (3,600 x 128-byte text records, preallocated, open/seek/write/close each second, a thread above the game's priority); rdbbox.py;
  README text for Ffm at a freeze.  No-PGO build only; emulator loop apart BBOX=1 and =0: 62/62 = hashref_gl43, content 61/61.
  Then block B (gl44_vita design) on the same tree, stop before PGO.
- Not touched: build_gl43/deliver, psp, u, gcda (gl43 runs on the PSP since 20:03:46).
- Housekeeping (disk 4.4 -> 5.6 GB): deleted after a look build_gl43/lay/{t0n.it,t1.it,t1n.it,battery_t0..t3.txt} and
  build_gl43/pgochk_u.  Why: gl44 adds code, so its u and its traces must be made again for its own order search; the .it traces
  only fit gl43's u.  Kept in lay: order_s*.txt, funcs, ls_*.out, wf, bases.
- Tree /home/claude/t44 = cp -a /home/claude/t43 (120 files, md5s in build_gl44/t44_base.md5); build_gl44/m links to t44
  (env.sh B=build_gl44, psp.sh TITLE "MGS2 harness gl44"; w00a still gen43/wg's B header).

## A1 2026-11-27 20:45 UTC: block A written and built (no-PGO n44, pk_n44); emulator gates running
- New unit t44/psp/bbox.c + bbox.h; small hooks in main_psp.c, xl_psp.c, gl1_glue.c, rt/iop_host.h (the PSP's copy only; the twin
  compiles runtime/iop_host.h, untouched).  Patch build_gl44/blockA.diff (t43 -> t44), psp.sh diff blockA_psp_sh.diff, tree md5s
  t44_blockA.md5: block B goes on top of these, so A carries over as one patch.
- H1 (always, no key): bb_rotate(g_dir) in main() right after g_dir is known, before log.txt's O_TRUNC (and before battery.txt's
  and crash.txt's truncations later in __wrap_mgs2_main / me_exc_install).  For log.txt, battery.txt, crash.txt, steps.csv,
  bbox.txt: remove *_prev2, *_prev1 -> *_prev2, file -> *_prev1 (log_prev1.txt, steps_prev1.csv, ...).  sceIoRename with full paths;
  if a rename fails: the bare name, then copy + remove (8 KB buffer on the main stack).  log.txt's tail (64 KB) is read for a line
  "end: " first; the new log's 2 "gl44 H1" lines (after title/folder) say what moved and whether the run before had its end line.
- H2 (MGS2_BBOX=1; default off; gl44 cfg build_gl44/cfg/mgs2_gl44.cfg = gl43's + MGS2_BBOX=1, marked gl44): bb_start after the
  cfg is read: bbox.txt 3,600 x 128 B = 460,800 B written blank (4 KB chunks), record 0 by the main thread.  Thread "bbox", prio
  0x10, 4 KB stack, created at the FIRST WINDOW (vb 100) by host_vb_begin: why: the picture's set-up packs the system's memory
  outside newlib's heap to 24 KB largest / 48 KB total (me_pic_plan 304 KB, nat groups 2 x 108 KB, ...; emu_f2's log), so a stack
  taken before it could move or refuse those blocks (and push them into newlib's heap); after vb 100 it comes from what is left.
  Why a new thread and not the battery thread: its 1-s cadence, file and semaphore stay apart from the battery thread's 5-s lines,
  gl28's lateness measure and SYS_DIAG's thread lists, and a battery-line or scePower hang cannot stop the black box (two
  independent witnesses).  Why 0x10: above the game (0x20) and the 0x11 threads (battery, HOME cb, diag_power), beside exc_writer.
- Record, 127 chars + \n: s seq7, t us10, c sceDisplayGetVcount6 (interrupt-driven), v game vb5 (me_mix_last_vb), n game thread's
  phase changes hex5, p tag2+detail2, d last pack read lsn6+sectors3, f picture turn/where, h loop heart6, a loop place word4
  (XmeSh.at), m ME's own heartbeat6, q head-tail2 (all from xl_psp_rec, as the battery thread), g ge_list_executed[0] (libpspgu's
  direct-list id) : sceGeListSync(id,1) peek, b mV,%,charging,external (scePowerGetBatteryVolt/LifePercent/IsBatteryCharging/
  IsPowerOnline: the battery thread's own getters, no other power call), l wake lateness ms, w the record before's write ms.
  Order: memory reads -> write (open, lseek, write, close) -> only then the system calls (battery, GE peek) for the next record, so
  a call that hangs leaves the second's record written.  Exit (gl1_glue's sceKernelExitGame wrap): bb_exit takes the semaphore
  (0.5 s) and writes an EX record.
- Tags (game thread only; plain stores g_bb_ph + g_bb_n++): host_enter/leave follow the timed parts (IO GS DP XL PI GW; a leave
  restores the part below or the base), base LG; HK host_vb_begin, PT scePowerTick, LC log_commit, HS from host_vb_begin's end to
  host_vb_end (state_hash's place in boot.c's hook: no boot.c edit), HE host_vb_end, OV the overlay fread, DR the pack read
  (g_disc_clock = bb_disc_clock, paired start/end calls; rt/iop_host.h stores lsn/k first), MW(why) in mq_wait / mq_probe after
  HOST_ENTER(HOST_GW), SH around gl_shot (now gl_shot_do) and gl_live's gl_bmp, EN finish, ST start.
- Size: bbox.o at -Os: .text 6.1 KB, .rodata 1.5 KB, .bss 0.96 KB; main_psp.o +0.8 KB, xl_psp.o +0.24 KB: about 9.6 KB more
  image = 9.6 KB less newlib heap.  NOTE for the final: gl43's final (f2) had 35 KB of room at the route's worst moment against
  the 32 KB gate; gl44's final will need that looked at.
- n44 [C]: mesec_chk, me_call_chk, xme_rest_chk, xme_me_chk PASS; 3 LOADs; EBOOT f9876b32.  ME side unchanged: the 59 ME-safe
  sources (ge/*.c/h, xme_me.c/h, xme_rest.S, me_sec.ld) byte-equal to t43's; ge_xme_me.o, ge_me_rt.o, xme_rest.o disassembly =
  gl43's final.  Imports vs gl43's final (g4_44.sh): 133 -> 134, the one new = sceIoRename (IoFileMgrForUser, a library already
  imported: .lib.stub same size, sceNid +4, sceStub.text +8); no object has a sce* reference gl43's lacks but bbox.o (new).
- Running 20:45: gates44a.sh (emulator a1 BBOX=1 / a0 BBOX=0 side by side, content vs twin sch, heap, rdbbox, steps, rot44).

## 2026-11-27 20:47 UTC, block A2 (block A's gates PASS on the no-PGO build n44 / pk_n44)
- Imports (g4_44.sh, against gl43's final) [C]: 133 -> 134 sce names, the one new = sceIoRename; xme_me / xme_rest / mesec / me_call PASS.
- Emulator, loop apart, PPSSPP 1.20.4, 501 s each [C emu]: a1 (MGS2_BBOX=1, the package's cfg) and a0 (=0): 62/62 = hashref_gl43
  each; content against the twin's sch arm 61/61 (50 K-N2 + 11 K-N2B frames, 0 differ) each; XP check 720 / 0; textures refused 0.
  Heap at the route's worst moment (vb 536): 191 KB above the break + 34 KB free top = 226 KB in both (gl43's no-PGO n1: 235 KB, so
  block A costs about 9 KB of heap, the image); outside the heap at the end a1 44 KB / a0 48 KB (the black box's 4 KB stack).
  CORRECTION to A1: gl43's final f2 had 138 KB at that moment (103 + 34), not 35 KB; the gate is 32 KB.
- Black box on a1: 460,800 B made in 23 ms, record 0 in 10.3 ms, thread 0x159 at 0x10 from vb 99; 1,150 records to the summary,
  0 failed writes, write time mean 12.9 ms / max 27 ms (emulator host I/O), lateness max 0.03 ms.  The run then sat at the end
  screen for ~31,700 emulated seconds, so the ring wrapped (seq 29,262-32,861): wrapping works.
- rdbbox.py: a1 -> "REACHED ITS END"; rot start 2 (killed) -> "BOTH END IN THE SAME SECOND", last tag LG, last read STAGE.DAT;
  a synthetic stall (rot s2's records + 10 with n/v frozen, tag MW03) -> "counter STOPPED while the heartbeat went on 10 s".
  Fixed in rdbbox: t is kept modulo 10,000 s (unwrapped along the chain; the span said -6401 s before).
- Cost on the emulator (stepcmp44, 7,400 timed steps vb 24,001-31,400) [C emu]: a1 - a0 mean +0.033 ms a step (43.322 vs 43.289);
  only 322 of 7,400 steps have the same time in both runs and single steps differ by -22 to +8 ms, so +0.03 is inside the
  emulator's own run-to-run noise.  PSP cost: UNKNOWN until a run (the log's last line and every record carry write time / lateness).
- H1 (rot44.sh, three starts in one folder: end / killed at window vb 300 / end): rot44_chk PASS 20/20 (_prev1 = the start before's
  file, _prev2 = the one before that, byte for byte; start 2 made no steps.csv, so start 3 moved steps_prev1 to _prev2 and made no
  _prev1; the H1 line says "has its end line" / "has NO end line" right; bbox.txt 460,800 B each start; the stand-in shot unchanged).
  The first try FAILED for a test bug: the watcher read the old log.txt before the new run had renamed it (fixed: it waits until
  log.txt is not the old one); kept as emu_rot_try1 / rot44_try1.out.
- Game folders ms/em_a1, em_a0, em_rot deleted after use.  Next: block B (V44) on t44.

## 2026-11-27 23:40 UTC, block B0 (resumed after the usage limit; PLAN CHANGED: straight through to the package)
- Tree re-checked against build_gl44/t44_blockA.md5: exactly 5 files differ, all my block B edits, each complete (python edits with
  asserts): boot/native_w00a.c (attic/gl44_vita's diff + review changes below), boot/native_w00a.h (the diff), boot/boot_x86.c (the act
  log by hand: the vita diff did not apply, mgs2/main's boot_x86.c has no gl43 seed log; MGS2_HALF=10 comment fixed), boot/boot.c +
  boot_x86.c (g_v44_n + "v44 turn" line), runtime/host_time.h (OPT2_V44 4194304, OPT2_DEFAULT excludes it).  Diff so far:
  build_gl44/blockB_part1.diff.  gen44/{wg,wpl}/gen_s_w00a.h = gen43's + v44_apply.py (one line at 0x31b47c each; a62f38c3 / 0ae208e9).
- Review changes to the proposal [C code]: (1) the stub first places pending G0 prims of the act (nat_ls_flush) if any (G0H not met for
  an earlier prim), else the act's last flush would put them over the last unit's tail; counted (g_v44_n[5], expected 0). (2) the
  counters live in boot.c / boot_x86.c (g_v44_n[6]) so the PSP's w00a overlay can count and a per-turn "v44 turn" line is printed like
  "b43 turn".  Reviewed: the rain.c skip 0x31b47c -> L31b6c0 jumps over both vertex loops (r21 iterations, r21-1 polls) and the fill
  loop (sp+152, -1 polls); everything L31b6c0 reads (r16-r20, r17) is set before 0x31b47c; G0R_RH brackets the whole loop in trace
  builds, so the skip leaves g_rh_off right.
- Coordinator 23:35: gl43's PSP log read; final table fixed by the coordinator (B on from t50 but anchors, V in alternate turns, see
  B1); go on through PGO, order, final, all gates, model, package build_gl44/deliver (no zip, no PC/PSP), drafts deviations_V44_draft.md
  and harness_draft.md here.

## 2026-11-27 23:55 UTC, block B1 (gl44's table; V44 on our tree, the twin's proofs PASS)
- TURN TABLE (tools/sched44.py; cfg/mgs2_gl44.cfg via tools/mkcfg44.py, 50 keys, MGS2_BBOX=1, MGS2_OPT2=ff9ff9ff).  Words: ff9ff9ff
  (no B, no V: bits 21 and 22 clear), ffbff9ff (B), fffff9ff (B+V); mkcfg44 asserts no other word.  Turn t = vb 24,000+100t ..+99.
  t0-t49 gl43's deck (route and hashref unchanged; every word ff9ff9ff, bit 22 cleared from gl43's ffdff9ff).
  t50 K-A anchor | t51-54 K-B, K-BV, K-BV, K-B | t55 K-N2BV + digests | t56 K-N2 anchor | t57-63 K-N2B, K-N2BV alternating
  (C c C c C c C) | t64-67 K7B, K7BV, K7BV, K7B | walk t68-72 C c C c C | t73 K-N2BV + digests.
  Anchors at the same turns as gl43's K-A t50 / K-N2 t56.  The 4-turn groups are ABBA, not ABAB (mirrored, so a linear drift over the
  4 turns cancels; t66-67 are the heavier K7 turns); the 7-turn group starts and ends on K-N2B as asked.  V turns 11 (t52 t53 t55 t58
  t60 t62 t65 t66 t69 t71 t73, the two digest turns t55 / t73 among them), B in t51-t73 but t56.
- Twin, route i to vb 37,800, 4 arms side by side, each with MGS2_HALF=10 and the seed and act logs from vb 23,000 [C twin]:
  off43 (gl43's table, bit 22 cleared): hashref = hashref_gl43 byte for byte; digests 2efe539dec17 / content d5f4355d9998 (14,203
  lines) = gl43's sch arm; seed log sv / st equal over gl43's range (from 28,900), sd equal (65,067 lines after gl43's first n=? lines).
  s44 (the cfg) / s44n (same, V cleared) / von (V in every word, MGS2_OPT2 too): seed log sv 27,764 / sd 98,024 / st 12,310 lines
  IDENTICAL in all three and off43; act log 13,059 passes, (act, seeds) and (act, seed, time) hashes 0 differ (md5 882c4e6d6bf9 in
  all four); iop and vfb equal on 685/685; xlive 37,800 / digests 14,203 / content 14,203 lines all equal; ee and gs differ from
  vb 29,300 in s44 (the first V turn t52 ends there) and from 23,300 in von (97 lines, as the design's 5.2: rain.c's hidden vertex
  buffers).  von's counters = the design's: line_sph 201,884 stubbed, 0 flushes, rain.c 36,930 skipped; short tails 0, placed first 0
  (my review guard never fired).  turnchk44 on s44: PASS (each turn's B and V44 line as the table).
- hashref_gl44 (cfg/, b6a845bf) = s44's: equal to hashref_gl43 to vb 29,200, differs from 29,300 (86 lines).  V44 needs its own.
- x86 host ticks per act call, whole run, arms in parallel (rough): rain.c von 240,762 vs s44n 502,339 (-52%); line_sph 64,143 vs
  74,212 (-14%); controls body_sph -2%, dgd +1%, gvd -2%.
- n44b (no-PGO PSP build with V44): psp.sh rc 0, xme_me / xme_rest / mesec / me_call PASS; EBOOT 2df82ff1.  pk_n44b with hashref_gl44.
- Running 23:55: gates44b.sh (emulator b1 package / b3 XP check), twin pgo / pgo0 arms, read-hash arms s44n / s44 / von (rh44.sh build).

## 2026-11-28 00:01 UTC, block B2 (block B's no-PGO gates PASS on n44b / pk_n44b)
- Imports (g4_44.sh against gl43's final) [C]: 134 sce names, the one new = sceIoRename (block A's); xme_me / xme_rest / mesec /
  me_call PASS.
- Emulator, loop apart, 436 s each [C emu]: b1 (the package: gl44's cfg, MGS2_BBOX=1, hashref_gl44) and b3 (XP check in every plan):
  62/62 = hashref_gl44 each; content 61/61 = the twin's s44 arm (all 61 are K-N2BV frames of t55 / t73: V's picture = the twin's),
  list digests differ 60 by design (native kind); XP check b3 114,008 plans, DIFFERING 0 (by result only 1,968 = gl43's f3);
  textures refused 0; heap at vb 536 191 + 34 = 226 KB (= block A's n44: V44 costs no heap); malloc at the end 192 KB in 1.
- turnchk44 [C emu]: the OPT2 word of all 74 turns = the table (ff9ff9ff / ffbff9ff / fffff9ff); 23 b43 and 23 v44 lines (t50-t72;
  t73's comes at the next turn, after the run's end), each on / off as the table; V44 counters = the twin's s44 to t72 exactly
  (stubbed 82,000, by G0 101,106, skipped 15,000, run 18,495, short tails 0, placed first 0).  turnchk44 fixed to accept t73's
  missing line (it said FAIL for that alone).
- rd44pc.py (rd/rd44pc.tpl.py + mkpc44.py) on b1: HASHES / DIGESTS / CONTENT EQUAL, SCHEDULE CHECK none.
- Black box on b1 (rdbbox): REACHED ITS END; write time mean 10 ms, lateness 0.
- Twin pgo / pgo0 arms (172 s each): cfg/hashref_gl44pgo.txt (1a046910), hashref_gl44pgo0.txt (4604091b); each = gl43's to vb 29,200,
  86 lines differ from 29,300 (V turns), as hashref_gl44 vs gl43.
- Running 00:01: pgo_a44.sh (5 instrumented builds + trainings, ~80 min); read-hash arms s44n / s44 / von.
