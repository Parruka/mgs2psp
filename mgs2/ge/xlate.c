/* xlate.c: the Stage A packet translator (xlate.h, renderer_notes 11.2).  VIF1 and GIF decoding follow tools/gsref.c (whose replay of
   the deck capture draws the expected frame); what gsref gets from running the VU1 programs, the translator gets from the uploaded data:
   the A+D packet a call kicks first and the PRIM of the tag template its geometry starts with (the rules below).  Two fronts feed it:
   xl_vif1 a word at a time (tools/gsref.c, the reference) and xl_vif1_span a span at a time, which decodes an UNPACK's data only when
   something reads it (renderer_notes 11.12 step 1). */
#include <string.h>
#include "xlate.h"
#include "gsfront.h"

static XlStats St;
#ifdef XL_CENSUS
#define XC(x) x
#else
#define XC(x)
#endif
static XlCallFn call_fn; static void *call_ctx;
static GfGs F; static GfPrimFn prim_fn; static void *prim_ctx; static struct GuMap *up_map; static XlPassFn pass_fn; static void *pass_ctx;   /* the GS front end (ge/gsfront.h): every GIF path's register writes and
                                                                   vertices, as the GS takes them; a VU1 call's own kicks are not seen */

/* ---- GS registers (A+D numbering) and the GIF ---- */
static uint64_t gr[256];
static void gs_write(int r, uint64_t v)
{
  r &= 0xff;
  if (r == 0x16 || r == 0x17) { const uint64_t M = 0xFFFFFFE003F00000ull; gr[r - 0x10] = (gr[r - 0x10] & ~M) | (v & M); }   /* TEX2: TEX0's CLUT fields */
  gr[r] = v;
}
typedef struct { int nloop, flg, nreg, ri, eop; uint64_t regs; } Gif;
static Gif G[4];
/* one quadword of a GIF path (src: its EE address, for IMAGE data); returns 1 when it ends a packet (EOP) */
static int gif_qw(int path, const uint32_t *q, uint32_t src)
{
  Gif *g = &G[path];
  uint64_t lo = q[0] | (uint64_t)q[1] << 32, hi = q[2] | (uint64_t)q[3] << 32;
  F.path = path; XC(St.gq[path]++);
  if (!g->nloop) {
    g->nloop = lo & 0x7fff; g->eop = lo >> 15 & 1; g->flg = lo >> 58 & 3; g->nreg = (lo >> 60 & 15) ? (int)(lo >> 60 & 15) : 16; g->regs = hi; g->ri = 0;
    if ((lo >> 46 & 1) && g->flg == 0) { gs_write(0x00, lo >> 47 & 0x7ff); gf_reg(&F, 0x00, lo >> 47 & 0x7ff); }
    St.gif_tags[path]++;
    if (g->flg == 1) g->nloop *= g->nreg;   /* REGLIST: 64-bit words */
    return !g->nloop && g->eop;
  }
  if (g->flg >= 2) { St.gif_image_qw++; XC(St.img_qw[path]++); gf_image(&F, src, 16); g->nloop--; return !g->nloop && g->eop; }
  if (g->flg == 1) {
    for (int h = 0; h < 2 && g->nloop; h++) { int r = g->regs >> (4 * g->ri) & 15; if (++g->ri == g->nreg) g->ri = 0; g->nloop--;
      if (r < 0xe) { gs_write(r, h ? hi : lo); gf_reg(&F, r, h ? hi : lo); } }
    return !g->nloop && g->eop;
  }
  int r = g->regs >> (4 * g->ri) & 15;
  gf_packed(&F, r, q);
  if (r == 0xe) gs_write((int)(hi & 0xff), lo);
  else if (r == 0x0) gs_write(0x00, lo & 0x7ff);
  else if (r != 0xf) gs_write(r, lo);   /* vertex registers keep their raw packed low word: the translator draws nothing from them yet */
  if (++g->ri == g->nreg) { g->ri = 0; g->nloop--; }
  return !g->nloop && g->eop;
}
/* m IMAGE quadwords of a path, the first from EE address src: what m calls of gif_qw do with them (the span fronts; G[path] must be in an
   IMAGE packet with at least m quadwords left) */
static void image_run(int path, uint32_t src, uint32_t m)
{
  F.path = path; XC(St.gq[path] += m); St.gif_image_qw += m; XC(St.img_qw[path] += m);
  gf_image_run(&F, src, 16 * m); G[path].nloop -= (int)m;
}

/* ---- VU1 data memory and VIF1 ---- */
static uint8_t VM[16384] __attribute__((aligned(16)));
static uint32_t VPsrc[2048];   /* guest address each micro instruction was uploaded from, as gsref names entries */
static struct { int cl, wl, mode; uint32_t mask, row[4], col[4], base, ofst, tops, itops, dbf; } V;
static uint32_t vcmd, vsrc0, ub[1024], dq[4], dqsrc; static int vleft, vn, dqn;

