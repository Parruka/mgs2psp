# The picture on the Media Engine: the hand-off (design)

2026-11-02, task #142, the roadmap's Next 1, after the probe (`me_probe_notes.md`) and the sizing study (`me_picture_study.md`). Design and x86 runs only: no PSP build was made for it. Updated 2026-11-03 with build order step 1 (task #147: the split in the x86 twin, `me_step1_notes.md`) in the answer, 4.1, 4.3, 4.5, 4.6, 4.8, 4.10, 5 and 6, and with step 2 (task #149: the picture code made ME-safe, `me_step2_notes.md`) in the answer, 4.8, 4.9, 6 and 7. [C] = measured or read in our code, logs and x86 runs; [S] = a public source, cited; [I] = my inference.

## The answer

- **What the ME has to see:** each transfer's bytes as they were when the game started it. On x86 a picture built two steps late from a copy of those bytes equals today's in every frame of the three routes, and every state hash equals today's. Without the data the packets point to it does not, and without the texture sources the game rewrites it does not either (section 1) [C].
- **When the game changes them** (sections 2 and 3) [C]:
  - In the transfer's own step, little: 12 game functions over the three routes (7 on all three, 5 of the indoor stage's own) write about 0.9 KB a frame into bytes in flight, nearly always the values already there (the deck: 10.4 MB written over the route, 42 KB changed). Ten more do it once each in the boot's first 34 vblanks.
  - In the next step, a lot: the game rebuilds its packets in the same buffers and rewrites part of the data they point to, 82-128 KB written a frame (18-29 KB of it changed).
  - The texture sources it rewrites all lie in two address ranges, the game's working data (0x12c0000-0x142ffff) and the codec's text page (0x1ed0000-0x1edffff), which hold 84-117 KB of a frame's 2.0-2.6 MB of texture sources. After the boot no byte of the others changed while in flight on any route, counted by 16-byte piece.
- **The design (section 4):**
  1. At each transfer the main CPU's own walk, which stays, publishes the transfer's start and the texture sources it found.
  2. The ME walks the transfer's DMA chain itself and copies what it reads into its own memory in that same step: the packets, the data they point to, and the texture sources in the two ranges. The next step does not begin before the copy is done.
  3. The 12 functions that write into bytes in flight in that same step get a guard: a store that would change bytes first puts the old bytes in a log, which the ME lays over its copy (kept per transfer, and only for the lines the copy holds: 4.3). They make 155-213 stores a vblank, so the guards cost the main CPU close to nothing [C for the count].
  4. The ME builds the picture from the copy and the other texture sources (read from RAM; no code writes them while in flight) during the frame's two steps; the main CPU starts the GE on the finished list.
  5. The boot's frames stay on the main CPU; the ME takes over at vb 60.
  6. The main CPU takes the picture back at a frame's end and carries on with the picture's state the ME leaves in main RAM (exact on x86); only a fault on the ME makes it start cold, which the picture shows until the game sends again what it keeps on the GS (4.6).
