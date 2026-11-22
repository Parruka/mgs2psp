# m1den: the FPU denormal rule, PSP against PS2 (audit M1, 2026-10-23, x86 only)

Dev tools that answered audit M1 (speed_notes.md §40). Results: `prof/m1_denormal_2026-10-23.txt`.

- `rewrite.py`: copies of the generated code (W/*.h) with every lwc1 / mtc1 site wrapped in M1RDF / M1RDFS / M1I2F (guest.h with
  `guest_m1.patch`, `-DM1DEN`), which record each denormal operand per site (`m1den.c`).
- `rewrite_vg.py`: every vu_vgrp call wrapped in M1VG (`-DM1VGT`), which logs each run in a window of vblanks (VF before, fallback).
- `m1den.c`: env `MGS2_M1_PS2=1` gives the PS2 rule (MXCSR DAZ + FTZ + RZ, FADD / FSUB without the PSP's zero rule);
  `MGS2_M1_VG=a:b` the vu_vgrp window, `MGS2_M1_VGSEQ`, `MGS2_M1_VGPC=<pc>` dump every run of one program.
- `build.sh <dir>` links against the x86 reference build's boot_rt.o (EXTRA=-DM1VGT for the trace build);
  `m1run.sh <dir> <tag> d|w [env]` runs a route like runref2.sh; `m1run2.sh` keeps the pkt: / m1vg lines, STOP=<vb>.
- `analyze.py <dir> <tag> d|w` (sites + hashes against the v21 references), `sites.py <log>`, `vgcmp.py <A log> <B log>`.
- The paths inside the scripts point at the ephemeral container (/home/claude/m1den, /home/claude/vgz/W): adjust before a rerun.

Use for a new route: build A (PSP rule) and B (`MGS2_M1_PS2=1`) on that route; ee / iop / vuh hashes and the per-vblank VIF1+GIF
packet hash (`MGS2_PKT_HASH=1:<end>`) must be equal between A and B. A difference names a site where the rules matter.
v30 (2026-10-26, the indoor route; `prof/m1_denormal_v30_in.txt`): `psp/v30.sh m1` runs it on a header set of the leaner generator:
rewrite.py also wraps `RDFB(` (tools/mm_base.py's base + offset load; `M1RDFB` in guest_m1.patch, kind lwc1b), build.sh takes M1W / M1RT /
M1C, m1run2.sh has the routes f and i, sites.py reads M1W.
