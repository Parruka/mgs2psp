# Audit of build v30 (2026-10-26)

Asked by Ffm (2026-10-26 15:59) while the v30 rerun with the Media Engine off runs on the PSP. Scope: build v30 (MGS2T_v30, the
indoor free-play timing route) and its first PSP run. Method: every claim in psp_harness.md "Build v30", play/w01a_route.md "The
timing route" and prof/model_v30_in.txt checked against the files in prof/, play/, recomp/ and the delivered folder on the PC, with
the audit's own scripts (not the port's hashcmp.py). Read only: nothing in Desktop\mg2 port or in the port thread's notes was
changed. [I] marks an inference.

## Summary

| Severity | Count | What |
|---|---|---|
| High | 0 | The build, its references and its exactness claims hold. |
| Medium | 3 | The switch-off analysis: one wrong run time, a tally with no ME-off denominator, and a rerun that cannot settle the question on its own. |
| Low | 8 | Notes gaps, small unstated choices, evidence left in an ephemeral container. |

## Verified (correct as written)

- **Delivered files.** All 15 md5s in `Desktop\mg2 port\MGS2T_v30` recomputed on the PC today and equal to the notes: EBOOT
  3ff10587..., the eight overlays (d00t 9b286ecb, d01t a579aaa6, init f4b9190d, n_title 6ceee4b5, r_title f6f459d8, r_tnk0 102b6e32,
  w00a 80aa55eb, w01a 644c627a), mgs2.cfg 992160ae, hashref.txt d666d226, pad.txt 7bc72ace, README 7f73b948, SLES_503.83 e3f4c0df
  (= mgs2/disc/SLES_503.83), mgs2disc.pak 189ca53f (91,461,632 bytes). memcard0 empty. The same md5s are in prof/emu_v30/me_build.txt.
  hashref.txt = prof/hashref_x86_in_v30.txt, pad.txt = play/pad_w01af.txt (md5s equal).
- **ME-off rerun files.** `MGS2T_v30_me_off`: mgs2.cfg d627bea2, README 35c9c6e1 (equal to the notes). Its cfg differs from v30's only
  in the ME block (`MGS2_SPU_ME=0` and the comment); the instructions (copy only mgs2.cfg) are right.
- **Pack.** Recomputed from play/disc_reads_in_v30.log (3,282 lines) with make_pak.py's own rules: 11 files, 31 ranges, 44,658
  sectors, 2,048-byte header, 91,461,632 bytes: equal to the notes and to the file on the PC. tools/make_pak.py md5 a5263acd as noted.
- **Coverage.** recomp/cov/cov_x30.txt = cov_x25 + 689 new entries (41 ELF, 91 w00a, 557 w01a), none removed.
- **x86 references.** prof/hashes_x86_in_v30.txt (378 lines) equals prof/hashes_x86_free_v28.txt at all 305 points to vb 30,500;
  first difference vb 30,600 (the routes part at 30,501). It equals the deck reference to vb 23,400 (pad diverges at 23,410).
  hashref_x86_in_v30.txt (685 lines) agrees with it at all 378 shared points; its 311 dense lines all carry vuh and equal the free
  route's.
- **Emulator exactness** (own script: ee / iop / vfb against the every-100 reference, ee + vfb against hashref, vuh, mix checkpoints):

  | log | every 100 | hashref | vuh | mix | split |
  |---|---|---|---|---|---|
  | emu_v30/log_deck (vs _deck_v27) | 63/63 | 370/370 | 311/311 | - | none |
  | emu_v30/log_w01a (vs _w01a_v27) | 63/63 | 370/370 | 311/311 | - | none |
  | emu_v30/log_free (vs _free_v28) | 124/124 | 431/431 | 311/311 | - | none |
  | emu_v30/log_in (build d) | 127/127 | 434/434 | 311/311 | - | none |
  | emu_v30/log_me1 (shipping EBOOT, ME=1) | 127/127 | 434/434 | 311/311 | - (ME not started on PPSSPP) | none |
  | emu_v30/log_es2 (shipping EBOOT, ME simulated) | 127/127 | 434/434 | 311/311 | 75/75 eq | none |
  | hw_v30/log (PSP run 1, to vb 18,000) | 39/39 | 346/346 | 311/311 | 36/36 eq | none |

  cfg_me1 / cfg_es2 equal the delivered mgs2.cfg except `MGS2_EXC_DRY` (and ME=2 for es2). The emulator ran the shipping program:
  same catcher handler 08b5b3fc as on the PSP, overlays 08bb6000-09016000 = ovl_arena + 0x460000.
