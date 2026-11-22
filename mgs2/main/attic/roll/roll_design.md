# Ship roll in the rain: what moves the camera, and what it means for speed

Written 2026-11-22 01:50 UTC by the roll-study workflow (read-only studies in /home/claude/main/rollstudy, three designs, each checked by an adversarial verifier). Base: gl35 as delivered (tree /home/claude/t35, build_gl35). gl36 is being built by another agent and is not changed here.

Marks: [C] measured or read in code, [C model] the PspProf cost model, [I] inferred (basis named), UNKNOWN. The cost model has overrated ME savings on the PSP about 4x lately. Single estimates have been off by 2x. No figure marked [I] or [C model] here has been reached on the PSP.

---

## 1. Summary for Ffm

**You are right: the small camera movements are the game's way of showing the ship riding the sea.** More precisely, it is a slow **bob**, not a tilt. Once per game step, the camera code (camerad.c) moves both the camera and the point it looks at up or down by the same amount. Nothing rotates and the horizon never tilts. The ship itself does not move [C]. The bob is two waves added together [C code, matched to twin dumps within 0.0005 units in 293 of 293 records]:
- one 96 units high that repeats every 512 game steps;
- one 64 units high that repeats every 179 steps.

At most it moves 160 units [C]. The w00a stage script switches it on at vb 29,067 [C]. Raiden's actor can fade it out: using item 5 does that, and which item that is remains UNKNOWN.

