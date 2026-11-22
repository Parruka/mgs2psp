# The picture on the Media Engine: build order step 2, the picture code made ME-safe

2026-11-03, task #149: `me_handoff_design.md` section 6 step 2. x86 runs, the ME unit's check on the PSP compiler's output, and PSP builds of the ME-safe code run on the emulator (no build for your PSP). [C] = measured or read in our code and runs; [I] = my inference.

## The answer

- **The picture code is ME-safe and still exact.** The 11 picture files (`ge/` xlive, xlate, gsfront, texconv, texcache, gelive, gebatch, xconv, vu1conv, gelist, gsup2) now call nothing outside their own unit: their memory comes from a heap of their own, their memcpy family and floorf/ceilf are our own, every call through a pointer goes to the core's own address range, and they have no jump tables. Built that way, every state hash and every frame equals today's run on the three routes (deck 252 hash lines and 11,766 frames, indoor route 378 and 17,664, codec route 400 and 18,931, none different), in today's path, in step 1's split, in the split with the ME's loop as one file, with every switch following its record while four configurations take turns, and with the heap growing from a small first region as it does on the PSP. The take-back at step 1's six points is exact too (section 2) [C].
- **On the PSP's main CPU** (the same code built for the PSP, run on the emulator): exact: 434 hash lines and 311 vuh equal gl16's, the 150 frame digests equal x86's, and the lists hold 216,085,943 words, as gl16's plain run [C]. The model (no PGO in either build) puts it at +1.1 ms a step on the deck and +0.2 inside, which is code placement, and -2.8 ms in the opening, the waste below [C]. Memory: newlib's break at its highest stands where gl16's does, to the page, after two fixes on the way: newlib's realloc grew the texture cache's table at the top of its heap by the whole new size, not by the growth, and a least region of 32 KB left 20 KB unused (section 5) [C].
- **A memory waste found on the way, and fixed (exact):** a GS-to-GS copy whose source the picture does not know (1,727 of the 1,728 on each route) cleared a buffer of 1 MB or 256 KB, then found its first source texel unknown and freed it: 1,499 buffers of 1 MB and 228 of 256 KB, about 1,556 MB cleared for nothing between vb 5,260 and 12,486 [C]. The copy now reads its source texels first and takes a buffer only when it knows them all. The x86 heap's peak fell from 2,540 KB to 1,528 KB [C]. gl16 does the same clearing on the PSP today: in the model's window at vb 10,000 it takes 3.36 ms a step, 0.35 ms after the fix, and the window is 2.8 ms a step faster (section 5) [C]. It goes into the next PSP build.
- **The ME's loop is one file, `ge/xlme.c`:** the late walk with the Stage B lanes, the switches each record carries, the translator's callbacks, a frame's end, its GE list and digest, a frame's start, and the GS map's reader of RAM. The x86 twin runs these same functions [C].
- **The ME's unit passes its check** (`mecheck.py` on the PSP compiler's objects, linked from the ME's 53 entry points): 257 functions, 292,076 bytes of code; no relocation in it refers to anything outside it; no code address in data; all 33 calls through a pointer go through `ME_CALLP`; no jump table; `$gp` used only by `ME_CALLP`, no `$k0`/`$k1`, no `$23` (the game's base register); the deepest stack 4,952 bytes [C].
- **Five changes to the design** (made in `me_handoff_design.md` 4.8 and 4.9):
  1. **One heap with regions added on demand, not fixed pools.** A two-level segregated-fit heap (TLSF): no system call and no lock, and deterministic, so the emulator's run of a build gives the PSP's numbers. Its first region is 768 KB (the picture's set-up takes 734 KB in 13 blocks). On the main CPU three hooks of the host's give it more: a region of the request's own size from newlib's heap when nothing fits, the region back once it is empty, and a region resized in place when a table that fills it doubles, as gl16's tables grow today (newlib's free top is then given back: section 5). On the ME the hooks stay unset: a request that does not fit is refused and counted (m14c in section 5; an open point for step 4 in section 6).
  2. **The compiler's built-ins stay.** `-ffreestanding -fno-builtin` (design 4.8, from the probe) turned `fabsf` and `sqrtf` into calls and stopped the compiler copying small fixed sizes inline. Without them the compiler keeps those as instructions, and the calls it leaves to `memcpy`, `memset` and `memcmp` are renamed to ours in the objects, as the probe did; the unit's check proves nothing else is left [C].
  3. **The thinning's scan moves to the host's side.** `fx_thin_scan` reads the game's RAM (the effects' actors), which the ME must not: the main CPU scans at the frame's first walked transfer and hands the ranges in (`gl_thin`); the converters take them at the frame's first effect call as before [C].
  4. **No doubles on the ME.** The depth scale is a float made once at set-up; `gb_vert`'s depth-by-scale branch, which only the replay tools use, is left out of the ME's build and counted if ever reached (0 on every route) [C].
  5. **The local copy's check comes first** (the waste above), in every build from now on [C].
