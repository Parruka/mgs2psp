# gl44 design: the Vita's rain list for our PAL deck (w00a), as exact stubs

2026-11-27, thread "Study how the Vita MGS2 port cut costs", for the MGS2 port lead. The Vita facts behind it are in mgs2/vita_port_study.md; this note is about our PAL code only.

Marks: [C code] read in our generated PAL code or our runtime; [C twin] measured on the x86 twin (route i, pad_w01af, to vb 37,800); [C model] the lead's PSP cost model figures quoted from rain_options_2026-11-21.md; [I] inferred; UNKNOWN.

Work area: my own copy `/home/claude/main` (cp of mgs2/main at 19:1x UTC) with the twin rebuilt from the project tree (scripts hdr.sh, x86.sh obj/dump, vu.sh, x86.sh twin). `twin.sh r18a` on that copy: 685 of 685 reference lines equal, hashref_r18a.txt equals prof/hashref_x86_in_gl18me.txt byte for byte [C twin]. Nothing under /mnt/project-files/mgs2/main was edited except this folder.

## 0. Short answer

- The Vita rule ("update the effect only on drawn steps") is not what our build needs for these six. In our build the five that cost anything are not drawn any more in the rain turns (line_sph hidden by G0H, rain.c by H3R with the streak calls left out, body_sph and charaspread by H3, ripple by gl43's B). An effect that is never drawn can be a stub on EVERY step, which saves more than skipping every other step, and it needs no parity signal at all.
- The stub form is the exact form of rain_options s5.3 and of gl43's B: the shared seed advanced by the translation's draw count, the translation's polls x 64 added to the clock, the state other code reads kept, only when no interrupt is due inside.
- Prototyped behind one new switch (MGS2_OPT2 bit 22, 4194304, `NAT_V44`) for the two that pay: line_sph and rain.c. Result on the twin (section 5): every act's seeds and game time, the sound hash and every picture equal to the off arm; RAM differs only inside rain.c's own hidden vertex buffers; x86 time of the two acts -41% and -77%; about -1.4 ms a PSP step in the rain [I].
- The other four: ripple needs a state-only form (small, wait only); body_sph cannot be cut without moving the dice (its draw count is its own pose tests); charaspread is already a stub (G0C); rain_cm is too small to matter.

## 1. The six, in our PAL w00a

How found: the actor's file name string in the stage image (the scheduler's name pointer at actor+0x28) and the creation call that passes act, die and name (P13f378 with r5 = act, r6 = die, r7 = name) [C code]. Readers: the lead's write/read trace prof/act_readers_vb24001_24100.txt as quoted in rain_options s5.3 [C twin, gl16 era].

