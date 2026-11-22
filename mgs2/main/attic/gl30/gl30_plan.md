# gl30 plan: finish X (decals that are a subset of their base) and make the pairing cheap (2026-11-20, thread lead)

## Why
- gl29x's PSP run (psp_harness.md "Build gl29x"): X fixed every decal it paired. live_31012 (frame 31006) still lost a 3.9k px shadow patch, inside the 0x42 coverage. The log shows 2 vertex-set fallbacks in most walk frames and 3-6 in 131 frames, more from vb 30,984. Those decals are drawn O, so they can still lose patches on the real GE.
- `fbana.py` (this folder) over 60 walk frames of build_gl29/ger: 654 decal batches pair exactly; **184 are subsets of one base**: the decal's distinct positions minus (0,5,0) are a subset of the base's positions, and its triangles are a subset of the base's triangles. The base is 8-20 batches back. Nothing else is left.
- Cost [C gl29 run 1]: X's walk list time ME 56.9 ms against O 48.1, main 51.8 against 48.1. Cause [C ger data, cost I]: every one of the 64 look-back batches passes dz_pair's translation filter (the whole ship shares one world matrix), so each decal computes, sorts and compares the keys of up to 64 bases. The uv/colour copy and the ATTR check are also quadratic in nv. Decals have 3-16 vertices.

## Changes (tree /home/claude/t30 = a copy of /home/claude/t29)
1. **Cheap reject** before any key work: a candidate base is examined only if one of its vertices lies within the offset limit (|d| <= 64 on each axis) of the decal's first vertex (a linear scan of int16s). The search order stays nearest first.
2. **Subset pairing ("XS")**: when no exact base is found, take the nearest same-matrix base whose distinct positions contain the decal's after one offset, and whose triangle set contains the decal's triangles after that offset. Draw it the Y way: the decal's own vertex array and strip runs, with each position replaced by its matched base position (decal minus offset). gl29's wait test showed Y restores the shadow as X does [C gl29 run 1: leftover Y 14.8-16.9k, X 13.5-15.2k, all on the trail]. Count it on its own (a new `sub` counter in the dz log text); the fallback counters stay.
3. **Linear-time attribute work**: replace the nv-squared loops (base vertex -> decal uv/colour; the ATTR check) with sorted (key, index) arrays and a merge join.
4. ME-safe as now: no libc, no statics; the same undefined symbols as gl23 (me_memcmp, me_memset); .me_bss stays 12 B; aux use bounded as now.

## Gates
- **G1 twin** (tw29.sh with TW_T=/home/claude/t30 and its own TW_X): the hashes equal gl29x's twin (build_gl29/x86/out_x1.log, hashref_x1.txt) on the same command line (`MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b "MGS2_GL_DZ=29490:31400:X"`, scripts/twin.sh pattern). Digests equal out_x1.log in every frame without a subset decal; the frames with one differ only through those decals' batches.
- **G2 census** (the twin's dz lines): vertex-set fallbacks 0 over vb 29,490-31,400, or each remaining one explained.
- **G3 picture**: an emulator run (em29.sh pattern on the gl30 EBOOT; the gl29x reference cfg cfg_emu/mgs2_gl29x_ref.cfg, shots every 2 vb from 30,500) against build_gl29/emu_x1/pic: changes only on the subset decals' pixels, within gl29's E5 margin (sub-pixel shift; px changing by more than 8 levels: a few a frame at most; none outside the decals). Hashes 62/62 and digests = the gl30 twin.
- **G4 cost [C model]**: the list-building cost of a main-turn walk window, gl29x against gl30 (PspProf or the cost model, as in earlier builds). Target: X within about 1 ms of O on main turns. The ME side is measured by the PSP run.
- **G5 build**: relink29.sh with T=/home/claude/t30 NAME=gl30 TITLE="MGS2 harness gl30 shadow fix"; xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk; psp.sh rc 0.
- **G6 reader**: rd29.py (or a copy rd30.py) reads the new counter.

## Delivery package (build_gl30/deliver, 13 files, as gl29x's)
- EBOOT, the 8 overlays, mgs2.cfg = gl29x's (build_gl29/x29/mgs2.cfg, 11f04b6f) with the header lines saying gl30, MGS2_GL_DZ=29490:31400:X unchanged, the same live captures and shots, README.txt for gl30 (what changed, what Ffm does: the same as gl29x, what the run should show), hashref.txt (72e0ca4c if G1 holds), pad.txt.

## Out of scope
X outside 29,490-31,400, the trail weight (DEVIATIONS B2), speed builds, the native renderer.