- **For you:** one decision before step 3 (the probe on your PSP): may the ME's eDRAM be switched on? It comes on a card.

## 1. What changed in the picture code

- **`ge/me_rt.h`, `ge/me_rt.c` (357 lines), new:**
  - `me_memcpy`, `me_memset`, `me_memcmp`: the C library's results; words when both ends share an alignment (16 bytes a turn), bytes otherwise. The x86 build counts overlapping copies (0 on every route) [C].
  - `me_floorf`, `me_ceilf`: newlib's rules; `me_rt_test.c` compares them with the C library's on all 2^32 floats: 0 different [C].
  - The heap: `malloc`, `calloc`, `realloc`, `free` and `aligned_alloc` over the regions it is given, up to 32 (`me_heap_init` the first, `me_heap_add` more that it keeps). The host's hooks (`me_heap_grow`, `me_heap_resize`; the main CPU's only) give a region when nothing fits, take back a region they gave once no block in it is in use, and resize a region a realloc's block is alone in. Its peak, regions, refusals and the hooks' calls are counted for the log. `me_rt_test.c` runs it under random loads with contents, alignment and a walk of every region checked every 1,000 steps: one region (2,000,000 steps in 1 MB and 8 MB, 500,000 in 64 KB), regions added and given back by hooks that refuse at random (six runs of 0.5-2 million steps), and tables that double from 1-4 KB to 1 MB beside small blocks, resized in place, moved to another alignment or refused at random, with and without a free block after them (1,300,000 steps, 19,878 resizes); no fault, every region given back, one free block left [C].
  - `ME_CALLP(f)`: a call through a pointer. The ME runs our code at the kernel alias of its address (0x80000000 | address; v30me6c), so on the PSP the pointer's top bit is set from `$gp`'s top bit: 0 on the main CPU (the EBOOT's `_gp`), 1 on the ME once its entry sets `$gp` to `_gp | 0x80000000` (step 3 and 4's entry code). Two instructions a call; the model's windows have about 2,400-2,900 pointer calls a step, so about 0.02-0.03 ms [I].
