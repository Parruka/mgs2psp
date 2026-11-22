# The ME probe build (gl14mp)

2026-11-02, task #132; me_picture_study.md section 7 (your card 2026-11-01 00:48: "After gl13's run"). It is gl14's game with a probe of the Media Engine (ME) added, and nothing else changed. It ships after gl14's run is read, so only one build is out at a time. [C] = read in our code or logs; [S] = a public source; [I] = my inference.

## What it answers

The study's plan (picture on the ME) depends on five numbers the PSP alone can give:

1. **Floats.** Do the ME's float results equal the main CPU's bit for bit under our FPU rule (FCR31 0x01000001: round toward zero, denormals flushed)?
2. **Speed.** What does the ME pay per instruction, per I-miss and per D-miss, against the main CPU on the same code? Together with the model's counts, these give s, the ME's time for the picture over the main CPU's.
3. **Sharing the RAM.** How much slower are the main CPU's steps while the ME streams memory as the picture would?
4. **The copy.** How fast can 3.4 MB be copied by the main CPU, by the ME and by the system's DMA copy, and what does each cost the other core?
5. **Power.** battery.txt with the ME busy, on the OSTENT.

## Part 1: at the start, before the game

`__wrap_hw_probe` runs it after the existing hardware probe, so main_psp.o is unchanged. main_psp.c calls the hardware probe before its pools are made and before the game is loaded [C: main_psp.c lines 676-693], so the game's RAM is still all zero then. Every result is a "mep" line in log.txt. In the emulator Part 1 takes 0.7 s, with no ME there [C: build 1's log]; on the PSP it takes longer, with the clock's 200 ms and the ME's jobs [I].

In this order (the two probes that could stop the ME come last):

1. **Memory.** The probe's big buffers (4,352 KB at most) are taken from the game's RAM, in an 8 MB window 16 MB in. The window is checked all zero first; if it is not, the probes that need it are skipped and the log says so. After the last probe the window is zeroed again and checked, before the game is loaded. The control block, the ME's region (16 KB, its stack at the top) and the ring (64 KB), 80 KB together, are one block of user memory outside the heap (sceKernelAllocPartitionMemory; the heap only if no block is free, and the log says which). The DMA helper thread's 8 KB stack is outside the heap too. So the game's heap is gl14's (see "Memory in the game").
2. **The start.** The ME starts the way v30me6c started it: Sony's way, kernel helpers at their kernel alias, resting between jobs (me_rest.S). It uses the same registers you OKed then and no others. If the ME will not start (for example in the emulator, which has no ME), the log says why, the ME parts are skipped and the main CPU's parts still run.
3. **The clock.** The ME writes its own Count register (`mtc0 $zero, $9`) once, when its loop starts, and times every job in its own ticks. Count read a constant until written [C: me_picture_study section 5]. Its rate is calibrated against the main CPU's microsecond clock over 200 ms, through one uncached word the ME rewrites every 3 µs or so. From then on the ME rests between jobs, so it does not poll the bus during the main CPU's own tests.
4. **The copy (probe 4).** 2 MB from a patterned buffer to another, checked word by word after each copy:
   - main CPU: memcpy, the runtime's claimed-line copy (cache 0x18, self-tested at every start [C: main_psp.c cde_selftest]), and the two copy kernels the ME also runs;
   - ME: the plain copy kernel, then its write-back of the destination (timed);
   - DMA: `sceDmacMemcpy` (a firmware call; no register of ours) at 256 B, 4 KB, 64 KB, 256 KB and 2 MB (per-call cost and rate), and `sceDmacTryMemcpy` of 2 MB (whether it returns before the copy is done). The source is written back before each copy, and the destination invalidated before it is read.
   - The DMA wait: a low-priority thread counts while the main thread waits in a 2 MB `sceDmacMemcpy`. If it counts, the wait sleeps and the main CPU is free during the copy. Only then the DMA helper thread is made and the DMA turns run.
