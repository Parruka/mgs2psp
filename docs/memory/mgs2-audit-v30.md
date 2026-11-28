---
name: mgs2-audit-v30
description: MGS2 audit of build v30 (2026-10-26, mgs2/audit_v30.md): build and exactness verified; switch-off analysis corrected (v24 run 1 ~22 min, ME-off denominator, one clean ME-off run is weak evidence)
metadata:
  type: project
  modified: 2026-10-26T16:08:30.139Z
---

Ffm asked 2026-10-26 15:59 for a full audit of v30 while the ME-off rerun ran. File: mgs2/audit_v30.md. 0 High, 3 Medium, 8 Low.

Verified: all 15 delivered md5s on the PC + the ME-off cfg/README; pack layout recomputed from the read log (11 files, 31 ranges,
91,461,632 bytes); cov_x30 = cov_x25 + 689; x86 in_v30 refs = free_v28 to vb 30,500, = deck to 23,400; every emulator log and the PSP
run 1 exact (own script); mix 75/75 (ck 0-60 = free route); M1 check; order numbers; model arithmetic and the 1.116 factor.

Medium (switch-offs, see [[mgs2-me-audio]]):
- v24 deck run 1 stopped ~22 min in [I] (steps 10.9 min + 347 hashes ~1.95 s; same method gives v30 1,254 vs 1,251 s measured), not
  "about 11 minutes". The 4 stops form 2 pairs: opening vb 18,300-19,000 at ~21-22 min (v24 r1 ME off, v30 r1) and free route vb
  33,600-34,200 at ~39 min (v26me2, v26me3 r2). Spot and time are confounded on scripted routes; ~28 runs passed the first stretch.
- Exposure: ME off 25 runs ~12.1 h, 1 stop; ME mixing 6 runs ~3.7 h, 3 stops (1 per ~74 min) [I]; ~4% chance by luck [I].
- One clean ME-off run is weak evidence (a 40 min ME run stays on ~58% anyway) [I]; plan 3-4 ME-off runs before deciding.
Low: VOX.DAT reads at vb 36,616-37,633 in the timed range (spikes); model equal-weights windows (time-weighted ~36.6 PSP); me0
emulator evidence only in the ephemeral container; no v30 section in speed_notes.

**Why:** the ME sound plan rides on reading the rerun correctly. **How to apply:** quote these numbers when the rerun's log arrives.
