MGS2T_v25f in PPSSPP 1.20.4 headless (JIT, PPSSPP_VFPU_RN=1), run e2, 2026-10-23 (psp_harness.md "Build v25f"; speed_notes §43).
Same files as the delivery (EBOOT md5 1ea7184c948cfc1649879249de97a2e8, the 8 overlays, pad_v25f, hashref = X25p.dense, mgs2disc.pak
md5 0792607195dcb838626456a41c8278e3) and mgs2_free.cfg = the delivered mgs2.cfg.
- log_free.txt: every state hash equal to the PC reference (124 / 124 every-100 points, 431 / 431 hashref points, vuh 311 / 311, no split);
  last hash vb 36500 ee 314171ae8c81260a iop 13d248834483805d vfb 586351.
- Hook: 1,038,884 calls (ELF 10,451, overlays 1,028,433), 0 not found; 9,200-9,892 per 100 vblanks in the timed steps (95.6 per step).
- Timed steps (vb 30500-36500, PPSSPP's clock, not the PSP's): mean 44.20 ms (logic 20.44, draw pipeline 19.41, IOP+sound 0.83, GS walk 3.52);
  v24's tutorial in the same emulator 36.54 (14.93 / 17.04 / 0.94 / 3.62). By phase: stand 44.79, walk 44.72, crawl 44.16, punch / fire 44.70,
  first person 40.75, alert 44.48. No disc read in the timed steps. No VU0 interpreter call after vb 29068 (273,114 calls before, all in
  d01t's boarding cutscene; the PC count at vb 36500 is the same).
- Memory at the end: malloc can still give 9,552 KB (heap block 0b153800, break 0b3c4000).
- battery_free.txt: the battery thread's lines, every 60 s of PPSSPP's clock; PPSSPP answers 100 %, 0x8002013a for the voltage and 0 for the
  rest, so this checks the format only. "vb" is the last vblank the hook saw (-1 before its first call).
- Run e1 (the build before the x_call fix) stopped at vb 29068 on stage function 0x106020 (an ELF address in w00a's translation): fixed in
  psp/xhook.c (ELF table first, then the loaded stage's).
