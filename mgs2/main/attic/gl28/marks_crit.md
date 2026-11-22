**Critique of the 2026-11-20 synthesis (black marks on the deck, gl26/gl27)**

I only read code and logs. I ran no builds or emulator runs and created no files or folders.

## 1. Corrections, each with evidence

**C1. The onset dates are not right, and the onset is weak evidence anyway.**
- GE2 was not "unchanged since gl23". In gl23 it ran only in alternate ME turns. "begun while the GE drew the frame before" is 50 at vb 30,600 and 31,000 but 0 at 30,400, 30,800 and 31,200 [C runs/gl23/log.txt:2707-2852]. GE2 in every ME turn starts in gl24: all five deck turns show 50 [C runs/gl24/log.txt:2709-2854].
- The walk already overlapped the main CPU's step in gl24, in alternate turns (TWO=2). At 30,600 and 31,000 "the arena" wait is 0.0 and "waited at hooks for the walk before" is 3,220.3 and 1,677.5 ms [C gl24 log, same lines]. What is new in gl25 is only that the walk also overlaps the next drawing step (that wait becomes 0.0).
- gl26 also turned on G1 cut b in every block, which changes the game from vb 24,100 [C DEVIATIONS.md:129].
- The owner said "probably gl25, unsure before". So the onset cannot put A above C, D or E. The synthesis's line "it does not match GE2 (gl23)" should say gl24.

**C2. B (display race) happens in both turn types, not "mostly ME turns".**
- gl27 run 2, in the pairs without the wait: lists drawn into the shown buffer were 221 in ME turns and 151 in main turns [C psp_harness.md:888].
- Why main turns race, in code: the HOOK runs its end (lists frame N+1), then its begin waits for the GE (mq_wait_self, xl_psp.c:1348-1356, so gl_show(N)). Then mq_frame's mq_service submits N+1 in the same vblank [C xl_psp.c:1857-1863].
- So before gl27 the race ran in every scene, in both turn types (up to 47 a turn in the opening, gl27 run 1). Yet the owner reported marks only on the deck, from gl26 [I]. The race is real, but it is not the marks.
- With the hold, B can only come back if the display latches the new buffer somewhere other than at the vcount step. That is UNKNOWN, and B should rank low.
- The synthesis's claim that flip_race is 0 by construction is correct [C xl_psp.c:1332-1334]. So is "only one list ever on the GE" [C :1328; gl_ge is cleared only in gl_show :300].

**C3. Class A mixes a cause the digest test can see with one it cannot.**
- The digest is made by the maker from its own cached view, inside xm_list, before XME_C_ALL [C ge/xlme.c xm_list; ge/xme.c:107-112].
- Input-side problems change the digest: late or transient reads, or stale lines the ME reads.
- An output-side hole does not change it: ME writes to the frame memory or pool not in RAM when the GE reads. That gives equal digests and marks in ME turns only, and the synthesis's table would blame it on B or C.
- So it needs its own line in the ranking, and only a coherence hash can test it.
- By inspection the write-back looks right: dwbinv_all before listed++ is the PSP's usual "cache 0x14 twice per index over 8 KB" [S]. So it is low.

**C4. D has a direction problem.**
- Blend 0x42 is G_REVSUB with destination fixed at W, which gives Cd − Cs·As [C ge/gelist.c:102]. The decal darkens.
- If the real GE fails the GEQUAL test on the decal, the deck shows lighter, not black.
- Black needs the decal applied twice, or a later depth-tested pass (lit passes 2-3; 312 a frame [C out_g22b.log, 30,930]) failing against the decal's depth. Neither has been shown.
- The onset argument against D is also weak. The depth map turns coarse identically in gl19's and gl26's game: frames with map_hi above 0.9M start at 30,848, and 197 frames have map_hi above 9M in 30,924-31,707 (the synthesis said 201) [C out_g19.log vs out_g22b.log].
- Net: D is low-medium, and its precondition test should look for lighter pixels inside the near-coplanar decal mask (zfight2/zfight3).