/* UNPACKs decoded when read (xl_vif1_span): a descriptor for each UNPACK whose data a span held whole, with where the data lies (guest RAM,
   unchanged until the transfer ends), the command, STCYCL, STMOD, STMASK, and ROW / COL as copied into RC (a copy each time they change,
   shared by the descriptors made until the next change); own[qa]: the number + 1 of the descriptor that wrote VU quadword qa last (0: VM
   holds it).  A quadword's value is its descriptor's vector for it, so quadwords can be decoded in any order */
typedef struct { const uint32_t *p; uint16_t addr, num, nw, rc; uint8_t fmt, md, cl, wl; uint32_t mask; } Ud;
#define UD_N 4096
#define RC_N 1024
static Ud UD[UD_N]; static uint32_t RC[RC_N][8]; static uint16_t own[1024] __attribute__((aligned(16))); static int ud_n, rc_n, rc_dirty = 1;
/* vector k of descriptor d into q, as unpack() writes it: any kind, bit by bit where needed (ud_vec takes the usual kinds first) */
static void ud_any(const Ud *d, uint32_t k, uint32_t *q)
{
  int c = d->fmt, vn_ = (c >> 2) & 3, vl = c & 3, m = (c >> 4) & 1, bits = 32 >> vl, ne = vn_ + 1, usn = d->md & 1, md = d->md >> 1;
  if (c == 0x0c && !md) { memcpy(q, d->p + 4 * k, 16); return; }   /* V4-32 without mask or ROW: the data as it is */
  uint32_t e[4];
  if ((c & 0xf) == 0x0d) {   /* V4-16 (whole vectors, so no read past the data): the four halfwords of vector k */
    if (usn) { const uint16_t *h = (const uint16_t *)d->p + 4 * k; e[0] = h[0]; e[1] = h[1]; e[2] = h[2]; e[3] = h[3]; }
    else { const int16_t *h = (const int16_t *)d->p + 4 * k; e[0] = (uint32_t)(int32_t)h[0]; e[1] = (uint32_t)(int32_t)h[1]; e[2] = (uint32_t)(int32_t)h[2]; e[3] = (uint32_t)(int32_t)h[3]; }
  } else if ((c & 0xf) == 0x05) {   /* V2-16: x, y, x, y */
    if (usn) { const uint16_t *h = (const uint16_t *)d->p + 2 * k; e[0] = e[2] = h[0]; e[1] = e[3] = h[1]; }
    else { const int16_t *h = (const int16_t *)d->p + 2 * k; e[0] = e[2] = (uint32_t)(int32_t)h[0]; e[1] = e[3] = (uint32_t)(int32_t)h[1]; }
  } else if ((c & 0xf) == 0x0c) { const uint32_t *w = d->p + 4 * k; e[0] = w[0]; e[1] = w[1]; e[2] = w[2]; e[3] = w[3]; }   /* V4-32 */
  else {   /* the rest bit by bit (V3's fourth lane reads on into the next vector, and past the data as 0) */
    if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; usn = 1; }
    uint64_t bitpos = (uint64_t)k * (uint32_t)(bits * ne);
    for (int j = 0; j < 4; j++) {
      int jj = vn_ == 0 ? 0 : vn_ == 1 ? (j & 1) : j;
      uint64_t bp = bitpos + (uint64_t)jj * bits; uint32_t wi = bp >> 5, sh = bp & 31, v;
      v = wi < d->nw ? d->p[wi] >> sh : 0;
      if (bits < 32) { v &= (1u << bits) - 1; if (!usn && (v >> (bits - 1) & 1)) v |= ~0u << bits; }
      e[j] = v;
    }
    if (vn_ == 3 && vl == 3) { uint32_t s = e[0]; e[0] = (s & 0x1f) << 3; e[1] = (s >> 5 & 0x1f) << 3; e[2] = (s >> 10 & 0x1f) << 3; e[3] = (s >> 15 & 1) << 7; }
  }
  if (!m) {   /* no mask: the data, plus ROW in MODE 1 (a waiting UNPACK has MODE 0 or 1) */
    if (md == 1) { const uint32_t *rc = RC[d->rc]; q[0] = e[0] + rc[0]; q[1] = e[1] + rc[1]; q[2] = e[2] + rc[2]; q[3] = e[3] + rc[3]; }
    else { q[0] = e[0]; q[1] = e[1]; q[2] = e[2]; q[3] = e[3]; }
    return;
  }
  int cyc = d->wl == 1 ? 0 : (int)(k % d->wl), row = cyc < 3 ? cyc : 3; const uint32_t *rc = RC[d->rc];
  for (int j = 0; j < 4; j++) {
    int mk = m ? (d->mask >> (row * 8 + j * 2)) & 3 : 0;   /* never 3: such an UNPACK does not wait (ud_ok) */
    q[j] = mk == 0 ? (md == 1 ? e[j] + rc[j] : e[j]) : mk == 1 ? rc[j] : rc[4 + row];
  }
}
/* the kinds the game sends (renderer_notes 11.10, the UNPACK census), taken straight from the data: 1 V4-32 in MODE 0 without a mask (a
   copy), 2 V4-16 signed in MODE 0 without a mask, 3 the same in MODE 1 (plus ROW), 4 V2-16 signed in MODE 0 in WL 1 with the mask row 0x10
   (x, y, ROW z, y), 5 V4-8 unsigned in MODE 0 without a mask (one a P07 call, codec_speed.md step 2; xl_ud_v48); 0 any other (ud_any).
   The same vectors as ud_any's, lane for lane */
