/* nge_cook.c: see nge_cook.h.  Plain C99 (stdint, string.h); x86 twin and psp-gcc. */
#include <string.h>
#include "nge_cook.h"

static inline uint32_t rd32(const uint8_t *ram, uint32_t a) { uint32_t v; memcpy(&v, ram + (a & NGE_RAM_MASK & ~3u), 4); return v; }
static inline uint32_t rd16(const uint8_t *ram, uint32_t a) { uint16_t v; memcpy(&v, ram + (a & NGE_RAM_MASK & ~1u), 2); return v; }
static inline uint32_t rd8(const uint8_t *ram, uint32_t a) { return ram[a & NGE_RAM_MASK]; }
/* a guest pointer to model data: main RAM (kseg0 / kseg1 / uncached-accelerated views), not the scratchpad, n bytes inside 32 MB */
static inline int in_ram(uint32_t a, uint32_t n)
{
  uint32_t seg = a >> 28;
  if (seg != 0 && seg != 1 && seg != 2 && seg != 3) return 0;
  a &= 0x0fffffffu;
  return a >= 0x100000u && a + n <= 0x2000000u;
}

void nge_arena_init(NgeArena *a, void *mem, uint32_t cap, uint32_t ge)
{
  a->mem = (uint8_t *)mem; a->ge = ge; a->cap = cap; a->used = 16; a->peak = 16; a->last = 0;   /* offset 0 stays "none" */
}
void nge_arena_reset(NgeArena *a) { a->used = 16; a->last = 0; }
static uint32_t take(NgeArena *a, uint32_t n, uint32_t align)
{
  uint32_t o = (a->used + align - 1) & ~(align - 1);
  if (o > a->cap || n > a->cap - o) return 0;
  a->used = o + n; if (a->used > a->peak) a->peak = a->used;
  return o;
}

uint32_t nge_desc_sum(const uint8_t *ram, uint32_t desc, int ng)
{
  uint32_t s = 0x9e3779b9u;
  for (uint32_t k = 0; k < 8u * (uint32_t)ng; k++) s = ((s << 1) | (s >> 31)) + rd32(ram, desc + 4 * k);
  return s;
}

