# The picture on the Media Engine (sizing study)

2026-11-02, task #130; your card 2026-11-01 21:52 ("After step 0"). A study only: no PSP build was made or run for it. [C] = measured or read in our code, logs and x86 runs; [S] = a public source, cited; [I] = my inference.

## The answer

- **The idea.** The main CPU runs the game's step and the small part of the GS walk the game itself sees. The Media Engine (ME), the PSP's second CPU, builds the picture of the step before: the walk again with the translator, the converters, the textures and the GE list. This is how the PS2 drew: its GS and VU1 worked while the EE ran on.
- **What it would give [I].** gl12's PSP run, everything on, ms a step (fps = 500 / ms). s is the ME's time for our picture code over the main CPU's (1.0 = as fast; the public benchmark says the two cores run at the same speed, section 5).

  | | today | main CPU would keep | ME at s 1.0 | ME at s 1.3 | a step at s 1.0 | at s 1.3 |
  |---|---|---|---|---|---|---|
  | deck, waiting | 153.4 (3.3 fps) | 79.9 | 80.0 | 104.0 | 80.0 (6.3 fps) | 104.0 (4.8 fps) |
  | inside | 95.4 (5.2 fps) | 41.5 | 58.3 | 75.8 | 58.3 (8.6 fps) | 75.8 (6.6 fps) |
  | codec call 1 | 65.3 (7.7 fps) | 22.8 | 47.7 | 62.0 | 47.7 (10.5 fps) | 62.0 (8.1 fps) |

- **These numbers leave out one cost:** giving the ME what the game sent. The game writes into some of its data after starting a transfer, so the ME needs a copy of what it reads, or must read it in time. A full copy on the main CPU would cost 8-15 ms a step [I]; section 3 has a cheaper plan, which the probe sizes.
- **Exactness.** The game's state does not depend on when the picture is built: on the x86 twin with the picture built one or two steps late, every state hash equals the normal run's [C, section 3]. With a copy of what each transfer reads, every GE list equals today's on the three routes; the frames that still differ differ only in textures whose source the game refilled after uploading them, a fault today's renderer has too, which the same fix cures (sections 3 and 9) [C].
- **For 25 fps** each core would get the whole 20 ms of a step instead of sharing it. The game side alone is 79.9 / 41.5 / 22.8 ms a step on these three stretches and the picture side 80.0 / 58.3 / 47.7, so the ME does not replace the logic work or Stage C: it lets the two be cut side by side, each to 20 ms, instead of both together to 20 (section 6).
- **Recommendation.** First a small probe build, after gl13's run is read: the ME's speed on picture-like code, its floats under our FPU setting, how much it slows the main CPU when both use the RAM, and how fast a copy can be made (section 7). Meanwhile write Stage C's new code so that it can run on the ME (no libc, its own pools, one clean hand-off). Then decide with the probe's numbers.

## 1. How it would work

- The PS2 game builds a frame's packets in one step and starts their DMA at the start of the next. On the x86 twin the transfers of a step with transfers all come in its first 3.5% (the first at 1.6%), deck route, 153,211 transfers in 11,739 steps [C: me9 `out_dd1.log`, "me9 totals"].
- Today the main CPU walks each transfer (DMA chain, VIF, GS) as it comes, and the translator and converters run inside that walk.
- With the ME: the main CPU keeps a walk of what the game sees (section 2) and hands the transfer to the ME; the ME walks it again for the picture, converts, and writes the GE list; the main CPU starts the GE on the finished list and shows the frame.
- Every other step is shown, so the ME has two steps for one picture. The picture would then appear one step later than now (20 ms at 25 fps) [I]. Holding it to one step would make the main CPU wait for the ME on the step after every shown step, which gives back much of the gain [I]. The x86 checks cover both: the picture built one step late (D1) and two steps late (D2).
- Inside and in the calls the ME's side is the larger. Keeping part of the picture work on the main CPU would balance the two, at best (main + ME) / 2: inside about 50 ms a step (10 fps), call 1 about 35 (14 fps) [I]. Splitting one frame's picture between two cores is more work than moving all of it, so that would come later.

## 2. What the game sees (stays on the main CPU)

