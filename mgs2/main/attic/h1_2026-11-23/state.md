# H1 exact form: state (H1 agent, 2026-11-23; newest block first)

Marks: [C] measured/read in code, [C model], [C twin], [I] inferred (basis named), UNKNOWN.
Scratch (container only, ephemeral): /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/h1/
  s19/ = copy of /home/claude/main/{boot,runtime} (gl19's game objects were compiled from these), s39/ = copy of /home/claude/t39
  {boot,runtime,ge,rt}, gen/gen_elf_wh.h = copy of main/gen/wh/gen_elf.h.  No file in main/, t39/ or build_gl* is changed.

## 2026-11-23 16:30 UTC: READER LIST (what the cut passes write, who reads it); verdict: the hash cannot be kept -> owner's call
(Shared findings.md could not be written: this agent's harness refuses report files from subagents. This block is the reader
list the design agent asked for; the final report to the coordinator carries the same.)
Method [C twin]: trace build tt, every guest load/store outside the drawing pipeline hashed per site over the whole route (base vs
c7), then per-load logs (MGS2_READ_LOG) at vb 535, 8,844, 15,759, 20,145, 23,262, 26,165, 29,071, 31,663 naming the reader and the
last writer of every load whose bytes differ (rlz_cmp.py), plus an order-free compare (h1/py/rlms.py).
Over the route: read counts equal in every vblank; 53 load sites (of 74,009) and 58 store sites (of 64,847) outside the pipeline
see different bytes; every other site sees the same bytes in the same order. Logged vblanks, differing reads of all reads:
535: 8 of 4,936 (order only); 8,844: 5,714 of 535,070; 15,759: 0; 20,145: 24; 23,262: 73; 26,165: 21; 29,071: 77; 31,663: 0.
Verdicts: RENDERER = only renderer code reads it; ORDER = same reads, other order; LEFTOVER = game loads bytes nobody acts on;
GARBAGE-IN = game code computes from bytes it never wrote that a pass left there; the result is stored but no game load outside the
pipeline ever sees different bytes from it on this route [C twin, route-wide per-site hashes].
 R1 page+0x34c halfword ("texture-list flag"): cleared at 0x11e788 (P11e5e0, pipeline start), set at 0x122c9c (P1226c8, sort chain),
    0x12f744 (P12f130, people), 0x135784 (P135338), 0x137e78 (P137730), 0x13db24 (PluginActor). Game loads: none [C grep]. RENDERER.
 R2 libdg frame code (P1238e8, P123b28, DG_DmaClear P123e80, P124380, DG_DmaSkip P124118, DG_DmaCheckEnd P124180, DG_DmaStart
    P123f18): DMA chain words, 0x18f366 (packets this step: 12 base / 9 c7 at vb 29,900 = RAM kind 6), 0x18dba8 (timer delta),
    0x1e6b28.. (one T1 timer sample per packet, read from 0x10000800). Readers: frame code itself; P11e5e0 reads 0x18f360. RENDERER.
 R3 packet buffers 0x1b0xxxx / 0x1d0xxxx, chain descriptors 0x1b8b30.. / 0x1b8f3c.., packet pointers 0x1e5080 / 0x1e50a0, list
    0x1e1080, 0x1e6a9c..: written by the cut passes, read by frame code and the runtime's DMA; no game load sees them differ. RENDERER.
 R4 vblank start (535, 8,844, 23,262, 29,071 and the 9,749 odd vblanks where load addresses differ): GS FINISH handler P123b70,
    MTS_SetEvent P15dbb8, MTS_CreateEvent P15d9d0, WaitAllDone P11d720, P11d6f0, P11d910, P1409b0 read the same words in another
    order (FINISH arrives earlier with fewer packets): multisets of (address, bytes, reader) equal at 535, 15,759, 31,663. ORDER.
 R5 w / z lanes left in VU0 registers or scratchpad by the passes' packet work (VIF codes such as 0x6c0880e8 in base, floats in c7):
    d00t vb 8,844: P2929b0 / P2936b8 store VF regs whose w lane is a leftover -> copied by P2933e0, P323b58, P323c38 (0x1706xxx,
    0x170fxxx-0x1714xxx, scratchpad) -> P165f78 sceVu0SubVector (w) -> P125f18 DG_PutVector (w unused, speed_notes 34). w00a
    P3bebf0 SetQuatSlerp: lqc2 of the scratchpad stack quadword 0x70003ec0, z/w = packet words in base; VU0 micro program
    vup_33268338_0 works lane by lane, z/w never reach x/y, P3bebf0's stores equal over the route. w00a P331160 (CLIP reload ANDed
    0x3330). w00a P2e59b0 -> GLDU copies (rain, w only). w01a P2e8200 / P3aaef8, d00t P295fa8 / P293a28 / P29f490, d01t P329668,
    w00a P3b7c88, w01a P3a3f90 (quadword copies, w lane), DG_SetMainLightDir P126818 (w, vb 24,697). LEFTOVER.
 R6 P168ad0 soft-float double add (libgcc fp-bit): copies a ZERO operand's unset normal_exp (stack); packed as zero. LEFTOVER.
 R7 d01t P21a798 (per-joint matrix build), vb 20,145 only: joint 21's quaternion (scratchpad 0x70002190) and translation
    (0x70002990..98) were never written by the game this step; base holds SortChainObjs P121c30's packet words (0x10000004,
    0x01000101), c7 older d01t data (20000.0, 804.0, 1.0). MT_QuatToMat P11c828 + sceVu0MulMatrix P165d78 build a matrix from it
    and GLDU/GSDU copy it into the object's joint array (+192, stride 304). GARBAGE-IN (the original reads the sort chain's leftovers).
 R8 d01t P335100 (VU0 lerp), vb 26,165-26,171: reads scratchpad 0x700001c0-0x700002b0 never written by the game; base = packet
    data from P1213b0 / P12d910 / P122138 / P124630, c7 = zeros/floats from P13fdd8 / P12dc90 / P12dbc0 / P12d810. Output read back
    only by P335100 itself. GARBAGE-IN.
