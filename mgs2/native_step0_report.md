# Native renderer step 0: container proof (2026-11-20)

The step-0 agent ran from 16:20 to about 17:50 UTC and wrote this report. It returned the text, and the thread lead saved it here. The design is in `native_renderer_design.md`; this report corrects it in section 6.

## Summary
1. **Batches identical: yes [C].**
   - Route i to vb 31,400 (gl28 settings) plus w01a to 37,800: 17,664 pictures.
   - The walk drew every 0x1214a0 group from its record through the native front end (`xl_p04_core`).
   - 0 of 1,094,606 df0 batches differ in any field, and every picture digest equals today's.
   - Reading the block from packet memory gives the same result.
   - In shadow and native modes the hashes equal hashref_g22b at all 378 checkpoints.
2. **Order provable: yes [C].**
   - All 7,183,707 walked groups sit exactly at their key, with 0 order inversions in any chain (46,473 groups in chain 6). UNKNOWN #9 is settled.
   - The key is SPR 0xc70 + 16 × SPR 0xc64. The game's write pointer [0x1e50a0] is stale at 1,153,583 of 1,424,391 calls.
3. **Builder really stubbed (on mode):**
   - The first try differed in 3,225 pictures. Lanes 3 and 4 (runs of effect prims and P07 slots) take a whole run of items in one call and skip past a group's merge point.
   - The fix stops those lanes at the next key (61,975 stops). After it, the GE list equals today's in all 17,664 pictures and all df0 batches are identical.
   - 92 rain pictures still differ, only in HUD vertex depth that nothing reads.
   - iop and k_time are equal at all 378 checkpoints. The ee hash differs at 336 because packet memory is no longer written; those differences are not classified yet.
4. **Counts [C]:**
   - Records: 1,424,391; groups: 7,218,344.
   - Overflow skips: 0. Least margin: 1,272,718 B.
   - Groups never walked: 34,637, all in 42 pictures whose buffer was never sent.
   - Snapshot bytes per picture, mean / most:

     | window | mean | most |
     |---|---|---|
     | rain | 71,624 | 90,304 |
     | rain, without block copies | 22,591 | 30,560 |
     | w01a | 39,210 | 70,240 |
     | deck before rain | 26,322 | 297,408 |
     | opening | 53,325 | 302,880 |
5. **UNKNOWN #10 [C]:**
   - Colour buffers never change in the rain or w01a.
   - On the deck before the rain there are 176 content changes in lit meshes, at most 6 a picture.
   - In the opening, 289 dynamic colour streams live in packet memory, with a one-frame lifetime.
6. **UNKNOWN #3 [C]:**
   - 0x12d910 returns the quadwords it wrote (10). Its caller uses that only as a flush threshold and copy count, so the stub must return 0.
   - 0x13bf78's return value is unused.
   - V3b's "flag" is a CLIP-flag stack slot of 0x331160 masked `& 0x3330`, so the changed bit never reaches a branch. This was checked at vb 29,400-29,403 only; the whole route is still owed.

## 0. What step 1 still needs
- The stub and record writer as main-CPU code (x86 uses host statics).
- The snapshot in shared memory, with the size or fallback rule from 6.5.
- XE_SNAP, the hold, and the key merge plus lane stop in ge/xlme.c.
- The four ME gates on the PSP toolchain. Only the x86 check ran: natp04.o's only undefined symbol is xl_p04_core plus the x86 stack protector, and it has no .data or .bss.
- Gate 1, the read-hash proof of the exact stub set. In on mode, ee differs at 336 checkpoints, from vb 4300.
- Turn arms and timings. x86 gives no speed figure.

## 1. Files
The tree is /home/claude/n0. The patch is scratchpad n0/n0.patch (842 lines), and it is container-only.