- **Sound checkpoints.** prof/mix_ck_x86_in_v30.txt has 75; ck 0-60 equal prof/mix_ck_x86_free_v26me.txt, ck 61 (29.76 M frames =
  620 s of sound, about vb 31,000) is the first that differs: consistent with the routes parting at vb 30,501.
- **PS2 float rule (M1).** prof/m1_denormal_v30_in.txt: A = reference 378/378 and 685/685, B = A in ee / iop / vuh and in VIF1 + GIF
  data of all 37,799 vblanks; zero rule only at the shared start's arctangent (vb 15,785-28,678). As claimed.
- **Code order.** order_ls_s1.txt: 4,674,300 -> 3,151,600 (-32.6%), held out 654,883 -> 443,735 (-32.2%); trace 2 2,344,911 ->
  1,600,479 (-31.7%), held out 403,693 -> 281,102 (-30.4%). Seed 2's weighted sum 8,927,133 against 8,930,921 (0.04% lower). As claimed.
- **Memory fit.** EBOOT code ends 08baca00 < ovl_arena 08bb6000; arena 4,587,520 bytes holds the largest overlay (w00a 3,830,624;
  757 KB spare). The PSP heap break peaked at 0b3ef000 (v29 0b3ee000, v26me4 0b45e000 with no trouble); kernel free memory 752 KB on
  every battery line, as v29.
- **Pad route.** pad_w01af.txt matches w01a_route.md part by part (stand 31,800-32,300; walk 32,300-34,110; crawl 34,110-34,510;
  punches, fire 34,530-34,840; first person 34,900-35,500; sneak 35,520-36,440; alert 36,440-37,800 with R 60, U 16, D 20, L 260, then
  U / D laps from 37,106). The part lengths give the cfg's 10 / 36 / 8 / 8 / 12 / 19 / 27 s. 61 hashes in the timed range (31,800-37,800
  every 100). The route's frames (alert at 36,803) were checked in the port's probe; not re-run here (x86 build is ephemeral).
- **Model.** prof/model_v30_in.txt: every window's ms recomputes from its instructions and misses with the cost formula (walk 31.93,
  alert 39.88); mean 33.46. The 1.116 factor recomputes from prof/hw_v29/steps.csv over v29's model windows (1.1223, 1.1107, 1.1163,
  1.1157). 33.46 x 1.116 = 37.3: "about 37" is the right reading of the method.
- **Run 1 facts.** Last committed window vb 18,300; battery.txt's last line t 1,249 s, vb 18,234, 79%, 4,086 mV, ME heartbeat
  16,349,963, 188,917 / 188,905 commands; pending-command lines 6 of 246 (v29 15 of 486); crash.txt has only the start lines; the
  comparison windows (v29 vb 18,400: 4 reads 128 KB, max 65.30 ms; v24 vb 19,000: 2 reads 50 KB) are as written. Auto sleep is
  covered: main_psp.c calls scePowerTick every window.

## Medium

**M1. v24 deck run 1 switched off about 22 minutes in, not "about 11 minutes".** The 11 minutes is its step time alone (10.9 min).
The run also took 347 state hashes (the 311 dense ones of vb 15,700-16,010 included), about 1.95 s each on the PSP: about 22 minutes
in all [I]. The same arithmetic gives 1,254 s for v30 run 1, against 1,251 s measured by battery.txt, so the method holds. This
changes the comparison in psp_harness.md "The four switch-offs compared": the four stops come in two pairs, each pair in the same
stretch of identical game states and at nearly the same time into the run:

| run | ME | stop (vb) | time into run |
|---|---|---|---|
| v24 deck run 1 | off | 18,900-19,000 | ~22 min [I] |
| v30 run 1 | on | 18,300-18,400 | 20.9 min |
| v26me2 | on | 33,600-33,700 | 38.6 min |
| v26me3 run 2 | on | 34,100-34,200 | 39.5 min |