What it means on screen: most of the ship moves about 5 PSP pixels up and down, and the nearest deck up to about 17 [I, from the converter's own vertices]. From one picture to the next the move is a fraction of a pixel [C]. On the PS2 the two waves last about 10 s and 3.6 s. On the PSP today they last about 45 s and 16 s, because they count game steps and the port runs about 4x slower in the rain [I, from the step rates]. They will speed up by themselves as the port gets faster.

Why it matters for speed: gl36 planned to reuse each ship piece's drawing work from the previous picture. Because of the bob, every ship piece's camera matrix changes in every picture, so that reuse never hits (0 of 254,907 pieces [C]). Everything else about the ship repeats exactly [C]. Only the 3 translation words of each piece's matrix change, plus the work that depends on them.

The bob cannot be changed in the game without changing the game. Rain, ripples, lights, sound and motion code all read it. Halving one of its waves for 4 pictures changed 7,252 values read outside the drawing [C]. It also changed how many times one light actor loops, and the game's state hashes differed at vb 29,700 [C]. So every option below leaves the bob exactly as the game makes it, and changes only how we draw it.

**The options** (the per-step figures are ME time in the rain wait. On the PSP, only about a third of a saving like this has shown up in the step time [I, gl35's keeps: -0.75 ms of ME time gave -0.2..-0.3 ms a step]):

| Option | What it is | What you would see | Exact? | ME saving: a ship piece / a rain-wait step | Work | Verdict |
|---|---|---|---|---|---|---|
| G. Change the bob in the game | Slow, shrink or stop the camera's bob in the game code | Less or no rolling | **No**: game state differs, and the hashes split by vb 29,700 [C] | Not costed (UNKNOWN) | Small | **Rejected**: breaks exactness and the "drawing-side cuts only" rule (card 2026-11-08) |
| S. Camera on the GE | Send the camera once a picture as the GE's view matrix, so ship pieces keep fixed matrices | Ship points move by up to 0.03 px in the rain and 0.18 px on the deck. 9.3% (rain) and 71% (deck) of points land on another 1/16-px step [C sim, PPSSPP-style] | **No**: GE words and pixels differ, and the real GE's maths is UNKNOWN | about 5-8 us / 1.5-2.4 ms [I, unmeasured] | Medium | **Rejected**: cannot be proven identical. A GE matrix path was once 4.6 px off |
| D1. Exact patch inside the converter | Keep each piece's camera-free results. Every picture, redo only the translation part with the converter's own float maths | Nothing | **Yes** [C probe: 0 differing] | -1 to -3.4 us / -0.3 to -1.0 ms over gl36 [I] | 400-600 lines [I] | **Not buildable as written**: needs 512 KB outside the heap, and only about 116 KB is there [C emulator] |
| D2. Hold the ship between pictures | While the bob has moved less than 1 px, redraw the ship from the last full picture. Everything else is drawn live | The ship lags its own bob by up to 0.48 px [C probe], then snaps back about every 0.7 s on average and at most 1.6 s apart [I sim]. Flicker where things touch the deck is possible (UNKNOWN) | **No**: the picture differs (the game state does not) | 14-17 us per held piece / 0.5-1.8 ms expected, ceiling 3.2-3.8 [I]. Rain wait only, 0 elsewhere | About 350 lines [I] | **Not recommended**: it degrades the rolling you pointed out, for about 1-2% |
| D3. Exact per-piece replay with a camera tail | Keep each piece's whole converter result. Every picture, recompute only the camera-dependent tail, bit-exact | Nothing | **Yes** [C probe: 254,409 of 254,409 predictions bit-equal] | -2 to -6 us / -0.5 to -2 ms over gl36 [I]. Rain walk 0 to -0.3 [I] | About 650 new + 60 changed lines, 0.8-1.6 MB of memory [I] | **Best exact option, but small**: build only if gl36's PSP run passes the go rule (section 6) |

**Recommendation: keep the rolling exactly as the game makes it, and build nothing for the bob now.**

1. Read gl36's PSP run first.
2. Build D3's wait tail next only if that run shows two things: the ME is still the limit in the rain wait, and gl36's own replays saved at least half of what was estimated for them on the PSP.
3. Otherwise drop the ship's per-piece work as spent and move on as planned (the main CPU in the rain).

Why:
- G and S cannot stay exact.
- D2 visibly changes the effect you just pointed out, and it saves only about 1-2% of the rain step.
- D1 has no memory to run in.
- D3 is exact and buildable, but its realistic gain is about 0.5-2 ms of ME time a step. That is roughly 0.2-0.7 ms of step time out of about 87 [I], close to its own stop line. gl35's keeps and gl36's replays are the same kind of work, so gl36's PSP numbers are the cheapest way to tell whether D3 can pay.

---

## 2. What moves the camera (game side, all on the gl35 twin)

Twin: build_gl35/tw/x86/boot, run from rollstudy (run1.sh). Its EE hashes equal hashref_g22b at vb 29,000-29,600 [C]. The rain wait is stage w00a, loaded at vb 29,053 [C].

### 2.1 Where the matrix comes from
- A ship group's matrix at T+0..3 (the constant block's first four quadwords) is the mesh's local-to-clip matrix at mesh+0x40.
  - It equals W·VP (row vectors, v·M), where W is the mesh's world matrix at mesh+0x00 and VP is channel 0's world-to-clip matrix at 0x1DF950 [C].
  - It matches float32(VP·W) bit for bit in 7,594 of 7,594 sampled calls [C, renderer probe]. A check of 211 static meshes gives a maximum relative error of 7.3e-7 [C dump].
- VP is written by DG_SetCamera 0x11f130. That is called from DG_SetCamera2 0x11f448, which is called once per game step from camerad.c's act 0x10b720 (ELF). In this run the actor's work is at [0x17D350] = 0x137DAA0 [C].
- DG_SetCamera2 builds the camera like this [C code]:
  1. look-at(eye, target) (0x11ef58);
  2. invert;
  3. RotMatrixZ by eye.w (the roll slot);
  4. invert again;
  5. build V^-1, V, P, VP, S·V and the depth-biased P'·V and S'·V.
  
  eye.w = 0 in every record checked [C]. So there is no roll and no pitch, only heave.

### 2.2 The sway code (0x10b720, at 0x10b8d8-0x10ba2c) [C code]
- For camera slot i, it runs only when all three hold:
  - mode (cam+152) >= 3;
  - |A[i]| > 0.1 (A at 0x17D358+4i);
  - bit 0x4000 of [work + i·0x900 + 2348] is clear.
- If [0x1E6C38] & 6 == 0 (not paused), the phases advance once per step:
  - ph1 += s1, with s1 = 2π/512 = 0.0122718;
  - ph2 += s2, with s2 = 0.0351016 rad, so 2π/s2 = 179.0 steps.
- **h = A·sinf(ph1) + min(A2max, A)·sinf(ph2)**, using newlib sinf 0x16ab50, with A2max = 64.0.
- Both eye.y and target.y of a stack copy get += h before DG_SetCamera2.
- Other details:
  - a phase above 3,141,592 rad is reset to 0;
  - if |s1| <= 0.001, a constant offset A is added instead.
- A2max and s2 are ELF .data constants at 0x17D3B8 and 0x17D3BC [C].
- Sizes: |h| <= 160 units, and |dh| <= 3.43 units a step [C formula].
- Measured h [C]:
  - 12.1..139.8 over vb 29,600-29,700;
  - -156.7..32.8 over vb 31,000-31,100.
  
  The formula matches (swayed eye.y - still eye.y) within 0.0005 in all 293 records [C].

### 2.3 Who turns it on, and how it fades [C code + write watch]
- The camera actor's init (0x10bca0) zeroes A and the phases at every stage load.
- The w00a script command handler 0x3b5168 reads option 'a' (A = 96) and option 'c' (s1 = 2π/c, c = 512) at vb 29,067. It saves copies at 0x17D378/0x17D388.
- From vb 29,068, raiden.c's act (0x290ce0 -> 0x272740) moves A each step by 0.5 (GV_NearSpeedF): toward the saved 96, or toward 0 while the counter at 0x18F7CC is above 0.
  - That counter goes down by 1 each step.
  - The item-use handler 0x220d78 adds 1,250 to it (capped at 1,000,000) when item 5 is used, then plays a sound.
  - Item 5's name is UNKNOWN.
- At vb 29,600, ph1 = 532·s1, so the bob started at vb 29,068 [C].
- In the rain wait, A, the mode (6), the pause flags (0) and the roll (0) are constant in every sample [C].
- w01a has the same reads of A [C grep].

### 2.4 What changes in the camera, and what does not [C]
- Between two pictures in the wait, only the translation words of channel 0 change:
  - VP row 3 (+0x34..0x3c) and V row 3;
  - the camera position y (+0xB4);
  - the translation rows of the other matrices.
  
  All 3x3 rotation parts are bit-identical (dump diff, vb 29,600 against 29,601).
- VP rows 0-2 are bit-identical in 101 of 101 consecutive pictures (vb 29,600-29,700), and in 91 of 91 samples taken every 10 vb over 29,600-30,500.
- The still camera is one fixed eye/target pair for the whole wait: (7015.2, 5782.9, 21943.3) -> (5656, 1277.3, 15474).
- So the bob is exactly separable: VP_sway = T(0,-h,0)·VP_still. Every group's matrix changes only in row 3, by -dh·VP[1]. That is the same 4-vector for every group of a picture, whatever the group's W [C numeric].
- But the game's own float bits of VP_sway cannot be rebuilt from VP_still and h, because the two matrix inversions round in between [C code]. So the separability only explains why things repeat. It cannot be used as a factor that we apply ourselves.
- In the rain walk (31,000-31,100), the camera also turns to follow Snake in 100 of 100 pictures [C].
- On the deck before the rain (24,051-24,079) it turns by 11.7 degrees [C].

### 2.5 Where the values sit in EE RAM [C]
- Still eye/target: at camerad work +0x780/+0x790.
- Swayed eye/target: at work +0x850/+0x860.
- Integer copies of the swayed eye and target: at [0x18F6D4] (= 0x1E89A0) + 1180..1208.
- h for a picture = (channel 0 +0xB4) - (work +0x784).

### 2.6 Why a game-side change is out [C, read-hash twin build_gl35/xt35/boot]
- The poke `MGS2_POKE 29600:17d3b8 = 32.0` halves the second wave and changes nothing else. The only reader of A2max is 0x10b720, and its code path stays the same.
- Effect in vb 29,600-29,603: 7,252 loads outside the draw pipeline saw different values, all in the same order. The readers include:
  - rain.c 0x31af28, ripple.c 0x332180, rain_gas_pers.c 0x331160, c_light.c 0x322d38, emb_control.c 0x370688 and inf_cam.c 0x30fa60;
  - DG_SetPos, DG_PutVector and DG_RotVector users, and MT_ActMotion/2;
  - ELF 0x1138c8/0x113b90 (they read the swayed eye; I that they are sound distance and pan helpers).
- At vb 29,613, c_light.c's act made 2 fewer loads.
- At vb 29,700 the EE and the IOP state hashes both differ, while k_time stays equal.
- Of the code traced in those 4 pictures, no enemy-AI code read the sway [C]. Whether AI reads it over longer spans is UNKNOWN. It is moot here, because the hashes already differ.
- So any game-side change breaks exactness: a lower rate, removing it, or a still camera in channel 0. It is also against the "drawing-side cuts only" card (2026-11-08) and the "game logic stays the original's" rule.

### 2.7 Size on screen
- From the game's S·V (vb 29,650 dump, viewport 512): 1 unit of h moves a point 8,000 units away by 0.067 GS px, and a point 4,000 units away by 0.13 GS px [C formula; I for which distances are typical].
- From the converter's own vertices on the PSP's viewport: about 0.055 PSP px a unit at the nearest deck vertices (clip w 4,208), and about 0.016 at the median (clip w 15,262) [I from C probe]. Over the 320-unit range of h, that is about 17 px and 5 px peak to peak.
- The two estimates differ by about 2x after scaling from GS to PSP pixels. Which one is right is UNKNOWN (section 8). No decision depends on it.
- Per picture: |dh| has a median of 2.62 and a maximum of 6.75 units (a picture every 2 steps) [C probe].
- Timing: on the PS2 at 50 steps a second the waves last 10.2 s and 3.6 s. On the PSP at about 87 ms a step they last about 45 s and 16 s [I].

---

## 3. How the bob reaches the renderer (gl35 code, unchanged in gl36's diff) [C code unless marked]

### 3.1 The GE words of a df0 group (native fast ship piece)
- **WORLD** (vc_xform, ge/vu1conv.c:20):
  - words 0..8 = M's columns 0..2 (rows x, y, w) x 32768;
  - words 9..11 = column 3, the translation.
  
  Sent as WORLDN plus 12 WORLDD for every batch.
- **VIEW** = identity. Exception: the C1/C2 people passes load their own matrix, and c1_off puts identity back.
- **PROJECTION** = (1, 1, pa, -pb), with pa and pb taken from M.
  - Resent (17 words) when the float bits differ from the last batch's.
  - In the rain wait pa is constant at 1.0000763 [C probe], and pb varies from 100.00195 to 100.00586.
- **Viewport z words** carry the frame's depth map, which comes from the whole frame's w range at gl_end.
- **Camera-dependent work besides the words:**
  - the cut test xc_off: 8 box corners times the full float world, with a 2 px margin;
  - the depth note xc_zr_df0, and the c3z bound xc_df0_zin;
  - pb;
  - the 0x42 decal pairing, which compares 34 GerXform words.

### 3.2 What repeats in the rain wait
- **Renderer probe** (vb 29,601-29,699, 50 pictures) [C]:
  - W is constant for all 28,837 fast groups (120 meshes);
  - M's columns 0..2 are bit-equal to the picture before in 5,871 of 5,871 df0 calls;
  - the whole M is never equal.
  
  So 9 of the 12 WORLD words repeat, while the translation (3 words), pb and the viewport z change.
- **gl36 probe** (reuse_design.txt section 1) [C], 254,987 fast groups:
  - 254,907 were found in the picture before;
  - 0 had every input equal;
  - 104,468 differed only in the matrix;
  - for drawn groups, the vertices, strips and batch state were equal every time;
  - the cut outcome was equal in 99.9%;
  - 54.1% of the groups were cut.

### 3.3 Splitting the camera onto the GE (option S) [C sim; the GE model is I]
- The split: WORLD = W per group, VIEW = VP once a picture.
- Simulated the PPSSPP way (float32 WORLD, then VIEW, then the divide; 24-bit matrix words):
  - **Rain wait, 276,562 points:** median 0.0035 px from today's position, max 0.031. 9.3% land on another 1/16 px step. Every depth moves (max 11.9 Z24 units).
  - **Deck, 3,618 points:** median 0.042 px, max 0.175. 71% change step.
  - **Against the PS2:** in the rain about as close as today (median 0.0017 against 0.0026 px); on the deck worse (0.048 against 0.018).
  - Anchoring the translations at a fixed camera does not help (10.3% rain, 77.5% deck).
- How the real PSP's GE combines WORLD and VIEW is UNKNOWN. Today VIEW is identity, so the GE's internal precision cannot make the PSP differ from the emulator. With a real VIEW it could.
- Fog reads view z, so the camera would have to go into VIEW, not into PROJECTION [I].
- VIEW would also have to be reset around every people pass.
- **Verdict: not exact.** It would need a new picture baseline and could not be proven on the PSP. Rejected.

### 3.4 Patching instead of splitting (the idea D1 and D3 build on) [C code]
- The converter adds the translation last, in both of these:
  - xc_off computes cx = ((px·w0 + py·w3) + pz·w6) + w9 (xconv.c:127/136);
  - xc_zr_df0 computes w = ((x·a + y·b) + z·c) + t (xconv.c:101/106).
- Both builds use -ffp-contract=off and no fast-math (tw35.sh GF, psp/Makefile BASEF), and the Allegrex FPU has no fused multiply-add.
- So kept float32 partial sums plus this picture's translation give the same bits as today.
- Float addition rounds monotonically under every IEEE mode, including the PSP's round-toward-zero with flush-to-zero. So min over vertices of fl(P+t) = fl(Pmin+t) whenever the values are finite.
- c3z's bound adds t first, so it has to run unchanged.
- A sim of the cut, computed world first and camera after, gave the same outcome as today in 29,688 of 29,688 boxes [C sim].

### 3.5 Work that stays per picture in any design [C code / I]
- the front end (walk, register, A+D with real CLUT loads, PRIM);
- the texture look-up (the frame's texture table is per picture);
- the drawn areas (the K1 keep already replays them);
- putting each batch into the frame;
- gx_frame's list build;
- the decal pairing;
- the frame's depth map.

Camera-dependent PSP stages today: transform 4.8 + cut 4.5 + GE transform 4.6 + depth 2.2 = 16.1 us of about 61 us a group [C PSP, gl35 keep 0, each stage with one ~0.46 us clock read].

---

## 4. The designs, with the verifiers' corrections

PSP basis for all of them [C PSP, psp_harness "Build gl35"]:
- In the rain wait ME turns: ME about 87 ms a step, about 4.3 ms of it idle.
- The native ship is 18.4-19.2 ms a step, about 305 groups a step (about 18 vertices each), 60.8 us a group (keep 0) or 58.4 (keep 15).
- Main-CPU logic is 43 ms a step, plus 8.5 ms waiting on fills.
- Stages, us a timed group: walk 5.8, register 9.8, A+D 5.1, call rest 1.2, host call + GS state 6.0, transform 4.8, positions 4.3, cut 4.5, attributes 1.5, texture 8.4, strips 1.6, GE transform 4.6, depth 2.2, into frame 1.4, way back 2.8, drawn areas 3.4, walk end 1.0.

### 4.1 D1: rows-guarded reuse inside the converter (exact)

**What it is.** It sits in xc_call's df0 branch, on top of gl36's split of vc_df0_pos_k into vc_df0_xf_k + vc_df0_pv.
- Entry: 256 B per fast native group with three static streams, keyed by the streams' guest addresses and counts plus the header count.
- L1 (camera-free) holds:
  - the box and i1;
  - alpha and noidx;
  - the strip and index counts;
  - the laid tail (vertices, strips, indices) in a pool. In the rain wait it averages 344 B and is at most 1,088 B [C probe].
- L2 (valid while the 9 rotation words repeat) holds:
  - the rotation words;
  - the 8 box corners' partial sums before the translation;
  - Pmin and Pmax of the vertices' partial w.
- Each picture:
  - the transform runs as today;
  - the cut becomes the corner sums plus this picture's w9..w11, then the rest of xc_off;
  - the depth note uses the extremes when every vertex is in front or none is, and today's loop otherwise;
  - c3z is unchanged;
  - a drawn group copies the GerXform and the kept tail into the frame room, with xc_state and twopass as today.
- Keys: MGS2_NAT_D1, D1_CYCLE, D1CHK. The table is cleared at every turn and every overlay load. The design asks for 1,024 entries plus a 256 KB pool, about 512 KB, all or nothing, outside newlib's heap.

**Exactness [C code, verified].**
- Same GE words and data bytes, so gl_digest proves it frame by frame.
- The kept values are today's own float32 subexpressions.
- The tail depends only on the streams, n, alpha and noidx.
- State hashes are untouched by construction, because the renderer only reads game memory.

**Probe p1** (rollstudy/d1, x86, reads only, 618/618 EE hashes equal to hashref_gl35) [C]:

| Window | Groups | L1 found | L2 rows equal | Box, i1, alpha differing | Cut from corner sums differing | Depth by extremes differing | Tail differing | Other GerXform words differing |
|---|---|---|---|---|---|---|---|---|
| Rain wait 29,600-30,500 | 254,988 | 100% | 100% | 0 | 0 (137,826 cut) | 0 | 0 (117,160 drawn) | 0 |
| Rain walk 31,000-31,100 | 10,847 | 99.4% | 13.5% of found | 0 | 0 (198 behind, 426 straddling) | 0 | 0 | n/a |
| Deck 24,000-26,200 | 47,066 | 94.3% | 0 | 0 | n/a | n/a | 0 | n/a |

**Designer's claims and the verifier's corrections:**

| Claim | Designer | Verifier |
|---|---|---|
| Feasible | yes | **NO as written** |
| Saving against gl35 | about -7 us a group | about -4 to -6 us a group [I] |
| Saving against gl36 + R2 | about -1.4 ms a step | -1 to -3.4 us a group = -0.3 to -1.0 ms a step of ME time [I] |
| Step time | 1-2 ms a step | about -0.1 to -0.4 ms a step [I, gl35's ME-to-step ratio] |
| Hit rate in a 50-picture turn | 99% | about 98% L1, 96% L2 [I]: the first picture records, the second records P |

**Why the verifier says NO.**
- The 512 KB block outside newlib's heap is not there.
- PSP_HEAP_SIZE_KB(-1024) (psp/main_psp.c:68) leaves about 1 MB outside the heap [C code], and the ME plan block and the two nat_groups arrays take part of it.
- On the emulator [C]:
  - 116 KB largest / 216 KB total after those arrays (gl35 report G5);
  - gl36's own 147 KB table was refused and fell back to 96 KB, leaving 116 KB largest / 120 KB total (build_gl36/emu_n1/log.txt lines 338-339).
- On the PSP [C]:
  - 736 KB outside the heap (largest 512 KB) in gl17;
  - 224/432 KB in gl30, before native step 1's 216 KB arrays.
  - Today: likely about 100-200 KB [I], exact figure UNKNOWN.
- The emulator's heap headroom is 64 KB at its lowest (gl35 G5) [C], so taking the memory from the heap cannot be gated there.

**Other corrections [I unless marked].**
- Attributes + strips: -2.6 us, not -3.1 (the clock reads stay).
- GE transform: +1.8 us, not +1.0. With OPT2_C3W on, today's positions and attributes stages already pay the frame room's store misses, and D1 only moves them into a memcpy. The main CPU measures 660 ns a cold 64 B line [C PSP, psp_harness line 143]. The ME's rate is UNKNOWN.
- Entry look-up: +0.7 to 1.3 us.
- The I-cache term was missing: plausibly +0.4 to 1.6 us a group. gl35's keep code added 3,400-6,700 I-misses a picture [C model].
- Data traffic: a drawn group reads about 600 B under D1 against about 380 B of streams today. D1 saves instructions, not memory traffic.
- The model ratio is wrong. Like for like (parts 5-12 + lw_walk), the model gives 36.5 ms a picture = 18.3 ms a step, against the PSP's 18.4-19.2. That is about 1.0x on this work [C model vs C PSP], not 3.3-4x.
- D1 removes about 3.5 ms a picture of parts 10-12, not 5.
- The walk's figure is 108 groups a step, not 108 a picture.

**Exactness conditions the verifier added.**
- Run gl33's stream check (MGS2_NAT_STCHK) with a 100-vb window over the whole route, with stream B included. NATG_INPL means only "outside packet memory", not "never rewritten".
- Write back and invalidate the D-cache over the table and pool at every hand-over between the ME and the main CPU, or keep a table per CPU.
- Invalidate L2 whenever L1 records a new box.
- Fall back when a partial sum is not finite.
- The 916 PSP digests prove only the frames they cover.

**Verdict: exact, not feasible as written, and superseded by D3** (the same idea plus the converter's set-up, see 4.3). A cut-and-depth-only D1 would fit about 96 KB (about 600 entries of about 160 B, no tail pool) [I]. That just fits the 116 KB block left after gl36 on the emulator [C], and would save only a part of D1's figure, roughly -1 to -2 us a group [I].

### 4.2 D2: bounded sway hold (a downgrade: pictures differ, game state does not)

**What it is.**
- Once per picture, at the first fast group, the picture is HELD if all of these hold:
  - that group's entry has equal rotation quadwords;
  - the clip offset c = q3_now - q3_entry gives a run-time bound B <= E PSP px (E = 1.0 recommended; B is float32 only, so the twin, the emulator and the PSP decide alike);
  - fewer than 8 pictures have been held in a row.
- In a held picture, each fast ship group replays its kept converter output from the last full picture:
  - counters;
  - the depth note;
  - the texture look-up run for real;
  - the blob copy (about 535 B), with the texture scale, b.tex and twopass patched from the live texture.
- Everything else is drawn live.
- Groups without a match are converted with q3 corrected by -c, so they stay with the held hull.
- Keys: MGS2_D2, D2_CYCLE, D2_E, D2_EVERY, D2_MAXRUN, D2_CHK.

**What the player sees** (twin probe rollstudy/d2, 200 pictures, vb 29,601-29,799 and 30,301-30,499; 612/612 EE hashes equal) [C].

Shift of the visible ship vertices against today, PSP px:

| Policy | Pictures held | Median | p99 | Max |
|---|---|---|---|---|
| Every 2nd picture full | 50% | 0.06 | 0.20 | 0.28 |
| Every 4th picture full | 75% | 0.11 | 0.53 | 0.83 |
| Bound E = 0.5 | 60% | 0.07 | 0.18 | 0.24 |
| Bound E = 1.0 | 77% | 0.13 | 0.36 | 0.48 |
| Frozen until the camera turns | 99% | 1.1 | 5.3 | 7.1 |

- The run-time bound is about 2x conservative (actual/bound at most 0.50) [C].
- All 3,350 drawn deck decals have a Z-writing fast base with the same matrix earlier in the picture [C], so the decal pairing holds.
- Frozen would remove the rolling: rejected by the designer.
- The verifier's corrections [I, sim rollstudy/py/sway_sim.py]:
  - with MAXRUN 8 about 74% of pictures are held, not 77%;
  - a full picture comes every 3.9 pictures on average, at most every 9, i.e. a snap about every 0.7 s on average and up to 1.6 s apart on the PSP (0.6-2.9 Hz, not 1-3);
  - the held hull's depth is off by up to about 6 units (|c_w| = 0.563 x the accumulated dh), not about 1.5. So flicker of ripples, splashes or soles against the deck is a real risk, and it is UNKNOWN until held frames are compared.

**Exactness.**
- State hashes: equal by construction [I]. The renderer has no path back into EE RAM (gu_trxdir returns on a local->host transfer, ge/gsup2.c:931-940) [C code]. Unproven until built: the "685/685, 62/62" in the design are gl35's figures, not D2's.
- GE words: differ in held pictures by design.
- Full pictures can differ too [C code]:
  - the next frame's class-3 window is this frame's depth range (gelive.c:260 with zraw=1 on the PSP);
  - the texture cache's LRU and residency diverge (124 groups with another cut outcome at E=1.0 [C]; 23 at every 2nd);
  - map_lo/hi carries over.
- The design also left out the depth note of drawn groups, which the frame's depth map needs.

**Saving.**
- Designer: 21-22 us per held group, 2.5-4.3 ms a step expected.
- Verifier [I]:
  - about 14-17 us per held group on a gl36 base;
  - the ceiling is 3.2-3.8 ms a step (305 groups x 74% x 14-17 us);
  - expected 0.5-1.8 ms a step of ME time, in rain-wait ME turns only (0 in the walk and on the deck, where the camera turns);
  - possibly about 0, if the guarded replay behaves like gl35's texture keep (+0.76 us a group [C PSP]).
- The host-call and GE-transform savings were overstated: today's vertices are written in place, so the blob copy is about 11 cold lines a drawn group.
- The emulator's timers cannot be the kill measure: the emulator does not run the real ME, and its times are "host time, direction only" [C, attic/gl35/report.txt:189; psp/xl_psp.c:1168].

**Memory.** About 160 KB of entries plus a 256 KB pool, "outside newlib's heap like gl36's" [design]. That runs into the same wall as D1: only about 116 KB is there on the emulator after gl36 [C].

**Design gaps the verifier listed.**
- Replay the drawn groups' depth notes.
- Require several groups to agree on c.
- Hold only when wmin_src - |c_w| > 0.
- Make the check mode free of side effects, using the entry's exact q3.
- Invalidate the table at every turn and every ME/main switch.

**Verdict: feasible, not exact, not recommended.** It visibly degrades the rolling that Ffm pointed out, for about 1-2% of the rain-wait step. It would need a DEVIATIONS.md entry and shots shown to Ffm first. Build it only if an exact option fails its word-identity check, and even then only after the gaps above are closed.

### 4.3 D3: kept converter with an exact camera tail (exact)

**What it is.**
- Keyed per group by its identity: the three static streams' places and counts, the header quadword and the material.
- The entry holds:
  - the 16 GS registers xc_gs reads, plus FOGCOL;
  - VU abs 2..5;
  - the VcXform and the 12 rotation words;
  - the box;
  - the 8 corners' partial sums and Pmin/Pmax;
  - i1;
  - for drawn groups, the GerBatch, the GerXform and the strip list.
- In xl_p04_fast, the front end, the A+D packet (CLUT loads), PRIM and the drawn areas (K1) run live as today. Instead of gl_call -> xc_call, a replay:
  1. checks the registers, abs 2..5 and the rotation against the entry;
  2. **wait tail** (rotation equal): computes world[9..11], pb and resid with vc_xform's code; the cut from corner sums + w9..w11; the depth from fl(Pmin+t)/fl(Pmax+t), or xc_zr_df0's own loop; c3z as today;
  3. **walk tail** (rotation changed): runs vc_xform through the memo, the full xc_off on the kept box, and the depth via c3z or the loop, then rebuilds the GerXform;
  4. for a drawn group: runs the texture look-up for real, gb_room, copies the GerXform with 4 words patched (texture fields from the live answer), writes the vertices straight from the streams in one lean pass, copies the strips, then gb_xform_in;
  5. mirrors every counter and static that gl_call and xc_call touch, and leaves vc_km exactly as the full path would.
- Any miss runs today's path and records.
- Keys: MGS2_NAT_RT, RT_CYCLE, RTCHK.

**Exactness [C code, verified].** Class A, as gl35/gl36: the GE words are identical, the GS model, texture cache and drawn areas are equal after every group, and every counter is equal. There is no new GE matrix maths, so the 24-bit matrix problem cannot arise.

**Probe** (rollstudy/d3, reads only, 618/618 EE hashes equal to hashref_gl35) [C]:
- **Rain wait.** Of 254,987 fast groups, 254,409 (99.8%) found an entry with every check equal.
  - VcXform, cut outcome and the frame's depth-range state were predicted bit-equal in 254,409 of 254,409. That includes 142 whose cut outcome flipped against the entry, and 228,088 decided by c3z.
  - Of 116,819 drawn groups with a drawn entry, the patched GerXform, the stream-written vertices and the strips were equal in all. The batch differed only in the texture index, in 7,431 (the live look-up supplies it).
- **Rain walk.** The wait tail was exact in 1,701 of 1,701 groups (728 depth fallback loops). The walk tail's rebuilt GerXform, strips and vertices were equal in 3,001 of 3,001 drawn groups.
- The probe predicted against a full path that had just run. **No replay has run yet**, so the counter and vc_km mirroring has no evidence until RTCHK runs.

**Designer's claims and the verifier's corrections** (verifier's figures [I] unless marked):

| Item | Designer | Verifier |
|---|---|---|
| Removed stages (net of clock reads) | 28 us | 11-15 us really removable. Only xc_gs and the dispatch go from "host call + GS state", 1.5-2.5 of 5.5 [C model shares]. "Way back" stays (0.5-1 us removable). The cut groups' positions are gl36 R2's saving, already in the base. The transform memo already hits 79.1% of wait groups [C, d3r] |
| D3's own cost | 4-6 us a group | 7-10 us: the entry's 10-13 cold lines (2.3-3.0 us), the drawn groups' stream and room misses (1.5-2 us), new I-misses (1.5-3 us), the compares, tail and counters (1.5-2.5 us) |
| Rain wait | about -22 to -24 us a group, -6.7 to -7.3 ms a step (upper) | **-2 to -6 us a group over gl36 = -0.5 to -2 ms a step of ME time**. The upper bound is about -4 to -5 ms a step |
| Model cross-check | 36% of in-walk ship work | 10-16% after D3's own cost, i.e. -1 to -2 ms a step against gl36 before the model discount |
| Rain walk | -12 us a group, -0.5 to -1.5 ms a step | 0-2 us a group, at most -0.3 ms a step. With 1,024 entries and no eviction, about 0 |
| Entry size | 620 B | about 816 B [C field sizes]. A replay touches 13 cold lines for a drawn group and 9-10 for a cut group |
| Table | 1,024 entries, cleared at stage loads | The table fills [C, rollstudy/d3r]: distinct identities since the w00a load are 578 through vb 30,453, then 669, 816, 1,015, 1,123, 1,255 and 1,260 by vb 31,400. With 1,024 entries the wait finds 100%, the walk 53.7% and the stretch after the walk 22.5%. With 2,048: 98.2% and 100%. With 512 the wait already loses 10% |

- Precedent: gl35's keeps were expected to save about 16 us a group from the same kind of arithmetic. The PSP measured -2.47 us, about -0.2 to -0.3 ms a step [C PSP].

**Conditions the verifier added.**
- Size the table at 2,048 entries (about 1.6 MB) or evict by age.
- Put the hook inside each host's callback (the ME's xm_call, the main CPU's on_call, the twin's on_call), after its gates and census, not in place of call_fn. A replay outside xm_call's p->now gate would draw into a frame that is not being made. gl36 declined R3 for the same reason.
- Mirror xc_rec_calls, XmPic.calls and g_ns.nv/ncut too.
- Recompute twopass and GERF_TEX from the live texture.
- Pass the translation words through vfl.
- Fall back on non-finite partials.
- Fall back in every x86 check or census mode.
- Score D3 against gl36, not against gl35 keep 0.

**Cross-checks the verifiers did not make (this write-up) [I unless marked].**
- **Memory.** D3 asked for 0.6 MB "outside newlib's heap like gl36's", and the verifier corrected that to 0.8-1.6 MB "against the 2.6 MB free". The 2.6 MB is newlib's malloc heap on the PSP at gl35's end [C PSP, gl36_plan.md]. Outside the heap there is about 116 KB on the emulator after gl36 [C]. So D3's table has to come from the heap on the PSP: 0.8-1.6 MB of about 2.6 MB, leaving about 1-1.8 MB.
- **The emulator cannot hold it.** Its heap headroom is 64 KB at its lowest in gl35 and 128 KB in gl36's emu_n1 [C]. Why the emulator has less memory than the PSP is UNKNOWN.
- **A way to keep the proof:** make the capacity a key.
  - The twin runs the check mode at the PSP's capacity and at the emulator's.
  - The emulator runs it at what fits (about 100-150 entries), only to check the PSP-compiled float code of the replay.
  - The decision to replay affects speed only, because every replay is proven exact on the twin at each capacity used.
- **Cache coherence.** The table is used by the ME in ME turns and by the main CPU in main turns, and the two D-caches are not coherent. D3 needs the same rule D1 got: clear at every turn change, and write back and invalidate the table's range on the CPU that hands over (or a table per CPU).
- **The per-turn clear also bounds the table.** Within one 100-vb turn the wait sees at most 578 identities [C]. The walk's count per turn is UNKNOWN, but it is at most the 1,260 seen since the load [C]. So 1,024 entries (about 0.8 MB) may be enough with the turn clear: a cheap twin re-run of d3r's occupancy with a per-turn clear settles it.

**Verdict: feasible and exact, the best exact option, but small.** Its expected gain is -0.5 to -2 ms a step of ME time, roughly 0.2-0.7 ms of step time [I, gl35's third], at or below its own stop line (5 us a group or 1.5 ms a step). It also needs 0.8-1.6 MB of the PSP's heap.

---

## 5. What all the options share [I unless marked]

- None of them can reach the gl36 target of -50% a ship group. 36-40 us of about 61 a group is walk, VU-memory model, A+D/CLUT, texture and drawn areas. That work does not read the matrix, and it is held back by the GS model's ordered effects, not by the camera [C code / gl36 design].
- **On step time.** The ME is the rain-wait limit, but not a pure one: it has 4.3 ms idle a step, and the main CPU waits 8.5 ms a step on fills [C PSP]. On gl35, -0.75 ms a step of ME time became about -0.2 to -0.3 ms a step of step time [I, psp_harness gl35]. Expect about a third of any ME saving here to show.
- **Precedent for replays on the ME:**
  - K1 (drawn areas) paid -1.6 us [C PSP];
  - K3 (texture) cost +0.76 us [C PSP];
  - gl36's R1/R2 PSP figures are not yet known.
  
  D3's replay is K3's kind of guarded replay, but larger.

---

## 6. Recommendation and go/kill rules

1. **Keep the rolling exactly as the game makes it.**
   - No game-side change (G): it breaks exactness [C].
   - No camera split onto the GE (S): not provably identical.
   - No hold (D2): it degrades the effect Ffm pointed out, for about 1-2%.
2. **Build nothing for the bob before gl36's PSP log is read.**
3. **Go rule for D3** (wait tail only, as one lever in turns on the gl36 base, in a new tree t37). Build it only if gl36's PSP log shows both of these:
   - (a) the ME is still the rain-wait limit (ME idle under about 5 ms a step, and ME turns slower than main turns);
   - (b) gl36's R1/R2 replays delivered at least half of their estimate on the PSP split timers: at least about 1.2 us a group of R2's about 2.3 for cut groups, and a measurable part of R1's register-stage saving.
   
   If (b) fails, D3's larger replay is unlikely to pay. Then drop the ship's per-group work as spent and move on to the main CPU in the rain, as planned.
4. **Kill rule for D3 on the PSP:** if the rain-wait ME turns' native ship time drops by less than about 5 us a group (about 1.5 ms a step) on the split timers, remove it and stop. The emulator's timers cannot settle it, because the emulator does not run the real ME [C].
5. **If D3 is built, its conditions:**
   - the verifier's list in 4.3;
   - the table in newlib's heap on the PSP, all-or-nothing at a fixed capacity per key;
   - a clear and D-cache write-back/invalidate at every turn change;
   - the stream-bytes check over 100-vb windows on the twin;
   - the twin gated at every capacity used;
   - a PGO and code-order retrain with both arms;
   - one build out at a time.

---

## 7. First test to run

In order, cheapest first:

1. **No build: read gl36's PSP log when it is back** (through the lead thread and the RC session):
   - the per-stage split by arm (R2's positions saving on cut groups; the register stage);
   - the ME idle and the turn times in the rain wait;
   - the memory lines: "native step 1 ... that memory before/after", "nat reuse ... before/after", and the heap headroom's low point on the PSP.
   
   This settles the go rule and tells us how much memory the PSP has outside the heap and in it.
2. **Twin, reads only, about 3 min** (rollstudy/d3r with a per-turn clear added to d3occ.h): the table occupancy per 100-vb turn over route i. This tells whether 1,024 entries (about 0.8 MB) are enough with the clear D3 needs anyway, or 2,048 are needed.
3. **Only if the go rule passes: D3's wait tail behind MGS2_NAT_RT + RTCHK** on t36's base.
   - Twin, route i to vb 31,400, with RT off, on, by turns in both orders, and in check mode. Required: 685/685 hash lines in each arm; all 17,664 frames equal in every column; the read hash equal; RTCHK 0 differing in the batch, blob bytes, XcZr, VcXform, vc_km and every counter.
   - Emulator: 62/62 + 916/916, with RTCHK at the emulator's capacity.
   - Then one PSP run in turns: kill rule 6.4.

---

## 8. Open questions

1. **Item 5.** Its use adds 1,250 steps to the counter at 0x18F7CC and fades the bob to 0. Which item it is, and whether route i ever uses it, is UNKNOWN. It does not matter to any draw-side design, which follows whatever the game does.
2. **Do the stage camera paths bob when they are active?** These are w00a 0x20b8c8/0x20ba8c and d01t 0x21d5b0. They were not active in the windows studied [C]. If they are, D3's walk tail covers them without assumptions.
3. **Does the camera ever rotate in the rain wait outside the sampled vblanks?** The rotation was bit-identical in 101 consecutive pictures and in 91 samples over 29,600-30,500, with the roll slot at 0 [C]. Beyond those it is UNKNOWN. The heave's period, which the renderer study asked about, is answered: 512 + 179 steps [C].
4. **Gameplay readers over longer spans** (enemy AI and the like). Only 4 pictures were traced, plus c_light.c at vb 29,613. UNKNOWN, but moot: the hashes already differ, so G is out regardless.
5. **The PSP's memory now.** Outside the heap after gl31's arrays and gl36's table: likely about 100-200 KB [I], UNKNOWN exactly. In the heap: about 2.6 MB free at gl35's end [C PSP]. Why the emulator has much less heap is also UNKNOWN.
6. **Static stream bytes over a whole turn.** The probes found 0 changes in 310,146 sightings (D1) and in 609,235 groups (gl36) [C], on route i only. Whether a model is ever rewritten in place at the same address inside a stage is UNKNOWN. The twin's STCHK over 100-vb windows, stream B included, is the check.
7. **The ME's own cost of a replay:** its memcpy rate, cold-line cost and I-cache cost for the new code are UNKNOWN. The main CPU's figures are measured: 660 ns a cold line copied, load miss 172-328 ns, store miss 317 ns [C PSP, psp_harness line 143]. Only a PSP run in turns settles it.
8. **How the real GE combines WORLD and VIEW, and whether its snap is 1/16 px:** UNKNOWN. This matters only for S, which is rejected.
9. **D2 only:**
   - whether the held hull's depth offset (up to about 6 units [I]) makes ripples, splashes or soles flicker against the deck: UNKNOWN until held frames are compared;
   - how far the texture LRU divergence moves full-frame words.
10. **Size of the bob on screen.** The game study's formula gives 0.067 GS px a unit at 8,000 units. D2's estimate from the converter's PSP-viewport vertices gives about 0.029 PSP px a unit there. After GS-to-PSP scaling, the two differ by about 2x. Which one is right is UNKNOWN. No decision here depends on it.

---

## 9. Files (all in the container; no game data, dumps or pictures written to the project folder)

- **Game study:** /home/claude/main/rollstudy/run1.sh, runx.sh, ts.sh, ts2.sh.
  - Scripts: /home/claude/main/rollstudy/py/watch.py, ana.py, rldiff.py, rlreg.py, acts.py, scan.py, rd.py, ship.py.
  - Records: d1.jsonl (vb 29,600-29,700), d2.jsonl (31,000-31,100), d4.jsonl (every 10 vb, 29,600-30,500).
  - Hashes: x_base/rh.txt, th.txt and x_poke/rh.txt, th.txt.
- **Renderer probe:** /home/claude/main/rollstudy/rend (build.sh, run.sh), with results in /home/claude/main/rollstudy/rend/results_r1.txt.
- **D1 probe:** /home/claude/main/rollstudy/d1, with results in /home/claude/main/rollstudy/d1/results_p1.txt.
- **D2 probe:** /home/claude/main/rollstudy/d2 (py/d2.py, d2b.py, d2c.py).
  - Results: /home/claude/main/rollstudy/d2/results_d2a.txt.
  - Hashes: /home/claude/main/rollstudy/d2/hashes_d2a.txt.
- **Bob schedule simulation:** /home/claude/main/rollstudy/py/sway_sim.py.
- **D3 probe:** /home/claude/main/rollstudy/d3, with results in /home/claude/main/rollstudy/d3/results_p2.txt.
- **D3 occupancy:** /home/claude/main/rollstudy/d3r (src/ge/d3occ.h).
  - Results: /home/claude/main/rollstudy/d3r/out/occ_o1.txt.
  - Hashes: /home/claude/main/rollstudy/d3r/out/h_o1.txt (621/621 equal to hashref_gl35).
- **Background:**
  - /mnt/project-files/mgs2/main/attic/gl36/reuse_design.txt;
  - /mnt/project-files/mgs2/main/attic/gl35/keep_design.txt and report.txt (G5);
  - /mnt/project-files/mgs2/psp_harness.md, "Build gl35";
  - /home/claude/main/build_gl36/emu_n1/log.txt (memory lines 250, 321, 338-339).
- Large intermediates (EE dumps, read logs, binaries, objects) were deleted by the study agents. The md5 of each deleted file is kept next to its results. Nothing under t33-t36, build_gl3* or rainstudy was changed.