| File | Diff | What |
|---|---|---|
| ge/xlate.c, ge/xlate.h | +27 -13 | `int xl_p04_core(const XlP04*)`: five data pointers, per-slot STCYCL/UNPACK/qwc, header UNPACK and words, key, q0. `xl_lane_p04` now fills it from the packet. |
| ge/natsnap.h (new) | +61 | Shared layout, offsets only. NatRec is 32 B a call, NatGrp 32 B a group. Also the chain list, the MSCAL table, 384 B block copies and 0x4A0 B chanl copies. |
| ge/natp04.c, ge/natp04.h (new) | +81, +28 | ME-safe front end: no libc, no syscalls, no statics except constants. Rebuilds the 7 or 8 template quadwords. Range-checks every guest pointer (DMAC view) and caps counts (nA, nB ≤ 128, nC ≤ 64). Calls the core when every slot is a REF; otherwise feeds the generic walk in walk order. |
| boot/nat_hook.c (new) | +364 | `MGS2_NAT` = 1 shadow, 2 native drawing, 3 on (stub plus merge by key). Holds the record, the copies, the census, all checks and the "nat:" summary lines. |
| boot/dg_lite.h, boot/gen_pre.h | +3, +5 | 0x1214a0 hook through GEN_LITE_FN / FN_ENTER. |
| boot/dispatch.h | +3 | chanl copy at 0x121060's return. |
| boot/boot_x86.c | +12 | nat_begin / nat_end around the pipeline call. |
| runtime/ee_io.h | +21 | nat_walk at every packet-buffer tag; `NAT_RUN_STOP` in lane_fx and lane_p07. |
| boot/xlive_chk.c | +60 | Per-picture lines natb, natz, natq; debug dumps behind MGS2_NAT_BDUMP / MGS2_NAT_XDUMP. |

## 2. What the record computes [C gen_elf.h 29115-29480, mesh_packets 3/3.1]
- Groups: the builder's loop in C over the group descriptors.
- Stream cursors advance even for a stream that is not sent. A stream is a REF if pointer and count are non-zero, else CNT 0.
- n = desc +0x0E/+0x0F/+0x10; cnt = +0x0C; material = +0x14.
- VU1 entry index:
  - idx = 2(d0&1) + clip.
  - Add 4 if pre-lit.
  - Then add 2 if d0&4, or else add 2 if (d0&2) and clip.
  - MSCAL = (int16)SPR[0xb9c + 2·idx] | 0x14000000.
- The full template goes on the first two groups of a call.
- Tests in the builder's order: skip if `(int32)end < dest + qwc`, then flush if `cap < qwc + m`.
- key = dest + 16·qwc, taken after the flush test.

## 3. Checks: shadow and native runs n1, n2, n2r [C]

| | opening | deck before rain | rain | w01a |
|---|---|---|---|---|
| pictures | 11,086 | 2,516 | 1,011 | 3,074 |
| records/picture, mean (max) | 89.9 (560) | 45.2 (553) | 127.7 (158) | 60.2 (110) |
| groups/picture, mean (max) | 449.6 (2,572) | 178.3 (2,079) | 539.3 (762) | 403.5 (726) |
| df0 batches | 27,298 | 39,566 | 217,936 | 809,806 |

**(a) Batch identity**
- Template words against the packet: 0 of 7,183,707 groups differ.
- Block copy against packet memory: 0 of 1,416,788 differ.
- Front end: 7,182,317 groups went to the core and 1,390 were fed word by word; 0 bad.
- Pass constants equal a chanl copy in all 84,187 pictures that have both.
- n0 equals the unmodified build, so the factoring is exact.

**(b) Order**
- 0 inversions overall and in every chain. No key ever falls in a chain other than its call's.
- Groups by chain, with the most other items walked between two of its groups:

  | chain | groups | items between |
  |---|---|---|
  | 2 | 1,067,042 | 8,448 |
  | 3 | 3,868,296 | 68,762 |
  | 4 | 2,117,538 | 22,275 |
  | 5 | 34,966 | 11,559 |
  | 6 | 46,473 | 17,831 |
  | 7 | 36,704 | 12,433 |
  | 8 | 12,688 | 10,576 |

- Opaque caller (pass 6): 1,335,740 records across chains 2, 3 and 4 (144,254 / 698,927 / 492,559).

**(c) Overflow:** 0 skips; least margin 1,272,718 B; 122,263 flushes before a group.