v24's and v30's routes are the same game state to vb 23,400; v26me2's and v26me3's are the same free route. Since the routes are
scripted, game spot and run time move together, so the data cannot tell them apart. Neither is a sure trigger: about 28 runs passed
vb 18,300-19,000 and six (v25f, v26, v26me, v26me4, v28, v29; three of them with the ME mixing) passed 33,600-34,200. "No common game spot or
time" should read "no exact common vblank; two clusters, each about 500-600 vblanks wide, at about 21-22 and 39 minutes".

**M2. The tally has no ME-off denominator.** "ME on: 3 switch-offs in 6 runs; ME off: 1" leaves out how many ME-off runs there were.
From the logs in prof/hw_* (steps + hashes, per run): ME off, 25 runs, about 12.1 hours, 1 switch-off; ME mixing, 6 runs, about 3.7
hours, 3 switch-offs. That is about 1 per 74 minutes against 1 per 12 hours [I]. If the rate were the same, 3 or more of the 4 would
land in the ME runs about 4% of the time [I]. So the ME is the leading suspect rather than "too few to blame". Caveat: the ME runs
are also the latest runs (an older battery, the kernel catcher from v26me4, KUBridge calls) and the longest routes, so the ME is not
proven either [I].

**M3. One clean ME-off run cannot clear or convict the ME.** At the ME runs' rate, a 40-minute run stays on about 58% of the time
anyway [I]. So if the rerun ends cleanly, that is weak evidence (likelihood about 1.6 to 1 for "the ME is the cause") [I]; if it
switches off, that is strong evidence the ME is not the only cause. The rerun README says the second half correctly but does not set
expectations for the first. Recommended: read the rerun's result this way, and plan 3 or 4 ME-off runs (or alternate ME off / on)
before deciding anything about the ME sound plan. Watch especially whether the rerun passes about 21-22 minutes and vb 18,300-19,000.

## Low

- **L1. Disc reads in the timed range, not in the notes.** The indoor route reads VOX.DAT 18 times (273 sectors, 546 KB) at vb 36,616,
  36,718, 36,752, 37,432 and 37,633 (the guard's radio lines). Each read is on the game thread (13-31 ms on the PSP, speed_notes
  §42), so expect max-step spikes there; effect on the mean about 0.03 ms [I]. steps.csv's disc columns separate them. The free route
  had none in its timed range.
- **L2. The model's mean weights the five windows equally;** the alert and evasion windows are 2 of 5 but the alert part is 27 of
  120 s. Weighted by the route's parts (stand, punches and sneak at the walk's cost) it is about 32.8 ms, or about 36.6 on the PSP
  [I]. Within "about 37".
- **L3. Header step rc 2** (v30.sh edited while the step ran). Mitigated: 7 headers byte-identical to v29's, and the x86 and emulator
  runs from them are exact on all four routes. Process risk only (the notes already say never to edit a running step's script).
- **L4. Evidence left in the ephemeral container.** The ME-off emulator check (cfg_me0, log_me0, steps.csv), the model's dumps and
  the build dirs exist only under /home/claude/v30; prof/ has none of the me0 files, so that check cannot be re-read. Copy them to
  prof/emu_v30 if that container still has them.
- **L5. No v30 entry in speed_notes.md** (last section §56). psp_harness.md and prof/model_v30_in.txt hold everything.
- **L6. Opening-speed comparison** (v29 30.96, v30 30.84 ms/step over "182 windows"): reproduces only without the first window
  (vb 200-18,300); with all 183 it is 31.08 / 30.96. Same conclusion (no regression); the window choice is unstated.
- **L7. Self-test offset:** v30 reached vb 18,000 10 s after v29 (1,244 s against 1,234 s), not 15 s; v29's O press and v30's slightly
  faster opening explain the rest [I]. Cosmetic.
- **L8. Cosmetic labels:** me_build.txt prints "ovl_arena 0x08bb6000 (v26 0x08ba0000)" where 0x08ba0000 is v30's build d, not v26.
  Seed 1 kept though seed 2 was 0.04% better: negligible.

## Not checked

- Rebuilding v30 or re-running PPSSPP: the build and emulator dirs are in the port thread's ephemeral container, not here.
- The route's frames (alert, evasion) and the -2 / +2 vb shift tests: taken from w01a_route.md, not re-run.
- State hash 2 (v31 work, psp_harness.md "State hash 2"): outside this audit.
