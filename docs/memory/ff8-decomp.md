---
name: ff8-decomp
description: Public byte-matching FF8 PS1 decompile (roengstrom/ff8-decomp) matches Ffm's exact discs; progress and how we may use it (survey 2026-11-11)
metadata:
  type: reference
  modified: 2026-11-11T01:29:56.532Z
---

Part of [[ff8-port]]. Survey: /mnt/project-files/ff8/decomp_survey.md (2026-11-11).

- github.com/roengstrom/ff8-decomp: PS1 USA SLUS_008.92, CC0, PsyQ 4.1/4.3 + splat + maspsx, SHA1-verified rebuild. Its four disc SHA1s equal Ffm's four .bin files (checked on the PC 2026-11-11), so its addresses = our recompile's.
- Progress (decomp.dev JSON at decomp.dev/roengstrom/ff8-decomp.json; github.com and decomp.dev refuse the container/web-fetch, the built-in browser works): 2026-11-10 70.6% code; main exe 100% (libgte, libgpu, libpress, libcd, sound, render, lzs); field.bin 81%; movie.ovl 100%; battle 38%; world 46%.
- Use: names for profile addresses (symbol files config/symbols.*.txt), hot-function rewrites, movie format (R7) from movie.ovl/libpress.
- RULE (Ffm 2026-11-11 01:31, "if it is useful lets us it, but always verify"): their C may be used directly in the FF8 port. Always verify: per-frame hash test (x86, PPSSPP, PSP), keep the PS1 cycle charge, replace and check PS1-only parts (GTE inline asm, overflow/load-delay reliance), record the decompile commit + function names per use.
- raw.githubusercontent.com works for reading its files in the container (not kept in the project).