- **The 11 files** (75 lines added, 32 changed or removed; `tools/dev/me14/patch_me14.py` and the local copy's patch make them from sb14's): the C library's names mapped by `me_rt.h` under `-DMGS2_ME_SAFE`; `ME_CALLP` at their 25 pointer calls; the depth scale as a float (`zkf`) and `gb_vert`'s branch out; the scan's ranges by `gl_thin`; `gl_tex_cb` made global (the ME calls it through a pointer, so the unit's check keeps it as an entry); the local copy's check.
- **Build:** the PSP flags of gl16 plus `-fno-jump-tables -DMGS2_ME_SAFE`, then `objcopy --redefine-sym memcpy=me_memcpy` (memset, memcmp likewise). The x86 twin builds the same files the same way (with `_FORTIFY_SOURCE` off, which would otherwise call glibc's checked copies).
- **Printing:** the PSP build of these files prints nothing; their reports are x86 only, and the replay writers' `fwrite` is main-CPU code the ME's unit drops [C].
- **`psp/xl_psp.c`** (the main CPU's side, built as before): the heap's first region and hooks at set-up (`MGS2_ME_HEAP_KB`, default 768; `MGS2_ME_HEAP_GROW_KB`, the least region the hook gives, default 0: each region the request's own size; `MGS2_ME_HEAP_RESIZE`, default on; after a resize of a region at the top of newlib's heap, newlib's free top given back: section 5), the thinning's scan at the frame's first walked transfer, and the heap's lines in log.txt (its counts and the hooks' first 48 events with newlib's break after each).

## 2. The checks on x86

Every row compared with sb14's x1 runs by `tools/cmp13.py`: hash lines / frames, "0" = none different [C]. All on the final code (`runs_f.sh`, `runs_l.sh`; 2026-11-03: `runs_f.sh` 06:30-07:41, the hooked twin 06:54-07:02 and 07:41-07:46, `turns_ref.sh` 07:22-07:31 UTC).

| run | deck (252 hash lines / 11,766 frames) | indoor (378 / 17,664) | codec (400 / 18,931) |
|---|---|---|---|
| A: today's path, the picture code ME-safe | 0 / 0 | 0 / 0 | 0 / 0 |
| A with the PSP's heap: a 768 KB first region and the hooks | 0 / 0 | 0 / 0 | 0 / 0 |
| 2a: step 1's split (the copy at its deadline, the guards' log, the picture two steps late) | 0 / 0 | 0 / 0 | 0 / 0 |
| 2b: 2a with the ME's loop as one file (`ge/xlme.c`) | 0 / 0 | 0 / 0 | 0 / 0 |
| 2b, every switch following its record, four configurations in turns every 100 vblanks from vb 1,000, against sb14's own twin in the same turns | 0 / 0 | 0 / 0 | 0 / 0 |

| take-back in 2a at a frame's end, the picture's state kept (TBK 15) | route | hash lines | frames after it, different |
|---|---|---|---|
| from vb 1,500 to 3,000 | deck | 30, 0 different | 750, 0 |
| from vb 6,000 | deck | 252, 0 different | 9,293, 0 |
| from vb 15,800 | deck | 252, 0 different | 4,477, 0 |
| from vb 24,000 | deck | 252, 0 different | 600, 0 |
| from vb 33,000 | indoor | 378, 0 different | 2,400, 0 |
| from vb 36,000 | codec | 400, 0 different | 2,000, 0 |

- **The heap** on x86 (64-bit pointers; one region of 256 MB in the twins, as before): peak 1,528 KB in use, 21 blocks at most, the same 21 allocations, 1 free and 2 reallocations on all three routes; 1,522 KB in use at the end; nothing refused [C]. Before the local copy's fix: peak 2,540 KB, 1,748 allocations and 1,728 frees [C].
- **The turns:** sb14's own twin in the same turns differs from its plain run in 521 / 972 / 1,907 frames, every one made while the four lanes were off (the fourth configuration), from vb 15,759 on: without lane 4 the codec faces lose C1, as step 1 found. 2b's frames equal sb14's in the same turns one for one, and every state hash is equal [C].
- **The heap as the PSP's main CPU runs it** (twin A with `MGS2_ME_HEAP_KB=768`: a first region of 768 KB and the hooks, which on x86 round a region up to 4 KB): exact on the three routes, with the 32 KB least region and with none (`MGS2_ME_HEAP_GROW_KB=0`, the PSP's default from me14p9): at each route's end 1,543 KB in 5 regions (a first region of 768 KB; the hook gave 4 regions and resized the table twice, took none back), the peak 1,528 KB in use, as with one region; on x86 the two settings give the same regions [C].
- **The scan by the host:** 11,696 / 17,547 / 18,818 scans (deck / indoor / codec) [C].
- **2b's counts:** the ME's loop made 13,181,293 / 18,469,771 / 16,063,344 converter calls, 2,370,056 / 3,134,631 / 4,239,678 primitives and every list (none too big); the late walk with the lanes gave the same groups and tags on each lane as 2a's, 0 misses [C].
- **The take-back in the twin** makes a new live frame without freeing the old one: 7-8 more blocks and 450-560 KB more at the peak (2,083-2,088 KB against 1,528) [C]. The twin's take-back is a test of the picture's state, not the PSP's code; step 4's take-back must free or reuse the frame [I].

## 3. The ME unit's check

`tools/dev/me14/mkme_psp.sh` compiles the 11 files, `ge/xlme.c` and `ge/me_rt.c` with the PSP flags and `-fstack-usage`, and links them (`ld -r --gc-sections`) from the ME's 53 entry points: the late walk and the translator's entries, the frame's calls, the switches and the loop's functions. `mecheck.py` reads the result:

- **Relocations:** none in the kept code refers to anything outside the unit (the relocatable link's symbol table still names four symbols of dropped code: three of libgcc's double helpers and `fwrite`); no code address in a data section (no jump tables, no function table) [C].
- **Pointer calls:** every `jalr` and `jr` through a register (33: 25 in the picture files, 4 in the ME's loop, 4 in the heap's hooks, which stay unset on the ME) has the `ME_CALLP` form before it. The first check of 2a found four that did not (xlive's `run`, `tc_texture_blk`, `gx_batch`, and `gu_image`'s tail jump through the map's data callback); they were wrapped and the next check passed [C].
- **Registers:** `$gp` read only by `ME_CALLP`; no `$k0`/`$k1`; no `$23` [C].
- **`break`:** 22, all `break 7`, the compiler's divide-by-zero checks after a `divu`. A division by zero stops the main CPU today (the crash catcher's record); on the ME it would stop the ME, which the step-4 loop must see as a fault and take back [I].
- **Stack:** the deepest path is 4,952 bytes (the walk, a VIF command, a converter call, a texture look-up, the GS map's node growing through `me_realloc` and `me_malloc`); the ME's stack needs that and a margin [C].
- **Address-taken functions:** `fb` and `fe` (texcache, inside the unit), `gl_tex_cb`, the loop's four callbacks; `page_fetch` (texconv's, used only by `tc_texture`, which the ME does not call) is dropped [C].

## 4. The ME's loop (`ge/xlme.c`, 189 lines)

- `xm_walk`: one transfer. A VIF1 transfer that the runtime's walk would have fed to the translator goes through the late walk with that record's lanes; any other through `xlv_dma`. The walk reads bytes only through `lw_q` and `lw_ptr`, the place where the ME's copy will be read in step 4. The lanes' recognizers are `lw_lanes.h`, generated from `runtime/ee_io.h` by `tools/mk_lw.py`, as in step 1.
- `xm_switch`: a record's switches, in the order `psp/xl_psp.c` sets them; `follow` says which ones follow the records (the PSP: all; the twin: those step 1's late path followed, or all with `MGS2_XM_FOLLOW=15`).
- `xm_call`, `xm_pass`, `xm_flush`, `xm_prim`: the translator's callbacks.
- `xm_end`, `xm_list`, `xm_begin`: a frame's end, its GE list and digest, a frame's start.
- `xm_ram`: the GS map's reader of EE RAM.
- No printing: the counts are fields the host prints.

