# Native GE renderer, step 2 (people native): results (container + emulator only)

2026-11-24, design agent. Tree: scratch `st2/t` = gl40's tree + step 1 (`st1g/t`) + step 2. The diff on top of step 1 is
`attic/ge_native/step2/t40_step2.diff`. No PSP package was built for delivery. The emulator EBOOTs are for the container only.
Every twin and emulator job ran at nice 19, one heavy job at a time.

Marks: [C] read from code or data; [C twin] counted on the x86 twin; [C emu] counted on the emulator (PPSSPP 1.20.4 headless);
[C model] computed; [I] inferred, with its basis; UNKNOWN.

## 0. Summary

| Question | Answer |
|---|---|
| People native on the twin? | **Yes.** Through the PSP's walk with REC copies and the check mode, ship + people native (KB2 384): 1,258,644 people groups were converted from cooks and compared with today's C2, 0 differences. 1,658,399 ship groups also had 0 differences. Content and drawn2 equal the C2-on reference (c1) in **17,664/17,664** pictures [C twin, g384]. |
| On the emulator with the picture loop apart? | **Yes.** The loop ran as its own thread (16 KB stack); the logs have 0 "me picture loop: no thread" lines. e2 (ship + people native) matches the twin hashref on 62/62 hash lines. Its content digest equals e0's (nothing native, C2 on) in **3,522/3,522** pictures from vb 24,000 to 31,400: 1,762 thread turns and 1,760 main-CPU turns [C emu]. |
| Step 1's emulator results | They were taken with the loop on the main CPU: step 1's logs (st1g/emu_g1np) carry "me picture loop: no thread". The step-2 gate was rerun with the loop apart (above). |
| The stale read gl41 found | It is visible here too. 125,715 of 257,483 native ship batches had a stub-time WORLD that differs from the converter's when the loop runs apart [C emu e2]. The twin shows 0 [C twin g384]. Nothing reads the stub's transform any more (only that counter), so the pictures are right. |
| People's cooked memory | 505 people meshes are cooked once each over the route: 1.36 MB in all, re-cooked when changed 0 [C twin g384]. Without the carve (c3: people native from vb 0, 2 x 256 KB), there were 17,901 cooks (16,409 re-cooks, nearly all in the opening, where 45 meshes alternate between 2 base sets): 36.7 MB, with 2,020 no-room. |
| Arena | Two regions of 256 KB until vb 24,000, then **384 KB** each, plus the people's 76 KB beside each; the growth happens at vb 24,007 on both the twin and the emulator. Ship no-room 0, people no-room 3, evicted 1,145, re-cooked after eviction 67 [C twin g384]. Frame data room after the carve is 972 KB, against a worst picture of 939 KB (nothing native, C2 on); 33 KB to spare [C twin kc]. |
| K-N2R / digit 2 | Done, each with its twin hashref. hashref_kr = hashref_kc byte for byte, so the bound kinds are picture-only. The emulator's ek matches it on 62/62 lines, and its content equals the twin's in 3,521/3,522 pictures. The one difference, vb 26,007, is a twin/emulator difference in every run, e0 included. |
| V3a | **Leftovers only, in every vblank sampled** [C twin]. 27 vblanks were logged read by read: one in each of the 22 clusters from vb 31,400, plus 5 more; the deck's 2 vblanks are step 0's. Of 2,524,877 paired reads (0 address differences), 1,539 see other bytes: 1,537 scratchpad w lanes (z and w in one block) and 1 stack byte read twice, with no later read affected. Read counts are equal in all 13,799 vblanks from 24,000. |

## 1. What was built