**C5. F (content) can be narrowed.** In all 461 twin frames from 30,400 to 31,450 there are 0 render-target, 0 unknown and 0 refused texture lookups, and 0 passes not drawn. So no untextured stand-in draws exist on the deck [C out_g22b.log 'xlive gb' lines, tallied]. Every frame but one has 33 frame-buffer lookups (gelive.c:41-48 kind 2). These are E's inputs, and they appear in both turn types.

**C6. TURN=300 and TWO=2 are not safe for gl28.**
- Both act on the whole run, from vb 0: two_on :1111, GE2 and GEFAST :1827-1828, flip_sel :1805, turns :1814.
- They change every ME turn, including codec call 1 (vb ~16,600). That is where gl27 run 1 switched off, and it is what gl28 studies.
- gl28's ME rest runs once per main turn (rs_allow and rs_end, t28/psp/xl_psp.c:1963-1968). TURN=300 cuts the rest/wake cycles to a third and makes ME stretches about 27 s long. That confounds the power study.
- FLIPWAIT=2's parity before 30,500 would change too.
- It also throws away the exact TURN=100 reference (build_bk_dig, 411/411).

**C7. Shot 30602 sits on a turn boundary.** The HOOK that begins a frame is posted and run before mq_turn_hook switches turns [C xl_psp.c:1857-1865]. Deck frames span two hooks. So shot_30600 is a frame whose drawing step was walked by the main CPU [I], not a TWO walk. Shots should sit 20 or more vb inside a turn.

**C8. Every shot tears the screen.** The shot path sets flip_force and then submits the next list in the same vblank as gl_show [C :410, :1332]. That clears the buffer still on screen: a possible black flash at each of the 9 pauses, which the owner may report as marks.

**C9. The owner's eye test cannot be scored as planned.** It asks for stretches of 10-50 s, recalled minutes later, while the marks come and go inside them. The last stretch (31,200 to the fade) is about 9 s.

**C10. Two of the container tasks need changes.**
- Task d (bk_gebuf with GEFAST): with one list in flight [C :1328, :980], what GEFAST could break is the hardware FINISH timing, which the emulator cannot show. Drop it.
- Task e: gl28's own emulator check digests only 31,561-31,959 [C build_gl28/mgs2_gl28_dig.cfg:104]. The deck reference must be made separately.

**C11. Minor fixes.**
- 197 frames, not 201.
- No gl25 shot_31112 exists in /home/claude/runs. Deck shots exist for gl18me, gl19, gl20, gl22 and gl26.
- A third frame buffer: VRAM is 2 MB. TXT ends at 0x1DC000, which leaves 144 KB [C :168-171]. A 544 KB buffer fits only in TXT's space, so it is not UNKNOWN.

## 2. Corrected ranking

| # | Cause | Turns | Seen by digests? | Likelihood | Decided by |
|---|---|---|---|---|---|
| A | Input-side ME reads under TWO: transient mid-step writes beyond the 0-40 ms sampled, or stale lines. A bad conversion can stay cached into main turns. | ME first | yes | low-medium | PSP digest window |
| D/E | Real-GE rasterization of deck content: coarse-map decal or passes 2-3 depth (D, from 30,848); 33 frame-buffer lookups a frame (E) | both | no | low-medium | eye marker by vb and map state; shots at coarse frames |
| F | Content also on the emulator, missed by about 30 stills | both | no (equal) | low | dense emulator dump |
| A-out | ME output write-back hole | ME | no | low | coherence hash (tier 2) |
| C | GE2 overlap or reuse, TSIZE over-read, FINISH timing | ME, GE2 on | no | low | GE2 A/B in the walk window |
| B | Display latch not at the vcount step | both | no | low | frame-buffer counter |

## 3. Recommended diagnostic set for gl28