## 5. The PSP build on the emulator

The same code built for the PSP (`tools/dev/me14/mkpsp.sh`: gl16's tree without PGO, the picture files ME-safe, `psp/xl_psp.c` with the heap's region, hooks and lines), run on PPSSPP 1.20.4 to the indoor route's end with the delivery's cfg. "hw" is the emulator's memory (its headroom is the PSP's plus 5,120 KB); "hwmem" gives the texture pool those 5,120 KB so that newlib's heap is the PSP's for everything else [C: gl3.sh]. Every run: 434 hash lines and 311 vuh equal gl16's (the profiled runs' last one aside: section 6), the 150 frame digests equal x86's, and the plain hw runs' lists hold 216,085,943 words, as gl16's [C]. The table gives newlib's break at its highest above where it starts, exact to the page, and the log's lowest headroom, which a trial malloc finds to 16 KB [C: main_psp.c].

| build | the picture's heap | hw: newlib's break at its highest, above its start / the lowest headroom | hwmem: the same |
|---|---|---|---|
| gl16 | newlib's malloc, as before | 6,512 KB / 5,592 KB, at vb 23,675 | 11,760 KB / 344 KB, at vb 23,675 |
| me14p | one region of 3 MB, before the local copy's fix | 8,340 KB / 3,760 KB, at vb 501 | not run |
| me14p2 | one region of 1,344 KB | 6,612 KB / 5,488 KB, at vb 501 | stops at the start: "disc: no memory for 361,520 bytes" |
| m14c | 768 KB, then hook regions of 128 KB (a larger request its own size), kept | not run | 11,980 KB / 96 KB, at vb 10,603; 5,760,780 refused requests for 262,144 bytes |
| me14p3 | 768 KB, then hook regions of the request's size (at least 32 KB), given back when empty | 6,664 KB / 5,428 KB, at vb 10,849 | 12,048 KB / 192 KB, at vb 22,979 |
| me14p5 | as me14p3, and the resize hook (which never fired: below) | 6,600 KB / 5,492 KB, at vb 10,849 (the same with no least region) | not run |
| me14p6 | as me14p5, the resize also with a free block after the table | 6,600 KB / 5,492 KB, at vb 10,849 (resized in place twice) | 11,780 KB / 312 KB, at vb 23,675 (three times) |
| me14p7 | as me14p6, newlib's free top grown by the shortfall before such a resize (it did not grow: below) | 6,600 KB / 5,492 KB, at vb 10,849 | 11,780 KB / 312 KB, at vb 23,675 |
| me14p8 | as me14p6, newlib's free top given back after such a resize | 6,532 KB / 5,560 KB, at vb 23,675 | 11,780 KB / 312 KB, at vb 23,675; with no least region 11,760 KB / 328 KB |
| me14p9 | as me14p8, no least region: each region the request's own size | 6,512 KB / 5,576 KB, at vb 23,675 | 11,760 KB / 328 KB, at vb 23,675 |

