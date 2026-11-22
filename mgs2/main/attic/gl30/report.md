# MGS2 gl30 build report (2026-11-20)

Built by the build agent from `attic/gl30/gl30_plan.md`. The agent could not write .md files, so the thread lead wrote this file from its final report (unchanged in substance). Not delivered when written.

## Summary
- EBOOT 9d2e688c5d4f7bf127a64a8d7cad0363. Gates G1, G2, G3, G5, G6 pass [C]; G4 (cost target about 1 ms) not met [C model]: X-O 2.29 ms a frame on a main-turn walk window (gl29x 10.16).
- Census vb 29,490-31,400: 12,247 targets, 10,306 exact, 1,941 subset (XS), fallbacks 0 (gl29x: 1,941 vertex-set fallbacks).

## What was built
- Tree /home/claude/t30 = copy of t29 (checked equal at the start: build_gl30/t30_initial.md5). Patch build_gl30/gl30.diff (77787347, 296 lines; t29 -> t30 plus the twin's xlive_chk.c).

| file | md5 | change |
|---|---|---|
| t30/ge/gelist.c | c0173220aec51128fc63cbc396c6e0d7 | dz_pair rewritten |
| t30/ge/gelist.h | d2cbeda62f35b02b0916d5cbeae59c71 | `int32_t nsub` at the end of GxDzCount |
| t30/psp/xl_psp.c | 1649af98a6eaf6861aa420fe21871824 | `sub:%ld` in the dz text; `subset` in the 'gl29 dz window' line |
| t30/ge/xme.c, xme.h | e4dc5e48, 4b7bbdb6 | unchanged |
| m30/boot/xlive_chk.c | ab40060a621d252e95b23ec25c809871 | the twin's digest line gains `sub:` |
| x86/boot (twin) | a857d9d9 | |
| psp/ge_gelist.o | 4cf06387 | |
| psp/MGS2T.elf | 585db160 | |

1. **Cheap reject (dz_near):** a candidate goes on to key work only if one of its vertices lies within 64 on each axis of the decal's first vertex. The walk is unchanged (nearest first, up to 64 back). A rejected candidate still sets the fallback reason as gl29 did (VSET if its 34-word matrix is equal, else MAT).
2. **XS (subset pairing):** only with arm X, while no subset base has been found yet, and only when the base has at least as many vertices as the decal. Offsets tried: the decal's smallest key minus each base key, |o|inf <= 64; the smallest that passes is kept. Tests: positions (dz_vin) and triangles (dz_tin), each a merge join over sorted keys. The nearest subset base is kept but the walk goes on for an exact base, which wins. With only a subset base the decal is drawn the Y way: its own vertices and strips copied to aux, each position minus the offset; counted in nsub.
3. **Linear-time attribute work:** sorted `key<<16 | index` arrays; the uv/colour copy is a merge join of (base key + offset) against them; the ATTR check compares neighbours in sorted order. Scratch at `(alow - cb) & ~15` as before.
4. **ME safety:** no libc, no statics added; undefined symbols me_memcmp and me_memset; .me_bss 12 B.

## G1 twin: PASS [C]
- tw30.sh, scripts/twin.sh pattern with `MGS2_HALF_CYCLE=b,b,b,b,b,b,b,b "MGS2_GL_DZ=29490:31400:X"`. hashref_g1.txt = gl29x's hashref_x1.txt byte for byte (4304f7e0); both differ from hashref_g22b in the known 177 lines from vb 20,200.
- Digests (g1cmp.py vs out_x1.log): 17,664 frames in both, content digests equal in all; 16,849 frames without XS equal (digests and dz counts); 815 frames with XS (vb 29,490-31,262) differ, and in all 815 gl29x's vertex-set fallbacks = gl30's subset pairs, every other count equal. Largest list+aux in the run 342,536 of 360,448 B, unchanged.
- Per batch (gerl harness over the 916 GER frames, gercmp.out): 1,941 batches differ (975 + 966) in 815 frames, all 0x42 XFORM decals drawn O by gl29 and from aux by gl30; every non-address word equal; the copy is the decal's own vertices moved by (0,5,0) in all 1,941; the address words point at the same positions. gl30 with XS off = gl29 byte for byte on all 916 frames.

## G2 census: PASS [C]

| | gl30 | gl29x |
|---|---|---|
| frames (all arm X) | 916 | 916 |
| 0x42 targets | 12,247 | 12,247 |
| paired exactly | 10,306 | 10,306 |
| subset (XS) | 1,941 | - |
| fallbacks base / matrix / vertex set / triangles / uv-colour / room | 0/0/0/0/0/0 | 0/0/1,941/0/0/0 |
| largest copy | 2,480 B (vb 30,560) | 2,144 B |
| largest list+aux in a GER frame | 163,148 B (vb 30,800) | 162,500 B |

Subset bases lie 3-20 batches back; after the cheap reject 1-3 candidates are left per decal.

## G3 picture: PASS [C]
- em30.sh tag g3 (536 s), EBOOT 9d2e688c, cfg_emu/mgs2_gl29x_ref.cfg (0e8d7d25), the same inputs as gl29x's emu_x1; 455 pictures; game folder deleted.
- Log (rd30_g3.out): 62/62 hashes = hashref_g22b; 916/916 digests = twin (455 ME turns, 461 main), dz text included; 458 shot lines, 0 with a problem, 0 GAVE UP, R 0.
- Pictures vs emu_x1/pic (picchk.py; luma; coverage = subset decals' screen triangles widened by 2 px): shots 162 identical, 257 differ; 2,618 px change by 2+, 2,540 inside this frame's XS coverage; outside the XS coverage of this frame and the one before: 6 px by 2+ (5 in frame 24,784, emulator noise known since gl26; 1 at 30,958), 0 px by 6+. 6 px change by more than 8 levels, at most 2 a frame (31,070), all inside the decals. Live captures: 16 identical, 20 differ, max 8 levels, 0 px over 8; live_31012 has 15 px changed by 2+. Trail carry-over of 1-2 levels at 31,086-31,124 and 31,264-31,278.
- List build, X window [C emulator time, not PSP]: gl30 ME 14,605.2 ms (avg 32.10), main 13,414.6 (avg 29.10); gl29x ME 15,448.4 (33.95), main 14,179.7 (30.76).

## G4 cost: TARGET NOT MET [C model]
PspProf (pgo.sh model), window vb 30,710-30,790 (main turn in the walk, 40 frames), gelist functions only (costfn.py), ms = (1.65 instr + 68 I-miss + 86 D-miss) / 333,000:

| build / arm | instructions | I-misses | D-misses | ms a frame |
|---|---|---|---|---|
| gl29x X | 193,419,574 | 1,122,839 | 472,075 | 32.74 |
| gl30 X | 145,685,049 | 820,831 | 408,828 | 24.88 |
| gl30 O | 136,142,024 | 752,967 | 290,593 | 22.59 |

- X-O 2.29 ms a frame (target about 1); gl29x 10.16. By kind: instructions +1.18, I-misses +0.35, D-misses +0.76.
- Where [C model, per PC]: the candidate walk (0892e240-0892e37f) about 1.08 ms, mostly dz_near (about 300k vertex checks, about 1,800 D-misses a frame); the X-only code inlined in gx_batch instructions +0.76, D-misses +0.74, I-misses +0.47; the dz_* functions about +0.47 (dz_sortk 0.18; gl29x 4.82); shared code I-misses -0.31. The dz code runs cold (no PGO profile; the profiles are gl20's).
- Why the walk is long [C, nearchk.py over 916 GER frames]: subset decals walk all candidates looking for an exact base. 0 of 12,247 targets have a subset base nearer than their exact base, so stopping at the first base of either kind would pair the same on this route and walk 100,062 candidates instead of 196,044 (about 0.4-0.5 ms a frame [I]). Not built (it changes the plan's rule).
- Scale [I]: gl29's PSP run measured main-turn X-O at 3.7 ms where the model gives 10.16 for the same code; if that ratio held, gl30 would be under 1 ms on the PSP. Not a pass.

## G5 build: PASS [C]
relink29.sh with T=t30, NAME=gl30, TITLE "MGS2 harness gl30 shadow fix" (relink30.out, psp.out). xme_rest_chk, xme_me_chk, mesec_chk, me_call_chk pass; psp.sh rc 0. ge_gelive.o and ge_xconv.o = gl23's; ge_gelist.o differs as expected.

| file | md5 |
|---|---|
| EBOOT.PBP | 9d2e688c5d4f7bf127a64a8d7cad0363 |
| ovl_d00t.bin | 3989459e7c1b3aa1a79c22e111d4d8a3 |
| ovl_d01t.bin | 8310b32b1e6ef97d011405b2b8bb0b00 |
| ovl_init.bin | 98d8db9a783f30e0f05a23a20f0e7e98 |
| ovl_n_title.bin | 181e5ea3603945478aef2cbe715c1a3f |
| ovl_r_title.bin | 28239a87c260b032e093e5e675324a32 |
| ovl_r_tnk0.bin | b9867759a1042e9c7d2a54d8639579ba |
| ovl_w00a.bin | 8b0507bc065e5a22e7a2950c870c933e |
| ovl_w01a.bin | e10b88587bc21b979366ba5afe746fed |

## G6 reader: PASS [C]
rd30.py = rd29 with an optional `sub:` field and a `subset` group; reads gl30's emulator log (455 + 461 digests = twin), gl29x's emulator log (rd30_x1.out) and gl29x's PSP log (rd30_hwx.out: 61 hashes, 755/755 digests). PSP log: `rd30.py --ref build_gl30/emu_g3/pic --twin build_gl30/x86/out_g1.log --hashref deliver/hashref.txt`.

## Delivery package (build_gl30/deliver)
EBOOT + 8 overlays (above); mgs2.cfg d0ff49e0 (gl29x's 11f04b6f with gl30 header lines; non-comment lines identical; MGS2_GL_DZ=29490:31400:X); README.txt 8fba4707 (as built by the agent); hashref.txt 72e0ca4c; pad.txt 7bc72ace; MANIFEST.md5 d727b1b1 (gl29's format, with the SLES_503.83 and mgs2disc.pak lines).

## UNKNOWN
- The ME-side list cost of gl30's X (the PSP run's 'gl29 dz window' line).
- The real PSP X-O on main turns and the model-to-PSP ratio on this code.
- Whether the real GE keeps the subset decals' shadow (live_31012 and the like).
- Why every twin differs from hashref_g22b in 177 lines from vb 20,200 (known, not this build).

## Files
- Scripts in build_gl30: tw30.sh, em30.sh, relink29.sh, g1cmp.py, gerl/{gerl.c, mk.sh, cmp.py}, picchk.py, rd30.py, costfn.py, subana.py, nearchk.py. Outputs: g1cmp.out, gercmp.out, subana.out, picchk.out, rd30_g3.out, rd30_x1.out, rd30_hwx.out, cost_m29x/m30x/m30o.out, relink30.out, psp.out, em30_g3.out, emu_g3/, model/.
- attic/gl30/: scripts, gl30.diff, mgs2_gl30.cfg, README_gl30.txt, MANIFEST_gl30.md5, out/ (13 text outputs). No pictures or game data.
- Scratch to delete when done: build_gl30/ger (591 MB), gerl/v29, v30, v30o (420 MB).

## Lead decisions (after the report)
- G4: ship as built and let the PSP run measure the ME and main-turn cost; the "stop at the first base of either kind" walk and a PGO pass covering the dz code go into a later build (one change per measurement).
- Independent verification before delivery: workflow verify-gl30 (code, gates, PSP hardware risk); result added below when read.
