# B proof: which VU0 results of the drawing code reach the game state (task #176)

Thread "MGS2 port", 2026-11-08. For Ffm's card of 2026-11-08 01:54 ("Game logic exact") and DEVIATIONS.md E1. Function names from the HD
reference list (our function-name lists), names only.

## The question

With "Game logic exact" the game logic's VU0 vector math rounds toward zero as on the PS2. The draw pipeline (the game's drawing functions)
and the box-cull micro programs could keep the VFPU's faster forms, which round to nearest, only if a PC proof shows their VU0 results
never reach the game state. If the proof fails, the choice goes back to Ffm (DEVIATIONS E1).

## Method [C, x86 twin]

- Build /dev/shm/vb1 (container only), dev switch `MGS2_VFPU_B=<mode>:<map>:<allow>`: each VU0 op takes its rounding from the host function
  that makes it. Mode 0: every op toward zero (the reference, the same as `MGS2_VFPU_RZ=1`). Mode 1: the listed functions round to nearest
  (the VFPU's rounding), the rest toward zero. Mode 2: mode 1 with the last bit of every result of a listed function flipped (poison).
- Route i (play/pad_w01af.txt) from boot to vb 37,800: title, opening, the deck (w00a), w01a. FPU toward zero in every run.
- Each run against mode 0 at every one of 37,799 vblanks:
  - rh / wh: hashes of the places game code reads and stores outside the draw pipeline (3,326,974,418 stores). Equal: no control flow changed.
  - wv / rv: per store site and lane (98,956) and per reader site and lane (114,408) outside the pipeline, the sum of the values. Equal: no
    changed value reached game code.
- A set passes only when all four are equal in both modes 1 and 2.

## Results

rh and wh were equal in every run: no run changed control flow outside the pipeline on route i. The values decide.

| Run | Functions on the VFPU's rounding | Store lanes differing | Reader lanes differing | Result |
|---|---|---|---|---|
| V | box cull: 660a267e_0, 96936c0f_0, 8baafbc3_0/2/6 | 79 | 66 | fails |
| g6 | 660a267e_0 | 8 | 4 | fails |
| g9 | 96936c0f_0 | 17 | 13 | fails |
| g8 | 8baafbc3_0/2/6 | 79 | 66 | fails (all of V) |
| M | OnePieceObjs 0x11fa58, DG_SetObjsMatrix 0x120538, StoreMatrixObjs 0x120860 | 41 | 48 | fails |
| gMa | OnePieceObjs | 41 | 48 | fails (all of M) |
| gMb | DG_SetObjsMatrix | 0 | 0 | passes |
| gMc | StoreMatrixObjs | 0 | 0 | passes |
| S | CalcSortZ 0x12dbc0, DG_Prim2Chanl 0x12dc90 | 111 | 114 | fails |
| gSa | CalcSortZ | 7 | 7 | fails |
| gSb | DG_Prim2Chanl | 111 | 114 | fails (all of S) |
| H | shadow chain: MakeShadowChainObjs 0x131050, WriteShadowChainObjs 0x131248, OrthoScreenMatrix 0x133950, GetParallelViewport 0x133a50, DG_ShadowChainChanl 0x133cc0 | 0 | 0 | passes |
| O | MakeMaxBound 0x1365e0, 0x139940, ChainObj 0x13bf78, PluginActor 0x13d540 | 19 | 16 | fails |
| gP | H's five + DG_SetObjsMatrix + StoreMatrixObjs | 0 | 0 | passes (both modes) |

In gP the seven functions made 456 M VU0 ops on the route, all inside the draw pipeline, every one of them poisoned in mode 2.

## How the failing parts reach the game state

- 660a267e_0 (the box cull, 1023 calls a deck step): its last value of VF10 stays in the register. d00t 0x2abe98 stores VF10 at 0x2ac2f4
  and 0x2ac33c (the same code in d01t at 0x2b3b64 / 0x2b3bac) after writing only some lanes itself, so the w lane is the box cull's; d01t
  0x329668 then copies those quadwords (0x3296dc-0x329704).
- 96936c0f_0: w lanes in d00t 0x2abe98, 0x2b4c90, 0x2b4f20, 0x323b58, 0x323c38; read in DG_PutVector 0x125f18 and DG_RotVector 0x125f78.
- 8baafbc3 (entries 0, 2, 6): the same w-lane paths, and game logic values: GM_ActControl 0x106d70 (x lane, the store at +655c) and
  HZX_GetLevelHeight 0x14e028 (x), CheckBlockBound 0x14f820 (z, w), HZX_MoveDynamicSegment 0x1511c8 (w), DG_MakeCameraMatrix 0x11ef58 (w),
  w01a 0x3a3f90 (y) and others.
- OnePieceObjs: w and zw lanes in 0x10faf0, 0x116d78, CheckBlockBound, HZX_MoveDynamicSegment, 0x15b288 and stage code (d00t 0x323b58 y).
- CalcSortZ: w lanes in d00t 0x323b58 and d01t 0x329668.
- DG_Prim2Chanl: everything above plus MT_QuatToMat 0x11c828 and sceVu0MulMatrix 0x165d78 (x y z).
- O: w lanes in d01t 0x2bb2a8, 0x2bb7c8, 0x329668, 0x329748, w00a 0x3b7c88; y in 0x16d0a8 and d00t 0x323b58.

Many of these are lanes the game stores but never decides on (w lanes of 3-vectors), but some are game logic (GM_ActControl's x, level
heights, the block bounds). By the standard of the card they all fail.

## The smallest exact set (B')

- On the VFPU: the shadow chain's five functions, DG_SetObjsMatrix, StoreMatrixObjs.
- Toward zero: the game logic, the three box-cull micro programs, OnePieceObjs, CalcSortZ, DG_Prim2Chanl, MakeMaxBound, 0x139940,
  ChainObj, PluginActor.

## Cost against A (everything toward zero) [C model census, task #173; PSP figures I]

| Rule | Rainy deck step, model ms | PSP ms [I] | Exact |
|---|---|---|---|
| Today (all VFPU) | 0 | 0 | no (E1) |
| A: everything toward zero | +9.3-10.4 | +8.6-9.7 | yes, everywhere by construction |
| B': A with the seven on the VFPU | +8.9-9.9 | +8.3-9.2 | yes on route i |
| B as first proposed (only the logic toward zero) | +2.35-3.22 | +2.3-3.2 | no: fails this proof |

- B' keeps 7,301 VU0 ops a deck step on the VFPU (DG_SetObjsMatrix 3,488, StoreMatrixObjs 3,813; the shadow chain makes none on the
  deck): 0.35-0.48 model ms (48-65 ns an op, #173's range), about 0.31-0.43 PSP [I]. Inside: 5,409 ops, 0.26-0.35 model ms.
- So B' costs close to A: about 4% less.

## Limits and what is left

- Route i only. On other stages B' is UNKNOWN. How we find out: the same pair of twin runs (mode 0 and B') on each new route. If B' goes
  into a build, an all-toward-zero twin (A) is the reference for every route's hash test.
- The box cull's leak is register leftovers (VF10) and VU0 data memory. No cheap exact form of its exit state is known [I]. A possible exact
  speed-up for later: run 660a267e_0 on the VFPU and compute its exact leftovers only when game code reads them; up to ~4.7 PSP ms a deck
  step [I], UNKNOWN until it is designed and proven.
- Recommendation: A. It is exact everywhere without a proof per route, and B' saves only 0.3-0.4 ms.

## Files (container, ephemeral)

/dev/shm/vb1: boot_vb.c (the switch, `VU_B_TEST`), run_g.sh / run_gm.sh, allow_g*.txt, cmp_g*.txt (each comparison), out_g*.log.gz.
