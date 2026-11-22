SYNTHESIS: black marks on the Tanker deck (gl26/gl27, PSP only), 2026-11-20

What I checked myself tonight [C]
- Timing of onset.
  - The GE2 overlap has not changed since gl23. The deck ME turn at vb 30,600 logs "begun while the GE drew the frame before 50" in gl23, gl24, gl25 and gl26 (runs/gl23/log.txt:2712, gl24:2714, gl25:2714, gl26:2867).
  - What changed in gl25 is that the walk now runs beside the main CPU's next step. "The main CPU waited at hooks for the walk before" is 3,220 ms in gl24 and 0.0 in gl25 and gl26.
  - gl25 did this in alternate ME turns only (TWO=2): its turns at 30,600 and 31,000 took 9.7 and 9.2 s, against 10.7-12.2 s for the others. gl26 did it in every ME turn (TWO=1).
  - The owner's "gl26, probably gl25" matches this change. It does not match GE2 (gl23) or GEFAST (gl22).
- The coarse depth map is not new. out_g19.log (gl19's game) and out_g22b.log both turn coarse at vb 30,850, with the same 201 frames of map_hi > 9M in 30,924-31,707. PSP shots of 31,112 exist from gl18me, gl19, gl20, gl22, gl25 and gl26.
- gl27's "0 lists drawn into the shown buffer" proves nothing about the screen. mq_service (psp/xl_psp.c:1332-1334) counts flip_race only when flip_on is 0. With the wait on, it counts a hold and returns, so the count is 0 by construction.
- Only one list is ever on the GE: mq_service submits only when !gl_ge (:1325). So ge_fin_cb's ge_done = mq_subm (:980) cannot release a newer list.
- MGS2_ME_PIC_TWO, GE2, GEFAST and FLIPWAIT set to 2 all use the same parity, (vb/(2*TURN))&1 (:1111, :1827-1828). Only one of them can be toggled in a run; setting two of them to 2 flips both in the same turns.
- Shots take the no-wait flip path (flip_force, :410).
- The cfg runs to vblank 31,400, but 7 of its 16 MGS2_GL_SHOT slots (31665 to 36943) lie past that, so they are free.
- gl28's cfg is gl27's minus HASH_DENSE, plus MGS2_ME_REST=1 and MGS2_BAT_REC=1. BAT_REC's battery lines come every 5 s and give t, vb and whose turn it is.
- Turn lengths on the walk are ME 8.3-9.4 s and main about 13-16 s. They are not 4.5/7.5 s; those figures are from the steady deck at 26,300-28,700.

1. Ranked causes

A. Walk on the ME reads game RAM while the main CPU runs its next step (TWO). This merges late-reads H1-H3, ge-buffers H3, shadow-render H2 and diagnostics H1/H3. Likelihood: medium.
- Mechanism: with TWO the walk overlaps the main CPU's next step. It reads image and CLUT bytes outside xc_keep (xlme.c:186) and makes the map's lazy reads (gsup2.c:230, :408) through the ME's cache. It could see a byte changed and put back inside one step, or stale data through a cache hole.
- Turn type: ME turns with TWO on. PSP only, because only there do two cores with separate caches run at once.
- For: the best fit with the onset above.
- Against:
  - Emulator stress at the latest walk point: 2,679/2,680 frames equal the twin. The only difference is vb 26,007, the known C7 late read.
  - Emulator mid-step sampling at 0-40 ms after the hook: 2,680/2,680 equal.
  - lchk4 on the gl27 game is clean in 26,100-31,400.
  - By inspection, every cache step is in place.
- [I] The early and late reads give equal digests, so a mix of old and new lines can only matter through a transient write or a cache hole.

B. Display: the screen shows a buffer the GE is clearing or drawing. Black is the clear colour. Likelihood: medium-low.
- For: black areas left by a partial redraw fit the symptom. TWO is what makes the next list ready at once (gl27's own analysis), so the onset fits. PSP only, since PPSSPP draws at enqueue. Drained shots cannot see it.
- Against: [I] the vcount hold should cover the latch, yet the marks persist with it on. gl27's counter says nothing either way.
- Turn type: mostly ME turns.

C. GE-time overlap (GE2/GEFAST). The ME's writes for frame N+1 reach memory the GE still reads for N, or the GE reads past a clamp texture's stored rows (TSIZE is a power of two; storage is h+1 rows). Likelihood: low-medium.
- For: ME turns only, PSP only, and invisible to digests and drained shots.
- Against:
  - The onset (overlap unchanged since gl23).
  - bk_gebuf held the GE busy 25 ms after each submit. It found 0 changes to list, arena, palettes or texture rows in 30,500-31,400.
  - Code review found no unguarded buffer.
- Not covered: GEFAST's callback release (off in bk_gebuf).

D. Real-GE rasterization with the coarse depth map. The subtractive deck decal (blend 0x42) sits within 1 GE unit of its base on 25-50k px from about 30,850. Likelihood: low.
- For: a darkening overlay, and the right place and window.
- Against:
  - The same condition was in every deck build since gl18me, with no report before gl25.
  - The gl26 drained PSP shot at 31,112 (a coarse frame, median margin 0.88) has no dark clusters.
  - Monotonic depth-rounding models fail only 0-14 px.
- Turn type: both. The clipping variant is also low: the discarded triangles are off-screen.

E. [I, new, no evidence] The GE textures from the buffer it is drawing (frame-buffer textures kind 2, the depth blur; about 33 lookups a frame on the deck). The results might depend on timing when both CPUs load the bus. Likelihood: low, UNKNOWN.
- Turn type: both, but more under TWO.
- Drained shots, taken with the bus quiet, would hide it.

F. Real picture content, not PSP-only: splash thinning, C1, TK_RT draws untextured, cut b. Likelihood: low.
- For: the emulator evidence is only about 30 still pictures.
- Against: all stills are clean on both the emulator and the PSP.
- Destination alpha is excluded by the code: the stencil test is off, so the alpha byte stays 0.

2. What is settled, and what can be settled tonight

Settled by the readers:
- Emulator digests 30,500-31,400 equal the twin, 411/411 (build_bk_dig).
- The late-read stress and mid-step runs: build_bk_late, _ctl, _mid, _hi0.
- lchk4 on the gl27 game (build_bk_lchk).
- The GE-buffer lifetimes under emulated overlap (build_bk_gebuf).
- Digests work on the PSP in both turn types (gl19, 150/150).

Settled now: the points listed at the top.

Can be done tonight in the container:
a) D on hardware: compare runs/gl26/shot_31112.bmp with a new emulator shot of 31,112, only on zfight2's within-1-unit pixel mask. This needs a twin GER replay at 31,112; the reader deleted their folder. About 30 min.
b) F: a dense emulator dump of 30,500-31,300 (a variant with the 16-shot cap lifted, or 4 parallel runs of 16 shots), then a scan for dark clusters and for changes from frame to frame. The pictures stay in the container.
c) A: rerun build_bk_late_mid with holds of 0-120 ms.
d) C: rerun build_bk_gebuf with the GEFAST callback release on.
e) The reference for the run below: the gl28 EBOOT (or gl27's) on the emulator with the new cfg. Its digests must equal out_g22b.log, and its 7 new shots become the reference pictures.

3. The next PSP run (gl28 plus cfg lines)

Changes to mgs2_gl28.cfg:

  MGS2_GL_DIG=30500:31400      (was 31561:31959, which lies past the run's end)
  MGS2_ME_PIC_TURN=300         (was 100)
  MGS2_ME_PIC_TWO=2            (was 1)
  MGS2_GL_SHOT=8762,10518,15763,23265,24246,24786,29502,30502,30602,30872,30932,31102,31114,31218,31252,31292

GE2=1, GEFAST=1 and FLIPWAIT=2 with FROM=30500 stay as they are. The new shots are at the frames the shadow-render reader replayed (30,930, 31,100, 31,112, 31,216, 31,290), plus pauses that mark the blocks.

The walk then runs in these blocks:

| vb | Turn | TWO |
|---|---|---|
| 30,300-30,599 | main | n/a |
| 30,600-30,899 | ME | on |
| 30,900-31,199 | main | n/a |
| 31,200-31,400 | ME | off (the fade starts at about 31,300) |

The 9 shot pauses on the walk become landmarks the owner can see. To tell the owner: "On the rain deck the picture pauses 9 times. Ignore a flash right at a pause. For each stretch, say whether you saw black marks on the deck floor: pause 1 to 2 (about 15 s), pause 2 to 3 (about 25 s), pause 3 to 7 (about 50 s, three pauses inside), and pause 7 to the fade (about 10 s)."

What the log and the owner's answer will show for each cause:
- A: PSP digests differ from out_g22b.log only in frames 30,600-30,899. The shot at 30,870 may show the marks. The owner sees marks only from pause 2 to 3.
- B or C: digests are equal and the shots are clean. Marks appear from pause 2 to 3 and from pause 7 to the fade, and none in the main stretches. By eye, B and C look the same; only the counter below splits them.
- D: digests are equal, but the shots from 30,870 to 31,290 show dark patches on the decal's sensitive pixels, in both turn types. Marks appear from pause 3 to the fade.
- E: digests are equal and the shots may be clean. Marks appear in every stretch, more in ME blocks.
- F: marks appear in every stretch, and also in tonight's emulator dump.

Compare digests with python3 -I main/tools/emucmp.py on the PSP log against build_gl19/x86/out_g22b.log, split by turn.

Optional small code addition, in a gl28 variant or in gl28 itself if it does not delay it: in mq_submit, call sceDisplayGetFrameBuf in mode 0 and in mode 1, compare each with the list's target (GL_FB0 or GL_FB1), and log vcount minus gl_show_vc. Count these per turn and print them on the turn line. A count above 0 in ME turns means B. It changes nothing on screen, needs no memory and costs two system calls a list.

Cost and risk:
- Digests add about 20 ms a step in main turns and about 7 ms a step in ME turns inside the window [C, gl19's log], roughly 10 s in all [I].
- TWO off in half the ME turns makes those deck turns about 25% slower [C, gl25: 10.7-12.2 s against 9.2-9.7 s].
- TURN=300 does not change the game (same hashes), but it reshapes gl28's per-turn figures for the ME rest, so gl28's report must read them by turn.
- The 7 new shots pause about 0.3 s and write about 400 KB each.
- Risk: the digests and shots shift the timing and could hide a cause that depends on timing. If the marks are gone in this run, that is itself a result.
- Nothing here changes the picture's content. The owner must be told about the pauses and the possible one-frame flash at a pause, which comes from the shots' no-wait flip.

4. Fixes per cause, each behind a switch
- A: MGS2_ME_PIC_TWO=0 as a cfg stopgap. It gives back the gl25/26 deck gain. The proper fix copies the read ranges at transfer end (xc_keep), or makes the map's lazy reads go through a fixed copy. For a cache hole: a whole-cache invalidate at each record.
- B: hold the next list until sceDisplayGetFrameBuf reports the new buffer (a new FLIPWAIT value), or wait for a vblank before submitting. A third frame buffer needs 544 KB of VRAM; whether that fits beside TXT at 0x154000 is UNKNOWN.
- C: MGS2_ME_PIC_GE2=0 or GEFAST=0 as cfg stopgaps. The proper fix guards whatever region the tests name.
- D: build the depth range only from vertices inside the clip volume (leave out those clamped in xc_zr_end) and set C_CLIP on batches outside it. Or give the 0x42 overlays a 1-2 unit depth bias. Either changes the picture slightly, so it needs the owner's OK.
- E: a pipeline drain or TSYNC before kind-2 batches, or texture from a copy of the buffer.

Safe to ride in gl28: only the display counter (always on; diagnostic only) and, if cheap, the B hold as a new FLIPWAIT value, off by default so it does nothing unless set. No fix should be on by default, because this run is the diagnosis and A and C already have cfg stopgaps.

5. UNKNOWNs
- Whether anyone watched the emulator live on the deck, or "not on the emulator" rests only on digests and stills.
- The owner's actual on/off rhythm, and whether he ever saw the marks in main turns.
- When the real display latches the new buffer relative to the vcount increment, and what sceDisplayGetFrameBuf returns in mode 0 and mode 1 on hardware.
- Whether the GE has finished all its reads and writes when the FINISH interrupt fires (GEFAST).
- How precisely the real GE interpolates depth, and how it behaves when texturing from the buffer it draws.
- Whether the marks appear in a drained frame of an ME turn: no PSP deck shot has ever been taken in an ME turn.

The reader build folders under /home/claude/main/ (build_bk_late, build_bk_late_ctl, build_bk_late_mid, build_bk_late_hi0, build_bk_gebuf, build_bk_dig, build_bk_lchk) are kept for reruns. My only files are /tmp/claude-0/-home-claude/a7d20bd4-7c06-5849-bdd4-c955873f09a9/scratchpad/syn/off.c and off.s; I made no builds and no emulator homes.