The four keys that differed at every unshown step (h1_notes): d00t P272fd0 and d01t P276b38 no longer differ anywhere with EVM
out of the cut (EVM was their cause) [C twin]; w00a P3bebf0 and w01a P3aaef8 remain, both lane leftovers (R5, speed_notes 34).
Why the hash cannot be kept: R1-R3 and the R5 lanes are bytes in RAM / scratchpad at the hash points; the cut passes write them on
unshown steps in the base and not in H1. No mask of renderer memory alone makes c7 equal: R5 leftovers also land in game memory
(0x1706xxx.., 0x18589cc, 0x1562a0c, 0x17fd42c..). Per the brief's stop rule this is the owner's call.

## 2026-11-23 16:05 UTC: per-pass hash runs done; trace (reader) runs done; READ_LOG runs going
- Per pass, on top of 1214a0, ee hash lines differing of 684 [C twin]: sort chain 603 (+ vfb 117), people 502, PluginActor 149 (from
  vb 23,000 on), shadow + spot 561 (shadow + spot cost 0.008 model ms a step), c5 561. Every pass moves the hash on its own.
  Pictures: frame digests 17,664 equal in every variant [C twin]; iop, virtual time, vuh equal in every variant.
- Trace build tt, rb2 (base) vs rc72 (c7), frame code (libdg/dma_ctrl, 9 functions) left out of the read hash [C twin]:
  read counts equal in all 37,799 vblanks; load ADDRESSES differ in 9,749 vblanks (535, 537, ... odd), values in 9,961.
  53 load sites and 58 store sites outside the pipeline see different bytes over the route. Per-site vblank lists: h1/rvv_c72.txt.
  Most are leftovers (stack / w-lane bytes) or a handful of vblanks; the steady ones: libdg frame code (renderer side), GLDU/GSDU
  (unaligned 64-bit copies, 3,847 vb in rain + w01a), w00a P3bebf0 SetQuatSlerp + P331160 (1,230 vb, the known lqc2-of-stack and
  clip-flag patterns), w01a P2e8200 / P3aaef8 (w01a, dest-masked / z-w slerp, known), d00t P323c38 (8,758-9,134).
