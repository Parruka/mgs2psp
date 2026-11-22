sb11: Stage C step 1's C1, the codec faces (P07) skinned, lit and texture-mapped by the GE (stagec_notes.md 1.7-1.9, tasks #134, #135).
2026-11-02.  A visual change: behind g_opt2 OPT2_C1 (16), OFF by default; Ffm's OK on shots of both forms comes before it is on anywhere.
Container tree /home/claude/sb11 = sb10's tree (tools/dev/sb10) + this; pristine copies of sb10's files in ge.orig, boot.orig, psp.orig,
runtime.orig (checked equal to sb10's).  sb11.diff (diff -ruN of ge, boot, psp, runtime against the .orig copies) holds all of it.  To
rebuild the tree in a new container: rebuild sb10's tree (tools/dev/sb10's README), then patch -p0 < sb11.diff in its root (checked:
it rebuilds today's files byte for byte); the scripts here go in the tree's root.  Not in the shared code yet.

What it is:
- ge/c1model.c, c1model.h (x86 only, MGS2_C1_MODEL=1): the census of P07's constants and a model of the GE's arithmetic next to today's
  exact vertices (stagec_notes 1.8).  Read-only: runs stay exact.
- ge/xconv.c xc_lit_c1, xc_c1_bound, xc_c1_hold_bound, xc_c1_zf, xc_c1_zin, xc_zr_c1; ge/vu1conv.c vc_p07_c1 (the constants by a key of
  their 33 words, p7_c1k / p7_c1x / p7_c1m), vc_p07_c1s (the slots and the checks, the box of the positions, the weights' sums),
  vc_p07_bone, vc_p07_c1b (the vertices and, where needed, each one's skinned view depth); ge/replay_fmt.h GerC1, GERL_C1; ge/gelist.c
  c1_draw, c1_off, same12 (the list: WORLD identity, VIEW the call's world, lights, ambient, bones by BOFS, texture matrix for pass 3);
  ge/gelive.c (the switch into XcHost.c1); boot/boot.c, runtime/host_time.h (OPT2_C1, OPT2_DEFAULT ~16); boot/xlive_chk.c and
  psp/xl_psp.c (the "P07 C1" log line); psp/main_psp.c (cfg 26 "face GE lighting on" in the timing windows).
- Forms, all giving the same lists (x86 three routes, both settings; the emulator's every frame): the first (vc_p07_c1v, one pass);
  c1opt (checks pass, writer, constants by key, word compares); c1opt2 (bones unrolled, key compared in place, 12-word compares, slot
  scan unrolled, the depth range by a box bound where it cannot move); c1opt3 (that bound by the box's centre and half extents).  Their
  PSP model figures: stagec_notes 1.9.

Scripts (run from the tree's root): mk.sh <dir> [flags] (the x86 twin), mkpsp.sh <dir> (the PSP model build, no PGO), chk11.sh /
chk11x.sh (one x86 route; chk11x.sh takes X=<build dir>), c1x.sh / c1x2.sh / c1x3.sh (the three routes in x1 / x3 / x4, a tag and env:
none = C1 off, MGS2_OPT2=ffffffff = C1 on), c1runs.sh (the census and model runs), c1pics.sh (PPSSPP master runs of the codec route with
shots and every frame's digests, both settings; B= the PSP build, O= where the outputs go), models_c1.sh / models_c1b.sh /
models_c1c.sh / models_c1d.sh (PspProf windows of the codec route, both settings, on sb11m / sb11m2 / sb11m3 / sb11m4).
Checks and readers (python3 -I): t1cmp.py (two x86 logs), digcmp.py (an x86 log against the emulator's: every frame's list and frame
digests), bmpcmp.py (two folders of shots), c1shots.py (comparison pictures for Ffm's folder: game pictures, personal use only),
fndelta.py (per-function model ms of two PspProf dumps), pcdis.py (one function's instructions with counts and misses), fnlines.py (one
function by source line, with a -g object of the same code), isets.py (I-misses by I-cache set: which hot code shares sets), hotlines.py
(the 64-byte lines a hot loop runs: its I-cache footprint).  Model comparisons: tools/dev/sb6/fitcmp.py.
Game data and pictures stay in the container (personal use only).