- What the game reads back from the walk (DMA and VIF registers, the GS's signal and finish events, interrupts) comes from the runtime's own walk, which stays at the transfer [C: the me9 x86 harness runs the translator from a queue with that walk unchanged].
- The decisions the picture needs from the game's counters (whether a step is shown or reduced, where a frame begins) are taken at the real step end and passed along [C: same harness].
- The rain and splash thinning reads the game's RAM; run at the step's first transfer it gave the same picture on the deck route [C: me9 `out_ds2.log`]. It stays on the main CPU and passes its result.
- The cost of that walk alone, before the picture was live, on the same windows: 5.05 ms a step on the deck, 2.93 inside, 3.75 and 3.52 in the two codec calls [C: v30 runs 4 and 5, prof/hw_v30/log_run4.txt and log_run5.txt].
- v30's logic on those windows was lower than gl12's: 40.0 against 41.95 on the deck, 18.4 against 21.1 inside, 7.35 against 9.32 in call 1 [C]. Part of that may be the picture code pushing the game's code and data out of the caches, which the ME would end [I]; the builds differ in other ways too, so the table does not count it.

## 3. Does the game change the picture's inputs before the ME reads them? (x86 twin, task #130 runs)

The me9 harness (`/home/claude/me9`, container only; boot/xlive_chk.c `MGS2_XLIVE_DEFER`, `MGS2_XLIVE_SNAP`) queues each transfer and runs the translator and the frame work late: at the end of the step the transfer came in (1 step) or at the end of the step after it (2 steps), as a second CPU would. With the copy (SNAP=2) it copies what each transfer's DMA chain reads at the moment the game starts it, as the main CPU would before handing it over, and counts what the game changed before the late run.

| route | late by | copy at the transfer | state hashes | frames differing | of them with a different GE list |
|---|---|---|---|---|---|
| deck (to vb 25,200) | 1 step | no | 252 / 252 equal | 1,339 of 11,766 | 8 |
| deck | 1 step | yes | 252 / 252 equal | 1,275 | 0 |
| deck | 2 steps | no | 252 / 252 equal | 5,823 | 3,187 |
| deck | 2 steps | yes | 252 / 252 equal | 1,275: the same frames, each equal to its 1-step-late frame | 0 |
| indoor route (to vb 37,800) | 1 step | no | 378 / 378 equal | 6,358 of 17,664 | 21 |
| indoor route | 1 step | yes | 378 / 378 equal | 959 | 0 |
| codec route (to vb 40,000) | 1 step | yes | 400 / 400 equal | 3,968 of 18,931 | 0 |

[C: me9 `out_dd1.log`, `out_ds2.log`, `out_d2n.log`, `out_d2s2t.log`, `out_id1.log`, `out_is2.log`, `out_cs2.log` against sb9's x1 references (deck `out_d2on.log`, indoor route `out_i2on.log`, codec `out_c2on.log`); `sumrun.sh`, `picdiff.py`]

- **The game's state never depends on when the picture is built:** every state hash equal in every run above [C].
- **With the copy every GE list is equal; what still differs is one texture.** Every differing frame saved from the copy runs (title vb 1,757; late deck 23,269; indoor route 26,007 and 36,627-37,799) differs in one texture only, the same kind each time: a 4-bit palette texture of 512 x 256 texels (71,808 bytes as converted, with its edge), in 1,200 to 31,741 of those bytes, its palette equal [C: the saved frames, me9 `gerdiff.py`]. The texture converter reads a texture's source from EE RAM by its address when a draw needs it, and the game refills some of its sources after uploading them; today's renderer reads them then, the late runs later, and either can differ from what the GS was sent.
- **Which is closer to the PS2.** The texture check (`MGS2_XLIVE_TEX=1`: it keeps the bytes each upload sent and compares every conversion with them), deck route: 718,144 of 13.6 billion converted texels differ from what was sent in the normal run, 204,234 with the copy two steps late; every palette entry equal in both [C: `out_dntex.log`, `out_d2s2t.log`, the route lines]. By stretch: the title (vb 1,700-4,199) 568,812 against 45,136 (the same 45,136 one step late, `out_stex.log`); the opening's text about the same (7,758-43,100 a window against 7,704-42,800); the codec call a little more with the copy (1,029-1,050 a window against 784-800). In the late deck the check finds no texel that differs from what was sent in either run, yet the frames differ in that texture. That check compares the conversion the map makes at each use, so a second one compared what the PSP's texture cache keeps with the map at every use, at the PSP's budget (`MGS2_XLIVE_TK=2688`; deck route, normal and one step late with the copy): in each run 1,530,518 texture uses compared, every palette equal, and the only uses that differ are the codec call's 16-bit image's, two a step from vb 15,759 to 19,898 (4,103; 100 in every window), where the check compares the GE's form of the texels (alpha bit set from TEXA) with the GS's, so they differ by design in that bit (the first texel 0x8004 against 0x0004; the rest of that image is not compared); none in the late deck [C: me9 `out_tkn.log`, `out_tkd.log`, the route lines]. So the late deck's frames do not differ in the cache; where they do is still open. For open item #84: the codec text page is the same in the cache and the map at every use, while the map differs from what was sent (above), so the stale text comes from the refilled source, not from the cache [C on x86's deck route; on the PSP I]. The cure is the same for today's renderer and for the ME: convert from the bytes as they were sent (section 9).
- **Without the copy, the game's later writes reach the picture:** on the deck one step late, 8 frames' GE lists and 56 more frames' content differ beyond the copy run's [C]. On the indoor route it is most of the picture: 21 frames' GE lists and 5,378 more frames' content differ beyond the copy run's 959, and from vb 26,000 to the end 5,083 of the 5,600 frames differ only without the copy [C: `out_id1.log`; `picsets.py`, `picstretch.py`]. Every frame that differs with the copy differs without it too [C]. So one step late is not safe without the copy either. What differs, in frames saved from a normal and a no-copy run of the indoor route [C: me9 `ger/jn*.ger` against `ger/jd*.ger`, `gerdiff.py`]: in the two codec calls (vb 15,759 and 26,188) 416 of the 549 screen-space vertices lie 9 to 10 pixels across and 1 down from the normal run's, which moves the screen boxes of 716 of the 746 batches: the game had moved them in its packet after the transfer; at vb 23,285 the whole frame differs (827 batches and 95 textures against 860 and 105: the late run walked a packet the game was already building again [I]); at vb 31,663 only the 4-bit texture, in 57,015 of its 71,808 bytes (with the copy 1,200 to 31,741).
- **Two steps late needs the copy.** By then the game has rewritten much of what the transfers read: 216.9 MB over the deck route, about 18.5 KB a step with transfers, in 7.9 million ranges of 25,589 transfers, against 4.18 MB (nearly all the boot's staging area) one step late [C: the two runs' snap totals]. Without it, two steps late on the deck, 5,823 frames differ and 3,187 of them in their GE lists (one step late: 1,339 and 8): 696 of the title's 1,333 frames and 3,740 of the 4,833 from vb 15,000 on differ only without the copy [C: `out_d2n.log`; `picsets.py`, `picstretch.py`].
- **The copy is large; what changes is small.** A step with transfers reads 3,362.7 KB on average on the deck, 3,607.6 on the indoor route and 2,952.1 on the codec route, 7,785.4 KB at most, 3.8% of it DMA tags [C: "me9 snap totals"]. About 2.0 MB of the deck's are texture uploads (23.9 GB in 251,529 uploads over 11,739 steps with transfers), nearly all repeats of content seen before (158 new texture keys over the route) [C: the texture check's totals]. 2.8% (indoor route) to 4.3% (codec route) of the copied bytes lie in the game's own image (below 0x1edbc0), the rest above it [C: "by place"]. What the game changed between the transfer and the late run, after the boot's staging area at 0xc00000 (4.17 MB at vb 0, whose uploads the renderer already keeps as its own copies): on the deck a few small runs of 16-208 bytes in about 15 steps and the codec text page at 0x1ed8500 [C]; on the indoor route 58,540 bytes in 12,671 copied ranges: the codec text page in the two calls (one range with 0.2-3.5 KB changed in each of 13 windows; the first three printed are at its address, 0x1ed8500) and, from vb 30,500 to the end, 34,785 bytes spread over 12,574 ranges (about 3 bytes each) above the game's image [C: the per-window snap lines]; on the codec route 456 ranges in 260 transfers in all [C].
- **What a copy would cost [I].** Copying 3.4 MB is about 54,000 cache lines: by the cost model's weight for a miss, 14 ms to read them and up to as much again to write the copy, so 15-30 ms for a shown step, 8-15 ms a step. Too dear for the main CPU. So the ME plan needs the texture uploads not copied but identified (the upload identities step 0 uses; bytes kept only for sources seen to change, which the identity code already marks), and for the packets either a copy made off the main CPU (by the ME itself, or by the system's DMA copy) or the ME reading each transfer before the game writes into it again. The probe (section 7) measures the copy's speed.

