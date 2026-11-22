import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
a_old = """static int add_tex(GlFrame *f, uint64_t key, const GerTex *t, int u0, int v0, const uint8_t *data, const uint32_t *pal, uint32_t size)
{"""
a_new = """#ifdef T1_SIM
#include <stdio.h>
/* x86 only (task #133, step 1 T1): what a second memo after the one-entry memo would answer.  Tables of n entries (the least recently used
   goes) keyed by TEX0, CLAMP and TEXA; "gen": an entry is taken only while gen_of is what it was when the entry was made; "any": taken
   whatever gen_of is (an upper bound: an identity check per page would lie between the two) */
#define T1S_MAX 256
typedef struct { uint64_t t0, cl, texa; long gen; } T1k;
static const int t1_n[] = { 1, 2, 4, 8, 16, 32, 64, 256 };
#define T1_NS (int)(sizeof t1_n / sizeof *t1_n)
static T1k t1_g[T1_NS][T1S_MAX], t1_a[T1_NS][T1S_MAX]; static int t1_gl[T1_NS], t1_al[T1_NS];
static long t1_asks, t1_memo, t1_look, t1_tex, t1_hg[T1_NS], t1_ha[T1_NS], t1_frames, t1_genmv, t1_lastgen = -1, t1_gl_in_frame;
static int t1_find(T1k *t, int *len, int n, uint64_t t0, uint64_t cl, uint64_t texa, long gen, int usegen, int put)
{
  int i;
  for (i = 0; i < *len; i++) if (t[i].t0 == t0 && t[i].cl == cl && t[i].texa == texa) break;
  int hit = i < *len && (!usegen || t[i].gen == gen);
  if (!put) return hit;
  T1k k = { t0, cl, texa, gen };
  if (i == *len) { if (*len < n) (*len)++; i = *len - 1; }
  memmove(t + 1, t, (size_t)i * sizeof *t); t[0] = k;
  return hit;
}
static void t1_print(void)
{
  fprintf(stderr, "t1 sim: frames %ld, texture asks %ld (one-entry memo %ld), look-ups after it %ld (a texture %ld), gen moves seen between look-ups %ld\\n",
          t1_frames, t1_asks, t1_memo, t1_look, t1_tex, t1_genmv);
  fprintf(stderr, "t1 sim: hits by entries, gen kept / any gen:");
  for (int k = 0; k < T1_NS; k++) fprintf(stderr, " %d: %ld (%.1f%%) / %ld (%.1f%%)", t1_n[k], t1_hg[k], t1_look ? 100.0 * t1_hg[k] / t1_look : 0.0,
                                              t1_ha[k], t1_look ? 100.0 * t1_ha[k] / t1_look : 0.0);
  fprintf(stderr, "\\n");
}
static void t1_once(void) { static int once; if (!once) { once = 1; atexit(t1_print); } }
#endif
static int add_tex(GlFrame *f, uint64_t key, const GerTex *t, int u0, int v0, const uint8_t *data, const uint32_t *pal, uint32_t size)
{"""
b_old = """  long gen = f->memo_on ? gen_of(f->map) : 0;
  if (f->memo_on && f->memo.valid && f->memo.t0 == t0 && f->memo.cl == cl && f->memo.texa == texa && f->memo.gen == gen) {
    f->n.tex_memo++;
    if (!f->memo_check) { *xt = f->memo.xt; return f->memo.ti; }
  }
  TkTex k; int kind = tk_get_um(f->tk, gr, ctxt, cl, &k);   /* sb9: through the memo by uploads (OPT2_TX_UMEMO; off: tk_get_cl, as gl12) */"""
b_new = """  long gen = f->memo_on ? gen_of(f->map) : 0;
#ifdef T1_SIM
  t1_once(); t1_asks++;
#endif
  if (f->memo_on && f->memo.valid && f->memo.t0 == t0 && f->memo.cl == cl && f->memo.texa == texa && f->memo.gen == gen) {
    f->n.tex_memo++;
#ifdef T1_SIM
    t1_memo++;
#endif
    if (!f->memo_check) { *xt = f->memo.xt; return f->memo.ti; }
  }
#ifdef T1_SIM
  t1_look++;
  if (t1_lastgen >= 0 && gen != t1_lastgen) t1_genmv++;
  t1_lastgen = gen;
  for (int s = 0; s < T1_NS; s++) { t1_hg[s] += t1_find(t1_g[s], &t1_gl[s], t1_n[s], t0, cl, texa, gen, 1, 0);
                                    t1_ha[s] += t1_find(t1_a[s], &t1_al[s], t1_n[s], t0, cl, texa, gen, 0, 0); }
#endif
  TkTex k; int kind = tk_get_um(f->tk, gr, ctxt, cl, &k);   /* sb9: through the memo by uploads (OPT2_TX_UMEMO; off: tk_get_cl, as gl12) */
#ifdef T1_SIM
  if (kind == TK_TEX) { t1_tex++;
    for (int s = 0; s < T1_NS; s++) { t1_find(t1_g[s], &t1_gl[s], t1_n[s], t0, cl, texa, gen, 1, 1); t1_find(t1_a[s], &t1_al[s], t1_n[s], t0, cl, texa, gen, 0, 1); } }
#endif"""
c_old = """void gl_begin(GlFrame *f)
{
"""
c_new = """void gl_begin(GlFrame *f)
{
#ifdef T1_SIM
  t1_frames++;
#endif
"""
for o, n in ((a_old, a_new), (b_old, b_new), (c_old, c_new)):
    if s.count(o) != 1: sys.exit("not once: " + o[:60])
    s = s.replace(o, n)
open(p, 'w', encoding='utf-8').write(s)
print("ok")