5. **Speed (probe 2), each core alone.** The kernels of me_probe_kern.c, compiled once under the ME's rules (no libc, no system call, no jump table) and run by both cores: the main CPU calls them at their user address, the ME at the kernel alias of the same physical code, so both caches see the same indexes. Each core times its own runs (main: the µs clock; ME: Count, calibrated).
   - in cache: an integer round (64 dependent addu / xor, plus the loop's 2) and a float round (16 dependent add.s / mul.s pairs, plus 3);
   - D-misses over 4 MB (65,536 lines): a dependent chase in random order (latency) and one load a line, four lines at a time (throughput).
6. **Each core's cost to the other (probe 3, at rest).**
   - the main CPU's kernels while the ME runs the paced stream, then the burst stream (the streams of Part 2), with what the ME moved meanwhile and its busy share;
   - the ME's kernels while the main CPU copies 1 MB over and over;
   - both cores' kernels while the DMA copies without a pause.
7. **Stores.** One store a line over 4 MB on each core (a miss plus the write-back of the line it evicts), then the write-back of the cache (main) or of the range (ME), timed.
8. **I-misses.** The existing probe's 1 MB of generated code on both cores: jumps to a new line in random order, and straight-line code. The ME invalidates its own I-cache first (index invalidate, as the start stub does).
9. **Floats (probe 1).** For this job only, the ME sets its own FCR31 to 0x01000001, the main CPU's rule [C: main_psp.c line 613]. Count and FCR31 are CPU registers inside the ME, not hardware registers. Both cores run the same function, at the same physical code, on 67,136 input pairs:
   - all pairs of 40 special values (signed zeros, smallest and largest denormals and normals, infinities, the NaN patterns the game makes 0xffffffff / 0xfffc56e0 / 0x7fc00000, 1.0, -1.0, 0.5, and values near a rounding edge), then 65,536 random words (every class: NaN, infinity, denormal, zero, normal);
   - the ops: add.s, sub.s, mul.s, div.s, sqrt.s, abs.s, neg.s, cvt.s.w, cvt.w.s, trunc.w.s, round.w.s, ceil.w.s, floor.w.s, and the compares c.eq, c.lt, c.le, c.un;
   - each core writes its results to its own array, plus FCR31 after each op's batch. The main CPU compares them word by word and prints, per op, how many are equal and the first three that differ (inputs and both results).
10. **The ME's claimed-line copy.** A 64 KB copy checked first, then 2 MB only if the check passed. If the ME stopped in the floats or here, it is started again, cold, for the turns (its caches set up with Index Store Tag, so no line the stopped ME held is ever written back).

Every job on the ME ends with its whole D-cache written back and emptied: the main CPU reuses the buffers after it, and a line left in the ME's cache could later be read in place of what the main CPU wrote there [C: me_probe_me.c run_job].

## Part 2: during the game, in turns (probe 3)

The turns are `MGS2_MEP_CYCLE=1000:100:0,1,0,2,0,3`. These are 100-vblank windows with the same slot rule as the speed-up turns (boot.c opt_window), so the existing window lines time them. Each test window sits between two windows with the ME at rest. Everything else is gl14's default: T1 and T2 on, C1 off, no speed-up turns.

| mode | the ME / DMA does, without a pause | stands for |
|---|---|---|
| 0, rest | ME at rest (halted), no DMA | gl14's all-on steps, with the ME started |
| 1, P | ME: reads 64-byte lines in order over a 3,456 KB window of game RAM (4 MB in); after each line, 291 dependent integer instructions (97 x 3, one cycle each: the time the model gives 177 of the picture's instructions at 1.647 cycles each); writes one line in 11 to a 64 KB ring | the picture on the ME: the model's mix of instructions and misses for the deck's picture side (46.5 ms of instructions, 14.2 of D-misses a step) [C: gl12's model, study section 5] |
| 2, B | ME: copies the same window into the ring as fast as it can, over and over, with plain stores (each ring line is read before it is written) | the worst case, and the ME making the packet copy itself |
| 3, D | DMA: a helper thread copies 64 KB from the window to the ring with `sceDmacMemcpy` every 2,757 µs, 3.4 MB per 150 ms, each source range written back first as a hand-off would. A period the thread is late for by a whole period is dropped, not made up in a burst, and counted. Only if Part 1 found that the wait sleeps; else the window runs as rest | the system's DMA making the packet copy (a step with transfers reads about 3.4 MB [C: study section 3]) |

- **What is measured.** The main CPU's ms a step in P, B and D windows against the mean of their two rest neighbours, per part of the route. tools/dev/mep/mepwin.py reads it with g14win.py's method. P's cost as it is stands for the picture on the ME. For the copies, the cost is scaled by the rate [I]: what the picture needs (3.4 MB a step at the rest steps' time) over the rate measured (MB per second of the window: the ME's over its busy time, the DMA's over its periods). Given gl14's log as well, it sets the rest windows against gl14's all-on windows at the same vblanks: the cost of the ME started and resting, with this build's code placement.
- **What the log adds per window.** A "mep vb" line: the mode; the ME's blocks, MB read and written, busy ms, rests (and how each ended) and kicks; the DMA's calls, MB, ms in the calls, ms writing back, errors and periods dropped.
- **Where the hook runs.** Mode changes happen in the per-vblank wrapper after main_psp.c's step timing. They fall between two steps, outside the timed time [C: main_psp.c host_vb_begin / host_vb_end].
- **How the ME is woken.** Only when a stream window starts (a kick, 136 µs [C: v30me6c]). Within a window the ME reads its mode from an uncached word between 64 KB blocks; back at rest, it halts until the next kick.
- **What the ME reads.** Game RAM, through its cache. Lines it reads are never dirty in the ME's cache, so nothing of the game's is ever written back. The ME writes only its own region, the ring and the control block.