## 4. What moves and what stays (PSP, gl12 run 1, everything-on windows, ms a step) [C: prof/hw_gl12/run1/log.txt]

| stretch | logic | draw pipeline | IOP | GS walk (with translator) | picture | total |
|---|---|---|---|---|---|---|
| deck, waiting (6 windows) | 41.95 | 29.04 | 2.38 | 32.89 | 47.12 | 153.38 |
| inside (30) | 21.12 | 13.76 | 2.23 | 22.58 | 35.74 | 95.43 |
| codec call 1 (22) | 9.32 | 5.84 | 2.43 | 18.58 | 29.08 | 65.25 |
| codec call 2 (14) | 8.29 | 5.88 | 2.27 | 20.30 | 30.29 | 67.03 |

- Main CPU: logic + draw pipeline + IOP + the game's own walk (5.05 deck, 2.93 inside, 3.75 call 1) + a hand-off of 1.5 ms a step [I: cache write-back at each transfer, one wake (136 us today [C]), starting the GE on the ME's list; 1.5 is a generous allowance].
- ME: the GS walk with the translator + the picture, times s. In gl12's model of the deck that is the converters 23.0 ms, translator 15.3, walk 9.6, texture look-ups 11.7, frame and batches 3.6, GE list 3.2, GS front end 2.3 and about half of libc's 5.7 [C: gl12's model window g12on, fitted weights; the model's whole step is 144.8 ms against the PSP's 153.4].
- gl13's step 0 takes 3.3 model ms a step off the deck (141.5 against gl12's 144.8 on the same window), 4.7 inside and 1.8 on the codec route, most of it on the picture side (2.7 of the deck's 3.3) [C: gl13's model, `psp_harness.md` "Build gl13"]; the table uses gl12 because its PSP numbers are measured.
- The sound mix is not in these numbers (the picture builds run without it). The full game needs it: 7.53 ms a step on the main CPU [C: v24s] or about 7 on the ME [I: idle polls, sound_notes.md]. On the ME it would share the ME with the picture.