- **Where the room went:** the texture cache's entry table (128 bytes an entry) doubles from 256 entries at vb 536 to 512 at vb 10,340 and 1,024 at vb 10,848; the hooks' events in the log show each step [C]. In me14p3 and me14p5 each doubling took a new region and gave the old one back below it: the resize hook never fired, since the hook gives regions rounded up to 64 bytes and v3's resize declined a table with a free block after it ("resized 0") [C]. Pools v4 allows that case (`me_rt_test.c` now pads the regions so that it runs: 8,725 of 15,013 resizes), and me14p6 resized the table in place, yet the break rose as before [C].
- **newlib's realloc** (its malloc is dlmalloc 2.6) grows a block that ends at the heap's free top in place only when the top already holds the growth. Otherwise it takes the whole new size from a top grown by sbrk and joins it to the block, so the break rises by the new size, not by the growth (72 KB for 32 KB of growth at vb 10,340, 132 KB for 64 KB at vb 10,848), and the rest stays free at the top, under the 128 KB at which newlib gives memory back by itself [C: newlib's `mallocr.c` and the events]. me14p7 first made and freed a block of the shortfall, to grow the top by that alone; newlib took it from a free chunk elsewhere, and nothing changed [C]. me14p8 gives the free top back (`malloc_trim`) right after a resize of a region that ended at the top while the top was short: the break then stood at 0b830000 and 0b840000 after the two doublings instead of 0b83a000 and 0b85b000, and its highest point moved to vb 23,675, where gl16's is [C]. During any realloc the old table and the new size are held at once, in gl16 as here; the log samples the break once a vblank, so neither run shows that moment [C: the code].
- **The last 20 KB** was a least region: at vb 23,674 a request of 12,312 bytes got a region of 32 KB [C]. me14p9 gives each region the request's own size, rounded up to 64 bytes: newlib's break at its highest then stands where gl16's does above its start, to the page, on both (6,512 and 11,760 KB) [C]. The heap block starts 4 KB higher in these builds and the log's trial mallocs go in 16 KB steps, so the lowest headroom reads 5,576 and 328 KB against gl16's 5,592 and 344 [C]. The picture's heap at the indoor route's end: 1,256 KB in 7 regions on the PSP's memory (a first region of 768 KB and 488 KB from the hooks), its peak 1,256 KB in use; 1,384 KB in hwmem, where the table doubles once more [C].
- **hwmem overstates the picture heap:** with 5 MB more texture pool the cache holds more textures and its table doubles once more, to 2,048 entries (262,144 bytes). gl16 and these builds are measured the same way, so the comparison holds; the PSP's own table stays at 1,024 entries on this route [C: the hw runs].
- **The model** (PspProf, 80 steps a window; ms = (1.647 instructions + 68.2 I-misses + 85.9 D-misses) / 333k a step): me14p6 against sb14m2, which is gl16's code in the same model build (no PGO) without the ME-safe files [C]. me14p7 to me14p9 change only the hooks' code in `psp/xl_psp.c`, whose events fall outside these windows:

| window | gl16's code | me14p6 | difference | instructions / I-misses / D-misses |
|---|---|---|---|---|
| deck, vb 30,810-30,890 | 165.2 ms | 166.3 ms | +1.1 (+0.7%) | +0.3 / 0.0 / +0.7 |
| inside, vb 33,410-33,490 | 104.3 ms | 104.5 ms | +0.2 (+0.2%) | +0.2 / -0.9 / +1.0 |
| the opening, vb 10,000-10,080 (stage d00t) | 57.9 ms | 55.1 ms | -2.8 (-4.8%) | -0.9 / +0.7 / -2.6 |

  - **The opening:** gl16's code spends 3.36 ms a step there in memset, clearing the local copies' buffers; this build 0.35 ms [C].
  - **The memory functions** (newlib's and ours together): deck 6.85 to 5.61 ms, inside 5.23 to 4.28. Ours take a little less than the picture's share of newlib's (3.56 ms against about 3.83 on the deck, that share read as what newlib's functions lose), and libgcc's double helpers, which `gb_vert` and `xc_xform` called for the depth scale, are gone (0.67 and 0.57 ms) [C].
  - **The rest is code placement** (neither build has PGO): the translator, the GS front end and the harness's glue take 1.5-2.5 ms more in instruction misses, the converters and the texture cache less, and the misses in all are equal on the deck and 0.9 ms fewer inside; the data misses are 0.7-1.0 ms more (the heap places blocks elsewhere) [C]. `ME_CALLP`'s two instructions a pointer call are inside the instructions' +0.2-0.3 ms [C]. A gl build places its code by its own PGO profile, so these placement terms come out otherwise there [I].

## 6. Open points for steps 3 and 4

- **The ME's entry:** set `$gp` to `_gp | 0x80000000` (for `ME_CALLP`) and the FPU's control word (FCR31) to the main CPU's, so that the converters' floats stay bit for bit (probe 1 showed equal results under our setting) [C: the code; I: the entry is step 3's].
- **The ME's stack:** at least 4,952 bytes plus the entry's frame.
- **The ME's heap:** the picture keeps its state in main RAM (design 4.6), its heap included; the ME takes it over at the hand-over without the hooks. The heap grew 8 times over the hw route in me14p9: regions at vb 30 and 33, three at vb 536, the table resized at vb 10,340 and 10,848, and a region at vb 23,674 [C]. My proposal for step 4 [I]: a request the ME's heap refuses is a fault like the others; the ME drops the frame, the main CPU takes it back at the frame's end and makes that frame with today's code, its hooks growing the heap, and the ME carries on from the next frame. That costs one main-CPU frame per growth, a handful in a route, and needs no mailbox between the cores. For it, every refused request must make the frame a fault: the picture code today goes on after some refusals (the texture cache keeps its table when it cannot double, m14c), which on the ME must not happen silently.
- **Faults:** a refused allocation, a `break 7` or a bad address on the ME must end in a take-back at the frame's end, not a hang [I].
- **The copy's layout:** the lanes look up bytes by address inside a transfer's chain (`lw_ptr`), so the ME's copy must be read by address, not only in walk order (design 4.2 says "walk order") [I: to settle in step 4's design].
- **The switches at the hand-over:** the ME takes the main CPU's switch state (`XmPic.sw`) with the picture's state [I].
- **The take-back's frame:** step 4's take-back frees or reuses the live frame (the twin's does not: section 2) [I].
- **A profiled run's last hash line** (vb 37,800, just after the profiled window) is not always x86's: it differed in gl16's own hwmem run and in one of me14p8's, and equalled x86's in the other four (me14p6's, me14p7's, me14p9's and me14p8's with no least region); every plain run's equals x86's. Compare a profiled run's other lines, and a plain run's all [C].

## 7. Files

`tools/dev/me14/` (README.txt, md5.txt, me14.diff): `me_rt.c`, `me_rt.h`, `me_rt_test.c`, `xlme.c`, `xlme.h`, `lw_lanes.h`, `mk_lw.py`; the patch scripts that make the picture files, the twins' glue and `psp/xl_psp.c` from sb14's; `mk.sh`, `mk_g.sh`, `mk_g2.sh`, `mkpsp.sh`, `mkme_psp.sh`, `mecheck.py`, `callmap.txt`; `runs_f.sh`, `runs_l.sh`, `run1g.sh`, `run1g2.sh`, `turns_ref.sh`, `turnsdiff.py`, `x86tab.py`, `verify_patches.sh`, `emucmp.py`, `fitcmp.py`, `f6all.sh` to `f9all.sh`. The container trees `/home/claude/me14` and `/home/claude/me14p6` to `/home/claude/me14p9` are ephemeral; the logs stay there.