static int ud_v48 = 1;   /* xl_ud_v48 */
typedef int16_t __attribute__((may_alias)) ud_h16;
typedef uint32_t __attribute__((may_alias)) ud_w32;
static int ud_kind(const Ud *d)
{
  uint32_t c = d->fmt, md = d->md;   /* md: unsigned | MODE << 1 */
  if (c == 0x0c) return md < 2;
  if (c == 0x0d) return md == 0 ? 2 : md == 2 ? 3 : 0;
  if (c == 0x0e) return md == 1 && ud_v48 ? 5 : 0;
  return c == 0x15 && !md && d->wl == 1 && (d->mask & 0xff) == 0x10 ? 4 : 0;
}
static inline __attribute__((always_inline)) void ud_dec(const Ud *d, uint32_t k, uint8_t *o, const int K)
{
  ud_w32 *q = (ud_w32 *)o;
  if (K == 1) { const ud_w32 *s = (const ud_w32 *)d->p + 4 * k; uint32_t a = s[0], b = s[1], c = s[2], e = s[3]; q[0] = a; q[1] = b; q[2] = c; q[3] = e; }
  else if (K == 2 || K == 3) {
    const ud_h16 *h = (const ud_h16 *)d->p + 4 * k; int32_t a = h[0], b = h[1], c = h[2], e = h[3];
    if (K == 3) { const uint32_t *rc = RC[d->rc]; q[0] = (uint32_t)a + rc[0]; q[1] = (uint32_t)b + rc[1]; q[2] = (uint32_t)c + rc[2]; q[3] = (uint32_t)e + rc[3]; }
    else { q[0] = (uint32_t)a; q[1] = (uint32_t)b; q[2] = (uint32_t)c; q[3] = (uint32_t)e; }
  } else if (K == 4) { const ud_h16 *h = (const ud_h16 *)d->p + 2 * k; int32_t x = h[0], y = h[1]; q[0] = (uint32_t)x; q[1] = (uint32_t)y; q[2] = RC[d->rc][2]; q[3] = (uint32_t)y; }
  else if (K == 5) { uint32_t w = k < d->nw ? ((const ud_w32 *)d->p)[k] : 0; q[0] = w & 0xff; q[1] = w >> 8 & 0xff; q[2] = w >> 16 & 0xff; q[3] = w >> 24; }
  else ud_any(d, k, (uint32_t *)o);
}
static void ud_vec(const Ud *d, uint32_t k, uint32_t *q)
{
  uint8_t *o = (uint8_t *)q;
  switch (ud_kind(d)) { case 1: ud_dec(d, k, o, 1); break; case 2: ud_dec(d, k, o, 2); break; case 3: ud_dec(d, k, o, 3); break;
                        case 4: ud_dec(d, k, o, 4); break; case 5: ud_dec(d, k, o, 5); break; default: ud_any(d, k, q); }
}
/* the vector of descriptor d that VU quadword qa holds */
static const uint16_t RCP[17] = { 0, 0, 32769, 21846, 16385, 13108, 10923, 9363, 8193, 7282, 6554, 5958, 5462, 5042, 4682, 4370, 4097 };
/* (o * RCP[cl]) >> 16 = o / cl for o < 1024 and 2 <= cl <= 16 (RCP[cl] = 65536 / cl + 1; checked for every o and cl) */
static uint32_t ud_k(const Ud *d, uint32_t qa)
{
  uint32_t o = (qa - d->addr) & 1023, cl = d->cl;
  if (cl == d->wl) return o;
  uint32_t b = cl <= 16 ? (o * RCP[cl]) >> 16 : o / cl;
  return b * d->wl + (o - b * cl);
}
static void vm_one(uint32_t qa) { const Ud *d = &UD[own[qa] - 1]; ud_vec(d, ud_k(d, qa), (uint32_t *)(VM + (qa << 4))); own[qa] = 0; }
static const uint32_t *vmq(uint32_t qa)   /* VU quadword qa as the program would read it */
{
  qa &= 1023;
  if (own[qa]) { vm_one(qa); XC(St.dec_read++); }
  return (const uint32_t *)(VM + (qa << 4));
}
static int vm_by_ud = 1;   /* xl_vm_mode */
/* vm_all's loop for one descriptor of kind K (ud_kind): the quadwords it still owns, with the address stepping of its CL / WL */
static inline __attribute__((always_inline)) void ud_run(const Ud *d, uint16_t id, const int K)
{
  uint32_t a = d->addr, num = d->num, wl = d->wl, cl = d->cl;
  if (wl == cl)
    for (uint32_t k = 0; k < num; k++) { uint32_t q = (a + k) & 1023; if (own[q] == id) { ud_dec(d, k, VM + (q << 4), K); own[q] = 0; XC(St.dec_flush++); } }
  else if (wl == 1)
    for (uint32_t k = 0; k < num; k++, a += cl) { uint32_t q = a & 1023; if (own[q] == id) { ud_dec(d, k, VM + (q << 4), K); own[q] = 0; XC(St.dec_flush++); } }
  else
    for (uint32_t k = 0, cyc = 0, skip = cl - wl; k < num; k++) {
      uint32_t q = a & 1023;
      if (own[q] == id) { ud_dec(d, k, VM + (q << 4), K); own[q] = 0; XC(St.dec_flush++); }
      a++; if (++cyc == wl) { cyc = 0; a += skip; }
    }
}
static void vm_all(void)   /* decode every pending quadword; the tables start again */
{
  if (!ud_n) return;
  XC(St.flushes++);
  if (vm_by_ud)   /* each descriptor's quadwords that it still owns, vector by vector (as many steps as vectors were unpacked) */
    for (int i = 0; i < ud_n; i++) {
      const Ud *d = &UD[i]; uint16_t id = (uint16_t)(i + 1);
      switch (ud_kind(d)) { case 1: ud_run(d, id, 1); break; case 2: ud_run(d, id, 2); break; case 3: ud_run(d, id, 3); break;
                            case 4: ud_run(d, id, 4); break; case 5: ud_run(d, id, 5); break; default: ud_run(d, id, 0); }
    }
  else for (uint32_t qa = 0; qa < 1024; qa++) if (own[qa]) { vm_one(qa); XC(St.dec_flush++); }
  ud_n = 0; rc_n = 0; rc_dirty = 1;
}
void xl_vm_mode(int by_ud) { vm_all(); vm_by_ud = by_ud; }
void xl_ud_v48(int on) { ud_v48 = on; }
/* an UNPACK that can wait: no filling (WL > CL; or WL 0), no running ROW (MODE 2, 3), no write-protected lane in the mask rows it uses,
   no quadword written twice; the rest keep unpack() as the words come */