/* ge/vu1conv.c vc_strips: a run of vertices without ADC from i (>= 2) makes the strip (i - 2, j - i + 3) */
static int strips(const uint16_t *fl, int n, uint16_t *runs, int max)
{
  int ns = 0;
  for (int i = 2; i < n; ) {
    if (fl[i] & 0x8000) { i++; continue; }
    int j = i; while (j + 1 < n && !(fl[j + 1] & 0x8000)) j++;
    if (ns == max) return ns;
    runs[2 * ns] = (uint16_t)(i - 2); runs[2 * ns + 1] = (uint16_t)(j - i + 3); ns++;
    i = j + 1;
  }
  return ns;
}
static uint32_t isqrt(uint32_t x) { uint32_t r = 0, b = 1u << 30; while (b > x) b >>= 2; while (b) { if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }

typedef struct { uint32_t d0, v, n[3], s[3]; } GrpIn;   /* one group as the stub reads it (boot/nat_stub.h rt_nat_p04) */
static void group_in(const uint8_t *ram, uint32_t desc, int i, uint32_t cur[3], GrpIn *g)
{
  uint32_t dd = desc + 32u * (uint32_t)i;
  g->d0 = rd32(ram, dd); g->v = rd8(ram, dd + 12);
  for (int k = 0; k < 3; k++) {
    g->n[k] = rd8(ram, dd + 14 + (uint32_t)k);
    if (cur[k] && g->n[k]) { g->s[k] = cur[k]; cur[k] += 16u * g->n[k]; } else g->s[k] = 0;
  }
}
static uint32_t vsize(int kind) { return kind == NGE_K_PRE ? 16u : kind == NGE_K_LIT ? 20u : 24u; }
/* the group's form and whether it can be cooked: 0 = yes, else NGE_GF_* */
static int group_ok(const GrpIn *g, int kind)
{
  if (!g->s[0] || !g->s[1] || (kind == NGE_K_PRE && !g->s[2])) return NGE_GF_NOSTREAM;
  if (g->v > 2 * g->n[0] || g->v > 2 * g->n[1] || (g->s[2] && g->v > 4 * g->n[2])) return NGE_GF_BIG;
  if (!in_ram(g->s[0], 8 * g->v) || !in_ram(g->s[1], 8 * g->v) || (g->s[2] && !in_ram(g->s[2], 4 * g->v))) return NGE_GF_NOSTREAM;
  return 0;
}
/* the list's words for nst strips: VTYPE, BASE, (VADDR,) PRIM .., RET */
static uint32_t list_words(const uint16_t *runs, int nst)
{
  uint32_t w = 3, at = 0xffffffffu;
  for (int k = 0; k < nst; k++) { if (runs[2 * k] != at) w++; w++; at = (uint32_t)runs[2 * k] + runs[2 * k + 1]; }
  return w;
}

/* one group's vertices into out (vsize(kind) a vertex), its ADC flags, box; returns NGE_GF_NEGUV for a negative UV (pre-lit) or 0 */
static int group_verts(const uint8_t *ram, const GrpIn *g, int kind, uint32_t alpha_ge, uint8_t *out, uint16_t *fl, int16_t *box)
{
  int lx = 32767, ly = 32767, lz = 32767, hx = -32768, hy = -32768, hz = -32768; uint32_t neg = 0, a = (alpha_ge & 0xffu) << 24;
  for (uint32_t i = 0; i < g->v; i++) {
    const uint32_t a0 = rd32(ram, g->s[0] + 8 * i), a1 = rd32(ram, g->s[0] + 8 * i + 4);
    const uint32_t b0 = rd32(ram, g->s[1] + 8 * i), b1 = rd32(ram, g->s[1] + 8 * i + 4);
    const uint32_t c = g->s[2] ? rd32(ram, g->s[2] + 4 * i) : 0;
    const int X = (int16_t)a0, Y = (int16_t)(a0 >> 16), Z = (int16_t)a1;
    neg |= c;
    lx = X < lx ? X : lx; hx = X > hx ? X : hx; ly = Y < ly ? Y : ly; hy = Y > hy ? Y : hy; lz = Z < lz ? Z : lz; hz = Z > hz ? Z : hz;
    fl[i] = (uint16_t)(b1 >> 16);
    if (kind == NGE_K_PRE) {   /* df0_pos + vc_df0_att (kind 2): w0 = C's u | v, w1 = r g b a, w2 = A's x | y, w3 = A's z (pad 0) */
      uint32_t w[4] = { c, (b0 & 0xffu) | (b0 >> 8 & 0xff00u) | (b1 & 0xffu) << 16 | a, a0, a1 & 0xffffu };
      memcpy(out + 16 * i, w, 16);
    } else {   /* lit: (w0 w1,) u v, colour d d d a (d = |N| / 4096 x 255 [I: the normal's scale]), n, p */
      uint8_t *o = out + vsize(kind) * i; const int16_t nx = (int16_t)b0, ny = (int16_t)(b0 >> 16), nz = (int16_t)b1;
      uint32_t d = (isqrt((uint32_t)(nx * nx) + (uint32_t)(ny * ny) + (uint32_t)(nz * nz)) * 255u + 2048u) >> 12; if (d > 255) d = 255;
      const uint32_t col = d | d << 8 | d << 16 | a; const int16_t nv[6] = { nx, ny, nz, (int16_t)X, (int16_t)Y, (int16_t)Z };
      uint32_t p = 0;
      memset(o, 0, vsize(kind));
      if (kind == NGE_K_SEL) { o[0] = (a1 >> 16) ? 0 : 128; o[1] = (a1 >> 16) ? 128 : 0; p = 2; }   /* the selector's w */
      memcpy(o + p, &c, 4); p = (p + 4 + 3) & ~3u;
      memcpy(o + p, &col, 4); p += 4;
      memcpy(o + p, nv, 12);
    }
  }
  box[0] = (int16_t)lx; box[1] = (int16_t)ly; box[2] = (int16_t)lz; box[3] = (int16_t)hx; box[4] = (int16_t)hy; box[5] = (int16_t)hz;
  return kind == NGE_K_PRE && (neg & 0x80008000u) ? NGE_GF_NEGUV : 0;
}

int nge_cook_mesh(NgeArena *a, const uint8_t *ram, uint32_t mesh, uint32_t alpha_ge, uint32_t *off)
{
  const int ng = (int16_t)rd16(ram, mesh + 0xcc);
  const uint32_t mfl = rd32(ram, mesh + 0xc0), desc = rd32(ram, mesh + 0xf4), pre = mfl & 1;
  uint32_t cur[3] = { rd32(ram, mesh + 0xdc), rd32(ram, mesh + 0xe0), rd32(ram, mesh + 0xe4) };
  if (ng <= 0 || ng > 1024 || !in_ram(desc, 32u * (uint32_t)ng)) return NGE_E_BAD;
  if (rd32(ram, mesh + 0xd0) & 0xfu) return NGE_E_OVR;
  const uint32_t u0 = a->used;
  const uint32_t mo = take(a, sizeof(NgeMesh), 16), go = mo ? take(a, (uint32_t)ng * sizeof(NgeGroup), 4) : 0;
  if (!go) { a->used = u0; return NGE_E_ROOM; }
  NgeMesh *m = nge_mesh_at(a, mo);
  m->mesh = mesh; m->desc = desc; m->sa = cur[0]; m->sb = cur[1]; m->sc = cur[2]; m->dsum = nge_desc_sum(ram, desc, ng);
  m->grp = go; m->ng = (uint16_t)ng; m->pre = (uint16_t)pre; m->alpha = alpha_ge & 0xffu; m->live = 1;
  for (int i = 0; i < ng; i++) {
    GrpIn g; uint16_t fl[256], runs[2 * 128]; NgeGroup *q = (NgeGroup *)(void *)(a->mem + go) + i;
    group_in(ram, desc, i, cur, &g);
    memset(q, 0, sizeof *q);
    q->kind = (uint8_t)(pre ? NGE_K_PRE : (g.d0 & 1) ? NGE_K_SEL : NGE_K_LIT); q->nv = (uint16_t)g.v;
    int f = group_ok(&g, q->kind);
    if (f) { q->flags = (uint16_t)f; continue; }
    const uint32_t vs = vsize(q->kind), vo = take(a, vs * g.v, 16);
    if (g.v && !vo) { a->used = u0; return NGE_E_ROOM; }
    q->vb = vo;
    q->flags = (uint16_t)group_verts(ram, &g, q->kind, alpha_ge, a->mem + vo, fl, q->box);
    const int nst = strips(fl, (int)g.v, runs, 128);
    q->nst = (uint8_t)nst;
    if (!nst) q->flags |= NGE_GF_NOSTRIP;
    const uint32_t nw = list_words(runs, nst), lo = take(a, 4 * nw, 4);
    if (!lo) { a->used = u0; return NGE_E_ROOM; }
    uint32_t *w = (uint32_t *)(void *)(a->mem + lo), n = 0, at = 0xffffffffu, ad = a->ge + vo;
    w[n++] = NGE_W(NGE_C_VTYPE, q->kind == NGE_K_PRE ? NGE_VT_PRE : q->kind == NGE_K_LIT ? NGE_VT_LIT : NGE_VT_SEL);
    w[n++] = NGE_W(NGE_C_BASE, (ad >> 8) & 0xf0000u);
    for (int k = 0; k < nst; k++) {
      if (runs[2 * k] != at) w[n++] = NGE_W(NGE_C_VADDR, ad + vs * runs[2 * k]);
      w[n++] = NGE_W(NGE_C_PRIM, NGE_PRIM_STRIP << 16 | runs[2 * k + 1]);
      at = (uint32_t)runs[2 * k] + runs[2 * k + 1];
    }
    w[n++] = NGE_W(NGE_C_RET, 0);
    q->list = lo; q->nw = (uint16_t)n;
  }
  m->bytes = a->used - mo; m->next = a->last; a->last = mo;
  *off = mo;
  return NGE_OK;
}

int nge_mesh_same(const NgeMesh *m, const uint8_t *ram)
{
  const uint32_t mesh = m->mesh;
  if (rd32(ram, mesh + 0xf4) != m->desc || rd32(ram, mesh + 0xdc) != m->sa || rd32(ram, mesh + 0xe0) != m->sb || rd32(ram, mesh + 0xe4) != m->sc) return 0;
  if ((int16_t)rd16(ram, mesh + 0xcc) != (int)m->ng || (rd32(ram, mesh + 0xc0) & 1) != m->pre) return 0;
  return nge_desc_sum(ram, m->desc, m->ng) == m->dsum;
}

void nge_relocate(NgeArena *a, uint32_t new_ge)
{
  for (uint32_t mo = a->last; mo; ) {
    NgeMesh *m = nge_mesh_at(a, mo); NgeGroup *g = nge_groups(a, m);
    for (int i = 0; i < m->ng; i++) {
      if (!g[i].list) continue;
      uint32_t *w = (uint32_t *)(void *)(a->mem + g[i].list), base = 0;
      for (uint32_t k = 0; k < g[i].nw; k++) {
        const uint32_t c = w[k] >> 24;
        if (c == NGE_C_BASE) { base = (w[k] & 0xf0000u) << 8; w[k] = NGE_W(NGE_C_BASE, ((new_ge + g[i].vb) >> 8) & 0xf0000u); }
        else if (c == NGE_C_VADDR) { const uint32_t old = base | (w[k] & 0xffffffu); w[k] = NGE_W(NGE_C_VADDR, old - a->ge + new_ge); }
      }
    }
    mo = m->next;
  }
  a->ge = new_ge;
}

uint32_t nge_mesh_bytes(const uint8_t *ram, uint32_t mesh, uint32_t *nv, uint32_t *ngr, uint32_t *nlit)
{
  const int ng = (int16_t)rd16(ram, mesh + 0xcc);
  const uint32_t desc = rd32(ram, mesh + 0xf4), pre = rd32(ram, mesh + 0xc0) & 1;
  uint32_t cur[3] = { rd32(ram, mesh + 0xdc), rd32(ram, mesh + 0xe0), rd32(ram, mesh + 0xe4) }, b, v = 0, lit = 0;
  if (ng <= 0 || ng > 1024 || !in_ram(desc, 32u * (uint32_t)ng)) return 0;
  b = (uint32_t)((sizeof(NgeMesh) + 15) & ~15u) + (uint32_t)ng * (uint32_t)sizeof(NgeGroup);
  for (int i = 0; i < ng; i++) {
    GrpIn g; uint16_t fl[256], runs[256]; int16_t box[6];
    group_in(ram, desc, i, cur, &g);
    const int kind = pre ? NGE_K_PRE : (g.d0 & 1) ? NGE_K_SEL : NGE_K_LIT;
    if (group_ok(&g, kind)) continue;
    const uint32_t vs = vsize(kind);
    b = ((b + 15) & ~15u) + vs * g.v; v += g.v; if (kind != NGE_K_PRE) lit += g.v;
    for (uint32_t k = 0; k < g.v; k++) fl[k] = (uint16_t)(rd32(ram, g.s[1] + 8 * k + 4) >> 16);
    (void)box;
    b = ((b + 3) & ~3u) + 4 * list_words(runs, strips(fl, (int)g.v, runs, 128));
  }
  if (nv) *nv = v;
  if (ngr) *ngr = (uint32_t)ng;
  if (nlit) *nlit = lit;
  return b;
}
