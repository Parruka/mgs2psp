---
name: mgs2-renderer-stage-a
description: MGS2 GE renderer Stage A on x86 (2026-10-21..26): packet translator ge/xlate.c, live check switches, texture cache key, GS memory map gsup.c, PSP texture path, VU1 entry and volume census
metadata:
  type: project
---

Status and plan: [[mgs2-renderer-work]]. Notes: mgs2/renderer_notes.md §11.10 (translator + textures), §11.12 (census, PSP plan, measurements).
- TRANSLATOR ge/xlate.c (+ gsfront.c front end, gebatch.c batches, xlive.c second walk, xldig.c digests): no micro code; per-entry rule (A+D packet + tag template). gsref XLATE/XLCONV/XLDIRECT/XLTEX byte-identical on the capture.
- LIVE ON BOTH ROUTES (§11.10 step 6; x86 tools/dev/build_xlive.sh + boot/xlive_chk.c): every vblank's packet hash = the exact walk's, 0 differing texels / palette entries at use. Switches: MGS2_XLIVE=from:to[:dig], _TEX, _TK, _TRX, _WATCH, _CENSUS, _VCEN, _FAST, _SYNC, _NOSEEN, _CG (callgrind window).
- CACHE KEY: EE address + size + 64 sampled words, and a full hash once a source has shown a second key -> 0 stale keys on both routes, 4.9 KB/vb (deck) / 15.0 (free). Only 2 sources per route ever change (the text image EE 0x1ed8500, T4HH 512x120 at 0x3000, and a T4 400x192).
- MAP ge/gsup.c: block lists (4 entries, lanes), other layouts, local copies, drawn areas (8 slots = GE render targets); boot's GS clear = zeros (gu_zero_empty). Details §11.10.
- PSP TEXTURE PATH ge/texcache.c (§11.10 step 7): content identities (gu_keys), tk_get converts on a miss, LRU + budget, never evicts this or last frame. Whole deck route check: 0 differing texels; ~2 MB converted in the heaviest frame, ~6 MB a route: a route needs eviction, a frame does not.
- VU1 ENTRY CENSUS (step 6): deck route 24 entries, 18 with a rule; without: 6 d00t cutscene entries (0x1a31e0 ...), P06 +0x738 / +0xab8 (0x1a0540 / 0x1a08c0, guards).
- CENSUS (§11.12; MGS2_XLIVE_VCEN=1, -DXL_CENSUS, tools/dev/vcen_sum.py): 1.0-1.5 M VIF1 words a vblank, texture uploads 3.0-4.8 MB of it on shown AND reduced vblanks; UNPACK only V4-32 / V4-16 / V2-16 mask 0x00ffff10 (no fill, no mode 2). Converters next: P06 (+0, +358, +738, +ab8) first; w01a's stage program entries 0x201a18/aa8/c10/e30 lack rules (rule by micro code words).
