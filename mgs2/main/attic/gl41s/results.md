# gl41s results (2026-11-23 19:50 UTC): levers P1, P3, E6 and probes (a) (b) (c), as patches with switches

Nothing went to the PSP and no build was packaged. Marks:
- [C twin]: the x86 twin, whole route.
- [C emu]: the normal headless emulator (JIT).
- [C model]: PPSSPPHeadless_fcr. ms a step = (1.65 instr + 68 I-miss + 86 D-miss) / 333,000 / 80, read on windows 29,610 (rain wait, first figure) and 30,910 (walk, second figure).
- [C static]: read from the code or the ELF.
- [I]: inferred.

## 1. Verdict per lever

Every lever is exact (yes):
- P1, P3 and E6 each run on and off by turn in one run.
- Twin, whole route [C twin]: each arm gave 685/685 hashes = hashref_gl38 byte for byte, and digests 8be40bf85beb x 17,014 = gl39's sch arm.
  - Arms on and alternating on code v6; alternating again on the final code v7.
  - The alternating arm cycles all 8 on/off combinations by turn, all-off included.
  - The off arm ran on v4 (v6 = v4 + the P3 fold).
  - E6 check: 0 differing in 73,347 hits.
- Emulator JIT gate on the final code (v7o) [C emu]:
  - 62/62 hashes;
  - digests K0 166 / K7 50 / K8 50, 0 differing.
- Every model run [C model]: 62/62 hash lines equal. There were 17 lever runs on 6 layouts, plus gl39's control.

### Model saving on the main CPU, lever on minus all off in one binary

The binary is the v6d layout (section 2). Figures are ms a step [C model], rain / walk.

| Lever | Exact | Main | Of it runtime (main-rt) | Instruction part, lever's own functions | Estimate |
|---|---|---|---|---|---|
| P1, VU0 call fast path | yes | -0.56 / -0.83 | -0.48 / -0.67 | Call path -0.22 / -0.31 against off; -0.70 / -0.85 against gl39 | -1.0 [C static]; -0.7 to -1.0 [I] |
| P3, box cull without 8 checks | yes | -0.04 / -0.18 | -0.11 / -0.17 | Box cull -0.15 / -0.15: 24 instr a call fewer, not 51 | -0.26 [C static] |
| E6, rehash after uploads | yes | -0.03 / -0.01 | -0.08 / -0.01 | Rehash -0.18 / -0.13; its own I- and D-misses +0.08 / +0.10 | -0.1 to -0.15 [I] |
| All three on | yes | -0.88 / -1.13 | -0.77 / -0.94 | Call path + rehash -0.88 / -0.98 against gl39; box cull -0.07 / -0.06 against gl39 | -1.36 to -1.41 (sum) |