- Code read: P168ad0 = soft-float double add (libgcc fp-bit): copies a ZERO operand's unset normal_exp (stack leftover); the
  result is packed as zero, the field is not used: harmless. w00a P331160: stores CLIP flags, reloads, ANDs with 0x3330: the
  differing byte is masked: harmless.
Running: READ_LOG runs (base rlb, c7 rlc) at vb 535, 8,844, 15,759, 20,145, 23,262, 26,165, 29,071, 31,663 (h1/tt) to name the
writer of every differing load. Reader list for the renderer seam: findings.md (kept current).

## 2026-11-23 15:30 UTC: per-pass hash runs (part), model estimate from gl39's profile, trace runs going
- Hash lines against base [C twin]: so (1214a0 + sort chain 122138 alone): ee 603/684 differ, vfb 117 (all of c7's vfb), iop 0,
  time 0. c5 (c7 without the sort chain: people, PluginActor, shadow, spot): ee 561/684, vfb 0, iop 0, time 0. pe / pa / ss running.
- Model [C model data, gl39 final build, build_gl39/model/ka39.{1,4,6}; inclusive cost from per-PC counts + call edges, shared
  callees split by call count; py/incl.py], ms a PSP step over ALL calls (shown + unshown): PluginActor 13d540 3.47 / 3.49 / 3.42
  (rain 29,610 / rain 30,210 / walk 30,910), sort chain 122138 2.01 / 2.01 / 1.92, people 130390 + 1303a8 1.55 / 1.57 / 1.62,
  shadow + spot 0.008, EVM 138ac8 + 138ae0 0.07 (so taking EVM out costs ~0.03 ms a step). c7 total 7.04 / 7.07 / 6.98; H1 cuts
  the unshown half: ~3.5 model ms a step [I: half of the inclusive cost; proportional split]. Not in it: fewer packets for the
  runtime's DMA walk, placement (gl17: I-misses +2.3 model before a layout search). UNKNOWN until a PSP-form model run.
- Step 1 (code read): our "texture-list flag" = the halfword page+0x34c (844): cleared for every page at the pipeline's start
  (0x11e788) and set to 1 by each pass that uses the page (0x122c9c, 0x12f744, 0x135784, 0x137e78, 0x13db24 = PluginActor).
  No load of it in the main ELF's game code (only these stores) [C grep of gen_elf.h]; w00a / w01a have 32-bit +844 loads on
  other structures. The trace runs decide whether any game load sees a changed value.
- Trace build tt (GEN_DG_TRACE -g1, every unit; ed_tr.py counters): first 1,000 vb hash lines = hashref. Full runs rb (base) and rc7
  started 15:24.

## 2026-11-23 15:25 UTC: what differs in RAM at the hash points (c7 = cut without EVM, against base) [C twin, RAM dumps]
- Dumps every 4,000 vb from 1,900 (both runs' hash lines as before). Differing 32-bit words: 1,900: 180; 5,900: 0; 9,900: 176;
  13,900: 0; 17,900: 30,842; 21,900: 8; 25,900: 172; 29,900 (rain deck): 39,348; 33,900 (w01a): 66,713.
- Kinds of memory that differ:
  (1) the two packet frame buffers 0x1b0xxxx / 0x1d0xxxx (up to 167 KB at 29,900; equal at 21,900 and 25,900);
  (2) chain descriptors 0x1b8b30-0x1b8c44, 0x1b8f3c-0x1b9070 (pointers into the frame buffer, 0 in c7);
  (3) packet pointers 0x1e5080 / 0x1e50a0, 0x1e6a9c-0x1e6bd4 (packet / VIF words), a linked list at 0x1e1080-0x1e10bc (33,900);
  (4) 0x1b4738-0x1b47a0: saved registers / return addresses (0x001224a4) / DMA register addresses: a stack's leftovers;
  (5) scratchpad 0x160-0x390, 0xad0-0xd2c, 0xc40-0x3fe0 (13 KB at 33,900);
  (6) one static word 0x18f364 (0x000c0000 base, 0x00090000 c7) at 29,900;
  (7) GAME memory written by game code outside the pipeline: w lanes of position quadwords (xyz equal, w = a VIF code / leftover
      float in base, 0 or another leftover in c7): 0x18589cc.. (9,900), 0x1562a0c.. (25,900). This is the "leftover" path.
- So no packet-skipping H1 can keep the gl39 hashref: kinds (1)-(6) are written by the cut passes themselves on unshown steps and are
  still in RAM at the hash points. Kind (7) needs the reader proof (is any of it acted on).

## 2026-11-23 15:20 UTC: twin rebuilt with MGS2_LITE_FN; the cut without EVM MOVES the state hash
- Twin "tb" (scratch h1/tb, mk_tb.sh): gen.o recompiled from the s19 copies (without DG_LITE_DYN it is byte-equal to build19/x86/gen.o
  [C]) with -DGEN_LITE -DDG_LITE_DYN, boot_rt.o from s39 (t39) with -DDG_LITE_DYN (without it byte-equal to tw2's boot_rt.o [C]), gl39's
  ge objects, build19's units. Run script tr.sh = twin.sh + tw_run39.sh words + runs39c.sh p1 keys.
- base (MGS2_LITE_FN unset = 0x1214a0 alone): 685/685 = hashref_gl38 (c9ddb593) [C twin].
- c7 = 1214a0,133cc0,1324f0,130390,1303a8,13d540,122138 (six-pass cut minus EVM 138ac8/138ae0): 604/685 differ [C twin]:
  ee 603 of 684 differ (equal only 4,300-8,700 and 12,000-15,500), gs (v1 renderer mask) the same 603, vfb (VFPU fallback COUNTER,
  checked by refcmp) 117, iop 0, virtual time 0 of 685, vuh (VU0 state, dense 15,701-16,010) 0 of 311.
- vb 100 (first split): 61 words differ in 5 ranges: scratchpad 0xc70-0xc78, 0x1b8f3c-0x1b8fa0 (chain descriptors), 0x1e5080,
  0x1e6b30, 0x1d001f0-0x1d00534 (frame buffer B) [C twin, RAM dumps].
Running: RAM dumps of base and c7 every 4,000 vb from 1,900 (h1/dumps, container only); the trace build tt (GEN_DG_TRACE + per-site
load/store value hashes, py/ed_tr.py) compiling.

## 2026-11-23 15:08 UTC: started
- Read: gl40_plan.md section 2, h1_notes.md, memory mgs2-h1, hd_study 7.1, gl39 state.md / report.txt, native_renderer_design.md 1.2-1.3
  (V1-V4 stub runs), speed_notes 33-34, DEVIATIONS E5.
- Found: build_gl19/ is gone; build19/x86/gen.o objcopied = gl39 twin's tw2/x86/gen_nat.o byte for byte (md5 6df94457) [C], so
  build19/x86 holds the gl19 game objects the gl39 twin links. The old trace twin sbh1 (READ_HASH=4, WRITE_HASH) is gone; t39's
  boot_x86.c has MGS2_READ_HASH=1 (rh per vblank) and MGS2_READ_LOG only.
- The state hash (hashref ee) covers ALL EE RAM + scratchpad, no mask [C boot_x86.c state_hash2]. Twin run: ~231 s.
Next: base twin with -DDG_LITE_DYN (gen.o recompiled), check 685/685 = hashref_gl38 (c9ddb593); then the cut without EVM.
