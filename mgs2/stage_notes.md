# stage.dat (PAL): facts

Source: Ffm's disc, `mgs2\stage.dat`, read from their PC 2026-10-17. 657,709,056 bytes, SHA-1 c019646f4735d302ec77dc63029ce25732a44b7d. Copy: `/mnt/project-files/mgs2/disc/stage.dat`. Game data, personal use only.

Unpacked with [tools-mgs](https://github.com/Joy-Division/tools-mgs) `dat-extract_enc` (zlib licence, lists SLES-50383 by signature) and the [JoyDict](https://github.com/Joy-Division/JoyDict) PAL dictionary. Re-run: `/mnt/project-files/mgs2/tools/extract_stage.sh` (output ~1.4 GB, kept outside the shared folder).

- Header: build date 11.12.2001 08:19:01 UTC, version 1, **123 stages**, game id 4010.
- Files by type (all stages): kms 14,325; tri 3,108; cv2 891; evm 529; sar 417; mar 416; var 253; o2d 187; gcx 131; lt2 129; **bin 124**; sdx 108; hzx 101; anm 38; zms 35; others < 30 each.
- Each stage dir has `cache/` (assets), `nocache/<stage>.bin`, and `pk*.sdx` sound banks.

## Per-stage code images (`nocache/<stage>.bin`)
- Every playable/demo stage carries a **full, separately linked copy of the game code**: 1.0–3.0 MB each (w00a 2,258,944; w11a 3,049,472; init 1,107,968). `r_*` resident stages hold only 2 KB.
- Header: word 0 = load address, **0x1F1C00 in all 123 images** (NTSC-U notes say 0x1F1480); word 1 = end of image; word 2 = end of bss (w00a: 0x418f10 / 0x42e530; w11a: 0x4da180 / 0x4f0f50); word 3 = 0x1F1C10; then (hash, address) pairs.
- The PAL ELF image ends at 0x1EDBB4, so the stage image sits directly above it.
- Content is code nearly end to end (w00a: 6,231 `jr ra`, 29,402 `jal`, all targets in range).
- **Stages are relinked, not shared:** w00a vs w01a match at the same offset in only 7,126 of 553,984 words; the first difference is at byte 0x40. Same functions, different addresses.
- Dedup estimate (functions split at `jr ra`, jump/immediate fields masked): 123 images = 257 MB of code spans, but only ≈13,455 distinct chunks, ≈5.9 MB excluding 97 chunks > 64 KB (8.8 MB, probably data). (Estimate, masking can merge near-identical functions.)

## Consequence for the recompile (inferred)
- Recompiling each image on its own: ≈2–3 MB of guest code per stage; at TT's 3.6× growth ≈ 8–11 MB of native code per stage, loaded per stage (one at a time).
- Alternative: recompile each distinct function once and map per-stage addresses through a table; smaller total, more work in `recomp.py`.
- Decide after measuring: the ELF's role (loader + libs?), and how the game jumps into the stage image.