## 4. On mode (runs n3, n4, z0, z3)
- At each top-level tag, groups with a key at or below the tag's address are emitted first, in record order. Every group of a call has the call's entry key.
- **n3, no lane stop:** 3,225 pictures differed. In a dump at vb 24,007, three sorted-mesh batches moved from list position 222 to 401 and picked up texture 103 instead of 97. The lane stop fixed it; `MGS2_NAT_STOP=0` turns it off.
- **n4 / z3, with the stop:** the list digest is equal in all 17,664 pictures, and df0 batches and other batch records are identical.
- **92 rain pictures (vb 30,854-31,082) still differ** in frame and content digests:
  - The only difference is the Z of through vertices in the last 9-17 batches. These are HUD sprites with depth test ALWAYS and depth write on.
  - No batch after them tests depth, and every GE list starts with a depth clear (ge/gelist.c 574).
  - A digest that masks exactly that depth (natq) is equal in all 17,664 pictures [C].
  - Source [I]: the game takes those Z values from memory it does not write that frame. They vary sprite to sprite (65,535 / 13,182) and change with the stub. The address is not traced.
- **n1b** (shadow, final build) equals n0.

## 5. UNKNOWNs
- **#9:** settled (section 3).
- **#10:** same-pointer content changes of stream B, by window:

  | window | changes | pre-lit | most a picture | pointer changes |
  |---|---|---|---|---|
  | opening | 92 | 0 | 7 | 4,463 |
  | deck before rain | 176 | 0 | 6 | 712 |
  | rain | 0 | - | - | 0 |
  | w01a | 0 | - | - | 0 |

  For pre-lit meshes, B = mesh+0xF0 in 1,117,409 of 1,117,698 records. The other 289 are the packet-memory streams (mesh+0xD0 = 0x20, opening only). Stream A changed 5,039 times, C 3 times.
- **#3, 0x12d910:**
  - Single exit at 0x12dbb8 (gen_elf.h 39336), returning (r12 - r13) >> 4.
  - Its caller 0x121c30 first runs the skip test end < dest + qwc (0x121d5c-74, independent of v0). It then runs the flush test cap (SPR 0xc68) < qwc + v0, then copies v0 quadwords. So the stub returns 0.
  - The skip test runs after the builder, so effect records (step 3) must apply it, against the virtual position.
- **#3, 0x13bf78:**
  - v0 is unused at its caller 0x13d540.
  - Slot 0x1b4818 (sp 0x1b47c0 + 88) of 0x331160 gets cfc2 VI18 after three VCLIPs (0x3318cc-e8), and is masked & 0x3330 (0x3318f4).
  - Bit 22 (0x401041 against 0x1041) is the 4th-oldest judgment, which the mask drops.
  - That it is the sea builder's VU0 leftover is [I].

## 6. Corrections to the design
1. Design 6.1: the converter is in xlate.c, not xconv.c.
2. Design 1.6: the key is the scratchpad packet control, not [0x1e50a0].
3. Design 6.1: merge by key in all chains, not "at chain 3's start". Opaque records span chains 2-4, with 99,408 other items between their groups.
4. New: the PSP walk (xlme.c) needs the same lane stop.
5. Design 1.5: the record is 32 B a call plus 32 B a group (rain mean 22.6 KB without blocks, 71.6 KB with them; at most 303 KB at loads). Two 128 KB buffers overflow at scene loads, so choose a per-call fallback when the snapshot is full, or expand groups on the ME.
6. Design 6.1: the overflow test runs against a virtual packet position.
7. Design 1.4: stub return values as in section 5.
8. Design 1.5: the block copy was optional on x86 (n2r gave the same pictures). Packet-memory colour streams need the same copy or hold as the block.
9. Lane 1 stays for other builders' P04 groups (915,578 in the opening).
10. Drop an unsent buffer's snapshot when the buffer is reused.
11. Design 3.2: a frame compare in on mode must mask never-read HUD depth, or the source of that depth must be found.

## Scratch
- Scratchpad n0/ (352 MB): logs out_<run>.log, binaries b0/bn/bo/bp/bq, build.sh, run.sh, cmp.py, hcmp.py. RAM dumps were deleted.
- Nothing was edited in main, t28, t29 or /mnt/project-files.
