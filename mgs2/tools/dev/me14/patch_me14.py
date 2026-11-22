# patch_me14.py <sb14 ge dir> <out ge dir> [<xlate.c> <out xlate.c>]: task #149 (the ME picture's build order step 2): the picture files ME-safe under
# -DMGS2_ME_SAFE (ge/me_rt.h): the C library's calls mapped to me_rt's, calls through pointers through ME_CALLP, zk as a float made
# at init, gb_vert's zk branch out on the PSP, the thinning's ranges handed in by the host (gl_thin) instead of fx_thin_scan in gl_call.
# Every replacement must match the counted number of times.  Writes the 11 files, gelive.h, gebatch.h, xconv.h to <out>.
import sys, os
S, O = sys.argv[1], sys.argv[2]
def edit(name, reps, src=None, out=None):
    s = open(src or os.path.join(S, name), encoding='utf-8').read()
    for old, new, n in reps:
        c = s.count(old)
        if c != n: sys.exit('%s: %d matches (want %d) for: %s' % (name, c, n, old[:120]))
        s = s.replace(old, new)
    open(out or os.path.join(O, name), 'w', encoding='utf-8').write(s)
INC = '#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */\n'
edit('xlive.c', [('#include "xlate.h"\n', '#include "xlate.h"\n' + INC, 1),
                 ('m->seen(m->ctx,', 'ME_CALLP(m->seen)(m->ctx,', 3)])
XL = [('#include "gsfront.h"\n', '#include "gsfront.h"\n' + INC, 1),
                 ('if (pass_fn) pass_fn(pass_ctx, c, k + 2);', 'if (pass_fn) ME_CALLP(pass_fn)(pass_ctx, c, k + 2);', 1),
                 ('if (call_fn) call_fn(call_ctx, &c);', 'if (call_fn) ME_CALLP(call_fn)(call_ctx, &c);', 1),
                 ('if (flush_fn) flush_fn(flush_ctx);', 'if (flush_fn) ME_CALLP(flush_fn)(flush_ctx);', 1)]
edit('xlate.c', XL)
if len(sys.argv) > 4: edit('xlate.c', XL, sys.argv[3], sys.argv[4])   # another xlate.c (me13's ge_e: the take-back's keep and load) and its out
edit('gsfront.c', [('#include "gsup.h"\n', '#include "gsup.h"\n' + INC, 1),
                   ('if (g->fn) g->fn(g->ctx, g, &g->d, v, n);', 'if (g->fn) ME_CALLP(g->fn)(g->ctx, g, &g->d, v, n);', 1)])
edit('texconv.c', [('#include <string.h>\n', '#include <string.h>\n' + INC, 1),
                   ('(TcUpload){ ram(rd32(pkt + 0x74)),', '(TcUpload){ ME_CALLP(ram)(rd32(pkt + 0x74)),', 1),
                   ('v = fetch(ctx, sx, sy);', 'v = ME_CALLP(fetch)(ctx, sx, sy);', 1),
                   ('      blk(ctx, gp == 0x13 ?', '      ME_CALLP(blk)(ctx, gp == 0x13 ?', 1)])
edit('texcache.c', [('#include "gslayout.h"\n', '#include "gslayout.h"\n' + INC, 1)])
edit('gebatch.c', [('#include "gebatch.h"\n', '#include "gebatch.h"\n' + INC, 1),
                   ('o->cur = -1; o->zk = zk; o->cap_t = max_tex;', 'o->cur = -1; o->zk = zk; o->zkf = (float)zk; o->cap_t = max_tex;', 1),
                   ('''    r.z = (float)(z > 65535 ? 65535 : z);\n  }''',
                    '''    r.z = (float)(z > 65535 ? 65535 : z);\n  }\n#endif''', 1),
                   ('''  else {\n    double z = (d->zpsm''',
                    '''#if defined(MGS2_ME_SAFE) && defined(__psp__)   /* task #149: no doubles on the ME; the live picture keeps the raw depth (gl_init, zk 0) */
  else { me_faults.zk++; r.z = 0; }
#else
  else {\n    double z = (d->zpsm''', 1)])
edit('gebatch.h', [('  double zk;                           /* GE depth = Z24 x zk (the file\'s zscale) */\n',
                    '  double zk;                           /* GE depth = Z24 x zk (the file\'s zscale) */\n'
                    '  float zkf;                           /* task #149: (float)zk, made by gb_init (no doubles on the ME) */\n', 1)])
edit('xconv.h', [('  double zk;               /* GE depth = Z24 x zk (the replay file\'s zscale) */\n',
                  '  double zk;               /* GE depth = Z24 x zk (the replay file\'s zscale) */\n'
                  '  float zkf;               /* task #149: (float)zk (hosts built with -DMGS2_ME_SAFE set it: gl_init) */\n', 1)])