| Part | Where | What it does |
|---|---|---|
| People's records | `boot/np6_stub.h`, `ge/natp06.h` | 0x12e8b8 (the people's packet builder) and 0x12e790 (its template set-up) are wrapped. The builder's loop runs in C: one 72 B `NatP6` record per group (key, constant block, 3 materials, 6 streams, header, MSCAL). The material memo is kept by the stub and cleared by the template set-up's wrapper. Arms 0-4 work as the ship's do. `MGS2_NP6=<arm>`, else `MGS2_NGE_CYCLE` digit 2 selects arm 3. |
| C2 cooked per model | `boot/nge_stub.h` (nge6_rec), `nge/` | Each people mesh (key mesh\|1) is cooked once with today's C2 vertex code (`vc_p06_c2v`, a2 0xff, no ADC) into the arena. The depth range is taken from the cook (`vc_p06_c2z`). On reuse, a content hash over streams A-D (n6_csum) is checked, contiguous from each cursor for 16·Σnq bytes. |
| Per-frame pass, blended vertices | `ge/xconv.c`, `ge/vu1conv.c` | N-ref: the GE reads the arena (`GERL_NGE` + `NgeC2Tail`). N-copy: the cooked vertices are copied, then the blended d (`vc_p06_c2bl`) and alpha are redone on the ME each frame. |
| Check mode | `MGS2_NGE_CHK=1` | Every native people group is also converted today's way and compared: vertices, strips, depth range, alpha. |
| REC copies (gl41's rule) | `ge/xme.c` xme_rec, `ge/xme.h`, `ge/xlme.*`, `XME_NATBYTES` | The ME's walk never reads the main CPU's arrays later. At REC, the record's copy holds the ship's NatGrps + their NgeRefs, then the people's NatP6 + their refs, each part 16 B aligned. The walk reads `XmPic.nat_r / np6_r`. A walk from RAM, where the main CPU waits, reads the main arrays. The ship's WORLD and projection come from the converter's own transform (`VcXform x = Dx`). |
| Carve after the opening | `boot/nge_stub.h` (nge_grow, NgeReg.lo), `psp/xl_psp.c`, `boot/xlive_chk.c` | Regions are allocated at KB2 from the start, but only the top KB1 (256 KB) is used until the growth. Frames begun at or after vb 24,000 get the smaller arena (begin bit 24 -> `fm_arena2`). The regions grow whole, and the people's two 76 KB blocks are placed right below them, once the guard passes. |
| Growth guard (PSP) | `psp/xl_psp.c` | Growth needs 3 GE finishes after the first smaller-arena frame, and the GE must have finished a list of a frame begun with the smaller arena (`gl_show`: lists go to the GE one at a time, in order). Then no list of a larger-arena frame is still waiting in the ME's results. |
| People's start | `nge_p6_vb()` | The people turn native only from growth vb + `MGS2_NGE_P6LAG` (default 100). The twin and the PSP then stop writing the people's packets at the same vblank, whatever the GE's timing. A growth later than that is counted (`N6_LATE`, "growth late"). |
| K-N2R | `ge/bound.h` kind 9 (`BD_NATIVE` 256, main-CPU side) | Keeps only chains 2-4 (`BD_CHI_NAT0..1`) and leaves every other transfer out before it is queued. The begin word carries `bd & 0xff`, so the ME sees kind 0. |
| `MGS2_NGE_CYCLE` digit 2 | `boot/nge_stub.h`, `boot/np6_stub.h` | Ship + people native in the turn. |
| PSP content digest | `ge/gelive.c`, `ge/xlme.*`, `ge/xme.*`, `psp/xl_psp.c` | gl41's "picture: vb N content H" line, mirrored: `gl_digest_c` on the loop's frame. |
| Heap fix (a), emulator build only | `ge/texcache.c` | gl41's TK_CAP0: the texture table is 128 KB from the start. |
| me_pic stack 16 KB | `psp/xl_psp.c` | As gl41 sets it, so the picture loop runs as its own thread on the emulator. |

## 2. Twin checks (vb 0-37,800, refcmp against the hashref named)

All runs use C2 on in every turn (`MGS2_OPT2=fffff9ff`), `MGS2_GX_SEM=2`, and go to vb 37,800.

| Run | What | refcmp | Pictures (17,664) | Check mode | Notes |
|---|---|---|---|---|---|
| a1 | people arm 1 (builder + compare) | gl38: 0 differ | = gl38's run | 2,163,292 records = their packets, word for word | the most groups in a picture: 494 before vb 24,000, 893 after; cap 1,024 |
| a2 | arm 2 (records drawn, packets stepped over) | gl38: 0 differ | all 5 digests = a1 | | |
| a3 | arm 3 (stubbed) | gl38: 623 differ from vb 6,300 (packets not written, as designed) | every digest = a1 | | own hashref (hashref_a3) |
| c3 | ship + people native from vb 0 (`MGS2_NGE=2`) | a3: 0 differ | content + drawn2 = c1 | 0 differences (people 1,306,866, ship 1,658,399) | 33 vertex/depth differences in the first version: one mesh's normals were rewritten in place; fixed by the content hash at reuse |
| c4 | c3 through the PSP's walk (`MGS2_NAT_LW=2`, REC copies) | a3: 0 differ | all 5 digests = c3 | | 2,163,292 people groups planned and copied |
| g416 | carve after the opening, 416 KB | | content + drawn2 = c1 and c3 | 0 differences | grown at vb 24,007; ship no-room 0, people no-room 0, evicted 1,027 |
| g352 | the same at 352 KB | g416: only vb 24,100 differs (people start 24,007 vs 24,100) | | 0 differences | ship no-room **4,297**, re-cooked after eviction 1,403: too small |
| g384 | the same at 384 KB (**the default now**) | g352: 0 differ (hashref identical) | content + drawn2 = c1 | 0 differences (people 1,258,644, ship 1,658,399) | ship no-room 0, people 3 (drawn today's way from their records), evicted 1,145, re-cooked after eviction 67 |
| kc | the turn words (section 4), 416 KB | gl38: equal until vb 24,200 (the first digit 2), then 134 differ | | | its own hashref, hashref_kc |
| kr | kc + `MGS2_BOUND_CYCLE` (K-N2R, K7) | kc: **0 differ**; hashref_kr = hashref_kc | K-N1/K-N2 = kc; K-A = kc but turn-edge pictures (section 4) | | |

"nat lw chains" (g416) [C twin] counts the native groups by the chain that carries them.
- **People:** chain 2: 1,352,578; chain 4: 16,380 (all of them in chains 2-4).
- **Ship:** chain 2: 31,211; 3: 2,110,541; 4: 69,597; 5: 15,433; 6: 2,527; 7: 101.

So K-N2R (chains 2-4) draws every people group and 99.2% of the ship's groups.

## 3. Memory

| Item | Size | Mark |
|---|---|---|
| People's record | 72 B (`NatP6`); 1,024 a packet buffer (most in a picture 893) | C |
| People's block per packet buffer | 72 KB array + 4 KB refs = 76 KB, one below each region (fm0, fm1) | C |
| Cooked regions | 2 x 256 KB until vb 24,000, then 2 x 384 KB (`NGE_KB2_P6`; `MGS2_NGE_KB2`) | C |
| Carve per frame memory after the opening | 384 + 76 = 460 KB: arena 1,236 KB, frame data room 1,236 - 264 = 972 KB | C |
| People's cooked bytes over the route | 1,426,816 B, 505 meshes, re-cooked when changed 0, no-room 3 | C twin g384 |
| Arena use after growth | 756 KB of 768 at the end (LRU: full); evicted 1,145 | C twin g384 |
| Emulator e2 to vb 31,400 | ship: cooked 765, evicted 872, re-cooked after eviction 37, no room 0; people: cooked 255, no room 3. The twin to vb 31,400 gives 37 re-cooks too; cooks and evictions were not compared line for line | C emu |
| Content hash at reuse (the people) | 132 KB of stream bytes per picture with people (741 MB / 5,468) | C twin |
| Partition 2 after step 1's tables | largest block 33 KB, total 57 KB (me_pic's 16 KB stack fits) | C emu |

**Frame data per stretch, max (KB) [C twin]**

| Run | deck | rain | after 31,401 |
|---|---|---|---|
| c1 (ship native, people C2 today's) | 541 | 547 | 880 |
| g384 (ship + people native) | 424 | 429 | 694 |
| kc, nothing native (K-A / K7 turns) | 589 | (in deck/rain) | 610 in the cycle; **939** past it (vb 36,755-36,907) |

**Carve size.** Each region's size was tried on the twin with ship + people native over the whole route [C twin]:

| Region | Ship no-room | People no-room | Re-cooked after eviction | Data room after carve | Spare at the 939 KB picture |
|---|---|---|---|---|---|
| 352 KB | 4,297 | 3 | 1,403 | 1,004 KB | 65 KB |
| 384 KB (chosen) | 0 | 3 | 67 | 972 KB | 33 KB |
| 416 KB | 0 | 0 | 0 | 940 KB | 1 KB |

At 416 KB a nothing-native picture past the cycle would fit with 1 KB to spare. 384 KB is the default.

**Pictures with drops: 0** in every run. The emulator logs "lists: 0 that did not fit" and dropped 0 in e0, e2 and ek [C emu].

## 4. Turn words and their twin hashrefs

**Proposed words for the step-2 checkpoint run (twin-checked).** From `MGS2_OPT_CYCLE=24000:100`, 25 blocks of 5 turns (125 turns, to vb 36,500):
- `MGS2_NGE_CYCLE=01220` x25
- `MGS2_BOUND_CYCLE=00097` x25

Each block runs these turns in order:

| Turn | Ship | People | Drawn |
|---|---|---|---|
| K-A | today's | builder | all |
| K-N1 | native | builder | all |
| K-N2 | native | native | all |
| K-N2R | native | native | chains 2-4 only |
| K7 | today's | builder | nothing |

C2 is on in every turn. The first block starts with K-A, so the first digit 2 comes at vb 24,200, after the growth.

**Twin hashrefs** (in `step2/cfg/`):
- `hashref_kc.txt` is for the turn words, with or without the bound cycle. kc and kr give identical hashrefs [C twin].
- `hashref_g384.txt` is for `MGS2_NGE=2` over the whole run, with the people starting at vb 24,100. It does not depend on KB2: g352 and g384 give identical hashrefs [C twin].

**What K-N2R and K7 leave behind.** K-N2R and K7 leave texture uploads out with their chains. A K-A picture after them can show a texture the host missed. In kr (twin) this gives 22 K-A pictures whose content differs from kc:
- 20 are pictures that straddle a turn edge, at vb 24,500, 25,000 and so on. Each is the last picture of a K7 turn, made at its last
  hook but counted here by the vblank where it ends;
- 2 are true K-A pictures well inside a turn: vb 29,072 and 31,663.

The emulator (ek against e0) shows 12 such K-A pictures, and ek equals the twin's kr in all of them [C emu]. This is the gl38 kinds' known behaviour, not a step-2 change [I: the chains' texture uploads are left out with them].

**Past the cycle.** After the last digit, the digit is `MGS2_NGE`'s value. A run past vb 36,500 should set `MGS2_NGE=1` or `2` there. Otherwise the 939 KB nothing-native pictures at vb 36,755-36,907 have only 33 KB to spare.

## 5. Emulator gate (picture loop apart)

Package: `st2/pk`, EBOOT md5 4ba692a6. It is emulator-only: gl40's PGO objects, with the 9 changed objects compiled again. It is linked like step 1's np build, without ME_PRE and END_PAD; its image is 28.5 KB above step 1's np build. It carries gl41's heap fix (a) (the texture table at 128 KB) and gl41's content line, both mirrored here. xme_rest_chk, mesec_chk and me_call_chk all PASS.

Config: gl40's file with C2 on, ending at vb 31,400.

| Run | Config | refcmp | Loop | Content vs e0 (vb 24,000-31,400) | Shots | Notes |
|---|---|---|---|---|---|---|
| e0 | `MGS2_NGE=0` | gl38 62/62 | thread, 0 "no thread" | (reference) | | textures refused 343 (the same in all three: render targets) |
| e2 | `MGS2_NGE=2`, KB2 384 | hashref_g384 **62/62** | thread, 0 "no thread" | **3,522/3,522 equal** (thread turns 1,762, main-CPU turns 1,760) | 51/52 byte-equal; shot_24784 differs by 429 px, max 4 steps, with an equal content digest. Step 1 found the same shot differing between gl40's own EBOOT and its off build: emulator noise | grown at vb 24,007 (guard: a frame begun at 24,001 drawn); people from 24,100; late growth 0; stub-time WORLD differs 125,715/257,483 (counted only) |
| ek | turn words + bound cycle | hashref_kc **62/62** | thread, 0 "no thread" | K-N1 750/750, K-N2 750/750 equal; K-A 695/707; K-N2R and K7 differ by design | | content = the twin's kr in 3,521/3,522; the one difference, vb 26,007, is the same in every emulator run and every twin run (e0 and c1 too) |

List and frame digests differ from e0 by design in native pictures, because of the CALLs. The content digest is gl41's `gl_digest_c`, which hashes a native batch as today's batch.

## 6. V3a read proof

**Runs.** The read-hash twin (`st0/xt/boot`, gl28's game settings), base vs V3a (`MGS2_LITE_ALL=0 MGS2_LITE_FN=1214a0,12e8b8`: both builders stubbed every step, the strictest case). The step-2 stubs write less than the game's code, and V3a writes nothing at all. All marks are [C twin].

**Whole route.** The read hash differs in these vblanks:

| Stretch | Vblanks that differ |
|---|---|
| Opening, vb 4,300-23,999 | 3,785 |
| Deck, from vb 24,000 | 2 (25,944 and 25,974: step 0's LEFTOVER R5) |
| Rain | 0 |
| w01a and on, from vb 31,400 | 1,756 |

The read count is equal in all 37,799 vblanks, so the control flow is the same. The state hash differs from vb 4,300, as designed: the packets are not written. The people are native only from vb 24,100 (growth + 100), so the opening is outside step 2; its differences were not classified.

**Read logs** (`MGS2_READ_LOG`, reads paired by vblank and index; `tools/dev/rl_cmps.py`):
- rl2: 5 vblanks (31,660, 31,661, 33,701, 36,501, 37,201): 780,298 reads, 337 see other bytes.
- rl3: 22 vblanks, one in the middle of each of the 22 clusters from vb 31,400: 1,744,579 reads, 1,202 see other bytes.
- The deck's 2 vblanks (25,944 and 25,974) are step 0's.
- Address differs in 0 reads in both.

| Reads that differ | Where | Lanes | Reader / last writer (same in both runs) | Verdict |
|---|---|---|---|---|
| 1,071 | scratchpad 0x70003e00-0x70003eff | z, w (1,008); w (63) | S_w01a_P3aaef8 / P3aaef8 (x and y equal) | leftover |
| 402 | scratchpad 0x70003900-0x70003dff | w | S_w01a_P2e8200 / P2e8200 | leftover |
| 64 | scratchpad 0x70003f88 | w | GLDU / S_w01a_P2e3bf0 | leftover |
| 2 | main RAM 0x001b4720 (a stack byte below P121c30's sp 0x1b47a0, re/pipeline_hot.md) | one byte | P175b18; last writer P1214a0 (base) / P135120 (V3a) | leftover (see below) |

In every scratchpad row, the bytes that differ are only in the w lane, or z and w for P3aaef8's block, whose x and y match. Base's builder left packet bytes in those scratchpad blocks. The stage's vector code carries them in the lane from one read to the next write, but no x or y read ever differs and the read counts are equal. So they never reach a value the game uses [C twin, the read logs; that the lane is unused is I].

The stack byte occurs in 2 vblanks (36,617 and one of 36,553 / 36,719), and in both it is the only read that differs. The next vblank's read hash is equal again: 36,618, 36,554 and 36,720 are all equal. So the value P175b18 reads there reaches no later read.

That byte comes from the ship's builder (0x1214a0, step 1's stub), not from the people's. On hardware, K-N1/K-N2 leave the same stack leftover [I: the native stubs are host code and write no guest stack].

**Verdict for step 2: V3a holds on the sampled vblanks.** Every differing read is a leftover with no later effect, in all 24 clusters from vb 24,000 (22 here, 2 in step 0). Read counts are equal over the whole route. Vblanks between the samples were not logged read by read; within a cluster the pattern repeats every other vblank [I].

## 7. Open items and flags

1. **C2 is off by default** (`OPT2_DEFAULT`). Native people need C2: digit 2 is a no-op without `OPT2_C2`. The checkpoint run needs C2 on in every turn, including K-A, so that its turns compare like with like. That changes the people's look against today's default (C2 vs today's people path). The lead decides.
2. **The content hash at reuse costs the main CPU.** It hashes 132 KB a picture with people [C twin]. The time cost is about 0.5-1.5 ms a picture [I: 135 KB of uncached reads at 100-250 MB/s]. It is the price of catching in-place rewrites; c3's 33 differences were one such rewrite. A cheaper check is possible (hash a sample of lines and the pointers) but untested.
3. **The eviction guard is the step-1 rule:** a mesh is kept for 8 pipeline calls and 3 GE finishes. The growth guard here is stronger: it waits until the GE has drawn a frame begun after the event. The same frame-vb rule would make evictions safe however far the ME lags [I]. 0 wrong pictures on the emulator with the loop apart (the content gate above).
4. **Structure growth the gl41 merge must carry.**
   - `XmeCtx` gains `fm_arena2` and `np6_g[2]`; `XmeSh` gains `np6_b` and `np6_used`; `XmeRes` gains `dig_c` (as gl41). The ME code and the main CPU share these headers.
   - `MpRec`/`XmeRec` people counts are int16 (at most 1,024 a buffer).
5. **The image grew by 28.5 KB of text** over step 1, so newlib's heap matters (step 1 section 6). Here the heap was fixed with gl41's (a) and without the NOBITS pads. gl41's own heap fixes (a)-(c) replace that.
6. **Overlap with gl41.** The ship's refs copied at REC with WORLD from Dx, the content digest line and TK_CAP0 are mirrored here from gl41's rule or diff. When step 2 goes onto gl41's tree, take gl41's versions and drop these hunks: `ge/texcache.c`; the `gl_digest_c` guard in `ge/gelive.c`; `dig_c` in `ge/xlme.*`, `ge/xme.*` and `psp/xl_psp.c`; the ship-ref part of `xme_rec`; and `VcXform x = Dx` in `ge/xconv.c`.
7. **K-N2R measures the main CPU with the ME still converting the native groups.** A true "ME rests" turn needs a main-CPU list writer (step 1 section 4.2). That is not built.

## 8. What the step-2 checkpoint build needs

1. **Tree.** gl41's final tree + `step2/t40_step2.diff`, minus the gl41-overlap hunks (section 7, item 6). The diff applies to step 1's tree and reproduces st2/t exactly [C]. It touches 21 files, 2 of them new (`ge/natp06.h`, `boot/np6_stub.h`).
2. **gen.o.** P12e8b8 and P12e790 are weakened with `_orig` added, as P1214a0 is. pspb.sh first checks each one's references: 1 call + 1 table word, and 2 calls + 1 table word.
3. **Config.**
   - C2 on in every OPT2 word.
   - `MGS2_NGE_KB=256`; `MGS2_NGE_KB2=384` (the default).
   - The turn words of section 4, with hashref_kc.
   - `MGS2_NGE` set for the vblanks past the cycle (section 4).
   - `MGS2_ME_PIC=4` with the loop on its own thread on the emulator; the ME on hardware.
4. **Gates before shipping it.**
   - Twin: check mode 0 differences; the hashref of the exact words.
   - Emulator with the loop apart:
     - 0 "no thread" lines;
     - 62/62 against the hashref;
     - content = the twin's in every K-N1/K-N2/K-N2R picture;
     - `np6 turn` lines show "growth late 0";
     - "picture: step 2: ... grown" before vb 24,200;
     - gl41's heap gate.
5. **PGO and order.** The people stub, nge6_rec and the N-copy pass are new hot code; train them in (design section 4.4).
6. **To read on hardware:**
   - the `np6 turn` and `nge turn` lines (no room, evictions, growth late);
   - the "grown" line's vblank (the GE's timing);
   - K-N2R - K-A and K-N2 - K-A per stretch (step 1 section 4.1).