static int ud_ok(uint32_t w)
{
  int c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff; if (!num) num = 256;
  if (V.wl < 1 || V.wl > V.cl) return 0;
  if (V.mode >= 2 && (c & 0xf) != 0xf) return 0;   /* V4-5 takes no MODE */
  if (V.wl != V.cl) {   /* the last quadword it writes stays in VU memory (WL = CL: num - 1 < 256 on) */
    uint32_t n1 = (uint32_t)num - 1, last = V.wl == 1 ? n1 * (uint32_t)V.cl : n1 / (uint32_t)V.wl * (uint32_t)V.cl + n1 % (uint32_t)V.wl;
    if (last >= 1024) return 0;
  }
  if (c & 0x10) for (int r = 0; r < (V.wl < 4 ? V.wl : 4); r++) for (int j = 0; j < 4; j++) if ((V.mask >> (r * 8 + j * 2) & 3) == 3) return 0;
  return 1;
}
/* own[o .. o + n) = id (o + n <= 1024), two entries a store where they share a word */
typedef uint32_t __attribute__((may_alias)) own2_t;
static void own_fill(uint32_t o, uint32_t n, uint16_t id)
{
  uint16_t *q = own + o;
  if ((o & 1) && n) { *q++ = id; n--; }
  own2_t v = id | (uint32_t)id << 16, *w = (own2_t *)q, *e = w + n / 2;
  while (w < e) *w++ = v;
  if (n & 1) *(uint16_t *)w = id;
}
/* the UNPACK w, its nw data words at p: a descriptor, and the quadwords it writes owned by it */
static void ud_new(uint32_t w, const uint32_t *p, uint32_t nw)
{
  int c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff; if (!num) num = 256;
  uint32_t imm = w & 0xffff, a = ((imm & 0x3ff) + ((imm >> 15 & 1) ? V.tops : 0)) & 0x3ff;
  if (ud_n == UD_N) { XC(St.full_ud++); vm_all(); }
  if (rc_dirty && rc_n == RC_N) { XC(St.full_rc++); vm_all(); }
  if (rc_dirty) { memcpy(RC[rc_n], V.row, 16); memcpy(RC[rc_n] + 4, V.col, 16); rc_n++; rc_dirty = 0; }
  Ud *d = &UD[ud_n++]; uint16_t id = (uint16_t)ud_n;
  d->p = p; d->addr = (uint16_t)a; d->num = (uint16_t)num; d->nw = (uint16_t)nw; d->rc = (uint16_t)(rc_n - 1); d->fmt = (uint8_t)(c & 0x1f);
  d->md = (uint8_t)((imm >> 14 & 1) | ((c & 0xf) == 0xf ? 0 : V.mode) << 1); d->cl = (uint8_t)V.cl; d->wl = (uint8_t)V.wl; d->mask = V.mask;
  XC(St.lz_cmds++); XC(St.lz_vec += num);
  if (V.cl == V.wl) { uint32_t n1 = 1024 - a < (uint32_t)num ? 1024 - a : (uint32_t)num;
    own_fill(a, n1, id); if (n1 < (uint32_t)num) own_fill(0, (uint32_t)num - n1, id); }
  else if (V.wl == 1) { uint32_t cl = (uint32_t)V.cl; for (int k = 0; k < num; k++, a += cl) own[a & 1023] = id; }
  else for (int k = 0, cyc = 0; k < num; k++) { own[a & 1023] = id; a++; if (++cyc == V.wl) { cyc = 0; a += (uint32_t)(V.cl - V.wl); } }
}