**Tier 0: cfg only.** Everything before vb 30,500 is unchanged, so the power and ME-rest study is unaffected.
```
MGS2_GL_DIG=30500:31400
MGS2_GL_SHOT=8762,10518,15763,23265,24246,24786,29502,30502,30622,30872,30932,31022,31102,31114,31218,31292
```
- The 7 new shots replace the 7 dead slots past 31,400.
- With TURN=100: ME turns at 30622 (fine map), 30872, 31022, 31218 and 31292; main turns at 30932 and 31102, plus the existing 31114. The coarse-map frames match the shadow reader's GER replay frames.
- Keep as they are: TURN=100, TWO=1, GE2=1, GEFAST=1, FLIPWAIT=2, FLIPWAIT_FROM=30500, ME_REST=1, BAT_REC=1.

**Tier 1: small code, each off by default.** None writes game RAM, and none changes picture content. In gl28 if it does not delay the build, otherwise a gl28d.
1. **`MGS2_EYE=1`: the owner's marker.**
   - In xl_psp_gl_vblank (outside the step's time) call sceCtrlPeekBufferPositive. On each press or release of the R trigger, log: `eye: R down|up vb N t ms turn me|main two ge2 flip`.
   - The game cannot see the pad: the real pad is read only at main_psp.c:458 (end screen) and exc3b.c:296 (crash catcher). Game input is pad.txt.
2. **`MGS2_GL_FBCHK=1`: the display counter.**
   - In mq_submit before sceGuStart, call sceDisplayGetFrameBuf once with IMMEDIATE (0) and once with NEXTFRAME (1).
   - Count lists whose target (GL_FB0/GL_FB1 by r->d) equals each answer, plus the largest vcount − gl_show_vc, per turn, on the gl27 display line.
3. **`MGS2_ME_PIC_GE2_AB=30500`: GE2 on in alternate ME turns, inside the walk only.**
   - After xl_psp.c:1828: `if (ge2_ab >= 0 && vb >= ge2_ab) ge2 = gl_vol2 && ((vb/(2*mq_turn))&1);`
   - Result: GE2 on at 30,600 and 31,000, off at 30,800 and 31,200. TWO stays 1, so the digests still test A in every ME turn.
4. **Optional: shots wait for the flip from FLIPWAIT_FROM on.**
   - At :410, set `flip_force = vb < flip_from`.
   - Then loop `mq_service(); mq_ge_finish();` with 200 µs delays until `!gl_ge && mq_cons == listed`.
   - The same frame is saved, without the tear.

**In the container, before the PSP run:**
- The gl28 EBOOT on the emulator with this cfg must give 372 hashes = 72e0ca4c, 411/411 digests equal to out_g22b.log, and the reference pictures for the 7 new shots.
- A dense emulator dump of 30,500-31,400, for F.
- The mid-step run widened to 0-120 ms (build_bk_late_mid), for A.
- shot_31112 on the zfight mask, looking for lighter pixels, for D.

**Tell the owner:**
- "Hold R while you see the black marks, and let go when they are gone. R does nothing to the game."
- "There may be a short flash at a picture pause." (Only needed if item 4 is not in.)
- Ask now, at no cost: "Did you see them before Snake starts walking, while he waits on the deck in the rain, or only during the walk?"

**How to read the results:**

| Result | Points to |
|---|---|
| Digests differ, first in an ME frame | A, input side |
| Display counter above 0 near R presses | B |
| R presses only in GE2-on ME turns (30,600s, 31,000s), digests equal | C |
| R presses in all ME turns, none in main turns, digests equal | A-out or E under bus load; next build adds the coherence hash or a windowed TWO toggle |
| R presses in both turn types, starting at about 30,850 or later | D (shots at coarse frames may show it) |
| R presses in both turn types, starting earlier | E or F (the dense dump decides F) |
| R presses only at shot vbs | shot tears |

**Cost and safety:**
- Digests add about 20 ms a step in main turns and about 7 ms a step in ME turns, only in 30,500-31,400: about 13 s in all [C gl19].
- 7 extra pauses of about 0.3 s and 400 KB each.
- A pad peek every vblank costs microseconds; the display counter costs 2 system calls a list.
- No flash, power or clock register writes. Only pad and display reads, VRAM reads, and file writes like the existing shots.
- Hashes must still equal 72e0ca4c. GE2_AB changes only the overlap, not what is drawn.
- If gl28 can take no code, Tier 0 alone still settles A. The rest moves to gl29.