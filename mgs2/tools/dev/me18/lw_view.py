#!/usr/bin/env python3
# lw_view.py <lw_lanes.h in> <out>: task #180 step 4b-1: ge/lw_lanes.h (tools/mk_lw.py's) made to read a transfer's copy through the late
# walk's view (ge/xlview.h; ge/xlme.c sets lw_view for a walk with one, 0 otherwise): lw_ptr's two returns and the lanes' re-pointing of t
# after a slot (lw_p07) and a prim (lw_fx) go through it; lw_ptr always inlined.  Each replaced text must be there exactly once.
import sys
s = open(sys.argv[1]).read()
R = [
 ("return b + 16 * n <= lw_size ? (const uint32_t *)(lw_mem + b) : 0; }",
  "if (b + 16 * n > lw_size) return 0; return lw_view ? (const uint32_t *)xv_whole(lw_view, b, n, XV_LANE) : (const uint32_t *)(lw_mem + b); }"),
 ("return o + 16 * n <= 0x4000u ? (const uint32_t *)(lw_spr + o) : 0; }",
  "if (o + 16 * n > 0x4000u) return 0; return lw_view ? (const uint32_t *)xv_whole(lw_view, XV_SPR + o, n, XV_LANE) : (const uint32_t *)(lw_spr + o); }"),
 ("tadr += 384; t = (const uint32_t *)((tadr & 0x80000000u) ? lw_spr + (tadr & 0x3ff0) : lw_mem + (tadr & (lw_size - 1) & ~15u));",
  "tadr += 384; t = lw_tp(tadr);"),
 ("tadr += 160; t = (const uint32_t *)((tadr & 0x80000000u) ? lw_spr + (tadr & 0x3ff0) : lw_mem + (tadr & (lw_size - 1) & ~15u));",
  "tadr += 160; t = lw_tp(tadr);"),
 ("static inline const uint32_t *lw_ptr(uint32_t a, uint32_t n)",
  "static inline __attribute__((always_inline)) const uint32_t *lw_ptr(uint32_t a, uint32_t n)"),   # (no call, as before the view: ge/xlview.h)
 ("   Do not edit: regenerate. */",
  "   Do not edit: regenerate (then tools/dev/me18/lw_view.py: task #180, the reads through the late walk's view). */"),
]
for a, b in R:
    n = s.count(a)
    if n != 1: sys.exit("lw_view.py: %d of %r" % (n, a[:60]))
    s = s.replace(a, b)
open(sys.argv[2], 'w').write(s)
print("lw_view.py: %d replaced" % len(R))