- **Checked on x86:** that copy, two steps late, gives every state hash and every frame of the three routes equal to today's (11,766, 17,664 and 18,931 frames; section 1) [C]. **Build order step 1 (2026-11-03, `me_step1_notes.md`):** the split itself in the x86 twin (the copy taken at its deadline with the guards' log laid over it, the Stage B lanes in the ME's walk, the picture two steps late, a 2 MB arena, the take-back) gives every state hash and every frame equal to today's too [C]. **Step 2 (2026-11-03, `me_step2_notes.md`):** the picture code made ME-safe (its own heap, memcpy family and floorf/ceilf, every call through a pointer into the core's own address range, no jump tables, no printing) is exact the same way, in the split too, and so is its PSP build on the emulator; the ME's unit passes its check (257 functions, 292,076 bytes; the deepest stack 4,952 bytes) [C].
- **The open point is the copy's memory and speed.** It holds 1,192 / 1,076 / 955 KB (deck / indoor / codec routes) in an average frame and up to 3,907 KB, in the part of the game all three routes share [C]; main RAM has about 340 KB to spare. The ME's own memory (eDRAM: 2 or 4 MB, the sources differ) suits it, and the copy's speed into it decides how often the main CPU waits at 25 fps. Whether today's ME start already reaches all of it, or it takes new register writes, is for a probe on your PSP to tell; any new write needs your OK (sections 4.9 and 7).
- **Cheaper in memory, dearer for the main CPU (section 4.11):** guarding the next step's writers instead of copying would cost the main CPU 0.6-1.3 ms a step for the texture-source writers alone and 1.1-2.4 ms for all of them [I], with guard lists of 25 to 135 functions that every new area of the game adds to. I do not recommend it.
- **Cost of the design [I]:** the ME copies for 6.7-8.4 or 10.7-13.4 ms a frame on average (at the probe's 146 and 91 MB/s); the main CPU spends about 0.6 ms a frame sharing the RAM with that copy, close to nothing on the guards, and waits at a step's start only when a frame's copy outlasts the step.
- **For you now:** one decision, on a card: may step 3's probe test bit 3 of the ME's 0xBC100050 (turn B) besides today's writes, with turn C left for later (section 7)? Nothing changes in gl16 or its run.

## 1. What the ME has to see (x86 runs)

The late-run harness (`/home/claude/me11`, container only; `boot/xlive_chk.c` with `MGS2_XLIVE_DEFER=2`, `MGS2_XLIVE_SNAP=2`, `MGS2_XLIVE_SNAPK`, `MGS2_XLIVE_SNAPU`, and for the design's copy `MGS2_XLIVE_SNAPP`, `MGS2_XLIVE_SNAPR`) builds the picture two steps late, as a second CPU finishing at the end of the frame's second step would. At each transfer it copies the bytes the transfer reads (found by a walk of its DMA chain and marked by kind); when the late walk runs, it lays only the chosen part of the copy over the game's RAM, and everything else comes from RAM as it is then. Two steps late is the latest the design allows.

| route | copy laid over RAM | state hashes | frames differing from today's | of them with a different GE list |
|---|---|---|---|---|
| deck (to vb 25,200, 11,766 frames) | everything the transfers read | 252 / 252 equal | **0** | 0 |
| deck | **the design's copy** (every kind; of the texture sources those in the two ranges) | 252 / 252 equal | **0** | 0 |
| deck | the same with the first census's list (13 regions of 64 KB, section 2) | equal | 1 (vb 24,589) | 0 |
| deck | all but the texture sources | equal | 2,114 | 249 |
| deck | the packets only (tags and the data after them) | equal | 3,055 | 2,963 |
| deck | nothing (sizing study, `out_d2n.log`) | equal | 5,823 | 3,187 |
| indoor route (to vb 37,800, 17,664 frames) | everything | 378 / 378 equal | **0** | 0 |
| indoor route | **the design's copy** | 378 / 378 equal | **0** | 0 |
| indoor route | all but the texture sources | equal | 3,774 | 326 |
| indoor route | the packets only | equal | 8,880 | 8,234 |
| codec route (to vb 40,000, 18,931 frames) | everything | 400 / 400 equal | **0** | 0 |
| codec route | **the design's copy** | 400 / 400 equal | **0** | 0 |
| codec route | all but the texture sources | equal | 2,192 | 249 |
| codec route | the packets only | equal | 10,152 | 9,506 |

[C: me11 `xd/out_{d,i,c}{all,rg,noup,tin}.log` and `xdbg/out_h{d,i,c}.log` against sb9's `x1/out_{d,i,c}2on.log`; `tools/picdiff3.py`]

- **A copy of what each transfer reads is enough.** With it every frame and every GE list equals today's, two steps late. The sizing study's "texture refill" frames (1,275 on the deck) do not appear: they came from that harness's stale shadow memory, not from the late picture [C: the same frames are equal here].
- **The data the packets point to (REF data) must be the bytes as sent.** Read late it changes most frames' GE lists [C].
- **So must the texture sources the game rewrites.** Read late they differ in the codec calls (the text page), in a few single frames on the deck, and often indoors from vb 26,000 on [C]. The texture sources outside the two ranges can be read late.
- **The one deck frame with the first list:** at vb 24,589 the game rewrote 2,427 bytes of a texture source at 0x1380000 between the transfer and the late walk; the first census had filed that change under region 137 (section 2). The debug run that found it (`MGS2_XLIVE_SNAPDBG=1`: the bytes not laid compared with RAM as the walk reads it) reported nothing else up to vb 24,700 [C: `xdbg/out_dbg.log`].

## 2. When the game writes into bytes in flight (x86 tracker and census)

The tracker (`/home/claude/me12`, container only; `mk.sh`, `boot/xlive_chk.c` `MGS2_XLIVE_WT`, `tools/wt_tables.py`, `wt_union.py`, `ws_cost2.py`): the game code built with every store reporting itself (`GEN_DG_TRACE`) and the runtime's scratchpad DMA reporting its copies. A write into bytes a shown frame's transfer read is counted by its store site (named by function), by when (class 0: in the transfer's own step; class 1: in the next step) and by kind; every store is also counted by its site. State hashes equal the normal runs' on every route [C: `wruns2.log`, `wruns3.log`]. Every byte the census found changed is explained by a reported write, except the boot's staging area at vb 0 (4.1 MB of texture sources at 0x0c00000-0x0c3ffff, refilled by the IOP side) [C].

| route | stores seen | written in flight: own step / next step | of it changed by the end of: own step / next step | functions |
|---|---|---|---|---|
| deck | 3.61 billion | 10.4 MB / 988 MB (0.9 / 82 KB a frame) | 42 KB / 213 MB (17.7 KB a frame) | 151 |
| indoor route | 5.43 billion | 16.4 MB / 2,305 MB (0.9 / 128 KB a frame) | 84 KB / 528 MB (29.3 KB a frame) | 224 |
| codec route | 5.51 billion | 16.8 MB / 2,010 MB (0.9 / 104 KB a frame) | 89 KB / 353 MB (18.3 KB a frame) | 191 |

[C: tracker `xd/out_w{d,i,c}2.log`; changed bytes from the me10 census `out_{d,i,c}{1,2}.log` less the boot's staging area; "next step" includes the own step]

The writers by set, over the three routes [C: `tools/wt_union.py`; stores a vblank from `ws_cost2.py` on `out_w{d,i,c}3.log`, every store site of the set's functions]:

| set | when, into what | functions: deck / indoor / codec | all routes together | on all three | their stores a vblank: deck / indoor / codec | in the design |
|---|---|---|---|---|---|---|
| G0 | own step, any kind (after the boot) | 7 / 12 / 7 | 12 | 7 | 155 / 213 / 171 | guarded |
| boot | own step, vb 0-33 | 10 / 10 / 10 | 10 | 10 | | the main CPU draws the boot |
| G1t | next step, texture sources | 14 / 24 / 17 | 25 | 13 | 21,330 / 20,391 / 22,294 | not needed: those sources are copied |
| G1r | next step, other data the packets point to | 75 / 123 / 97 | 136 | 65 | 39,992 / 38,261 / 36,885 | not needed: copied |
| G1p | next step, the packets | 71 / 94 / 90 | 109 | 67 | | not needed: copied |

The G1r and G1p counts include the runtime's scratchpad DMA (38-48 copies a vblank). G1t includes `GSDU`, the generated code's out-of-line helper for unaligned 8-byte stores, which every game function's unaligned stores go through (4,478-8,362 of G1t's stores a vblank). For scale, the game makes 143,353 / 143,576 / 137,707 stores a vblank in all.

- **G0** [C]: `S_d00t_P299fb8` (a palette source at 13bf1a0, about 1 KB a frame in 5,964 of the deck's frames from vb 3,558), `P11de00` (01e0558), `S_d01t_P2a1200` (11bad70), `S_w00a_P31c0d0` (13f36f8), `S_w00a_P3a4138` (1429d2c, a texture source), `S_w00a_P11e0a8` (01dfd10, once), `P16d1ac` (the codec's text page at 1ed8508, a texture source); indoors also `S_w01a_P30c8b8` (139e6b8), `S_w01a_P322198` (13f7338), `S_w01a_P30d6f8` (13a0670, and a texture source at 13a8f38), `S_w01a_P350f40` (14fd444) and `S_w01a_P3926e0` (a texture source at 13d4cfc). Their stores a vblank: `P16d1ac` 47-61, `S_d00t_P299fb8` 30-47, `P11de00` 26-55, the rest under 31.
- **Boot** [C]: `P100df0`, `P101410`, `P11dc78`, `P12ac70`, `P12ae10`, `P13f2a8`, `P13f8a8`, `P13f990`, `P13fb68`, `P13fdd8`, once each between vb 0 and 33 (tags and data at 01b4xxx, and at 0c000xx), the same on all three routes.
- **Most of these writes put back the value already there**: in the own step 42-89 KB changed of 10-17 MB written. So the guard logs only real changes (section 4.3).
- **The lists grow with new areas.** The indoor stage (w01a) adds 5 of G0's 12 functions. Every stage of the game needs its own tracker run before its frames go to the ME (section 5) [C for these routes; I for the rest of the game].

**The texture sources the game rewrites** (census by 16-byte piece, each piece filed under its own 64 KB region; frames after vb 60) [C: me11 `xdbg/out_t{d,i,c}.log` "me11 rg" lines, `tools/patch_tally.py`]:

| route | texture sources a frame | in the two ranges | bytes changed in flight in the two ranges over the route (regions) | outside them |
|---|---|---|---|---|
| deck | 2,290 KB | 116.8 KB | 859,627 (12c, 138, 13a-142, 1ed) | 0 |
| indoor route | 2,637 | 102.7 | 2,456,039 (12c, 13a-142, 1ed) | 0 |
| codec route | 2,082 | 83.7 | 2,310,534 (12c, 13a-142, 1ed) | 0 |

- **Most of the texture sources lie elsewhere,** in 0x0a90000-0x11affff and 0x1d90000-0x1efffff (5-80 KB a frame in each region of 64 KB; textures loaded with the stage and the menus [I]), and none of their bytes changed while in flight after the boot. The boot's staging area (0x0c00000-0x0c3ffff) changed only at vb 0, before the first frame (vb 31) [C].
- **Within the working data**, the regions with no rewritten source (12d-137, 139) hold under 1 KB a frame of texture sources, so the design copies the whole range rather than a list of regions [C].
- **The first census (me10) filed each changed range under the region where the range starts.** Its list of 13 regions had 0c0 (the boot's staging only) and 137 instead of 138, where a REF range starting in 137 ran on; the x86 check of section 1 found it [C].

## 3. How much the copy holds

Per shown frame after the boot, in 16 KB steps [C: "me12 copy sizes" lines of `out_w{d,i,c}{2,3}.log`; the design's copy: "me11 hist" lines of `xdbg/out_h{d,i,c}.log`, its largest exact]:

| route | what is copied | half of the frames | 90% | 99% | 99.9% | largest |
|---|---|---|---|---|---|---|
| deck | **the design's copy** | ≤ 1,248 KB | 2,144 | 2,992 | 3,216 | 3,907 |
| indoor route | | ≤ 1,024 | 1,824 | 2,992 | 3,184 | 3,907 |
| codec route | | ≤ 624 | 1,808 | 2,976 | 3,184 | 3,907 |
| deck | all but the texture sources | ≤ 1,104 KB | 1,936 | 2,768 | 2,992 | 3,696 |
| indoor route | | ≤ 912 | 1,696 | 2,768 | 2,944 | 3,696 |
| codec route | | ≤ 592 | 1,664 | 2,752 | 2,944 | 3,696 |
| deck | the packets only | ≤ 208 | 336 | 512 | 528 | 576 |
| indoor route | | ≤ 176 | 320 | 512 | 528 | 576 |
| codec route | | ≤ 160 | 320 | 512 | 528 | 576 |
| deck | everything, with all texture sources | ≤ 3,488 | 5,104 | 6,928 | 6,976 | 7,792 |
| indoor route | | ≤ 3,760 | 5,264 | 6,720 | 6,976 | 7,792 |
| codec route | | ≤ 2,624 | 4,816 | 6,720 | 6,976 | 7,792 |

- **The design's copy** holds 1,192 / 1,076 / 955 KB in an average frame on the deck / indoor / codec routes [C: "me9 snap totals" less the boot's five steps]: the packets and the data they point to (the "all but the texture sources" rows), the texture sources in the two ranges and the images some packets carry in themselves. Its largest is 3,907 KB, at vb 6,271 on all three routes.
- **Where the large frames are** [C: the "me11 block" lines, per 1,000 vblanks, which count frames over 1,536, 1,741, 2,048 and 2,736 KB]: the three routes run the same to vb 23,000 (the indoor and codec routes to vb 30,000). That part holds 1,832 of its 10,764 frames over 1,741 KB, 1,234 over 2,048 KB and all 308 over 2,736 KB (these in vb 5,000-6,999). After it the deck route's last 970 frames hold up to 2,274 KB (153 over 1,741, 69 over 2,048), and the indoor and codec routes' frames from vb 23,000 to 30,999 hold up to 2,274 KB too (213 over 1,741, 4 over 2,048). From vb 31,000 to the routes' ends no frame holds more than 1,523 KB indoors (3,229 frames) or 621 KB on the codec route (4,501 frames, the calls).

## 4. The design

### 4.1 At each transfer (main CPU)

- The runtime's own walk stays on the main CPU: it gives the game what it reads back (DMA and VIF registers, the GS's signal and finish events, interrupts), as today [C: me_picture_study.md §2].
- It publishes, per transfer, the walk's inputs (channel, CHCR, MADR, QWC, TADR) and the texture sources it met (its GS packet walk follows every GIF tag and skips the image data, so it has their ranges [C: `runtime/ee_io.h`]), and per frame the step decisions the picture needs (shown or reduced, where a frame begins) and the thinning's result [C: the me9 harness passes these the same way].
- **Also in each record** (step 1, x86 [C]; `me_step1_notes.md` 2 and 4): whether the runtime's walk would feed the transfer to the translator (VIF1 on the fast path), its Stage B lanes, and g_opt and g_opt2 as they were at the transfer; the frame's start record carries the frame's own. The ME's walk takes its lanes and switches from the record, not from the moment it runs: a test build's turns change them every 100 vblanks.
- It does not list the spans: a frame has about 12,800 of them on the deck (150 million in 11,739 frames [C: me11 `out_dnoup.log`]), so the ME walks the chain itself (section 4.2).
- At the end of each transfer the main CPU writes back its D-cache (16 KB; the game's last packet writes may still be in it) and publishes the record through an uncached word. A resting ME polling that word needs no kernel call; today's wake through kuKernelCall costs 136 µs (task #57) [C].

### 4.2 The copy (ME, phase 1) and its deadline

- The ME walks each published transfer's DMA chain (the runtime's chain walk, the same code) and copies every span it reads into its arena (section 4.9), except texture sources outside the two ranges, dropping its own cache lines for a span before it reads it [S: media-engine.md §3]. The tags it walks are as sent: no code writes them in flight after the boot [C].
- The arena holds the spans in the order the walk meets them, so the ME's second walk (phase 2) reads them in that order with no address look-ups; only the texture sources left in RAM are read by address [I].
- **Deadline: the start of the next step.** At the step's end (the runtime's vblank hook, before any game code of the next step) the main CPU waits until the ME has copied the whole frame. The next step is where the game rebuilds its packets (section 2), so nothing the game does then reaches the copy [C for when the writes come].
- **Speed** [C: probe]: alone, the ME copied 146 MB/s (claimed lines); in the game 91-92 MB/s each way, writing back to main RAM. Into its own memory only the reads cross the main bus, so it should be nearer the first [I]. At 91 / 146 MB/s: an average frame in 10.7-13.4 / 6.7-8.4 ms, the largest in 44.0 / 27.4 ms [I].
- At 25 fps a step is 20 ms and the transfers come in its first 3.5% [C: study §1], so the main CPU waits in frames whose copy takes more than about 19.3 ms: at 91 MB/s those over 1,712 KB (deck 2,057 frames, 17.5%; indoor 2,110, 12.0%; codec 2,110, 11.2%; none from vb 31,000 on), at 146 MB/s those over 2,752 KB (308 on each route, 1.6-2.6%, all in vb 5,000-6,999) [I, from section 3]. Today's steps are 60-150 ms, so it never waits.

### 4.3 The guards and the old-bytes log

- **Where:** every store in the 12 functions of G0; in the runtime, the IOP side's writes into game RAM (SIF DMA, disc reads) and the scratchpad DMA, which are our own code. The code generator emits the guarded form for the listed functions [I].
- **What a guarded store does:** if a frame is in flight, the store would change the bytes there, its 64-byte line is one that the step's transfers started so far copy (the filter: a bitmap of their lines, cleared at the deadline), and the line is not yet in the log with the step's present count of started transfers: copy the line's 64 old bytes, its address and that count into the log, write that log line back from the cache, `sync`, raise the log's count (an uncached word), `sync`, and store. A store that changes nothing logs nothing.
- **The ME's side:** at the end of its copy it reads the log's count and lays over each transfer's copy the entries made after that transfer started (their count of started transfers is its number or more), the newest first, so where a line was logged twice the oldest bytes win: the bytes as they were when the transfer started, which are the bytes as sent. A step has up to 19 transfers [C: x86]. One table for the whole frame, as this section first had it, would give a transfer that started after a logged store the bytes from before that store, which that transfer never read [I].
- **Why the filter** (step 1, x86 [C]; `me_step1_notes.md` 3): while frames are in flight the runtime itself writes 3.2-4.5 GB into game RAM over a route (the scratchpad DMA, the IOP's copies, the disc reads), almost all of it outside the copy. With the filter the log holds 17-19 lines a step on average and at most 490 (31 KB) after the boot; without it (the deck) 4,328 on average and up to 17,699 (1.1 MB), and the picture is still exact. On the PSP the bitmap needs the copy's spans, which only the ME walks: either the main CPU marks the lines in its own walk (about 0.6 ms a frame [I]) or the ME publishes them as it walks, with a count of finished walks, and a guard logs every line of a transfer not yet walked [I]. Step 4's timing decides.
- **Why the order is safe:** the entry is published before the store, and the store's new bytes reach RAM only after it (later still from the main CPU's write-back cache). So if the ME's copy saw a new byte, the ME, reading the count after its copy, sees the entry that restores it. An entry laid over bytes the ME copied before the store carries the same bytes, so it changes nothing [I].
- **When:** from the frame's first transfer to the end of its step (the deadline). A G0 store into a texture source outside the two ranges would need the guard until the ME has read it; none does on the three routes [C: section 2's census].
- **Cost:** G0's 12 functions make 155 / 213 / 171 stores a vblank on the deck / indoor / codec routes, counting every store site in them [C]. At an estimated 10-20 cycles a guarded store [I], that is 5-13 µs a step on the main CPU. Over the deck / indoor / codec routes the log took 199,125 / 339,754 / 315,881 lines (the runtime's writes 6,520 / 6,950 / 6,950 of them), 7,615 / 22,643 / 11,033 of which held bytes the deadline copy would otherwise have read new [C: x86, step 1].
- **What if a writer is missing:** an unguarded function writing in-flight bytes in the same step would let the ME see the newer value in that frame: a picture difference in that frame, never a state difference (the game's state never depends on the picture [C: every run above]). The checks of section 5 find it on every route they cover.

### 4.4 Texture sources

- Those in the two ranges (84-117 KB a frame) are copied in phase 1, with the rest of the frame. The others (about 2.0-2.5 MB a frame) are not: the ME reads them from the game's RAM when its walk reaches them, through the upload identities today's renderer uses (Stage C step 0). After the boot no game code wrote any of their bytes while in flight on the three routes [C].
- **A safety net for areas no census has covered:** a source outside the ranges whose bytes the texture identity sees change between two uploads joins the copied set from then on. A missed range then costs at most one frame per such source, never a state difference [I].
- So every texture source the ME reads is what was sent, which today's renderer does not always do (study §9, open item #84). In those frames the ME's pictures would be closer to the PS2's than today's: a visual change, shown to you with shots first.

### 4.5 The picture (ME, phase 2) and the GE

- The ME runs the translator's walk over its copy (the same code as today: walk, translator, converters, textures, batches, GE list), writes the GE list and the frame's data in main RAM, writes back its cache over them, and sets the frame's done word.
- The main CPU, at the step where the frame is due, sees the done word, starts the GE on the list (the sceGe calls stay on the main CPU) and shows the frame. The picture appears one step later than today (study §1) [I].
- The single frame arena stays: the ME waits for the GE to finish a frame before writing the next one's batches, as the main CPU does today [C: today's code].
- **The Stage B lanes run in the ME's walk** (step 1, x86 [C]; `me_step1_notes.md` 2): today they live in the runtime's walk (`runtime/ee_io.h` `vif1_chain_fast`) and hand the converters whole groups with the records that C1 (lane 4) and step 3's trims (lane 1) read; a late walk without them lost C1 from the first codec call (2,070 / 3,716 / 4,265 frames differed on the three routes). The ME's walk runs them on its copy for the transfers the runtime's walk would have fed to the translator, with each transfer's lanes and switches from its record (4.1).

### 4.6 The queue, its deadlines and the take-back

- **One frame at a time.** Frame F's transfers come at the start of step N, its copy is done by the start of step N+1, and its picture by the start of step N+2, when F+1's transfers come. If the ME is still on F then, F+1's first transfer waits for it, as a PS2 game waits for its GS [I].
- **Take-back.** The ME counts a heartbeat word. If the main CPU waits past a limit (for example 200 ms) without the heartbeat moving, the ME has stopped (an exception on the ME is a silent hang [S: media-engine.md §3]): the main CPU holds it in reset as Sony does, shows the previous frame again, and builds the picture itself from the next shown frame on, with today's code, which every build keeps. The frame the ME was building is dropped: the main CPU cannot read the ME's own memory [S].
- **The picture's state lasts from frame to frame** (step 1, x86 [C]; `me_step1_notes.md` 5): the game sets GS registers and loads textures and palettes into the GS's memory in one frame and draws with them in many later ones, and the picture code keeps its model of both: the translator's state (the GS registers, the GIF and VIF state, VU1's memory and the sources of its micro program, the GS front end: 29 KB) and the GS map (what the GS's memory holds: 320 KB of tables, and its records); and the live frame hands its depth window to the next frame (four floats). The texture cache is part of it too, in one case found: the cache keeps a texture's texels as first converted for as long as its identity holds, and the map reads an upload's texels from EE RAM when a texture is converted, not when they were sent (`ge/gsup.c` `xf_bytes`; the open point of `renderer_notes.md` 11.10), so where those bytes change under the same identity only the cache still holds the texels as first drawn. Started cold (the translator reset, a new map, the texture cache emptied), as this section first had it, the picture differed from today's at all six points tried, mostly to the route's end: from vb 1,500 in 173 of the next 750 frames; on the deck from 6,000 none until vb 23,269, then 890; from 15,800 (in a codec call) 2,939 of 4,477; from 24,000 540 of 600; indoors from 33,000 all 2,400; on the codec route from 36,000 all 2,000. The x86 check of "Shown frames only" had left frames out but kept that state. With the first three kept, every frame equals today's at five of the points; on the codec route from 36,000, 578 of 2,000 frames still differed, with today's GE lists but other texels: the codec's subtitle text page alone, in rows 120-127 of its GE texture, below the game's 120-row image (2,024 of its 32,768 bytes; the subtitle sprite's last screen row reads row 120: #84's row-220 line), which the conversion after the take-back filled otherwise than today's run had, under the same identity [C], as when an upload's source bytes change after it was sent [I]. With all four kept, every frame equals today's at the six points; the rest of the live frame and the GE list may start afresh.
- **So a planned take-back comes at a frame's end:** the main CPU stops queueing transfers, the ME finishes the frames already queued, writes back its cache over the picture's state in main RAM and rests, and the main CPU carries on with that state from the next frame: the boot's hand-over (4.10) the other way round. The frames too large for a 2 MB arena (4.9) go this way, and so would a test build's turns between the ME and the main CPU (exact on x86: the twin's runs above and its 2 MB arena runs). The PSP does not sleep in a run: the harness holds the power lock [C: `me_startup_study.md`].
- **An unplanned stop** (an exception or a hang on the ME: a fault of ours) may leave that state half changed, so the main CPU starts cold: safe, but the picture is wrong as above until the game sends again what it keeps on the GS. A copy of the state at every frame's end would make this case exact too, but it would have to hold the texture cache's texels as well (up to its budget, 2,688 KB in these runs) and be made every frame: not planned. A fault is found and fixed on x86 and the emulator first, and the log names it.
- The log says when and why (heartbeat, frame, phase), and crash.txt keeps it as now.

### 4.7 Caches

- The two cores' caches are not kept in step [S: media-engine.md §3]. Main CPU: writes back its D-cache before publishing a record, and the log's line before raising its count. ME: drops its lines before reading spans, texture sources and the log; writes back the GE list, the frame's data and the converted textures before setting the done word [I].
- Shared control words (records, counts, done and heartbeat words) are read and written through the uncached alias only (0x40000000 | address), as the probe's control block was [C: psp/me_probe.h].

### 4.8 ME-safe picture code

The picture code that moves: `ge/` xlive, xlate, gsfront, texconv, texcache, gelive, gebatch, xconv, vu1conv, gelist, gsup2, and the ME's loop, `ge/xlme.c` (the late walk with the lanes, the switches each record carries, the translator's callbacks, a frame's end and start, the GS map's reader of RAM). `psp/xl_psp.c` stays on the main CPU: the set-up, the system calls, the printing and the timing. The thinning's scan (fxthin) stays on the main CPU too: it reads the game's RAM, so the main CPU scans at the frame's first walked transfer and hands the ranges in (`gl_thin`). **Done in step 2** (2026-11-03, `me_step2_notes.md`), exact on x86 and, built for the PSP, on the emulator [C]:

- **Memory:** the 38 allocation sites use the picture's own heap (`ge/me_rt.c`: two-level segregated fit; no system call and no lock; deterministic, so the emulator's run of a build gives the PSP's numbers), not fixed pools. Its first region is 768 KB (the picture's set-up takes 734 KB in 13 blocks). On the main CPU three hooks of the host's give it more: a region of the request's own size from newlib's heap when nothing fits, the region back once it is empty, and a region resized in place when a table that fills it doubles, as gl16's tables grow today; newlib's free top is given back after such a resize, since its realloc takes the whole new size at the top of its heap (`me_step2_notes.md` 5). On the ME the hooks stay unset, so a request that does not fit is refused; it must make the frame a fault, taken back (an open point for step 4: `me_step2_notes.md` 6).
- **memcpy / memset / memcmp:** our own, the calls the compiler leaves renamed in the objects (`objcopy --redefine-sym`), as the probe did; `floorf` and `ceilf` our own too, equal to newlib's on all 2^32 floats [C].
- **Printing:** none in the PSP build of the 11 files; their counts are fields the main CPU prints.
- **System calls:** none in the ME's unit; they stay on the main CPU's side of the split.
- **Code addresses:** every call through a pointer (33 in the ME's unit) goes through `ME_CALLP`, which sets the address's top bit from `$gp`'s: 0 on the main CPU, 1 on the ME, whose entry sets `$gp` to `_gp | 0x80000000`; the files are built with `-fno-jump-tables` [C].
- **The rest of the probe's rules:** `-G0`, no VFPU, nothing undefined, no code address in data: `tools/dev/me14/mecheck.py` checks the unit the PSP compiler makes (linked from the ME's 53 entry points: 257 functions, 292,076 bytes; the deepest stack 4,952 bytes) [C]. `-ffreestanding -fno-builtin` is not used: it turned `fabsf` and `sqrtf` into calls; without it the compiler keeps them as instructions, and the check proves no other call is left [C].
- **Floats:** the ME's results equal the main CPU's bit for bit under our FPU setting [C: probe 1], so the ME's entry sets FCR31 as the main CPU has it (step 3). No doubles in the unit: the depth scale is a float made at set-up [C].
- **Faults:** the unit has 22 `break 7` (the compiler's divide-by-zero checks); on the ME a division by zero, a refused allocation or a bad address must end in a take-back at the frame's end, not a hang (step 4) [I].
- **The lanes** (4.5): their recognizers (`ge/lw_lanes.h`, generated from `runtime/ee_io.h`) and the late walk are in `ge/xlme.c` [C].
- The x86 twin builds the same files the same way, so every change is checked there first.

### 4.9 Memory

| place | room | needs |
|---|---|---|
| the ME's own eDRAM | 2 MB on the PSP-1000 and 4 MB on later models by one source [S: media-engine.md §1], 2 MB on every model by another [S: Wikipedia, in ark4_options.md]; the ME alone reads it, so its reads stay off the main bus | a probe on your PSP-3000 (its size, whether what is written stays, the copy's speed), in three turns, one change each. **A**, today's start only: by the reload project's notes the eDRAM goes with the ME's bus clock, bit 0 of 0xBC100050, which today's 7 sets [S: psp-media-engine-reload README, "The 0xBCC00000 Registers"]. **B**, 0x0f instead of 7 on the ME's side: bit 3, which the custom core sets [S: psp-media-engine-custom-core me-core.h:33] and which by its place is the eDRAM bus gate that Sony's ME code switches on and off with requests of its own (meSysregAwEdramBusClockEnable and Disable, 387 and 388 [S: uOFW me_wrapper.c:270-279]; the bit's place is my inference [I]). **C**, three VME writes (0xBCC00000 = -1, 0xBCC00030 = 1, 0xBCC00040 = 1), with which the reload project stops a hardware block that the ME core's settings leave changing parts of the eDRAM by itself [S: reload README]; the same source says the last two "seem to be related to frequencies" of that block, unconfirmed, and today's start already writes them with Sony's values, 8 and 2 [C: psp/me/hle_me_mgs2.c, ME_SONY_BOOT], so C would change two registers that may set a block's frequencies: left out of the first probe, and asked for again only if its pattern check shows the eDRAM changing by itself (2026-11-03, the second card). B and C are new register writes: **your OK** (2026-11-03 these replace this row's earlier "eDRAM bit" wording, which stated the bit's meaning as a source's) |
| main RAM | about 340 KB of heap headroom at the lowest; 643 KB free outside the heap at the end, the largest block 431 KB [C: gl14mp run]; step 2's build on the emulator: newlib's break at its highest as gl16's, to the page; the lowest headroom read 5,576 KB on the PSP's memory and 328 KB with the texture pool 5 MB larger, against gl16's 5,592 and 344 [C: `me_step2_notes.md` 5] | nothing, but far too small for the copy |
| main RAM: the picture's heap (step 2) | a first region of 768 KB (the set-up takes 734 KB) and regions the main CPU's hooks add: 1,256 KB in 7 regions at the indoor route's end, the peak 1,256 KB in use [C: step 2's build on the emulator] | the ME takes it over as it is at the hand-over; a request it cannot fit there is a fault, taken back (4.8) |
| VRAM, 2 MB | two frame buffers and the depth buffer take 1,360 KB; the harness's text screen 544 KB at 0x154000; 144 KB free [C: psp/xl_psp.c] | the text screen made smaller or dropped, and a probe of whether the ME reaches VRAM [I: unknown] |
| VRAM in 4 MB mode (PSP-2000 and later) | +2 MB | the GE's eDRAM size switch (renderer_notes §11.7): a kernel call, **your OK** |

**The arena** [I]: it needs as much room as a frame's copy, 3,907 KB at most on these routes. With 4 MB of eDRAM every frame fits (the ME's code runs from main RAM, as it does today). With 2 MB, the frames over about 2,000 KB do not: 1,303 of the deck route's 11,734 frames, 1,238 of the indoor route's 17,585 and of the codec route's 18,857 (counted over 2,048 KB [C]), all but 69 of them before vb 23,000. Those would be drawn by the main CPU with today's code, one frame at a time, carrying the same state in main RAM as at the boot's hand-over (4.10), so in them the main CPU's step is as long as today's. The probe of section 6 step 3 tells which case your PSP-3000 is.

### 4.10 The boot

- The main CPU draws the frames to vb 60 with today's code, and the ME takes over at the first shown frame after it. Ten boot functions write into tags and data in flight between vb 0 and 33, and at vb 0 the IOP side refills the staging area (4.1 MB of texture sources at 0x0c00000-0x0c3ffff), which no game store reports; from vb 34 on no boot writer appears on any route [C]. A cfg line can move the hand-over.
- At the hand-over the main CPU writes back its cache and the ME carries on with the same picture state in main RAM: the translator's state, the GS map, the live frame's depth window and the texture cache (4.6).

### 4.11 Alternatives, and why not

- **Guard the next step's texture-source writers (G1t) instead of copying the two ranges:** 25 functions, 21,330 / 20,391 / 22,294 stores a vblank (deck / indoor / codec) [C]. At an estimated 10-20 cycles a guarded store [I], 0.6-1.3 ms a step on the main CPU, which is where 25 fps is hardest, against about 1 ms a frame of copying on the ME. Most of it is `S_d00t_P323b58` (7,831-12,431 stores a vblank), which wrote into a texture source in 2 frames of the opening, and `GSDU`, the helper every unaligned 8-byte store of the game goes through.
- **Design C, the packets only (at most 576 KB) and guards on every next-step writer of referenced data (G1r) too:** G0, G1t and G1r together are 148 functions with 40,494 / 38,715 / 37,247 stores a vblank, 1.1-2.4 ms a step [I], and lists that every new area of the game adds to. It would need no eDRAM if the ME reaches VRAM, but it puts the cost on the wrong core.
- **The hybrid, G1r guards only in frames that do not fit the arena:** the guard's first test still runs in every store of those functions in every frame, about a third of design C's cost [I].

## 5. Checks

- **x86, every route:** the twin with the design's split (the copy as late as the ME may make it, at the end of the transfer's step; the picture as late as it may come, at the end of the next step; the logs from the guarded functions; the boot on the main CPU to vb 60): every state hash and every frame equal to today's run, the texture-source frames of 4.4 aside, which must equal the full-copy run's (section 1).
- **x86, the other paths:** a small arena forced (frames that do not fit) and a take-back forced at a few vblanks, each with every frame equal. Done in step 1: the 2 MB arena exact on the three routes; the take-back at a frame's end, with the picture's state kept, exact at six points (cold it is not: 4.6) [C].
- **x86, every new route or stage:** one tracker run and one census by 16-byte piece first (with `MGS2_XLIVE_SNAPDBG=1`, the bytes the design does not copy compared with RAM at the late walk); a new G0 function or a new range of rewritten texture sources joins the lists before the stage's frames go to the ME.
- **Emulator:** PPSSPP has no ME; the same loop runs there as a main-CPU thread [S: media-engine.md §5], with the same frame digests as x86.
- **PSP:** the frame digests equal x86's and the emulator's, every hash line exact, the ME's heartbeat and timing lines, on the OSTENT.

