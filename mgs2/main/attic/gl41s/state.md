# gl41s: the small exact levers for gl41 (P1, P3, E6) + probes, as patches with switches (newest block last)

Agent: gl41 lever-prep agent. Nothing goes to the PSP; no build is packaged. Sources: attic/top3/levers_top3.md (§2, §3 row E,
§5 P1 / P3, §8 gl41), psp_harness.md "Build gl39" + its 4 rules, attic/gl39/state.md + report.txt, attic/gl40/gl40_plan.md + state.md.
Work copies (container, temporary): scratchpad gl41s/ (t = copy of /home/claude/t39, x19 = copy of build_gl40/x19/x86's objects).
Not touched: /home/claude/main sources, /home/claude/t39, build_gl* trees, scratchpad/h1.

## 2026-11-23 15:50 UTC, block 1: start
- Read the sources above. gl40 changes (diff t39 t40 at 15:48): boot/nat_stub.h, boot/xlive_chk.c, ge/xlme.c, ge/xlme.h,
  psp/xl_psp.c, rt/kernel.h, runtime/kernel.h (+ link / order / pad files).
- Which file each build compiles [C, psp.sh + tw39.sh + #include chain]:
  - PSP boot_rt.o: boot/boot.c with -I boot -I rt -I runtime -I psp: ee_io.h, kernel.h, sif.h, iop_host.h from rt/; guest.h from
    runtime/; vu0.h from runtime/ (included by runtime/guest.h with quotes: the includer's folder first; rt/ has no vu0.h);
    gen_vu.h = -DMGS2_GEN_VU=$G/vp22/gen_vu.h (G = <link farm>/gen -> /home/claude/main/gen; md5 feb948ce).
  - x86 twin boot_rt.o: boot/boot_x86.c with -I runtime -I boot only: runtime/ee_io.h, runtime/kernel.h, runtime/vu0.h, same gen_vu.h.
  - So: P1 = runtime/vu0.h (both builds); E6 = rt/ee_io.h (PSP) AND runtime/ee_io.h (twin); P3 = gen_vu.h (both).
- Switches: new MGS2_OPT2 bits (runtime/host_time.h, which gl40 does not change), alternated per turn by the existing
  MGS2_OPT2_CYCLE: no new harness code in nat_stub.h / xl_psp.c (gl40's files).

## 2026-11-23 16:36 UTC, block 2: code written, twin proven, PSP control = gl39
- Code (scratch gl41s/t, a copy of t39): runtime/host_time.h OPT2_P1 bit 19, OPT2_P3 bit 20, OPT2_E6 bit 21 (on in OPT2_DEFAULT and
  in gl39's cfg words, so gl39's cfg runs them on; all off = ffc7f9df); runtime/vu0.h (E6 cache of 4 images + hook in vu_hash; P3
  hook macro; vu_run_shared moved to the new runtime/vu_l41.h with P1); runtime/vif.h + rt/ + runtime/ee_io.h (E6_DIRTY / E6_LOST
  at every VUPROG writer; l41_init + counter line in io_vblank); tools/gl41_genvu.py (P3 copy vupv_96936c0f_0_p3 + hook + vuc_fns
  table for P1, each rule checked on the text; 7 negative tests rejected as they should).
- Rule check [C static]: all 16 single-register checks of the box cull are redundant by the rule (8 first ones -> next check
  of the same register with 2 vmul.q r, r, t between; 8 second ones -> the out_ multi-check with nothing between). P3 = the 8 first.
- x86 twin (tw41.sh: only boot_rt.o compiled again; gl39's tw2 relinked with the scratch x19 objects = tw2's boot byte for byte):
  arms on (gl39 schedule, levers on + probe (c) at the 5000th box cull), alt (8 on/off patterns by slot + E6 check), off:
  685/685 = hashref_gl38 byte for byte each, digests 8be40bf85beb x 17,014 = gl39's sch arm each [C twin]. E6 check: 48,673 hits,
  0 differing. Probe (c) on x86: 690 forced inputs, 0 different.
- PSP: overlay compile (tools/l41_overlay.py: #line keeps gl39's file names and line numbers in scratch copies of the 6 headers;
  -I<ov> first) -> control boot_rt.o = build_gl39/u/boot_rt.o byte for byte (d9304cff), and its relink (rl41.sh, gl39's pads
  and order) = gl39's EBOOT 52eb4246 byte for byte. So the variant differs from gl39 only where the levers changed code.
- Found: E6 with 4 images misses about 45% of rehashes in the rain window [C twin counters]; checking the image working set.

## 2026-11-23 18:18 UTC, block 3: E6 redesigned, P3 fixed to cover the rain wait's calls, layout shift found
- (Container restarted at about 17:42; scratch survived. Re-read this file, checked m_ctl's run: 62/62 hash lines equal, 502 s.)
- E6, old design (cache of 4 whole images, 16 KB): misses 43-45% in the rain wait; a trace of the images [C twin, E6_N 32]: rain
  10 images, walk 11, every upload starts at word 0 and ends at 152, 282, 390, 512, 524 or 806. A hit still read the stored image
  (cold lines) and a miss scanned the other images: likely a loss. Replaced by a multi-level FNV-1a suffix-state cache (4 levels x 16
  pairs, 512 bytes, no image copies): FNV-1a's step is a bijection of the state for a fixed word, so while words [L, 1024) stay
  unwritten the final hash is a bijection of the state at L. A rehash hashes from word 0 up to the first level that holds its state.
  Simulated on the trace: words hashed a rehash 548 (rain), 671 (walk), 542 (route) of 1024, hits 99% [C twin trace + sim].
- P3 found incomplete: the box cull is three texts (vupv_96936c0f_0, vupv_8baafbc3_0, vupv_660a267e_0) that GCC folds into one
  function in gl39's ELF (one address, 08a45bec). The rain wait calls it only as 8baafbc3, the walk as both [C model m_off]; the first
  P3 hooked 96936c0f only. Now gl41_genvu.py proves the three texts equal after copy propagation and renaming (2 negative tests
  rejected, 1 dead-copy change rightly accepted), checks the rule on each, and hooks all three to one P3 copy; GCC still folds the
  hooked forms (v6 ELF: 660a267e and 96936c0f are 8-byte thunks to 8baafbc3_0).
- Layout: the first variant with every lever off ran +5.9 ms a step on main and +8.4 on the ME against gl39 [C model m_off vs m_ctl],
  almost all I-misses spread over game code and ME: the text grew 22 KB under gl39's pads. So the levers are measured on vs off in one
  binary, and gl41 needs its pads / order tuned again after the merge.
- Twin, final code (v6) [C twin]: arms on and alt 685/685 = hashref_gl38 byte for byte, digests 8be40bf85beb x 17,014 = gl39's sch
  arm; E6 check 0 differing in 73,347 hits; probe (c) 690 forced inputs, 0 different. (Arm off passed on v4; v6 = v4 + the P3 fold.)
- Patches: 4 stacked (P1, P3, E6, probes) + one all-in-one, against gl39; they apply to t39 and to t40 alike (no file in common with
  gl40's changes). Model runs on v6 under way.

## 2026-11-23 19:55 UTC, block 4: model savings per lever, layout found, results written (results.md)
- (Usage limit reset at 17:40; re-read this file, checked the last outputs.) Final code v7 = v6 + the counter line counted in
  l41_vblank (io_vb stays 0 on the PSP): twin alt arm 685/685 + digests as block 3; emulator JIT gate v6o and v7o 62/62, digests
  K0 166 / K7 50 / K8 50, 0 differing; probe (c) 690 forced, 0 different [C emu]. Patches regenerated and copied (19:00).
- Model, lever on minus off in one binary, layout v6d (below), main ms a step rain / walk [C model]: P1 -0.56 / -0.83, P3 -0.04 /
  -0.18 (box cull -0.15 instr), E6 -0.03 / -0.01, all on -0.88 / -1.13. Instructions against gl39: VU0 call path + rehash -0.88 /
  -0.98 all on (-0.48 / -0.54 of it already with the switches off: vu_run out of line with vuc_dispatch inlined). 17 lever runs on
  6 layouts + gl39's control, every one 62/62 hash lines equal.
- Layout: off against gl39 +6.3 ms main / +5 ms ME (v6o) [C model], all I-misses. Causes found: P1 takes the C forms' addresses,
  so GCC makes no .isra.0 clones and 12 hot order lines (~600 KB) matched nothing (renamed: v6c, +3.6); vuc_dispatch's 19 KB gone
  (text pad 3660: box cull on gl39's sets) + RO_PAD 0x850 + .me_data / .me_bss on gl39's sets (scripts/me_sec_pad3.ld) (v6d:
  +3.2 main / +3.3 ME). The rest needs a new order (tools/layout_sim.c + layout_order.py on a gl41 profile): gl41's build step.
  P3 copy placement rule: form's sets + 2 lines (patches/gl41_order_v6d.diff).
- Files: results.md (verdicts, savings, ELF proof, patches, what is left); evidence.txt (the runs' proof lines); patches/gl41_order_v6a.diff, _v6d.diff;
  scripts/rl41p.sh, scripts/me_sec_pad3.ld. Done; scratch cleaned next.