#ifdef XL_CENSUS
static void up_census(uint32_t w)
{
  int c = (w >> 24) & 0x7f, vn_ = (c >> 2) & 3, vl = c & 3, m = (c >> 4) & 1, num = (w >> 16) & 0xff; if (!num) num = 256;
  uint32_t imm = w & 0xffff; int usn = imm >> 14 & 1; if (vn_ == 3 && vl == 3) usn = 1;
  St.uq[c & 0x1f] += num; St.uq_open += num;
  uint64_t key = (uint64_t)(c & 0x1f) | (uint64_t)(V.mode & 3) << 5 | (uint64_t)usn << 7 | (uint64_t)(imm >> 15 & 1) << 8 | (uint64_t)(V.cl & 0xff) << 16
                 | (uint64_t)(V.wl & 0xff) << 24 | (m ? (uint64_t)V.mask << 32 : 0);
  int i; for (i = 0; i < St.nuk && St.uk[i] != key; i++) {}
  if (i == St.nuk && i < 64) St.uk[St.nuk++] = key;
  if (i < 64) { St.uk_cmds[i]++; St.uk_vec[i] += num; }
}
#else
#define up_census(w)
#endif
static void unpack(uint32_t w, const uint32_t *d, int nw)
{
  int c = (w >> 24) & 0x7f, vn_ = (c >> 2) & 3, vl = c & 3, m = (c >> 4) & 1, num = (w >> 16) & 0xff; if (!num) num = 256;
  uint32_t imm = w & 0xffff, addr = (imm & 0x3ff) + ((imm >> 15 & 1) ? V.tops : 0); int usn = imm >> 14 & 1;
  int bits = 32 >> vl, ne = vn_ + 1; if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; usn = 1; }
  uint64_t bitpos = 0; int cyc = 0; uint32_t a = addr; up_census(w);
  for (int k = 0; k < num; k++) {
    uint32_t e[4];
    for (int j = 0; j < 4; j++) {
      int jj = vn_ == 0 ? 0 : vn_ == 1 ? (j & 1) : j;
      uint64_t bp = bitpos + (uint64_t)jj * bits; uint32_t wi = bp >> 5, sh = bp & 31, v;
      v = wi < (uint32_t)nw ? d[wi] >> sh : 0;
      if (bits < 32) { v &= (1u << bits) - 1; if (!usn && (v >> (bits - 1) & 1)) v |= ~0u << bits; }
      e[j] = v;
    }
    if (vn_ == 3 && vl == 3) { uint32_t s = e[0]; e[0] = (s & 0x1f) << 3; e[1] = (s >> 5 & 0x1f) << 3; e[2] = (s >> 10 & 0x1f) << 3; e[3] = (s >> 15 & 1) << 7; }
    bitpos += (uint64_t)bits * ne;
    uint32_t *q = (uint32_t *)(VM + ((a & 0x3ff) << 4)); int row = cyc < 3 ? cyc : 3;
    if (own[a & 0x3ff]) { vm_one(a & 0x3ff); XC(St.dec_write++); }   /* pending: the lanes this write leaves keep their value */
    for (int j = 0; j < 4; j++) {
      int mk = m ? (V.mask >> (row * 8 + j * 2)) & 3 : 0;
      switch (mk) {
        case 0: { int md = (vn_ == 3 && vl == 3) ? 0 : V.mode;
          if (md == 1) q[j] = e[j] + V.row[j]; else if (md == 2) { V.row[j] += e[j]; q[j] = V.row[j]; } else if (md == 3) { V.row[j] = e[j]; q[j] = e[j]; } else q[j] = e[j]; break; }
        case 1: q[j] = V.row[j]; break;
        case 2: q[j] = V.col[row]; break;
      }
    }
    a++; if (++cyc == V.wl) { cyc = 0; a += V.cl - V.wl; }
  }
  if (V.mode >= 2) rc_dirty = 1;   /* a running ROW may have changed it */
}

/* the GIF packet a program kicks from VU memory (XGKICK), up to its EOP */
static int kick_vm(uint32_t qa)
{
  for (int k = 0; k < 1024; k++) if (gif_qw(1, vmq(qa + k), 0)) return 1;
  memset(&G[1], 0, sizeof G[1]); return 0;
}
/* entry names: the guest address the micro code was uploaded from.  P04 entry X is 0x1983e0 + X + 8 * (X >> 11) (one 8-byte MPG
   command per 2 KB chunk); P06 is uploaded from 0x19fe08, P07 from 0x1a31e0, P09 from 0x1a8148, P01 from 0x194478, the stage program
   from 0x201f78 */