## 5. What we know about the ME

- Same core speed as the main CPU: a benchmark that splits work between the two gets x2.0-2.9 at the same clock, and its FPU results were bit-identical to the main CPU's on a PSP-1000 and a 3000 [S: psp-me-bench, web_research_2.md §9; third_party/psp-developer/references/media-engine.md §1].
- **What s depends on.** In gl12's model of the deck the picture side is 65% instructions and 35% cache misses (instructions 46.5 ms, I-misses 11.0, D-misses 14.2 of 71.6 model ms a step, with half of libc) [C: gl12's model window g12on, the fit's three costs]. Instructions cost the same on a core of the same speed, so s is about 0.65 + 0.35 x (the ME's cost of a miss over the main CPU's) x (its number of misses over today's) [I]: a miss 1.5 times as dear gives about 1.2, twice as dear about 1.35. Alone on its core the picture code would also miss less, since the game's code and data no longer push it out of the caches [I].
- Its own caches: D-cache 16 KB, 2-way, 64-byte lines [S: media-engine.md §1]. No VFPU in use on the ME [S: same]. Our picture code uses no VFPU and no inline assembly [C: ge/*.c and psp/xl_psp.c in sb9].
- The caches of the two cores are not kept in step: the writer writes back, the reader drops its lines, small flags live uncached [S: media-engine.md §3]. A D-cache is 16 KB, so a write-back of the whole main D-cache at each transfer is cheap [I].
- No libc and no system calls on the ME [S: media-engine.md §3]. Our picture code allocates in about 29 places (texture cache, batches, frame, GS memory map; mostly set-up and doubling) and calls memcpy / memset / memcmp about 200 times [C: grep of the 13 files the PSP build compiles, 6,028 lines]: the ME version needs its own pools and its own copies of those three.
- An exception on the ME is a silent hang [S]: the main CPU needs a deadline and must take the picture back itself (today's code path, kept in every build).
- Another port moved GE list building to the ME with no failures, and its ME writing 30-60 KB of GE words a frame changed the main CPU's logic time by 0.0 ms [S: media-engine.md §3, §6]. Ours would read about 3.4 MB a step on the deck (above): the RAM sharing is the main unknown.
- The ME's clock register read 0 in our builds [C]; the reference says it counts once the ME writes it first [S: media-engine.md §3], so a probe can time the ME directly.
- We start the ME as Sony does and let it rest between jobs (v30me6c: three clean runs, about 2.75 hours without a stop) [C: memory mgs2-me-audio]. A busy ME draws more power [I]; test runs stay on the OSTENT.

## 6. The 25 fps budget with and without the ME

20 ms a step is 25 fps, 25 ms is 20 fps, 16.7 ms is 30 fps. Today (gl12, ms a step):

| stretch | game side (main CPU) | picture side | without the ME: both must fit in | with the ME: each must fit in |
|---|---|---|---|---|
| deck, waiting | 79.9 | 80.0 | 20 (-87% of 153.4) | 20 (-75% each) |
| inside | 41.5 | 58.3 | 20 (-79% of 95.4) | 20 (-52% / -66%) |
| codec call 1 | 22.8 | 47.7 | 20 (-69% of 65.3) | 20 (-12% / -58%) |

Plus the sound mix's ~7 ms on one side or the other. Without the ME both cuts must add up; with it each side has its own 20 ms [I].

## 7. The probes (one small build, after gl13's run is read)

1. **Floats:** the converters' float operations on both cores under our FPU setting (FCR31 0x01000001: round toward zero, denormals flushed), the same inputs, compared bit for bit. The benchmark's identical results were under the default rounding [S].
2. **Speed:** the same code at the same addresses on both cores: work in cache, data reads that miss the D-cache over a few MB, and code larger than the I-cache. That gives the ME's three costs for our cost model (per instruction, per I-miss, per D-miss), and with the model's counts the ME's time for each picture part, so s from real numbers.
3. **Sharing the RAM:** the main CPU's step times on the deck while the ME streams the reads and writes the picture would make (about 3.4 MB read, a few hundred KB written a step), in turns of 100 vblanks on and off, as our timing turns do.
4. **The copy:** the speed of copying a few MB on each core, and with the system's DMA copy (`sceDmacMemcpy`, a firmware call our SDK declares; no register of ours), and what each does to the other core's time [I: the numbers that decide section 3's plan].
5. Power comes with it: battery.txt with the ME busy, on the OSTENT.

They use only the ME registers you already OKed (Sony's start, the wake at 0xBC100044, the memory range) [C: me_startup_study.md]; the ME writes only its own clock register (a CPU register inside the ME). This is a new use of the ME, so I am telling you before it is built.

## 8. What else this costs

- Porting: the 13 picture files to ME-safe C (pools, own memcpy family, no printing), a hand-off queue with a deadline, the take-back path, cache handling at both ends. The checks stay as now: x86 first (the late-run harness above), then the emulator (PPSSPP has no ME: the same loop runs as a main-CPU thread there [S: media-engine.md §5]), then the PSP.
- The textures: keep the bytes of each upload from a source seen to change (the identity code already marks such sources and hashes them whole), and convert from them; it also fixes today's renderer (section 9).
- The packets: a copy of what each transfer reads made off the main CPU, or the ME reading each transfer before the game writes into it again, checked on x86 with the late-run harness (section 3) [I].
- The thinning's scan and the step decisions on the main CPU, passed with the transfer.
- One step more between the game and the screen (section 1).

## 9. Found on the way (fidelity, not speed)

Today's renderer converts a texture from EE RAM when a draw needs it, by the address it was sent from; the GS keeps what was sent. Where the game sends from a buffer and refills it, the two can differ. On the deck route (normal run, x86 with the texture check) that happens in 64 of 252 windows: the title area (vb 1,700-4,199, 0.01-0.7% of the texels converted there), the opening's text (vb 13,300-13,999, up to 0.11%) and the codec call's text page (vb 15,600-19,899, about 800 texels in every 100 vblanks); 718,144 of 13.6 billion texels converted over the route, and every palette equal [C: `out_dntex.log`, me9 `texsum.py`]. The text page is the game's 512 x 120 4-bit text image at EE 0x1ed8500, one of the two sources the Stage A notes found changing [C: memory mgs2-renderer-stage-a]; open item #84 (stale codec text) may have the same cause [I]. Copying everything sent is too much (3.4 MB a step with transfers on the deck, section 3), so the fix is to keep a copy, at the upload, only of the sources seen to change (the texture identities already notice a source that changes). New open item on the roadmap.
