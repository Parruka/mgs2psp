/* gebatch.c: see gebatch.h.  The rules are tools/gsref.c's export as it was before it moved here (2026-10-23). */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "gebatch.h"
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */

/* room for one more element in *p (capacity *cap, elements of size sz), doubling from first; 0, or -1 with *p unchanged */
static int grow(void **p, int n, int *cap, size_t sz, int first, int fixed)
{
  if (n < *cap) return 0;
  if (fixed) return -1;
  int c = *cap ? *cap * 2 : first; void *q = realloc(*p, (size_t)c * sz);
  if (!q) return -1;
  *p = q; *cap = c; return 0;
}
int gb_init(GbOut *o, double zk, int max_tex)
{
  memset(o, 0, sizeof *o); o->cur = -1; o->zk = zk; o->zkf = (float)zk; o->cap_t = max_tex;
  o->t = calloc((size_t)max_tex, sizeof *o->t); o->u0 = calloc((size_t)max_tex, sizeof *o->u0); o->v0 = calloc((size_t)max_tex, sizeof *o->v0);
  return o->t && o->u0 && o->v0 ? 0 : -1;
}
int gb_arena(GbOut *o, void *mem, uint32_t size, uint32_t nb, uint32_t nv)
{
  uint32_t vb = ((uint32_t)(nb * sizeof(GerBatch)) + 63) & ~63u, db = (vb + (uint32_t)(nv * sizeof(GerVert)) + 63) & ~63u;
  if (db >= size) return -1;
  o->b = mem; o->cap_b = (int)nb; o->v = (GerVert *)((uint8_t *)mem + vb); o->cap_v = (int)nv; o->d = (uint8_t *)mem + db; o->cap_d = size - db;
  o->fixed = 1; gb_reset(o); return 0;
}
void gb_reset(GbOut *o) { o->nb = o->nv = o->nt = 0; o->nd = 0; o->cur = -1; }
/* n more bytes at the next 64-byte boundary of the data block: their offset (the padding zeroed), or GB_NONE */
static uint32_t data_at(GbOut *o, uint32_t n)
{
  while (o->nd + n + 64 > o->cap_d) {
    if (o->fixed) { o->dropped++; return GB_NONE; }
    uint32_t c = o->cap_d ? o->cap_d * 2 : 1u << 20; uint8_t *q = realloc(o->d, c);
    if (!q) { o->dropped++; return GB_NONE; }
    o->d = q; o->cap_d = c;
  }
  uint32_t a = (o->nd + 63) & ~63u; memset(o->d + o->nd, 0, a - o->nd); return a;
}
uint32_t gb_put(GbOut *o, const void *p, uint32_t n)
{
  uint32_t a = data_at(o, n); if (a == GB_NONE) return a;
  memcpy(o->d + a, p, n); o->nd = a + n; return a;
}
uint32_t gb_left(const GbOut *o)
{
  if (!o->fixed) return 0xffffffffu;
  uint32_t a = (o->nd + 63) & ~63u; return a + 64 < o->cap_d ? o->cap_d - a - 64 : 0;
}
uint8_t *gb_room(GbOut *o, uint32_t cap)
{
  uint32_t a = data_at(o, cap); if (a == GB_NONE) return 0;
  o->room = a; o->room_cap = cap; return o->d + a;
}
int gb_tex(GbOut *o, const GerTex *t, int u0, int v0)
{
  if (o->nt >= o->cap_t) { o->dropped++; return -1; }
  o->t[o->nt] = *t; o->u0[o->nt] = u0; o->v0[o->nt] = v0; return o->nt++;
}
int gb_state(const GfDraw *d, int ti, uint32_t entry, GerBatch *b)
{
  static const uint8_t GEP[8] = { 0, 1, 2, 3, 4, 5, 6, 3 };   /* GU_POINTS LINES LINE_STRIP TRIANGLES TRIANGLE_STRIP TRIANGLE_FAN SPRITES */
  int u = 0;
  memset(b, 0, sizeof *b);
  b->tex = ti >= 0 ? (uint16_t)ti : 0xffff; b->prim = GEP[d->prim & 7];
  b->flags = (d->iip ? GERF_GOURAUD : 0) | (ti >= 0 ? GERF_TEX : 0) | (d->abe ? GERF_BLEND : 0) | (d->ate ? GERF_ATEST : 0) | (d->zte ? GERF_ZTEST : 0) |
             (d->zte && !d->zmsk ? GERF_ZWRITE : 0) | (d->fge ? GERF_FOG : 0) | (d->tcc ? GERF_TCC : 0);
  b->ztst = d->ztst; b->atst = d->atst; b->aref = d->aref; b->afail = d->afail;
  /* alpha tests that need no GE alpha test: ALWAYS passes; NEVER with AFAIL FB_ONLY is the PS2's way to draw without writing Z */
  if (d->ate && d->atst == 1) b->flags &= ~GERF_ATEST;
  if (d->ate && d->atst == 0 && d->afail == 1) b->flags &= ~(GERF_ATEST | GERF_ZWRITE);
  if (d->ate && d->atst == 0 && d->afail != 1) u |= GB_U_ANEVER;
  b->ba = d->ba; b->bb = d->bb; b->bc = d->bc; b->bd = d->bd; b->fix = d->fix; b->tfx = d->tfx;
  b->sx0 = (int16_t)(d->scx0 * 480 / 512); b->sy0 = (int16_t)(d->scy0 * 272 / 512); b->sx1 = (int16_t)((d->scx1 + 1) * 480 / 512 - 1); b->sy1 = (int16_t)((d->scy1 + 1) * 272 / 512 - 1);
  b->entry = entry;
  if (d->fbmsk) u |= GB_U_FBMSK;
  if (d->date || d->fba || d->pabe) u |= GB_U_DFP;
  if (d->ate && d->afail > 1) u |= GB_U_AFAIL;
  return u;
}
GerVert gb_vert(const GbOut *o, const GfDraw *d, const GfVtx *v, int ti)
{
  GerVert r; int a = v->a >= 0x80 ? 0xff : v->a * 2;
  r.c = (uint32_t)v->r | v->g << 8 | v->b << 16 | (uint32_t)a << 24;
  r.x = (float)(v->x - d->ofx) * 0.05859375f; r.y = (float)(v->y - d->ofy) * 0.033203125f;   /* / 16 x 480 / 512 and / 16 x 272 / 512, exact */
  if (o->zraw) r.z = (float)(d->zpsm == 0x32 || d->zpsm == 0x3a || v->z < 0xffffff ? v->z : 0xffffff);
#if defined(MGS2_ME_SAFE) && defined(__psp__)   /* task #149: no doubles on the ME; the live picture keeps the raw depth (gl_init, zk 0) */
  else { me_faults.zk++; r.z = 0; }
#else
  else {
    double z = (d->zpsm == 0x32 || d->zpsm == 0x3a) ? v->z : (v->z > 0xffffff ? 0xffffff : v->z) * o->zk;   /* Z16 / Z16S as they are; the GS clamps Z to the buffer's range; GE depth = Z24 x zk */
    r.z = (float)(z > 65535 ? 65535 : z);
  }
#endif
  float u = 0, w = 0;
  if (d->tme) {
    if (d->fst) { u = v->u / 16.0f; w = v->v / 16.0f; } else { float iq = v->q ? 1.0f / v->q : 0; u = v->s * iq * (float)(1 << d->tw); w = v->t * iq * (float)(1 << d->th); }
    if (ti >= 0 && o->t[ti].kind == 2) { u *= 480.0f / 512.0f; w *= 272.0f / 512.0f; }
    else if (ti >= 0) { u -= o->u0[ti]; w -= o->v0[ti]; }
  }
  r.u = u; r.v = w; return r;
}
int gb_prim(GbOut *o, const GerBatch *bs, const GfDraw *d, const GfVtx *const *vv, int n)
{
  GerBatch b = *bs; int ps2 = d->prim, ti = b.tex == 0xffff ? -1 : b.tex, minva = 255;
  GerVert gv[3]; for (int i = 0; i < n; i++) { gv[i] = gb_vert(o, d, vv[i], ti); if (vv[i]->a < minva) minva = vv[i]->a; }
  GerBatch *c = o->cur >= 0 ? &o->b[o->cur] : 0;
  if (c) b.twopass = c->twopass;   /* set after the batch starts: not part of the state compared */
  int same = c && !memcmp(c, &b, offsetof(GerBatch, first)), app = 0;
  if (same) {
    if (ps2 == 4 && c->prim == 4) app = !memcmp(&gv[0], &o->last[0], sizeof gv[0]) && !memcmp(&gv[1], &o->last[1], sizeof gv[0]) ? 2 : 0;
    else if (ps2 == 5 && c->prim == 5) app = !memcmp(&gv[0], &o->v[c->first], sizeof gv[0]) && !memcmp(&gv[1], &o->last[1], sizeof gv[0]) ? 2 : 0;
    else if (ps2 == 2 && c->prim == 2) app = !memcmp(&gv[0], &o->last[1], sizeof gv[0]) ? 1 : 0;
    else if (ps2 == 4 || ps2 == 5 || ps2 == 2) app = 0;
    else app = -1;   /* lists: append all */
  }
  int from = app > 0 ? app : 0;
  if (o->nv + n - from > o->cap_v) {   /* room for the vertices first, so a primitive that does not fit leaves no batch behind */
    if (o->fixed) { o->dropped++; return -1; }
    int c2 = o->cap_v ? o->cap_v : 65536; while (o->nv + n - from > c2) c2 *= 2;
    GerVert *q = realloc(o->v, (size_t)c2 * sizeof *q); if (!q) { o->dropped++; return -1; }
    o->v = q; o->cap_v = c2;
  }
  if (!app) {
    if (grow((void **)&o->b, o->nb, &o->cap_b, sizeof *o->b, 4096, o->fixed)) { o->dropped++; return -1; }
    b.first = o->nv; b.twopass = 0; o->b[o->nb] = b; o->cur = o->nb++;
  }
  for (int i = from; i < n; i++) { o->v[o->nv++] = gv[i]; o->b[o->cur].count++; }
  o->last[0] = n >= 2 ? gv[n - 2] : gv[0]; o->last[1] = gv[n - 1];
  /* two passes for AFAIL FB_ONLY with Z write when a texel or a vertex can be below AREF (I: alpha = vertex x texel >> 7) */
  if (d->ate && d->afail == 1 && d->zte && !d->zmsk && d->atst >= 2) {
    int ta = ti >= 0 && o->t[ti].kind == 1 && d->tcc ? o->t[ti].minalpha : 0x80;
    if ((minva * ta >> 7) < d->aref) o->b[o->cur].twopass = 1;
  }
  return 0;
}
int gb_xform(GbOut *o, const GerBatch *b, const void *blob, uint32_t size)
{
  if (grow((void **)&o->b, o->nb, &o->cap_b, sizeof *o->b, 4096, o->fixed)) { o->dropped++; return -1; }
  uint32_t a = gb_put(o, blob, size); if (a == GB_NONE) return -1;
  o->b[o->nb] = *b; o->b[o->nb++].first = a + b->first;
  if (o->live) o->cur = -1;
  return 0;
}
int gb_xform_in(GbOut *o, const GerBatch *b, uint32_t size)
{
  if (size > o->room_cap || grow((void **)&o->b, o->nb, &o->cap_b, sizeof *o->b, 4096, o->fixed)) { o->dropped++; return -1; }
  o->nd = o->room + size; o->room_cap = 0;
  o->b[o->nb] = *b; o->b[o->nb++].first = o->room + b->first;
  if (o->live) o->cur = -1;
  return 0;
}
int gb_xform_at(GbOut *o, const GerBatch *b, uint32_t base)
{
  if (base + b->first >= o->nd || grow((void **)&o->b, o->nb, &o->cap_b, sizeof *o->b, 4096, o->fixed)) { o->dropped++; return -1; }
  o->b[o->nb] = *b; o->b[o->nb++].first = base + b->first;
  if (o->live) o->cur = -1;
  return 0;
}
int gb_slot(GbOut *o)
{
  if (grow((void **)&o->b, o->nb, &o->cap_b, sizeof *o->b, 4096, o->fixed)) { o->dropped++; return -1; }
  GerBatch *b = &o->b[o->nb]; memset(b, 0, sizeof *b); b->kind = GERK_THROUGH; b->prim = 4; b->tex = 0xffff; b->first = (uint32_t)o->nv;   /* (no
                                                                    vertices: what the slot draws until gb_fill_in fills it) */
  if (o->live) o->cur = -1;
  return o->nb++;
}
int gb_fill_in(GbOut *o, int i, const GerBatch *b, uint32_t size)
{
  if (i < 0 || i >= o->nb || size > o->room_cap) { o->dropped++; return -1; }
  o->nd = o->room + size; o->room_cap = 0;
  o->b[i] = *b; o->b[i].first = o->room + b->first;
  return 0;
}
int gb_fill_at(GbOut *o, int i, const GerBatch *b, uint32_t base)
{
  if (i < 0 || i >= o->nb || base + b->first >= o->nd) { o->dropped++; return -1; }
  o->b[i] = *b; o->b[i].first = base + b->first;
  return 0;
}
int gb_write(const GbOut *o, FILE *f, uint32_t vb, uint32_t w, uint32_t h)
{
  GerHeader hd = { GER_MAGIC, vb, (uint32_t)o->nb, (uint32_t)o->nv, (uint32_t)o->nt, o->nd, 0, 0, 0, 0, w, h, (float)o->zk };
  hd.off_batch = 64; hd.off_vert = hd.off_batch + o->nb * sizeof(GerBatch); hd.off_vert = (hd.off_vert + 63) & ~63u;
  hd.off_tex = hd.off_vert + o->nv * sizeof(GerVert); hd.off_tex = (hd.off_tex + 63) & ~63u;
  hd.off_data = hd.off_tex + o->nt * sizeof(GerTex); hd.off_data = (hd.off_data + 63) & ~63u;
  static const uint8_t zero[64];
  fwrite(&hd, sizeof hd, 1, f); fwrite(zero, 64 - sizeof hd, 1, f);
  fwrite(o->b, sizeof(GerBatch), o->nb, f); fwrite(zero, hd.off_vert - hd.off_batch - o->nb * sizeof(GerBatch), 1, f);
  fwrite(o->v, sizeof(GerVert), o->nv, f); fwrite(zero, hd.off_tex - hd.off_vert - o->nv * sizeof(GerVert), 1, f);
  fwrite(o->t, sizeof(GerTex), o->nt, f); fwrite(zero, hd.off_data - hd.off_tex - o->nt * sizeof(GerTex), 1, f);
  fwrite(o->d, 1, o->nd, f);
  return ferror(f) ? -1 : 0;
}