static const XlRule RULES[] = {
  { 0x198b60, 0xe8, 0x15, 0, 0, 0 },   /* P04+780 lit mesh */
  { 0x198e58, 0xe8, 0x15, 0, 0, 0 },   /* P04+a70 skinned lit mesh */
  { XL_DF0,   0xe8, 0x15, 0, 0, 0 },   /* P04+df0 pre-lit mesh */
  { 0x199478, 0xe8, 0x15, 0, 0, 0 },   /* P04+1088 pre-lit, back-face cull */
  { XL_FX,    0xe8, 0x15, 0, 0, 0 },   /* P04+25f0 effect meshes */
  { XL_FX2,   0xe8, 0x15, 0, 0, 0 },   /* P04+2870 effect meshes, back-face cull */
  { 0x19af68, 0xe8, 0x15, 0, 0, 0 },   /* P04+2b60 line strips */
  { 0x19bb50, 0xe8, 0x15, 0, 0, 0 },   /* P04+3740 view-aligned sprites */
  { 0x19be50, 0xe8, 0x15, 0, 0, 0 },   /* P04+3a38 rotated quads */
  { 0x19fe08, 0xe8, 0x15, 0, 0, 0, 0, { 0, 0 }, 1 },   /* P06+0 lit mesh, 1-3 passes */
  { 0x1a0160, 0xe8, 0x15, 0, 0, 0, 0, { 0, 0 }, 1 },   /* P06+358 skinned */
  { 0x1a0540, 0xe8, 0x15, 0, 0, 0, 0, { 0, 0 }, 1 },   /* P06+738 a per-vertex colour factor (the guards) */
  { 0x1a08c0, 0xe8, 0x15, 0, 0, 0, 0, { 0, 0 }, 1 },   /* P06+ab0 skinned with the factor */
  { 0x1a31e0, 0xe8, 0x3, 0, 0, 0, 0, { 0, 0 }, 2 },    /* P07 the codec faces: skinned, lit, 1-3 passes; the template T+3 */
  { 0x1a8148, 0x78, 0x11, 1, 1, 0 },   /* P09 patches */
  { 0x194478, 0x314, 0x319, 1, 1, 0x1944c0, 0x319, { 0x130, 0x220 } },   /* P01 instanced mesh: the MSCAL kicks the A+D and copies the
                                                                             tag template into both output buffers, the MSCNT draws */
  { 0x1944c0, -1, 0x319, 0, 1, 0 },
  { 0x201f78, -1, 0x9, 0, 1, 0, 0x9, { 0x180, 0x320 } },   /* stage program (2D lines and markers); the setup entry copies the tag
                                                             template into both buffers' T+0xd0 */
  { 0x202008, -1, 0x9, 0, 1, 0 },
  { 0x202170, -1, 0x9, 0, 1, 0 },
  { 0x202390, -1, 0x9, 0, 1, 0 },
};
const XlRule *xl_rule(uint32_t e)
{
  for (unsigned i = 0; i < sizeof RULES / sizeof *RULES; i++) if (RULES[i].entry == e) return &RULES[i];
  return 0;
}
/* the entries a call has named, by a hash of the entry: its index in St.ent (64 when the table was full) and its rule, so that a call
   does not search the two lists (cleared by xl_reset) */
static struct { uint32_t entry; int16_t i, used; const XlRule *r; } EC[256];
static unsigned ec_slot(uint32_t e) { unsigned h = (e * 2654435761u) >> 24, n = 0; while (EC[h].used && EC[h].entry != e && ++n < 256) h = (h + 1) & 255; return n < 256 ? h : 256; }
static const XlRule *rule_of(uint32_t e) { unsigned h = ec_slot(e); return h < 256 && EC[h].used ? EC[h].r : xl_rule(e); }
/* entry e's index in St.ent, added there if new (as the search in call did), and its rule (0 for entry 0) */
static int ent_index(uint32_t e, const XlRule **r)
{
  unsigned h = ec_slot(e);
  if (h < 256 && EC[h].used) { *r = EC[h].r; return EC[h].i; }
  int i; for (i = 0; i < St.nent && St.ent[i] != e; i++) {}
  if (i == St.nent && i < 64) { St.ent[i] = e; St.nent++; }
  *r = e ? xl_rule(e) : 0;
  if (h < 256) { EC[h].entry = e; EC[h].i = (int16_t)i; EC[h].used = 1; EC[h].r = *r; }
  return i;
}
/* P06's extra texture passes (re/vu1_programs.md 3.4): after the main pass the program kicks pass 2's A+D packet (T+0xf0) when header y
   has 0x10, and pass 3's (T+0xf8) when it has 0x100, 0x200 or 0x400, each before the same vertices again (0x2f8 checked against gsref:
   the CLUT loads of both packets in this order; 0x100 and 0x400 alone not seen).  P07 (kind 2) the same with its flags in T+0.w (0x10:
   T+0xf0 at 0x1940; 0x200: T+0xf8 at 0x1c28; 0x100 and 0x400 not seen) */