## Safety and checks

- **Registers.** The ME start and rest use only the registers OKed for v30me6c (Sony's start, the wake at 0xBC100044, the ME's interrupt enable and flags at 0xBC300008 / 0xBC300000, the memory range). The ME also writes two of its own CPU registers: Count (once) and FCR31 (the float job). The DMA goes through the firmware call only.
- **Game state.** The game's state is untouched. The ME and the DMA only read game RAM; they write the ring, the ME's region and the control block. The boot buffers in game RAM are zero again before the game is loaded. The state hashes must equal gl14's on x86, the emulator and the PSP.
- **Emulator.** The emulator has no ME. There the P and B windows do nothing, and the D windows run the firmware call as the emulator models it. The 1.20.4 runs (434 hashes, 311 vuh) and the master run (199 digests, 16 shots) must equal gl13n's, the reference with C1 off in every frame.
- **Deadlines.** An exception on the ME is a silent hang [S: media-engine.md section 2]. Every wait on the ME has a deadline, every vblank checks the ME's exception word, and a P or B window with no block done counts as a stop. Then the log says so, the ME is held in reset, the probe stops and the game goes on.
- **Memory in the game.** The heap is gl14's except that it starts 64 KB later, because the code grew (heap block 0b1eee00 against gl14's 0b1dee00; at the start, the same 12 KB arena with 4 KB in use) [C: the emulator logs]. So its lowest headroom is 64 KB below gl14's: about 312 KB on the PSP [I: gl14's estimate, 376 KB]. The 80 KB block and the thread's stack come from what is free outside the heap: 764 KB on the PSP at gl13's start, largest block 512 KB [C: gl13 run 1 log line 228]; in the emulator 93 KB less is free there than in gl14 (671 KB, largest block 431 KB).
- **Build 1's lesson.** The first build took its boot buffers from the heap and freed them. Its ME block stayed in the heap above them, so the heap could not shrink back (2,140 KB, 2,055 KB of it free in 3 chunks) and the game's pools went higher. The lowest headroom in the emulator was 5,192 KB at vb 1, against gl14's 5,612 KB: 420 KB less [C: build 1's log lines 261 and 3640; gl14n's line 3242]. On the PSP that would have run out [I]. Build 2 takes the buffers from game RAM before the game is loaded, and the block from outside the heap.
- **Code placement.** The new units go after gl14's objects, with pads so that gl14's code keeps its addresses and its data their offsets modulo 8 KB (v30me6.sh's method: tools/dev/padfit.py). So the rest windows can also be set against gl14's own run.
- **Power.** Test runs on the OSTENT, as always. battery.txt is as in gl14.

## How the answers decide the plan (study section 3)

- **s.** The ME's three costs times the model's counts give the ME's ms a step for each picture part. With the main CPU's part, that gives the two-core step on the deck, inside and in the calls.
- **Floats.**
  - All equal: the converters can move as they are.
  - Some ops differ: those ops need a fix-up on the ME, or their converters stay on the main CPU.
- **The packets.**
  - A cheap DMA copy that gives the CPU back, with D windows within noise of rest: the DMA copies each transfer's reads.
  - Otherwise the ME copies them, at its copy rate, if B's cost is small.
  - Otherwise the ME reads each transfer before the game writes into it again. This needs no copy, but a stricter hand-off.
- **Sharing.** P's cost is what the main CPU loses with the picture on the ME. It goes into the two-core estimate.

## The build (build 2, 2026-11-02 15:24)

- **Link.** psp/gl14mp.sh on gl14's build d. gl14's 3,219 code sections of its own objects keep their addresses. The library code moved +0x9f88 (196 of 294 sections; the rest +0). Data moved by multiples of 8 KB (g_mem 0x09084100, gl14's 0x09076100). Pads 0x14f0, 0x1d4, 0x1d98, 0x1cf4. The overlay arena is at 0x08c00000 (gl14's 0x08bf2000), each overlay gl14's size. No new library function is linked (sce* calls apart): gl14mp.sh checks it, because a new one would move the libraries' code. EBOOT md5 7b063663.
- **Emulator, build 2 [C]** (the scratchpad's mp/mp_chk.sh and mp/mpcmp.py):
  - Boot lines (all three runs): the window in game RAM 0 words not zero before, 4,352 KB used at most, 0 words not zero after; the 80 KB block at 0bdbc000 in user memory outside the heap; the boot probe 0.78 s; the DMA wait gives the CPU back (the other thread ran 94% of a 2 MB copy), so the DMA turns run.
  - 1.20.4 `emu hw` (NOPROF): to the end; 434 hash lines, 311 vuh, 199 frame digests and 16 shots equal gl13n's. Heap block 0b1eee00 (gl14 0b1dee00); its highest break 0b84d000 at vb 23,675 (gl14n 0b83d000 at the same vblank), so the lowest headroom 5,548 KB against gl14n's 5,612: the 64 KB, no more. Outside the heap 643 KB free at the end (largest block 431 KB).
  - 1.20.4 `emu hwmem` (texture pool 2,688 + 5,120 KB, so the heap is the PSP's size): to the end; 434 hash lines, 311 vuh and 16 shots equal gl13n's; the lowest headroom about 300 KB, at vb 23,675. The 199 list digests are equal but 64 frame digests differ from gl13n's: the frame digest also covers each texture's record, texels and palette, and the pool is 7,808 KB there against the reference's 2,688. gl13's own build run the same way (`TAG=g13m`, sbg13/emu) gives the same 434 hash lines, 311 vuh, 199 frame digests and 16 shots as this run, so the difference is the pool, not the probe. gl13 that way has its lowest headroom at about 392 KB against its PSP run's 404 KB, so this build's should be about 312 KB on the PSP [I].
  - PPSSPP master (the delivery's files, MGS2_EXC=0): to the end; 434 hash lines, 311 vuh, 199 frame digests and 16 shots equal gl13's master run.
  - The D windows: 61 in the run, a median of 968 calls of 64 KB a window (60 to 6,492), 0 periods dropped, 0 errors. The calls times 2,757 µs match each window's time: vb 31,600 made 1,370 calls in 100 steps of 38.0 ms. The emulator's steps are short (no cache misses); on the PSP a 100-step window takes 6 to 15 s, so 2,200 to 5,400 copies [I]. The copies also run outside the steps (6,492 in 15,901-16,000, where a state hash at every vblank takes time between the steps), so the reader takes the DMA's and the ME's traffic per second of the window, not per step.

## Run 1 on your PSP (2026-11-02): the answers

You started it 17:02 on the OSTENT; the files came 17:56 (`prof/hw_gl14mp/run1/`: log.txt bf56e0fb, battery.txt 77f49c2e, crash.txt 39d87dbf, steps.csv 5c94b896; the 16 shots in the container, /home/claude/hw_gl14mp/shots). Reader: `tools/dev/mep/mepwin.py <log> <gl14 run 1 log> <battery.txt>` (means); the medians by the scratchpad's mepmed.py.

- **The run [C].** To the end: vb 37,800 at battery t 2,801 s, **46.7 min** (gl14 45.9). crash.txt: the catcher's start lines only. The ME started as in v30me6c (stage 3 of 3, Sony's steps, the check words right), took 162 kicks, had no exception and no stop in a turn, and was held in reset at the end. The DMA made 188,495 calls (11,780.9 MB) with 0 errors; 23 periods were dropped, all inside.
- **Exact [C].** The 434 hash lines and 311 vuh lines equal gl14's run. The 199 frame digests (all 4534b532d7799a2c) equal this build's emulator runs, which are gl13's because C1 is off here. The 16 shots equal gl13's PSP shots byte for byte.
- **Memory [C].** The lowest heap headroom was about 340 KB: gl14's 404 less the 64 KB the heap starts later, as the emulator said (my estimate of 312 was low). At the end it was 432 KB, with 643 KB free outside the heap (largest block 431 KB).
- **Power [C].** The OSTENT alone (external power 0, charging 0 in all 661 readings). 58% / 3,972 mV -> 45% / 3,912 mV (939 -> 738 mAh by the HOME press; gl14 1,270 -> 1,026), 20 C, CPU 333 / bus 166 throughout. The gauge moves in steps too coarse to time the 100-vblank turns, so the ME's own draw cannot be read from it.

1. **Floats: all equal [C].** 17 ops (add, sub, mul, div, sqrt, abs, neg, the conversions and roundings, four compares) ran on 67,136 input pairs each (the 40 special values in all pairs, then random words) under FCR31 0x01000001. Every result and every FCR31 afterwards was equal bit for bit on the two cores. So the converters' float code can move to the ME as it is.
2. **Speed: s is about 1.0 [C].** The same kernels at the same addresses, the ME's time over the main CPU's: ALU 0.996, FPU 0.996, a load a line over 4 MB 0.975, a pointer chase 0.977, a store a line 0.987, a jump to a new code line 0.976, straight-line code from RAM 0.960. The ME's Count runs at 331.343 MHz. So the ME runs our kind of code as fast as the main CPU, its misses 2-4% cheaper, and the study's "s 1.0" column holds (me_picture_study.md "The answer").
3. **Sharing the RAM [C].** The main CPU's ms a step in each test window against its two rest neighbours (medians; n windows in brackets where few):

| moving memory | to vb 15,600 | call 1 | viewfinder | call 2 | deck waiting | inside |
|---|---|---|---|---|---|---|
| P: the ME's picture-like stream (reads 55 MB/s, writes 5) | -0.02 (-0.1%) | +0.70 (+1.2%) | +2.33 (+2.2%) | +0.84 (+1.4%) | +0.50 (+0.3%) (1) | +1.39 (+1.5%) |
| B: the ME's copy (91-92 MB/s each way) | +2.97 (+6.9%) | +3.20 (+5.4%) | +2.64 (+4.1%) | +2.92 (+4.7%) | +7.14 (+4.8%) (2) | +4.24 (+4.7%) |
| D: the DMA copy (`sceDmacMemcpy`, 22.7 MB/s) | +4.15 (+8.5%) | +7.56 (+12.6%) | +7.05 (+9.3%) | +7.19 (+11.6%) | +17.20 (+11.5%) (2) | +12.58 (+13.9%) |

   - The ME started and resting costs nothing: the rest windows are within -0.15..+0.12 ms a step of gl14's everything-on windows at the same vblanks, so the pads kept gl14's code placement.
   - At the start, the main CPU's kernels with the ME's paced stream running took x1.000-1.009 their time alone, and a load a line x1.045 with the burst. With the DMA copying (337 MB/s) they took x1.22-1.24 (ALU, FPU) and x1.92-2.16 (loads, chase). The ME's loads took x1.27 while the main CPU copied.
   - Scaled to the picture's 3.4 MB a step at the rest steps' time [I: the loss taken as growing with the rate]: P about +1.0 ms a step inside and +0.2 on the deck; B +1.7 inside and +1.8 on the deck; D +20 inside and +17 on the deck.
   - Count wraps every 12.96 s, so the deck's windows (about 15 s) and the dense hashes' windows read short by whole wraps. The reader now adds them from battery.txt's wall times, and the rates come out the same in every part: P 54.8-55.6 MB/s, B 90.6-92.4.
4. **The copy [C].** 2 MB alone: the main CPU's memcpy 76 MB/s, its claimed-line copy 191, the ME's plain copy 103 (then 0.7 ms writing back), the ME's claimed-line copy 146; the DMA 196 MB/s in 2 MB calls and 24 µs for a 256-byte call, and its wait gives the CPU back (another thread ran 99% of a 2 MB copy). In the game the ME's copy ran at 91-92 MB/s each way.

**What this decides** (the rules in "How the answers decide the plan") [I unless marked]:
- **Floats equal:** the converters move as they are. **s about 1.0:** the study's s 1.0 column.
- **The packets:** the DMA is out. At 22.7 MB/s it already costs the main CPU 9-14% of a step, and the picture's 3.4 MB a step would cost it 17-20 ms. The ME's copy is cheap for the main CPU (about 1.7 ms a step for 3.4 MB), but it costs the ME its own time: 3.4 MB at 91 MB/s is 37 ms a step, more than half of the ME's picture work. So the copy must shrink. Textures handed over by their upload identity instead of their bytes (step 0's identities; about 2.0 of the deck's 3.4 MB) leave about 1.4 MB of packets: 15 ms a step on the ME at 91 MB/s, about 11 at its claimed-line rate, or 7 on the main CPU with its claimed-line copy, wherever there is room. Which bytes need a copy at all is the next x86 question: within a step the game rewrites only a few KB of what it sent (me_picture_study.md section 3).
- **The two-core step** on this run's rest windows (= gl14 everything on, C1 off). The main CPU keeps the logic, the draw pipeline, IOP and sound, the game's own walk (5.05 ms a step on the deck, 2.93 inside, 3.75 in call 1: v30 runs 4 and 5), 1.5 ms of hand-off and P's cost. The ME takes the rest of the walk and the picture, at s 1.0:

| | today [C] | main CPU | ME | a step, with the copy on the core that has room |
|---|---|---|---|---|
| deck waiting | 149.6 (3.3 fps) | 80.3 | 71.2 | about 82-88 (5.7-6.1 fps) |
| inside | 91.3 (5.5 fps) | 42.6 | 51.5 | about 52 (9.6 fps) |
| call 1 | 58.8 (8.5 fps) | 23.4 | 37.6 | about 38 (13 fps) |

- Left out: what the move may give back to the game's code (alone in the main CPU's caches; v30's logic ran 2-3 ms a step below gl12's, study section 2), and the sound mix (about 7 ms on one core or the other).
- **Order:** the roadmap's "Next 1", alternating with Stage C. C2 (the people on the GE) is in its x86 build now. The ME's design comes after it: which bytes are copied and by which core, the picture code made ME-safe (its own pools and copies of memcpy / memset / memcmp, no printing), the hand-off queue with a deadline, and the take-back path.

## Files

- psp/me_probe.c (9c3c08d3): the main CPU's side. It holds gl1_glue.c's wraps plus the probe: memory, ME start, boot probes, turns, window lines, the DMA helper.
- psp/me_probe_me.c (972cac52): the ME's loop, its jobs and streams (hle_me.h's rules), linked with psp/me_rest.S (e47ffb2b).
- psp/me_probe_kern.c (5c0928cd): the kernels both cores run (the ME's rules; the main CPU calls the same code). psp/me_probe.h (fa785362): the control block and the kernels' list.
- psp/gl14mp.sh (48027827): the build on gl14's objects, with v30me6.sh's checks (nothing undefined in the ME's unit, no relocation in its read-only data, no small-data access), the pads, the library rule and elfcmp against gl14.
- tools/dev/mep/mepwin.py (c5fe5fd9): the reader of the run: `mepwin.py <gl14mp log.txt> [<gl14 log.txt> | -] [<gl14mp battery.txt>]`; with the battery file it adds Count's wraps from the wall times (2026-11-02, run 1).