| Actor | Act (die) | Name string | What its act does in our build today | Draws from the seed 0x1fc954 | Read by code outside it |
|---|---|---|---|---|---|
| rain.c | 0x31af28 (0x31b770) | 0x4129b0 | message check 0x31ade8; show/hide path (H3R keeps +164 bits 12-13 on); a countdown at actor+420 (0x31b2e4-0x31b344); the entry loop 0x31b3c8-0x31b6fc over actor+392 entries (16 on the deck, two prims each) at actor+432, 32 bytes each; after the loop 0x31b6fc-0x31b72c | 0 or 1 a step, in the countdown only (reset to 16 + (draw & 31)) [C code]. The vertex loops use VU0's own random register, not the seed [C code] | 59 words a step [C twin]: the intensity float 0x1f86c8 (d_splash_camera reads it), the entry vectors, and after the loop the last entry's vector stored at 0x41e080, actor+80 copied to actor+84 and to 0x41e090, then P33b410(0x18e800, 0x41e080, 0x1f86c0) [C code] |
| line_sph.c (8 actors) | 0x333380 | 0x413260 | message check 0x333290; hide test (global 0x1ed438 or actor+84); matrix call P125a78(0x1df950); per unit (actor+80 units, descriptors at actor+96, 80 bytes each, prim at +64): flip prim+180, c = word0 << 5 vertices, four draws a vertex | 4 a vertex, fixed per actor: 768 / 1,024 / 1,280 / 1,536 / 5,504 an act, 12,928 a step for the 8 [C twin] | 6 words a step [C twin]: the seed and scratch leftovers [I]. The descriptors are read only by the act [C code] |
| ripple.c | 0x332180 (0x3328c8) | 0x413250 | message check 0x332090; hidden flag actor+16592; a 4-point box test against the camera (P125a78, P125f18 on actor+8288) and a distance test (actor+84 against 0x1df950+180); only when shown: 16 rings a set, actor+8280 sets: ring radius (actor+8384 + 16 i + 4) grows 40.0 a step; past 800.0 it respawns with 3 draws (centre x and z at actor+80 + 16 i, new radius 120 + 500 u) [C code] | 3 a respawn: 17 a step in the wait, 4.5 in the walk [C twin]. 0 when hidden or off screen [C code] | 150 words a step [C twin]: putobj 16, rain_cm 9, ft_splsh 9, blur 9. ft_splsh (Raiden's foot splash) reads the rings' state |
| rain_cm.c | 0x32a7d0 (0x32b980) | 0x412fd8 | a 2,256-byte actor; helper 0x32a6f0; hides/shows prims at +164, calls P10e9e8, P165f78, P1418b8 and matrix helpers [C code]; what it shows: UNKNOWN (not studied further: too small) | 0 [C twin] | - (it reads rain_gas_pers and ripple) [C twin] |
| charaspread.c | 0x2d30c8 (draws in 0x2d2858) | 0x40f858 | G0C: its particle sets as a native stub (lives, births, draws, random calls, time kept) | 32 a step in the wait, 82 in the walk [C twin] | 8 words a step; it receives message 3 from other actors; stopping it changes the route (rain_options s5.4) |
| body_sph.c | 0x3271a8 | 0x412e80 | NBS: its parts natively, same draws and time | 316 a step in the wait (271-343: one draw per pose test passed), 243 in the walk [C twin] | 6 words a step (fade_lgt 5) |

For reference, the non-rain users of the seed on the deck that must not move: the guards (watcher.c 0x40a2d8, attacker.c 0x40c910 name strings), chim_smo.c, the lamps, item boxes [C twin, rain_options s5.2].

## 2. "Shown step" in our build

- The rule is in rt_dg_call (boot/boot.c 1127, boot/boot_x86.c the same): with OPT_LITE, a draw-pipeline call is REDUCED (not shown) when `k_vblanks != dg_shown_vb && k_vblanks - dg_shown_vb < 2`, i.e. it comes in the vblank right after a shown call; otherwise it is shown and `dg_shown_vb = k_vblanks` [C code].
- It is decided at the pipeline call, which comes AFTER the step's scheduler pass. An act therefore cannot read "this step is shown" as a fact; it can only predict it. The exact predictor at act time is the same expression with the vblank count at that moment, wrong only when a vblank falls between the act and the pipeline call [C code]. A wrong guess costs one step of staleness in an effect, nothing else.
- Do not key on g_half_step's parity (MGS2_HALF's counter): scheduler passes and shown steps drift apart whenever a step takes two vblanks (the d01t deck drops 38 steps in 500 vblanks [C twin, rain_options s5.1]).
- If a signal is wanted (for an effect that IS still drawn): latch once per scheduler pass, at the FN_ENTER(0x13f140) hook where patch_half.py already counts passes: `g_step_unshown = (g_opt & OPT_LITE) && k_vblanks != dg_shown_vb && k_vblanks - dg_shown_vb < 2` (dg_shown_vb made visible to it). It is a function of the virtual clock only, so it is the same on x86, the emulator and the PSP.
- Cutscene wait words other than 1 (0x18db74 = minimum vblanks a step): with 2 or more, every pipeline call is at least 2 vblanks after the last shown one, so every step is shown and the latch never fires: the effects run every step, as on the PS2 [C code]. The same holds whenever the game itself drops a step. So the latch is "on" only in the strict one-vblank cadence, exactly where we show every second step. (The Vita does the same thing by other means: at 30 its skip clock marks every other step.)
- For the two prototyped stubs none of this is used: they are the same on every step.

