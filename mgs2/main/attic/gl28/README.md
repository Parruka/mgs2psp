# gl28 (2026-11-20): power changes + the black-marks diagnostic

gl28 = gl27 + three power changes + five diagnostic switches for the deck's black marks.
Tree in the container: /home/claude/t28 (t28b = gl28 without the diagnostic). Mainline files in ../../psp, ../../scripts, ../../tools, ../../cfg are now gl28's.

## Power (the PSP switching off on battery; memory mgs2-switch-offs)
- `MGS2_ME_REST=1`: the ME HALT-rests during main-CPU turns (psp/xme_rest.S = v30me5's me_rest.S, renamed; handshake in psp/xme_me.c and psp/xl_psp.c, the rs_* block). Writes: only the approved ME kick (0xBC100044) and the ME's own 0xBC300008/0xBC300000.
- `MGS2_HASH_DENSE` removed from the PSP cfg (a full-RAM hash every vblank in 15,700-16,010).
- `MGS2_BAT_REC=1`: recorder fields in battery.txt's 5-s line (psp/v26me_bat.c).

## Black-marks diagnostic (memory mgs2-deck-marks)
- `MGS2_EYE=1`: hold R while the marks show; each press/release writes an 'eye' line (turn, flip, shown buffer, GE state). Pad peek only; the game still plays pad.txt.
- `MGS2_GL_LIVE=30500:31400:100:8`: copies of the shown buffer without draining the GE (live_<vb>.bmp), one every 100 vb mid-turn plus up to 8 at R presses inside the window.
- `MGS2_GL_FBCHK=30500`: sceDisplayGetFrameBuf counts per turn (does the GE draw into the scanned buffer?).
- `MGS2_ME_PIC_GE2_AB=30500`: GE2 on only in the ME turns at 30,600/31,000/..., off at 30,800/31,200/...
- `MGS2_GL_SHOT_FLIPWAIT=1`: shots in the flip-wait window wait for the GE instead of forcing the flip.
- `MGS2_GL_DIG=30500:30900` digests in the walk.

## Files
- gl28.diff (1f3b0ca8): t27 -> t28 plus main's changes and cfgs. gl28d.diff (8c5a6ffc): t28b -> t28 (diagnostic only).
- gl28_apply.py (c2fb6e3e): `cp -a t27` + the .gl27bak main copies -> t28 (tested TREE_IDENTICAL).
- relink28.sh: relinks build_gl28 from build_gl27's objects + the changed units.
- mgs2_gl28.cfg (144c3244, PSP), mgs2_gl28b.cfg (4b342ad4, without the diagnostic), mgs2_gl28_dig.cfg (886e9cdd, emulator: DIG 30500:31400, 32,000 vb).
- rd28.py (5e535244): the run reader (hashes, digests, ME rest, GE2 a/b, display/fbchk, eye, live captures with black clusters, battery).
- marks_plan.md / marks_crit.md / marks_reads.json: the marks study (night 2026-11-19/20).

## Checks [C]
- EBOOT ee11e43f. Emulator: 66 hashes = hashref_g22b (72e0ca4c), 411/411 digests = twin, live captures equal the emulator's frames, no terrain clusters. Switch-off run = gl28b (150/150 digests, deterministic lines identical). Eye test: presses outside the window take no capture.
- Checks: xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk PASS; no new warnings. Two reviews; one fault fixed (R captures outside the window).
- xl_psp_gl_vblank lost its PGO profile (new calls); runs once a vblank outside the step [I negligible].
