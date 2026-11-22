# patch_wtdbg.py <in xlive_chk.c (6098915d)> <out>: task #180 4b-2 b3 follow-up: me12's tracker with MGS2_WT_DBG=v0:v1:a0-a1[,a0-a1...]
# (hex addresses): every reported store into the ranges, every transfer (and whether its walk reads the ranges), every hook, in vb v0..v1
import sys, hashlib
src, out = sys.argv[1], sys.argv[2]
t = open(src).read()
assert hashlib.md5(t.encode()).hexdigest().startswith('6098915d'), 'unexpected source'
def rep(old, new, cnt=1):
    global t
    assert t.count(old) == cnt, (old, t.count(old)); t = t.replace(old, new)
rep("static long cur_vb;\nstatic int wt_on, wt_dg;",
"""static long cur_vb;
static int wt_on, wt_dg;static long wd_v0 = -2, wd_v1 = -1, wd_seq; static uint32_t wd_a[8][2]; static int wd_n;   /* MGS2_WT_DBG (b3 follow-up) */
static int wd_on(void)
{
  if (wd_v0 == -2) { const char *e = getenv("MGS2_WT_DBG"); wd_v0 = -1;
    if (e && sscanf(e, "%ld:%ld:", &wd_v0, &wd_v1) == 2) { const char *p = strchr(strchr(e, ':') + 1, ':') + 1; unsigned a, b; int k;
      while (wd_n < 8 && sscanf(p, "%x-%x%n", &a, &b, &k) == 2) { wd_a[wd_n][0] = a; wd_a[wd_n][1] = b; wd_n++; p += k; if (*p != ',') break; p++; }
      fprintf(stderr, "wtdbg: vb %ld-%ld, %d ranges\\n", wd_v0, wd_v1, wd_n); } else wd_v0 = -1; }
  return wd_v0 >= 0 && cur_vb >= wd_v0 && cur_vb <= wd_v1;
}
static int wd_hit(uint32_t a, uint32_t n) { for (int i = 0; i < wd_n; i++) if (a < wd_a[i][1] && a + n > wd_a[i][0]) return 1; return 0; }""")
rep("""  wt_all++; if (ws_t) ws_count(w);
""", """  wt_all++; if (ws_t) ws_count(w);
  if (wd_on() && wd_hit(a & (GSZ - 1), n ? n : 1)) fprintf(stderr, "wtdbg %ld: store vb %ld step %d writer %lx at %07x n %u\\n", ++wd_seq, cur_vb, wt_step,
                                                       (unsigned long)w, (unsigned)(a & (GSZ - 1)), n);
""")
rep("""  gx_seen = 1;                            /* MGS2_XLIVE_GX""", """  if (wd_on()) fprintf(stderr, "wtdbg %ld: transfer vb %ld step %d ch %d chcr %08x madr %08x qwc %x tadr %08x%s\\n", ++wd_seq, cur_vb, wt_step, ch, chcr, madr,
                       qwc, tadr, skip_now ? " (skipped)" : "");
  gx_seen = 1;                            /* MGS2_XLIVE_GX""")
rep("""    if (df_snap) { df_walk(mem, spr, chcr, madr, qwc, tadr); if (wt_on) wt_kick();""",
"""    if (df_snap) { df_walk(mem, spr, chcr, madr, qwc, tadr);
      if (wd_on()) for (long i = 0; i < df_nr; i++) if (wd_hit(df_r[i].a, df_r[i].n)) fprintf(stderr, "wtdbg %ld:   it reads %07x+%u (kind %u)\\n", ++wd_seq,
                                                                                    (unsigned)df_r[i].a, (unsigned)df_r[i].n, (unsigned)df_r[i].k);
      if (wt_on) wt_kick();""")
rep("""  if (wt_on) wt_step++;   /* me12: writes from here on are in the next step */
""", """  if (wt_on) wt_step++;   /* me12: writes from here on are in the next step */
  if (wd_v0 >= 0 && vb >= wd_v0 && vb <= wd_v1 + 1) fprintf(stderr, "wtdbg %ld: hook vb %ld: step %d from here\\n", ++wd_seq, vb, wt_step);
""")
open(out, 'w').write(t)
print('ok', hashlib.md5(t.encode()).hexdigest()[:8])