## 3. Exactness plan, actor by actor

The form (as DEVIATIONS G1 and gl43's B): keep (a) the seed's draw count, by the jump x -> A_k x + C_k where the count is known before the work; (b) the clock: the translation's interrupt polls x 64 (K_POLL_CYCLES), added only when NOW + P x 64 < k_poll_at, else the translation runs; (c) every word other code reads. Drop what only the hidden prims' drawing would read.

| Actor | Pure per-call rule? | What it needs | Gain [C model for today's cost; the cut is I unless section 5 says C twin] |
|---|---|---|---|
| line_sph | YES. The count is 4 c per unit, known from the descriptor; the polls are nat_ls_prim's P today | Nothing new: G0 already jumps the seed. What G0 still pays for RAM exactness: the last vertex of each unit (float registers), the last copy's register images, and the scratchpad image of the copies, rebuilt vertex by vertex in nat_ls_flush when the act returns. The stub drops those three, but for 32 vertices of the image that another actor reads (5.3) | x86: -41% of the act [C twin]; of 1.07 ms a step on the PSP [C model]: about -0.4 [I] |
| rain.c | YES after its own prologue. The countdown (the only seed draw) and each entry's vector update stay translated; the vertex loop and the fill loop of each in-place entry are skipped, their polls (v + f per entry) added. The last entry stays translated, as under G0R, so the vector stored at 0x41e080 after the loop is the translation's | Both sets' prims hidden (H3R) and the streak calls left out (MGS2_GL_STREAKS=0). The loops write only the new vertex buffer, one stack word (the clip flags) and VU0 registers [C code] | x86: -77% of the act [C twin]; of 1.22 ms a step on the PSP [C model]: about -0.9 [I] |
| rain_cm | Yes (0 draws, constant 4,160 cycles = 65 polls a step) but its own state would need reading first | not worth it | < 0.1 |
| ripple | NO. The draw count comes from the ring radii, so the radii must be run: a state-only native form (16 x sets rings: radius += 40.0, past 800.0 three draws and the two centre floats), the box test kept translated, the VU vertex work dropped. Needs gl43's B hide of the ring prims | a new native form + check mode | <= 0.33 in the wait, ~0 in the walk |
| body_sph | NO. One draw per pose test passed: to know the count the tests must run, and the tests are the cost (NBS already does them natively). The Vita rule (return before the vertex work on unshown steps) changes the count, so the guards' dice move | only with Ffm's word that the dice may move, or a private-dice design (rain_options s5.2 DICE) | 0.87 wait / 0.41 walk at most half |
| charaspread | Already done (G0C keeps lives, births, draws, time) | - | - |

So the exact version of the Vita list is: line_sph and rain.c as every-step stubs now; ripple as a small follow-up; body_sph stays.

## 4. rain.c's streaks: never drawn, and is an every-step stub safe?

- Never drawn in our build [C code + C twin]: H3R keeps the first set's hide bits on (H3R_WR at 0x31afd8) and sets bit 12 on the second set each act (h3r_second), and with MGS2_GL_STREAKS=0 the renderer leaves the rain's streak calls out whole (ge/gelive.c gl_line_none). Twin evidence in section 5: with the vertex loops skipped, every one of the 37,831 picture lines of the route is equal to the off arm's.
- What the loop writes besides vertices [C code]: per entry, the entry's 16-byte vector at entry+0 (moved a fraction toward the vector at 0x1f8730 each step: this is state, kept), the two prims' flip words +180 (kept), a stack word, VU0 registers. After the loop: 0x41e080 (the last entry's vector), actor+84, 0x41e090 and a call with them (kept: the last entry runs translated).
- So a full "return before the loop" stub is NOT safe: the entry vectors and the three globals would stop. The safe stub is the one prototyped: every entry's prologue translated, loops skipped.
- VU0's random register is used only by these loops: a census of the translated code finds its instructions nowhere else (5.3) [C code]. With the stub it advances 1/16 as often (the last entry still runs its loops), so the last entry's hidden vertices come out different; nothing else can see it.

## 5. Twin prototype

### 5.1 What was built (my copy only)

- Switch: MGS2_OPT2 bit 22 (4194304), `NAT_V44` in boot/native_w00a.c. Bit 21 is gl43's B. NOTE: today's cycle words (fffff9df, fffff9ff, ffdff9ff) have bit 22 SET, so an "off" turn must clear it (ffbff9df here).
- line_sph, in nat_ls_prim (no header change): after every guard G0 has today (plain RAM, ranges apart, c in 4..256, and NOW + P x 64 < k_poll_at), when G0 and G0H both apply to the prim: seed = A[c] x seed + C[c] (the table's entry c = 4 c draws), the hide bits, NAT_ADV(P x 64), the last wrapper's LO/HI and saved ra word, the loop's registers (R1, R16-R18, R20-R23, R30, R31, F20, F21). NOT done: the last vertex (F0-F6), the last copy's register images (R2-R7), and the scratchpad image, with one exception found by the dumps (5.3): for the act's LAST unit the first 32 vertices of A's image are placed at 0x70000000 (NAT_V44_TAIL), because putmodel.c works on scratchpad vectors 16..21 that it never wrote. nat_ls_flush has nothing left to place when the act returns.
- rain.c, one hook in the wpl header at 0x31b47c: `if (G0R_LITE()) goto L31b6c0;`. g0r_lite() says yes only for an in-place entry (g0r_d: every G0R guard passed, the second set hidden, no poll due in the whole loop), with the bit set and every first-set prim hidden too (+164 bits 12-13). It adds (v + f) x 64, v = max(r21 - 1, 0), f = max(sp+152 - 1, 0): the back-edge polls of the vertex loop and of the fill loop, the same terms g0r_call already counts. The entry's prologue (flip words, the entry vector update at 0x31b454-0x31b470) and its tail stay translated; the last entry runs whole, as under G0R.
- An act log for the proof (x86 only, boot_x86.c): MGS2_ACTLOG=<file>:<from>:<to>[:<full from>:<full to>] with MGS2_HALF=10. Per scheduler pass one line: pass, vblank, acts called, a hash of every act call's (act address, seeds 0x1fc954 / 0x1fbd74 / 0x1ff554 at entry) and one that adds the virtual time at entry; in the full window one line per act; at the end per act its calls and host ticks. WARNING: MGS2_HALF is hex. "16" is 0x16 = lamps + sea at half rate; my first pair ran that way by mistake and was thrown away (it gave the same equalities and the same ticks).

### 5.2 Runs

Route i to vb 37,800 with scripts/twin.sh's settings. off: MGS2_OPT2 and both cycle words ffbff9df. on: fffff9df. w00a's acts run in two stretches of this route, 2,462 scheduler passes in all: a first visit from vb 23,259 (its traces in the EE hash last to 24,100) and the aft deck from about 29,050 to the stage change near 31,400 [C twin].

| Check | Result [C twin] |
|---|---|
| Off arm against the reference twin r18a | ee, iop, gs, vfb equal on every hash line: the log switch and the patch change nothing while the bit is clear |
| Seed and time log, off against on (final form, and the same for the first form without the tail) | 27,686 passes, 0 differ, with or without the time hash (md5 of the pass lines c153c8e8678c in both). Every act of every actor, the guards included, enters with the same seeds and the same virtual time in both arms, in both w00a visits and after |
| IOP (sound) hash | equal on all 685 lines |
| Frame buffer hash (vfb) | equal on all 685 lines |
| Picture lines (xlive, every drawn picture, the deck included) | 37,831, all equal |
| EE and GS state hash | differ on 97 lines: 23,300-24,100 (first w00a visit), equal again 24,200-29,000, differ 29,100-37,800. Only rain.c's own hidden vertex buffers: section 5.3 |
| Counters, on arm | line_sph: 201,884 prims, all stubbed, 0 translated, 0 scratchpad flushes (off: 19,696 flushes), 0 last units shorter than the tail. rain.c: 36,930 entries skipped = 15 of 16 in each of 2,462 acts |

Time, x86 host ticks per act call (rdtsc, from the act's call to the next act's call; both arms ran side by side on the 4-core container; unchanged acts as controls):

| Act | off | on | change |
|---|---|---|---|
| line_sph (0x333380; 8 calls a step), final form (tail 32) | 12,991 | 7,707 | -41% |
| line_sph, first form (no tail; its own pair of runs) | 13,817 | 6,445 | -53% |
| rain.c (0x31af28) | 217,334 | 49,802 | -77% |
| body_sph (control) | 399,781 | 397,488 | -0.6% |
| ripple (control) | 49,297 | 51,655 | +5% |
| charaspread (control) | 15,902 | 15,923 | 0% |
| rain_cm (control) | 2,460 | 2,426 | -1% |

Per rain step that is 42 k ticks for line_sph and 168 k for rain.c, together about 1.4% of the twin's average step on this route (14.7 M ticks, the draw pipeline included).

PSP size [I]: the same ratios on the model's 1.07 and 1.22 ms a step give -0.44 and -0.94, about -1.4 ms a step in the rain (wait and walk alike: both acts do constant work), K7 66.1 -> about 64.7. The ratios need not carry over (the PSP does this vector maths with VFPU, the twin in C). The model's ms a step was NOT run here: PspProf needs the PSP build, which this container does not have. UNKNOWN until the lead's model run and a PSP turn.

### 5.3 What differs in RAM (why the EE hash moves)

EE RAM dumps of both arms at vb 24,000 / 27,000 / 30,000 / 33,000 / 36,000, compared word by word over all 32 MB (the scratchpad included, as it stands at that vblank) [C twin]:

| Form | 24,000 | 27,000 | 30,000 | 33,000 and 36,000 |
|---|---|---|---|---|
| First form (no scratchpad image at all) | 50 words | 0 | 8,490 words | 8,490 |
| Final form (tail 32) | 50 words | 0 | 8,448 words | 8,448 |

- The 8,448 words (and the 50 of the first visit, when few vertices move) are all inside rain.c's streak prims' vertex buffers: 15 skipped entries x 2 KB (stale), and 2 x 1.5 KB of the 16th entry, which still runs but gets other vertices because VU0's random register has advanced less. Hidden prims, never drawn. After the deck they are freed blocks nothing reuses up to the end of the route, which is why the EE hash stays different to vb 37,800.
- The 42 extra words of the first form were 6 vectors of 16 at actor+0xd20 in each of two putmodel.c actors. Split runs to vb 30,000: the rain.c part alone gives exactly its 8,448 words; the line_sph part alone gives exactly these 42. So putmodel.c works on scratchpad vectors it did not write, left there by line_sph's last unit (an uninitialised read in the game itself). Tail runs (line_sph part alone, dump at 30,000): with the first 16 vertices of the last unit's A image placed, the 42 words still differ; with 22, 32, 64 or the whole unit, 0 words differ in the whole RAM. So putmodel reads vertices 16..21 (0x70000100-0x7000015f); 22 is the measured minimum, the final form places 32.
- Nothing else differs at any of the five dumps: no register image, no stack word, nothing in the stage image or the ELF's data. The dropped registers (F0-F6, R2-R7) and B's scratchpad image were not seen to reach memory. A dump is a snapshot at a vblank, not a proof for every load: that is the read hash's job (5.4).
- VU0's random register [C code]: its macro-mode instructions occur at 26 sites in each rain.c copy (w00a: all between 0x31b4e4 and 0x31b5e0) and nowhere else in the ELF or the d00t, d01t, w00a and w01a units. The compiled VU0 micro programs show no use of it [I, by name search of gen_vu.h].

### 5.4 Not done here

- The cost model and the emulator (no PSP toolchain here).
- The read hash (the lead's rh43.sh twin). Expect what B showed: loads of leftovers (scratchpad, stack below sp) that no later code uses; it has to be run to know.
- A per-turn schedule arm (both arms here were whole-run). By the code, switching per turn is safe: the stub leaves no pending state (nat_ls_pn stays 0, g0r_d is per entry), and with the bit off the next act places everything again [C code]; not run.
- A NAT_CHECK build: its per-call compare would flag the stub by design (the dropped leftovers ARE the difference), so the stub must be off in check builds; the seed and time log is the proof instead.


## 6. For gl44's build

0. Files here: `native_w00a.c.diff`, `native_w00a.h.diff` (the stub and the rain.c skip), `v44_apply.py` (the one header hook), `boot_x86_actlog.diff` (the act log, x86 only), `cmp.py` and `dcmp.py` (the comparisons above, with my container paths), `run2.sh` (the run line). No game-derived text in any of them.
1. Apply `native_w00a.c.diff` and `native_w00a.h.diff` (boot/, against the tree as it was at 19:1x UTC today; both files were unchanged in mgs2/main when I diffed), then `python3 -I v44_apply.py <wpl gen_s_w00a.h>` after the G0R hooks are in (it anchors on the address comment `/*31b47c*/` and on L31b6c0 / G0R_START; it stops if anything is missing). boot.c needs nothing: the bit is read in native_w00a.c. Add the bit to host_time.h's list (OPT2_V44, off in OPT2_DEFAULT). `boot_x86_actlog.diff` is the log, x86 only.
2. Turn words: bit 22 is set in today's words. Clear it in every turn that is not a V44 turn, or flip the sense.
3. A V44 turn needs, in the same turn: G0 + G0H (line_sph), G0R + H3R and the streak calls left out (rain.c). Without them the stub steps aside by itself (the guards) and the translation or today's natives run.
4. Gates, as for B: seed log off / schedule / on; IOP, vfb and pictures; read hash; emulator against its own hashref (the EE hash moves in V44 turns).
5. DEVIATIONS: nothing the player sees or feels changes (every picture, the sound, every act's dice and time are the reference's on the twin). It is an exactness-class entry like E5/G1: RAM differs in leftovers nobody was seen to read.
6. Follow-ups, by size:
   - rain.c's last entry: it still runs whole (1/16 of the loops and its four copies). Making it in place too needs the after-loop vector (VF10 at 0x31b714) kept; small.
   - ripple: the state-only form of section 3 (at most 0.33 ms in the wait [C model]).
   - The earlier deck stages on this route run OTHER copies of the same rain code with no natives at all: line_sph at 0x2ad098 and 0x2b4908 (23,979 and 24,664 calls; 77 k and 53 k x86 ticks a call against about 13 k for w00a's under G0), rain.c at 0x2a0058 (3,083 calls), rain_slow.c at 0x2b1280 (5,665 calls, 582 k ticks a call), body_sph at 0x2a3a48 / 0x2ac2d8 [C twin, act log]. If those decks' speed matters (the d01t cutscene), porting G0 / G0R / this stub to those units is a known lever; the hooks are by address, so each unit needs its own anchors [I].
   - body_sph by the Vita's rule (skip on unshown steps) only if Ffm lets the deck's dice move; with the latch of section 2.