edit('xconv.c', [('#include <math.h>   /* c1opt2: fabsf (xc_c1_zin) */\n', '#include <math.h>   /* c1opt2: fabsf (xc_c1_zin) */\n' + INC, 1),
                 ('zk = (float)h->zk;', '''zk =
#ifdef MGS2_ME_SAFE
    h->zkf;
#else
    (float)h->zk;
#endif''', 1),
                 ('h->tex(h->ctx,', 'ME_CALLP(h->tex)(h->ctx,', 4)])
edit('vu1conv.c', [('#include <string.h>\n', '#include <string.h>\n' + INC, 1)])
edit('gelist.c', [('#include "vu1conv.h"\n', '#include "vu1conv.h"\n' + INC, 1),
                  ('H->addr(H->ctx,', 'ME_CALLP(H->addr)(H->ctx,', 4)])
edit('gsup2.c', [('#include "gslayout.h"\n', '#include "gslayout.h"\n' + INC, 1),
                 ('m->ram(m->ctx,', 'ME_CALLP(m->ram)(m->ctx,', 4),
                 ('if (m->trx_fn) m->trx_fn(m->trx_ctx, gr);', 'if (m->trx_fn) ME_CALLP(m->trx_fn)(m->trx_ctx, gr);', 1),
                 ('    fn(ctx, x->no, x->src, got, first, nd);', '    ME_CALLP(fn)(ctx, x->no, x->src, got, first, nd);', 1),
                 ('if (m->data_fn) m->data_fn(m->data_ctx,', 'if (m->data_fn) ME_CALLP(m->data_fn)(m->data_ctx,', 1)])
edit('gelive.h', [('  struct FxThin *ft; const uint8_t *ram; int ft_done;\n} GlFrame;',
                   '''  struct FxThin *ft; const uint8_t *ram; int ft_done;
  /* task #149 (-DMGS2_ME_SAFE): the scan runs on the host's side at the step's first transfer (fx_thin_scan reads the game's RAM, which
     the ME must not), and gl_thin hands its ranges in; ft_on: the thinning is on (the host sets it with ft) */
  int ft_on, ft_n; XcThin ft_r[16];
} GlFrame;
void gl_thin(GlFrame *f, const XcThin *r, int n);   /* task #149: this frame's thinned ranges (the host's fx_thin_scan, FX_RANGES at most) */
int gl_tex_cb(void *ctx, const uint64_t *gr, int ctxt, XcTex *t);   /* task #149: XcHost.tex as gl_init installs it (global, so that the
                                                                        ME unit's check can keep it as an entry: the ME calls it) */''', 1)])
edit('gelive.c', [('#include "fxthin.h"   /* task #114 */\n', '#include "fxthin.h"   /* task #114 */\n' + INC, 1),
                  ('static int xtex_cb(void *ctx, const uint64_t *gr, int ctxt, XcTex *t)\n',
                   'int gl_tex_cb(void *ctx, const uint64_t *gr, int ctxt, XcTex *t)   /* task #149: was static xtex_cb (gelive.h) */\n', 1),
                  ('f->h.tex = xtex_cb;', 'f->h.tex = gl_tex_cb;', 1),
                  ('f->h.zk = f->o.zk;', 'f->h.zk = f->o.zk; f->h.zkf = f->o.zkf;', 1),
                  ('float zs, zo; gx_depth_map(f->map_lo, f->map_hi, (float)f->o.zk, &zs, &zo);',
                   'float zs, zo; gx_depth_map(f->map_lo, f->map_hi, f->o.zkf, &zs, &zo);   /* (task #149: (float)zk, made at init) */', 1),
                  ('  f->ft_done = 0; f->h.nthin = 0;   /* task #114 */\n}',
                   '  f->ft_done = 0; f->h.nthin = 0;   /* task #114 */\n  f->ft_n = 0;                      /* task #149 */\n}\n'
                   'void gl_thin(GlFrame *f, const XcThin *r, int n)\n'
                   '{\n  _Static_assert(FX_RANGES == 16, "GlFrame.ft_r");\n  if (n > 16) n = 16;\n'
                   '  for (int i = 0; i < n; i++) f->ft_r[i] = r[i];\n  f->ft_n = n < 0 ? 0 : n;\n}', 1),
                  ('''  if (f->ft && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2)) {   /* task #114: the frame's thinned effects, as they are now */
    f->ft_done = 1; f->h.nthin = fx_thin_scan(f->ft, f->ram); f->h.thin = f->ft->r; }''',
                   '''#ifdef MGS2_ME_SAFE
  if (f->ft_on && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2)) {   /* task #149: the ranges the host's scan found (gl_thin) */
    f->ft_done = 1; f->h.nthin = f->ft_n; f->h.thin = f->ft_r; }
#else
  if (f->ft && !f->ft_done && (c->entry == XL_FX || c->entry == XL_FX2)) {   /* task #114: the frame's thinned effects, as they are now */
    f->ft_done = 1; f->h.nthin = fx_thin_scan(f->ft, f->ram); f->h.thin = f->ft->r; }
#endif''', 1)])
print('patched')
