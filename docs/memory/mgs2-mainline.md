---
name: mgs2-mainline
description: MGS2's one-folder build tree (mgs2/main/, rebuilt 2026-11-18 after the container reset): build_all.sh steps, checks passed, PPSSPP path, what was lost (PGO, route-d VU0 images)
metadata:
  type: project
  modified: 2026-11-18T21:37:31.766Z
---

- Rebuilt 2026-11-18 (container reset had deleted sb17/sb18/sbg18/me18e/v31): sources + scripts in **/mnt/project-files/mgs2/main/** (115 files, README.md; no generated headers, objects or game data); the working copy is /home/claude/main (build outputs in build/ and gen/, container only).
- `build_all.sh` steps: hdr (mk_headers 274 s), x86 objects (90 s), vu-dump + vu (165 s), twin-build (27 s), twin-check (157 s; NOCHECK=1 skips), psp (195 s), emu only with EMU=1 (450 s). A rerun refuses while build/psp or gen/vudump exist: delete those two by literal path first. From scratch 908 s; reproducible (EBOOT 9f2ed223 and 8 overlays equal md5 for md5).
- Checks [C]: x86 twin r18a 685/685 lines = prof/hashref_x86_in_gl18me.txt (hashref md5 50de900f, byte-equal); plain PSP build on PPSSPP 1.20.4: 434/434 hashes, 311 vuh, 150 frame digests = twin, ME mode-4 counts = hwme5e; emulator headroom ~2,412 KB.
- PPSSPP 1.20.4 headless: /home/claude/ppsspp/src/ppsspp-1.20.4/build/PPSSPPHeadless (patch tools/ppsspp_1204_mgs2.diff in main/, VFPU_RN part reconstructed; FFmpeg off; build ~9 min). The master 83e616ea is not rebuilt yet.
- Lost / differs from gl18me: no PGO profile and no code order (PSP speed UNKNOWN, likely slower [I]); VU0 header vp22i lacks route d's 3 images (fe0033d5, b641370c, de078118: interpreted, exact); VP_KEEP=vp9 lost (5 functions' order); x86 IOP flags from boot.c's documented line (IOP hashes match). runtime/vu0.h has E1's VU_RZ_ALL behind an #if not defined.
- gl19 (2026-11-18): MGS2_HALF switch (tools/patch_half.py on P13f140, boot.c rt_half_skip, 18 act addresses; bits 1 ropes 2 lamps 4 sea 8 trail), cov_gl19.txt adds w01a 2b2b28; env.sh B=, emu.sh CFG= REF= TW=; cfg/mgs2_gl19fb.cfg + hashref_gl19fb.txt af1c02eb; build_gl19me = gl19 with the ME Count fix (EBOOT bf14ff6a). main_psp.c fixed w_cfg[35] (deck steps were counted twice in gl18me logs: n/over20/over40 doubled, means right).
- Owed: PGO + code order pass (gl20), reference shots (none survived; master 83e616ea not rebuilt).
- gl20 (2026-11-19, build_gl20/psp): PGO trained in this tree (gcda3, container only), psp.sh shadow folder MGS= one path (static functions' profile ids hash the source path), code order build_gl20/ord2 + lay (2 rounds), ME timers patch (scratchpad metime/apply.py; it changed 8 profiled functions' checksums, so it was applied before the last training run). Lost profiles 4 (trail, 0.07 ms). Exact on the emulator.
- CONTAINER RESET 2026-11-18: rebuilt /home/claude/pspsdk and /home/claude/mgs2 (SLES_503.83, stage.dat md5 ac624091, stage/ 123 stages); lost trees' files in /home/claude/bk/home (PC _claude_tmp\bk_home_2026-11-16.tar 27b22588).