static void passes(const XlCall *c, int kind)
{
  static const struct { uint32_t bits; int off; } P[2] = { { 0x10, 0xf0 }, { 0x700, 0xf8 } };
  uint32_t y = kind == 2 ? vmq(c->top)[3] : vmq(c->top + 0x14)[1];
  for (int k = 0; k < 2; k++) if (y & P[k].bits) {
    St.ad_kicks++; if (!kick_vm((c->top + P[k].off) & 0x3ff)) St.ad_bad++;
    if (pass_fn) pass_fn(pass_ctx, c, k + 2);
  }
}
static uint32_t last_entry; static int last_cont;
static void call(uint32_t pc, int cont)
{
  XlCall c; memset(&c, 0, sizeof c);
  c.pc = pc; c.cont = cont; c.top = V.tops; c.itop = V.itops;
  if (V.dbf) { V.dbf = 0; V.tops = V.base; } else { V.dbf = 1; V.tops = V.base + V.ofst; }
  if (cont) { const XlRule *p = rule_of(last_entry); c.entry = p ? p->cont : 0; }
  else c.entry = last_entry = VPsrc[pc & 2047];
  c.ad_qa = c.tag_qa = ~0u;
  St.calls++; if (cont) St.calls_cont++; else if (!c.entry) St.calls_noentry++;
  if (cont && last_cont && !St.cont2++) St.cont2_after = last_entry;
  last_cont = cont;
  const XlRule *r; int i = ent_index(c.entry, &r);
  if (i < 64) St.ent_n[i]++;
  XC(if (i < 64) St.ent_uq[i] += St.uq_open; St.uq_open = 0);
  if (r && r->ad >= 0) {
    c.ad_qa = (r->ad + (r->ad_abs ? 0 : c.top)) & 0x3ff; St.ad_kicks++;
    if (!kick_vm(c.ad_qa)) St.ad_bad++;
    c.ad = 1;
  }
  if (r && r->cp_dst[0]) for (int k = 0; k < 2; k++) { uint32_t d = r->cp_dst[k] & 0x3ff; memcpy(VM + (d << 4), vmq(r->cp_src), 16); own[d] = 0; }
  int dctx = 3;   /* the contexts the call may draw in: the template's, else both */
  if (r) {   /* the template's PRIM, as the geometry tag sets it (PRE) */
    c.tag_qa = (r->tag + (r->tag_abs ? 0 : c.top)) & 0x3ff;
    const uint32_t *t = vmq(c.tag_qa);
    if (t[1] >> 14 & 1) { gs_write(0x00, t[1] >> 15 & 0x7ff); gf_reg(&F, 0x00, t[1] >> 15 & 0x7ff);
      uint64_t at = (F.gr[0x1a] & 1) ? F.gr[0x00] : F.gr[0x1b]; dctx = 1 << (int)(at >> 9 & 1); }
  }
  gf_unknown(&F, GF_U_ALL);   /* the program kicks its own tags and vertices: PRIM, the vertex queue and the vertex fields */
  c.vm = VM; c.gr = gr;
  if (call_fn) call_fn(call_ctx, &c);
  if (r && r->passes) passes(&c, r->passes);
  gf_drew(&F, dctx);   /* after the callbacks: the call reads its texels before it draws */
}