Notes on the table:
- The game side moves ±0.05 to ±0.15 ms with timing-dependent game loops, in either direction. The runtime column is the cleaner reading.
- P1 against gl39: the P1 code shape already saves 0.48 / 0.54 ms of instructions with the switch off. vu_run now has two callers, so GCC keeps it out of line with vuc_dispatch and vuc_entry inlined.
  - Call path + rehash instructions: gl39 1.566 / 1.782 ms, off 1.082 / 1.239, P1 0.866 / 0.933, all on 0.688 / 0.799 [C model].
  - So off is not byte-equal to gl39 in vu_run_shared / vu_run. It is equal in function (the twin's off arm).
- P3: 831 instructions against the form's 865 [C static]. The 8 dropped checks are vcmp + bvtl pairs.
  - The switch hook costs 16 instr a call when off (box cull +0.08 ms against gl39) and 10 when on.
  - The model counts no VFPU compare-branch stall. The stall saved on hardware is UNKNOWN and is not included.
- E6 hashes 548 / 671 of 1,024 words a rehash (simulated on the trace) [C twin trace]; the whole route averages 542 [C twin counters].
  - vu_hash_e6 is 1,464 bytes and runs about 10 times a step. It is cold each time: +258 / +298 I-misses a step.
  - In the v6o layout E6 read -0.24 / -0.18 on the main CPU. It is layout-sensitive.

### The same levers in other layouts [C model], main, rain / walk

| Lever | v6o (gl39 order + new lines) | v6a (P3 copy on the form's sets) | v6d |
|---|---|---|---|
| P1 | -1.02 / -1.34 | not run | -0.56 / -0.83 |
| P3 | -0.29 / -0.56 | -0.01 / -0.03 | -0.04 / -0.18 |
| E6 | -0.24 / -0.18 | not run | -0.03 / -0.01 |
| All on | -0.64 / -0.72 | -1.19 / -1.39 | -0.88 / -1.13 |

- v6o: its I-miss baseline is inflated, so the I-miss parts there are mostly layout.
- v6a: the P3 copy shared the sets of the hook lines; box cull I-miss +0.11.
- v6d: P3 copy at form + 0x14080; box cull I-miss -0.01.

## 2. Layout: the main risk for gl41 (found here; the fix is gl41's build step)

The off binary against gl39 (ms a step, main / ME) [C model]:

| Build | Main | ME |
|---|---|---|
| v6o: gl39's order, pads and folding + new lines | +6.33 / +6.54 | +5.64 / +4.86 |
| v6c: + clone names fixed | +3.62 / +3.55 | +5.66 / +4.74 |
| v6d: + text pad, RO_PAD and ME data pads | +3.15 / +3.38 | +3.27 / +3.01 |

All of it is I-misses (+18k a step main, +17k ME); the instructions are -0.49 / -0.34 ms in gl41's favour.

v6c, clone names fixed:
- P1 takes the C forms' addresses (the vuc_fns table), so GCC no longer makes their .isra.0 clones.
- 12 hot order lines (`.text.vup_*.isra.0`, about 600 KB) matched nothing, and the forms fell out of the hot list. That shifted the whole layout.
- Fix: those 12 lines renamed to the plain names.

v6d, pads:
- `. = . + 3660;` after vu_run puts the box cull back on gl39's sets. It closes vuc_dispatch's 19 KB hole, less vu_run.
- RO_PAD 0x850 puts .rodata on gl39's sets.
- .me_data and .me_bss on gl39's sets: me_sec_pad3.ld, ME_DPAD 0x19e0, ME_BPAD 0x1258. This fixed the ME D-misses (+3,654 to -591 a step).

The residual is I-cache conflicts. The region between vu_run and the box cull sits 1.2 KB off its gl39 sets, and vu_run and vu_hash_e6 are new hot code. Examples:
- box cull I-miss 2,382 against gl39's 493;
- io_dma_start +3,185;
- P11fa58 +2,179.

Only a new order fixes this. gl41 must rerun tools/layout_sim.c + tools/layout_order.py on a gl41 profile, as build_gl39/lay did, and retune RO_PAD and the ME pads. Otherwise gl41 all on would run about +2.3 ms main and +3 ms ME slower than gl39 [C model, v6d all on against gl39].

Placement rule for the P3 copy, needed in any new order [C model, v6a against v6d]: on the form's sets, two lines in:
```
_l41_f = .;   (before vupv_8baafbc3_0)
. = . + ((_l41_f + 0x80 - .) & 8191);   (then vupv_96936c0f_0_p3)
```
- The on path runs 3 lines of the hooked form (the hook and the jump at form+0xd9c) and then the copy.
- The copy is 260 bytes shorter than the form, so a 2-line offset keeps it inside the form's sets and off the hook's lines.

## 3. ELF proof [C static]

Final code v7o (gl39's pads + order v6o):
- pc_v7/boot_rt.o 9710506b; ELF 34462e70; EBOOT 33ac4dc9.
- 3 LOAD segments; 133 sce names = gl39.
- The switch tests are in place:
  - `ext 0x13` in vu_run_shared (P1);
  - `ext 0x14` in the box-cull hook (P3);
  - `ext 0x15` before `jal vu_hash_e6` (E6).
- Box cull: the gl39 form has 865 instructions / 58 vcmp; the hooked form 896 / 58 / 26 bvtl; the P3 copy 831 / 50 / 18.
- The three box-cull texts still fold: 660a267e and 96936c0f are 8-byte thunks.
- Size against gl39: text +18,684, data +16, bss +3,520. That is about 22 KB off the heap headroom, so the low point would be about 42 KB of gl39's 64 [I].
- v7b (the same objects, order v6b) linked; EBOOT 10f317fd.

Measurement layout v6d (v6 objects; v6 = v7 but the vblank counter fix):
- EBOOT d5ed4f90.
- 3 LOAD segments; 133 sce names.
- Gates: xme_rest_chk, mesec_chk, me_call_chk PASS.
- Size against gl39: text +24,556 (pads included), bss +8,216. About 33 KB of the 64 [I].

The probes are off by default and read-only:
- No register writes beyond user-mode code (they save and restore the VU0 registers they use).
- No system call beyond sceKernelGetSystemTimeWide, which the runtime already imports.

Probe results so far:
- (c): 690 forced inputs, 0 different, on x86 and in the emulator [C twin, C emu].
- (b): "holds" in the emulator only. The hardware VFPU is UNKNOWN.
- (a): ran in the emulator; its timings mean nothing there.
- `cache 0x1e` is a user-mode cache op of the same class as the `cache 0x18` that gl39's rt_qcopy_nov already runs on hardware. Whether 0x1e itself traps or blocks on the PSP is UNKNOWN until probe (a) runs.

## 4. Patches (attic/gl41s/patches/, against the gl39 tree; they apply to t39 and t40 alike)

| File | Content | Files touched |
|---|---|---|
| gl41_1_p1.patch | P1 + counter line | runtime/host_time.h, runtime/vu0.h, runtime/vu_l41.h (new), rt/ee_io.h, runtime/ee_io.h |
| gl41_2_p3.patch | P3 hook | runtime/host_time.h, runtime/vu0.h |
| gl41_3_e6.patch | E6 | runtime/host_time.h, runtime/vu0.h, runtime/vu_l41.h, rt/ee_io.h, runtime/ee_io.h, runtime/vif.h |
| gl41_4_probes.patch | probes | runtime/vu0.h, runtime/vu_l41.h |
| gl41_all.patch | the four in one | — |
| gl41_order_v6o.diff / _v6a / _v6d | order lines against build_gl39/ord/final/layout_elf.ld | v6d = the measured layout |

- Overlap with gl40: none. gl40 changes boot/nat_stub.h, boot/xlive_chk.c, ge/xlme.c, ge/xlme.h, psp/xl_psp.c, rt/kernel.h and runtime/kernel.h; my files are byte-identical in t39 and t40. The series was checked again on copies at 19:05: it gives the same files on both.
- The order and pad files will conflict with gl40's own, and must be made again (section 2).
- P3 also needs gen_vu.h made again: `python3 -I tools/gl41_genvu.py` on gl39's vp22/gen_vu.h (input feb948ce, output a00691cd).
  - It proves the three box-cull texts equal, checks the rule on each, and adds the hook and the copy.
  - It also adds the vuc_fns table that P1 needs.

Switches are MGS2_OPT2 bits, alternated by turn with MGS2_OPT2_CYCLE:
- bit 19 P1, bit 20 P3, bit 21 E6;
- all off = ffc7f9df; gl39's words already have them on;
- cfg/mgs2_l41_gate.cfg cycles all 8 combinations.

Measurement keys, off by default:
- MGS2_E6_CHECK=1;
- MGS2_L41_LOG=1 (the counter line every 100 vblanks);
- MGS2_PROBE41=<mask> (1 = a, 2 = b);
- MGS2_PROBE41_CULL=<n> (c).

## 5. What is left

1. Merge the series onto gl40's tree after gl40's PSP log, run gl41_genvu.py, then make a new order and pads for gl41 (section 2). Without them, gl41 loses more to I-misses than the levers gain.
2. Hardware: the lever A/B by turn, and probes (a), (b), (c). The VFPU stall that P3 removes, and the `cache 0x1e` behaviour, are UNKNOWN until then.
3. Heap: +22 KB (v7o) to +33 KB (v6d) against gl39's 64 KB low point [I]. Read the low point on the first gl41 PSP log.
4. E6 is close to zero in the model (-0.01 to -0.24, by layout). It is the first lever to drop if text or heap is short.