## 6. Build order

1. x86: the split in the twin (the record from the runtime's walk, the ME's chain walk and copy, the guards and log in the generated code and the runtime, the picture from the copy, the boot hand-over, the take-back), exact on the three routes against today's runs. **Done 2026-11-03** (`me_step1_notes.md`): exact, with four changes to this design: the lanes in the ME's walk (4.5, with each record's lanes and switches, 4.1), the log kept per transfer and its filter (4.3), and the take-back at a frame's end with the picture's state kept (4.6).
2. x86: the picture code made ME-safe (pools, own memcpy family, no printing, kernel aliases, no jump tables), still exact. **Done 2026-11-03** (`me_step2_notes.md`): exact on x86 and, built for the PSP, on the emulator, with five changes to this design (4.8): one heap with regions the main CPU's hooks add instead of fixed pools; the compiler's built-ins kept; the thinning's scan on the main CPU's side; no doubles on the ME; and the GS map's local copy reads its source before it takes a buffer (a waste found on the way: about 1,556 MB of buffers cleared for nothing on each route, which gl16 does on the PSP too).
3. PSP probe (with your OK): the ME's eDRAM on your PSP-3000 (size, the copy's speed into it, the main CPU's cost while it runs), whether the ME reaches VRAM, and the guards' cost. Like gl14mp: small, in turns, after the run before it has been read.
4. The first ME picture build: the ME builds the picture in turns with today's path, exact against x86 and the emulator.

## 7. For you

- One decision, before step 3, on a card (2026-11-03, the second card; the first, withdrawn unanswered, left out that two of C's registers may set frequencies): may the probe test turn B (bit 3 of 0xBC100050 on the ME's side) after turn A (today's writes only), with C (the three VME writes, two of them perhaps frequencies) left for later? Recommended: A and B. Builds after it keep only the writes the probe shows are needed.
- Nothing changes in gl16 or its run.