/* the end of a command's data: STMASK, STROW, STCOL, an UNPACK decoded now */
static void vif_end(void)
{
  int c = (vcmd >> 24) & 0x7f;
  if (c == 0x20) V.mask = ub[0];
  else if (c == 0x30) { memcpy(V.row, ub, 16); rc_dirty = 1; }
  else if (c == 0x31) { memcpy(V.col, ub, 16); rc_dirty = 1; }
  else if ((c & 0x60) == 0x60) unpack(vcmd, ub, vn < 1024 ? vn : 1024);
}
static void vif_cmd(uint32_t w);
void xl_vif1(uint32_t w, uint32_t src)
{
  St.words++;
  if (vleft) {
    int c = (vcmd >> 24) & 0x7f;
    XC(St.wd[c]++);
    if (!vn) vsrc0 = src;
    if (c == 0x50 || c == 0x51) { if (!dqn) dqsrc = src; dq[dqn++] = w; if (dqn == 4) { dqn = 0; gif_qw(2, dq, dqsrc); } }
    else if (c == 0x4a) { if (!(vn & 1)) VPsrc[((vcmd & 0xffff) + vn / 2) & 2047] = vsrc0 + 4 * vn; }
    else if (vn < 1024) ub[vn] = w;
    vn++;
    if (--vleft) return;
    vif_end();
    return;
  }
  vif_cmd(w);
}
/* a VIF1 command word */
static void vif_cmd(uint32_t w)
{
  int c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff; uint32_t imm = w & 0xffff;
  vcmd = w; vn = 0; dqn = 0; St.cmds[c]++;
  switch (c) {
    case 0x00: case 0x10: case 0x11: case 0x13: case 0x06: case 0x07: return;   /* NOP FLUSHE FLUSH FLUSHA MSKPATH3 MARK */
    case 0x01: V.cl = imm & 0xff; V.wl = imm >> 8 & 0xff; return;
    case 0x02: V.ofst = imm & 0x3ff; V.dbf = 0; V.tops = V.base; return;
    case 0x03: V.base = imm & 0x3ff; return;
    case 0x04: V.itops = imm & 0x3ff; return;
    case 0x05: V.mode = imm & 3; return;
    case 0x14: case 0x15: call(imm, 0); return;
    case 0x17: call(0, 1); return;
    case 0x20: vleft = 1; return;
    case 0x30: case 0x31: vleft = 4; return;
    case 0x4a: vleft = (num ? num : 256) * 2; return;
    case 0x50: case 0x51: vleft = (imm ? imm : 65536) * 4; return;
  }
  if ((c & 0x60) == 0x60) {
    int vn_ = (c >> 2) & 3, vl = c & 3, n = num ? num : 256, bits = 32 >> vl, ne = vn_ + 1; if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; }
    if (V.wl > V.cl) { n = (n / V.wl) * V.cl + (n % V.wl < V.cl ? n % V.wl : V.cl); St.fills++; }
    vleft = (int)(((uint64_t)n * bits * ne + 31) / 32);
    if (!vleft) unpack(w, ub, 0);
    return;
  }
  St.unk++;
}
/* k data words of DIRECT / DIRECTHL from guest address s on: whole quadwords straight from the span, IMAGE runs in one piece */
static void direct(const uint32_t *p, uint32_t k, uint32_t s)
{
  for (uint32_t j = 0; j < k; ) {
    if (!dqn && k - j >= 4) {
      Gif *g = &G[2];
      if (g->nloop && g->flg >= 2) { uint32_t m = (k - j) / 4; if (m > (uint32_t)g->nloop) m = (uint32_t)g->nloop; image_run(2, s + 4 * j, m); j += 4 * m; }
      else { gif_qw(2, p + j, s + 4 * j); j += 4; }
      continue;
    }
    if (!dqn) dqsrc = s + 4 * j;
    dq[dqn++] = p[j++];
    if (dqn == 4) { dqn = 0; gif_qw(2, dq, dqsrc); }
  }
}
void xl_vif1_span(const uint32_t *p, uint32_t n, uint32_t src)
{
  uint32_t i = 0;
  St.words += n;
  while (i < n) {
    if (!vleft) { vif_cmd(p[i++]); continue; }
    int c = (vcmd >> 24) & 0x7f; uint32_t k = n - i < (uint32_t)vleft ? n - i : (uint32_t)vleft, s = src + 4 * i;
    XC(St.wd[c] += k);
    if (!vn) vsrc0 = s;
    if (c == 0x50 || c == 0x51) direct(p + i, k, s);
    else if (c == 0x4a) { for (uint32_t t = (uint32_t)vn + (vn & 1); t < (uint32_t)vn + k; t += 2) VPsrc[((vcmd & 0xffff) + t / 2) & 2047] = vsrc0 + 4 * t; }
    else if ((c & 0x60) == 0x60 && !vn && k == (uint32_t)vleft && ud_ok(vcmd)) {   /* all its data here: decoded when read */
      up_census(vcmd); ud_new(vcmd, p + i, k); vn = (int)k; vleft = 0; i += k; continue; }
    else for (uint32_t j = 0; j < k; j++) if ((uint32_t)vn + j < 1024) ub[vn + j] = p[i + j];
    vn += (int)k; vleft -= (int)k; i += k;
    if (!vleft) vif_end();
  }
}
void xl_gif3(const uint32_t *q, uint32_t src) { gif_qw(3, q, src); }
void xl_gif3_span(const uint32_t *p, uint32_t nq, uint32_t src)
{
  for (uint32_t i = 0; i < nq; ) {
    Gif *g = &G[3];
    if (g->nloop && g->flg >= 2) { uint32_t m = nq - i < (uint32_t)g->nloop ? nq - i : (uint32_t)g->nloop; image_run(3, src + 16 * i, m); i += m; continue; }
    gif_qw(3, p + 4 * i, src + 16 * i); i++;
  }
}
void xl_flush(void) { vm_all(); }
void xl_vm_sync(void) { vm_all(); }
const uint32_t *xl_vm_q(uint32_t qa) { return vmq(qa); }
void xl_vm_logical(uint8_t *out)
{
  memcpy(out, VM, sizeof VM);
  if (ud_n) for (uint32_t qa = 0; qa < 1024; qa++) if (own[qa]) { const Ud *d = &UD[own[qa] - 1]; ud_vec(d, ud_k(d, qa), (uint32_t *)(out + (qa << 4))); }
}
void xl_reset(void)
{
  memset(&St, 0, sizeof St); memset(gr, 0, sizeof gr); memset(G, 0, sizeof G); memset(VM, 0, sizeof VM); memset(VPsrc, 0, sizeof VPsrc);
  memset(&V, 0, sizeof V); V.cl = V.wl = 1; vleft = vn = dqn = 0; last_entry = 0; last_cont = 0;
  memset(own, 0, sizeof own); ud_n = rc_n = 0; rc_dirty = 1; memset(EC, 0, sizeof EC);
  gf_reset(&F, prim_fn, prim_ctx); F.up = up_map;
}
void xl_on_call(XlCallFn fn, void *ctx) { call_fn = fn; call_ctx = ctx; }
void xl_on_prim(GfPrimFn fn, void *ctx) { prim_fn = fn; prim_ctx = ctx; F.fn = fn; F.ctx = ctx; }
const GfGs *xl_front(void) { return &F; }
void xl_uploads(struct GuMap *m) { up_map = m; F.up = m; }
void xl_on_pass(XlPassFn fn, void *ctx) { pass_fn = fn; pass_ctx = ctx; }
const uint64_t *xl_gs(void) { return gr; }
const uint8_t *xl_vm(void) { vm_all(); return VM; }
const XlStats *xl_stats(void) { return &St; }
