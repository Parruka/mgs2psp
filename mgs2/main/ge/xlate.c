/* xlate.c: the Stage A packet translator (xlate.h, renderer_notes 11.2).  VIF1 and GIF decoding follow tools/gsref.c (whose replay of
   the deck capture draws the expected frame); what gsref gets from running the VU1 programs, the translator gets from the uploaded data:
   the A+D packet a call kicks first and the PRIM of the tag template its geometry starts with (the rules below).  Two fronts feed it:
   xl_vif1 a word at a time (tools/gsref.c, the reference) and xl_vif1_span a span at a time, which decodes an UNPACK's data only when
   something reads it (renderer_notes 11.12 step 1). */
#include <string.h>
#include "xlate.h"
#include "hot39.h"   /* gl39 */
#include "gsfront.h"
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */
#include "gsup.h"    /* gl34: gu_clut_load (xl_p04_fast) */
#include "natsnap.h"   /* gl36: NatGrp (xl_nr_begin) */
#ifndef __psp__
#include <stdlib.h>   /* gl36: getenv (the x86 debug line) */
#endif
#ifndef __psp__
#include <stdio.h>
#endif
#include "natsplit.h"   /* gl34: NS_MARK */

static XlStats St;
#ifdef XL_CENSUS
#define XC(x) x
#else
#define XC(x)
#endif
uint32_t xl_gs_gen; int xl_keep_bits, xl_keep_nf, xl_keep_chk;   /* gl35 (xlate.h) */
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
  xl_gs_gen++;   /* gl35 */
  uint64_t lo = q[0] | (uint64_t)q[1] << 32, hi = q[2] | (uint64_t)q[3] << 32;
  F.path = path; XC(St.gq[path]++);
  if (!g->nloop) {
    g->nloop = lo & 0x7fff; g->eop = lo >> 15 & 1; g->flg = lo >> 58 & 3; g->nreg = (lo >> 60 & 15) ? (int)(lo >> 60 & 15) : 16; g->regs = hi; g->ri = 0;
    if ((lo >> 46 & 1) && g->flg == 0) { gs_write(0x00, lo >> 47 & 0x7ff); gf_reg(&F, 0x00, lo >> 47 & 0x7ff); }
    St.gif_tags[path]++; ME_BEAT();
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
  xl_gs_gen++;   /* gl35 */
  F.path = path; XC(St.gq[path] += m); St.gif_image_qw += m; XC(St.img_qw[path] += m);
  gf_image_run(&F, src, 16 * m); G[path].nloop -= (int)m;
}

/* ---- VU1 data memory and VIF1 ---- */
static uint8_t VM[16384] __attribute__((aligned(16)));
static uint32_t VPsrc[2048];   /* guest address each micro instruction was uploaded from, as gsref names entries */
static struct { int cl, wl, mode; uint32_t mask, row[4], col[4], base, ofst, tops, itops, dbf; } V;
static uint32_t vcmd, vsrc0, ub[1024], dq[4], dqsrc; static int vleft, vn, dqn;
uint32_t xl_vm_wgen;   /* gl34 (xlate.h) */
static uint32_t usrc; static int uvert;   /* task #114: XlCall.src so far; the command is an UNPACK to T + 0x100 */

/* UNPACKs decoded when read (xl_vif1_span): a descriptor for each UNPACK whose data a span held whole, with where the data lies (guest RAM,
   unchanged until the transfer ends), the command, STCYCL, STMOD, STMASK, and ROW / COL as copied into RC (a copy each time they change,
   shared by the descriptors made until the next change); own[qa]: the number + 1 of the descriptor that wrote VU quadword qa last (0: VM
   holds it).  A quadword's value is its descriptor's vector for it, so quadwords can be decoded in any order */
typedef struct { const uint32_t *p; uint16_t addr, num, nw, rc; uint8_t fmt, md, cl, wl; uint32_t mask; } Ud;
#define UD_N 4096
#define RC_N 1024
static Ud UD[UD_N]; static uint32_t RC[RC_N][8]; static uint16_t own[1024] __attribute__((aligned(16))); static int ud_n H39, rc_n H39, rc_dirty = 1;
static int lane_on, need_on; static uint32_t ud_vecs;   /* sb1: xl_lane, xl_need; the vectors of the pending descriptors */
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
static int ud_eager_on = 1, ud_mark_on = 1;   /* sb8 (task #121): xl_ud_eager, xl_ud_mark */
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
  if (vm_by_ud && ((!lane_on && !need_on) || ud_vecs <= 1024))   /* each descriptor's quadwords that it still owns, vector by vector (as many steps as vectors were unpacked) */
    for (int i = 0; i < ud_n; i++) {
      const Ud *d = &UD[i]; uint16_t id = (uint16_t)(i + 1);
      switch (ud_kind(d)) { case 1: ud_run(d, id, 1); break; case 2: ud_run(d, id, 2); break; case 3: ud_run(d, id, 3); break;
                            case 4: ud_run(d, id, 4); break; case 5: ud_run(d, id, 5); break; default: ud_run(d, id, 0); }
    }
  else for (uint32_t qa = 0; qa < 1024; qa++) if (own[qa]) { vm_one(qa); XC(St.dec_flush++); }
  ud_n = 0; rc_n = 0; rc_dirty = 1; ud_vecs = 0;
}
/* sb1: the pending quadwords of qa .. qa + n - 1 decoded, each by its descriptor: the first pending one met decodes every quadword its
   descriptor still owns (vm_all's loop for it), as vm_all would have, since nothing changes a descriptor's data within a transfer */
static void need_q(uint32_t qa, uint32_t n)
{
  for (uint32_t k = 0; k < n; k++) {
    uint32_t q = (qa + k) & 1023; uint16_t id = own[q];
    if (!id) continue;
    const Ud *d = &UD[id - 1]; XC(St.need_runs++);
    switch (ud_kind(d)) { case 1: ud_run(d, id, 1); break; case 2: ud_run(d, id, 2); break; case 3: ud_run(d, id, 3); break;
                          case 4: ud_run(d, id, 4); break; case 5: ud_run(d, id, 5); break; default: ud_run(d, id, 0); }
  }
}
static const uint32_t *vmw(uint32_t qa) { return (const uint32_t *)(VM + ((qa & 1023) << 4)); }   /* a quadword need_q decoded */
void xl_vm_mode(int by_ud) { vm_all(); vm_by_ud = by_ud; }
void xl_ud_v48(int on) { ud_v48 = on; }
void xl_ud_eager(int on) { ud_eager_on = on; }
void xl_ud_mark(int on) { ud_mark_on = on; }
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
/* sb8 (task #121): the same fill, always inlined, for ud_eager's straight copy */
static inline __attribute__((always_inline)) void own_fill_i(uint32_t o, uint32_t n, uint16_t id)
{
  uint16_t *q = own + o;
  if ((o & 1) && n) { *q++ = id; n--; }
  own2_t v = id | (uint32_t)id << 16, *w = (own2_t *)q, *e = w + n / 2;
  while (w < e) *w++ = v;
  if (n & 1) *(uint16_t *)w = id;
}
/* sb10 T2 (xlate.h): the generations and sources of VU addresses 0 .. 63, slot by slot */
uint32_t xl_vgen[16], xl_vsrc_gen[16]; const void *xl_vsrc[16];
static void lo_all(void) { for (int s = 0; s < 16; s++) xl_vgen[s]++; }
/* quadwords a .. a + last (mod 1024) written, some or all of them: each slot of 0 .. 63 that range meets takes a new generation */
static void lo_touch(uint32_t a, uint32_t last)
{
  a &= 1023;
  if (last >= 1023) { lo_all(); return; }
  uint32_t e = a + last;
  if (a < 64) { uint32_t hi = e < 63 ? e : 63; for (uint32_t s = a >> 2; s <= hi >> 2; s++) xl_vgen[s]++; }
  if (e >= 1024) { uint32_t hi = e - 1024 < 63 ? e - 1024 : 63; for (uint32_t s = 0; s <= hi >> 2; s++) xl_vgen[s]++; }
}
/* the highest quadword (from the first) that num vectors with CL / WL write: WL a cycle, the next cycle CL on (unpack()'s stepping; with
   WL > CL a cycle may reach past the next one's start: then at most this); WL 0: everything.  (The forms every UNPACK of the captures
   takes, CL = WL and WL 1 or 2, without a division) */
static inline uint32_t up_last(uint32_t num, uint32_t cl, uint32_t wl)
{
  if (wl == cl) return wl ? num - 1 : 1023;
  if (wl == 1) return (num - 1) * cl;
  if (wl == 2) return ((num - 1) >> 1) * cl + (cl < 2 ? 1 : (num - 1) & 1);
  return wl ? (num - 1) / wl * cl + (wl > cl ? wl - 1 : (num - 1) % wl) : 1023;
}
/* a straight copy (V4-32 in MODE 0 without a mask, CL = WL) of num quadwords from p to a .. a + num - 1, not wrapping, after lo_touch:
   each whole slot of 0 .. 63 in it holds the 64 bytes at its place in p */
static void lo_src(uint32_t a, uint32_t num, const uint32_t *p)
{
  for (uint32_t s = (a + 3) >> 2; s < 16 && 4 * s + 4 <= a + num; s++) { xl_vsrc[s] = p + 4 * (4 * s - a); xl_vsrc_gen[s] = xl_vgen[s]; }
}
/* the UNPACK w, its nw data words at p: a descriptor, and the quadwords it writes owned by it */
static void ud_new(uint32_t w, const uint32_t *p, uint32_t nw)
{
  int c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff; if (!num) num = 256;
  uint32_t imm = w & 0xffff, a = ((imm & 0x3ff) + ((imm >> 15 & 1) ? V.tops : 0)) & 0x3ff;
  xl_vm_wgen++;   /* gl34 */
  { uint32_t last = up_last((uint32_t)num, (uint32_t)V.cl, (uint32_t)V.wl);   /* sb10 T2 */
    if (a < 64 || a + last >= 1024) { lo_touch(a, last); if ((c & 0x1f) == 0x0c && !V.mode && V.cl == V.wl && a + (uint32_t)num <= 1024) lo_src(a, (uint32_t)num, p); } }
  if (ud_n == UD_N) { XC(St.full_ud++); vm_all(); }
  if (rc_dirty && rc_n == RC_N) { XC(St.full_rc++); vm_all(); }
  if (rc_dirty) { memcpy(RC[rc_n], V.row, 16); memcpy(RC[rc_n] + 4, V.col, 16); rc_n++; rc_dirty = 0; }
  Ud *d = &UD[ud_n++]; uint16_t id = (uint16_t)ud_n;
  d->p = p; d->addr = (uint16_t)a; d->num = (uint16_t)num; d->nw = (uint16_t)nw; d->rc = (uint16_t)(rc_n - 1); d->fmt = (uint8_t)(c & 0x1f);
  d->md = (uint8_t)((imm >> 14 & 1) | ((c & 0xf) == 0xf ? 0 : V.mode) << 1); d->cl = (uint8_t)V.cl; d->wl = (uint8_t)V.wl; d->mask = V.mask;
  XC(St.lz_cmds++); XC(St.lz_vec += num); ud_vecs += (uint32_t)num;
  if (V.cl == V.wl) { uint32_t n1 = 1024 - a < (uint32_t)num ? 1024 - a : (uint32_t)num;
    own_fill(a, n1, id); if (n1 < (uint32_t)num) own_fill(0, (uint32_t)num - n1, id); }
  else if (V.wl == 1) { uint32_t cl = (uint32_t)V.cl;
    if (ud_mark_on && a + (uint32_t)(num - 1) * cl < 1024) {   /* sb8 (task #121): no wrap, four entries a turn */
      uint16_t *q = own + a; int k = num;
      for (; k >= 4; k -= 4, q += 4 * cl) { q[0] = id; q[cl] = id; q[2 * cl] = id; q[3 * cl] = id; }
      for (; k > 0; k--, q += cl) *q = id; }
    else for (int k = 0; k < num; k++, a += cl) own[a & 1023] = id; }
  else if (ud_mark_on && V.wl == 2 && V.cl > 2 && a + (uint32_t)(num - 1) / 2 * (uint32_t)V.cl + (uint32_t)((num - 1) & 1) < 1024) {
    uint16_t *q = own + a; uint32_t cl = (uint32_t)V.cl; int k = num;   /* sb8: CL > 2, so the last quadword is the highest; two a turn */
    for (; k >= 2; k -= 2, q += cl) { q[0] = id; q[1] = id; }
    if (k) q[0] = id; }
  else for (int k = 0, cyc = 0; k < num; k++) { own[a & 1023] = id; a++; if (++cyc == V.wl) { cyc = 0; a += (uint32_t)(V.cl - V.wl); } }
}
/* sb8 (task #121, xl_ud_eager(1)): ud_new for a lane's constant block (header or material) in the one form every lane constant takes
   (the census): V4-32 without a mask in MODE 0 (ud_kind 1: the data as it is), CL = WL, not wrapping.  Its quadwords are copied into VM
   now, as a read or vm_all would decode them, and VM owns them again (own 0: a pending quadword of an older descriptor that this block
   overwrites whole is not decoded, as with ud_new).  The VU memory a read, a flush or xl_vm_logical sees is the same; no descriptor,
   nothing pending.  Any other form, or off: ud_new */
static void ud_eager(uint32_t w, const uint32_t *p, uint32_t nw)
{
  uint32_t c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff, imm = w & 0xffff; if (!num) num = 256;
  uint32_t a = ((imm & 0x3ff) + ((imm >> 15 & 1) ? V.tops : 0)) & 0x3ff;
  if (ud_eager_on && (c & 0x1f) == 0x0c && !V.mode && V.cl == V.wl && a + num <= 1024) {
    const ud_w32 *sp = (const ud_w32 *)p; ud_w32 *q = (ud_w32 *)(VM + (a << 4)), *e = q + 4 * num;
    xl_vm_wgen++;   /* gl34 */
    for (; q < e; q += 4, sp += 4) { uint32_t x0 = sp[0], x1 = sp[1], x2 = sp[2], x3 = sp[3]; q[0] = x0; q[1] = x1; q[2] = x2; q[3] = x3; }
    own_fill_i(a, num, 0);
    if (a < 64) { lo_touch(a, num - 1); lo_src(a, num, p); }   /* sb10 T2 (a + num <= 1024: no wrap) */
    XC(St.eg_cmds++); XC(St.eg_vec += num); return;
  }
  ud_new(w, p, nw);
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
  { uint32_t a0 = addr & 0x3ff, last = up_last((uint32_t)num, (uint32_t)V.cl, (uint32_t)V.wl); if (a0 < 64 || a0 + last >= 1024) lo_touch(a0, last); }   /* sb10 T2 */
  uint64_t bitpos = 0; int cyc = 0; uint32_t a = addr; up_census(w);
  xl_vm_wgen++;   /* gl34 */
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
static inline __attribute__((always_inline)) int ent_index(uint32_t e, const XlRule **r)   /* gl35: pinned in line (gl33 had it so) */
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
    if (pass_fn) ME_CALLP(pass_fn)(pass_ctx, c, k + 2);
  }
}
static uint32_t last_entry; static int last_cont;
/* sb5: the pending quadwords of qa .. qa + n - 1 decoded, each as vmq decodes it (own[] read two entries at a time when qa and n are even
   and the range does not wrap) */
static void vm_dec(uint32_t qa, uint32_t n)
{
  qa &= 1023;
  if ((qa | n) & 1 || qa + n > 1024) { for (uint32_t k = 0; k < n; k++) vmq(qa + k); return; }
  const own2_t *w = (const own2_t *)(own + qa);
  for (uint32_t k = 0; k < n / 2; k++) if (w[k]) { vmq(qa + 2 * k); vmq(qa + 2 * k + 1); }
}
static void call(uint32_t pc, int cont, const XlRec *rec)
{
  XlCall c; memset(&c, 0, sizeof c);
  xl_gs_gen++;   /* gl35 */
  c.pc = pc; c.cont = cont; c.top = V.tops; c.itop = V.itops; c.src = usrc; usrc = 0;
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
  NS_MARK(NS_AD);
  if (r && r->cp_dst[0]) for (int k = 0; k < 2; k++) { uint32_t d = r->cp_dst[k] & 0x3ff; memcpy(VM + (d << 4), vmq(r->cp_src), 16); own[d] = 0; xl_vm_wgen++;
    if (d < 64) lo_touch(d, 0); }   /* sb10 T2 */
  int dctx = 3;   /* the contexts the call may draw in: the template's, else both */
  if (r) {   /* the template's PRIM, as the geometry tag sets it (PRE) */
    c.tag_qa = (r->tag + (r->tag_abs ? 0 : c.top)) & 0x3ff;
    const uint32_t *t = vmq(c.tag_qa);
    if (t[1] >> 14 & 1) { gs_write(0x00, t[1] >> 15 & 0x7ff); gf_reg(&F, 0x00, t[1] >> 15 & 0x7ff);
      uint64_t at = (F.gr[0x1a] & 1) ? F.gr[0x00] : F.gr[0x1b]; dctx = 1 << (int)(at >> 9 & 1); }
  }
  gf_unknown(&F, GF_U_ALL);   /* the program kicks its own tags and vertices: PRIM, the vertex queue and the vertex fields */
  if (rec && rec->lane == 1 && c.entry == XL_DF0) {   /* sb1 lane: what vc_df0 reads besides the streams (its matrix, the header, fog, alpha, viewport) */
    for (uint32_t k = 0; k < 4; k++) vmq(c.top + k);
    vmq(c.top + 0x14); for (uint32_t k = 2; k < 6; k++) vmq(k);
    c.rec = rec;
  } else if (rec && rec->lane == 2 && (c.entry == 0x19fe08u || c.entry == 0x1a0160u || c.entry == 0x1a0540u || c.entry == 0x1a08c0u)) {
    /* sb2 lane 2: what P06 reads besides the streams (vu1conv.c p06_main, xconv.c xc_lit): T + 0 .. 0x14 (the matrix, lights, colours,
       skin, pass 3's matrix, the header), T + 0xfe, 0xff (pass 3's scale and offset), abs 2 .. 5 (fog, alpha, viewport) */
    for (uint32_t k = 0; k <= 0x14; k++) vmq(c.top + k);
    vmq(c.top + 0xfe); vmq(c.top + 0xff); for (uint32_t k = 2; k < 6; k++) vmq(k);
    c.rec = rec;
  } else if (rec && rec->lane == 4 && c.entry == 0x1a31e0u) {
    /* sb5 lane 4: what P07 reads besides the streams (vu1conv.c vc_p07_rec, vc_xform; xconv.c xc_lit): T + 0 .. 3 (the header, the cull
       word, the GIF tags), abs 2 .. 11 (fog, the clamp and alpha, the viewport, V), abs 16 .. 19 (pass 3's matrix), abs 24 .. 31 (the
       lights, their colours, the ambient), abs 32 .. 63 (the bones), T + 0xfe, 0xff (pass 3's scale and offset) */
    vm_dec(c.top, 4); vm_dec(2, 10); vm_dec(16, 4); vm_dec(24, 40); vm_dec(c.top + 0xfe, 2);
    c.rec = rec;
  } else if (rec && rec->lane == 3 && (c.entry == XL_FX || c.entry == XL_FX2)) {
    /* sb6 lane 3: what P04+25f0 / +2870 read besides the streams (vu1conv.c vc_fpos_rec, vc_xform; xconv.c xc_fpos): T + 0x10 .. 0x13 (the
       matrix), 0x14 (the header), 0x15 (the tag template, read above), abs 2 .. 5 (fog, alpha, viewport) */
    vm_dec(c.top + 0x10, 6); vm_dec(2, 4);
    c.rec = rec;
  }
  if (rec) c.lane = rec->lane;
  c.vm = VM; c.gr = gr;
  NS_MARK(NS_PRE);
  if (call_fn) ME_CALLP(call_fn)(call_ctx, &c);
  NS_MARK(NS_BACK);
  if (r && r->passes) passes(&c, r->passes);
  NS_PMARK(NS_P_PASS);
  gf_drew(&F, dctx);   /* after the callbacks: the call reads its texels before it draws */
  NS_MARK(NS_DREW);
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
    if (!vn) { vsrc0 = src; if (uvert) usrc = src; }
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
  vcmd = w; vn = 0; dqn = 0; uvert = 0; St.cmds[c]++; ME_BEAT();
  switch (c) {
    case 0x00: case 0x10: case 0x11: case 0x13: case 0x06: case 0x07: return;   /* NOP FLUSHE FLUSH FLUSHA MSKPATH3 MARK */
    case 0x01: V.cl = imm & 0xff; V.wl = imm >> 8 & 0xff; return;
    case 0x02: V.ofst = imm & 0x3ff; V.dbf = 0; V.tops = V.base; xl_vm_wgen++; return;
    case 0x03: V.base = imm & 0x3ff; xl_vm_wgen++; return;
    case 0x04: V.itops = imm & 0x3ff; return;
    case 0x05: V.mode = imm & 3; return;
    case 0x14: case 0x15: call(imm, 0, 0); return;
    case 0x17: call(0, 1, 0); return;
    case 0x20: vleft = 1; return;
    case 0x30: case 0x31: vleft = 4; return;
    case 0x4a: vleft = (num ? num : 256) * 2; return;
    case 0x50: case 0x51: vleft = (imm ? imm : 65536) * 4; return;
  }
  if ((c & 0x60) == 0x60) {
    int vn_ = (c >> 2) & 3, vl = c & 3, n = num ? num : 256, bits = 32 >> vl, ne = vn_ + 1; if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; }
    if (V.wl > V.cl) { n = (n / V.wl) * V.cl + (n % V.wl < V.cl ? n % V.wl : V.cl); St.fills++; }
    uvert = ((((imm >> 15 & 1) ? V.tops : 0) + (imm & 0x3ff) - V.tops - 0x100) & 0x3ff) == 0;   /* task #114: to T + 0x100 */
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
    if (!vn) { vsrc0 = s; if (uvert) usrc = s; }
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
/* sb1, Stage B lane 1 (xlate.h) */
int xl_lane_p04(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp)
{
  int nr = 4 + q0; const uint32_t *cn = t + 4 * nr;
  if (vleft || V.mode >= 2) return 0;
  { int cl = V.cl, wl = V.wl, ok; V.cl = 3; V.wl = 1;   /* the streams' UNPACKs as xl_vif1_span would test them (the V4-32 ones always wait) */
    ok = ud_ok(t[4 * (nr - 3) + 3]) && ud_ok(t[4 * (nr - 2) + 3]) && ud_ok(t[4 * (nr - 1) + 3]); V.cl = cl; V.wl = wl; if (!ok) return 0; }
  static XlRec R H39; int ok = 1; uint32_t n = t[4 * nr + 4] & 0xffff;   /* the header's count */
  for (int j = 0; j < nr; j++) {   /* each REF: its TTE words (STCYCL, the UNPACK), then its data, all waiting */
    const uint32_t *g = t + 4 * j; uint32_t w = g[3], nw = 4 * (g[0] & 0xffff); int c = (int)(w >> 24 & 0x7f), s = j - q0 - 1;
    St.words += 2 + nw; St.cmds[1]++; V.cl = (int)(g[2] & 0xff); V.wl = (int)(g[2] >> 8 & 0xff); St.cmds[c]++; XC(St.wd[c] += nw);
    up_census(w); if (s < 0) { ud_eager(w, dp[j], nw); continue; }   /* sb8: the block and the material at once */
    ud_new(w, dp[j], nw);
    { const Ud *d = &UD[ud_n - 1]; int k = ud_kind(d);   /* a stream: A, B (V4-16), C (V2-16 masked) */
      R.p[s] = d->p; R.num[s] = d->num; R.kind[s] = (uint8_t)k;
      if (n > d->num || (s < 2 ? k != 2 && k != 3 : k != 4)) ok = 0; }
  }
  St.words += 10; St.cmds[1]++; V.cl = V.wl = 1; St.cmds[0x6c]++; XC(St.wd[0x6c] += 4);   /* the CNT: STCYCL, the header's UNPACK and data */
  up_census(cn[3]); ud_eager(cn[3], cn + 4, 4);   /* sb8: the header at once */
  vsrc0 = tadr + 16 * (uint32_t)nr + 16; usrc = 0;
  memcpy(R.row, V.row, 16);   /* ROW as the descriptors copied it (it changes only with STROW or a running ROW, which copy again) */
  R.lane = 1;
  vcmd = cn[8]; vn = 0; dqn = 0; St.cmds[0x14]++;
  call(cn[8] & 0xffff, 0, ok ? &R : 0);
  St.cmds[0x00] += 3; vcmd = 0;   /* the three NOPs */
  xl_lane_groups[1]++;
  return 1;
}
/* native step 1 (native_renderer_design.md 6.1; xlate.h XlP04): lane 1's core with the group's words and pointers as arguments, for a
   group that comes from a record (ge/natp04.h) and not from a packet.  xl_lane_p04 above is left as it is (its profile and code are
   gl29's): this is the same body reading g instead of the packet's quadwords */
int xl_p04_core(const XlP04 *g)
{
  int q0 = g->q0, nr = 4 + q0;
  NS_MARK(NS_PTR);
  if (vleft || V.mode >= 2) return 0;
  { int cl = V.cl, wl = V.wl, ok; V.cl = 3; V.wl = 1;
    ok = ud_ok(g->unp[nr - 3]) && ud_ok(g->unp[nr - 2]) && ud_ok(g->unp[nr - 1]); V.cl = cl; V.wl = wl; if (!ok) return 0; }
  static XlRec R H39; int ok = 1; uint32_t n = g->hdr[0] & 0xffff;   /* the header's count */
  for (int j = 0; j < nr; j++) {   /* each REF: its TTE words (STCYCL, the UNPACK), then its data, all waiting */
    uint32_t w = g->unp[j], nw = 4 * g->qwc[j]; int c = (int)(w >> 24 & 0x7f), s = j - q0 - 1;
    St.words += 2 + nw; St.cmds[1]++; V.cl = (int)(g->stc[j] & 0xff); V.wl = (int)(g->stc[j] >> 8 & 0xff); St.cmds[c]++; XC(St.wd[c] += nw);
    up_census(w); if (s < 0) { ud_eager(w, g->dp[j], nw); continue; }
    ud_new(w, g->dp[j], nw);
    { const Ud *d = &UD[ud_n - 1]; int k = ud_kind(d);
      R.p[s] = d->p; R.num[s] = d->num; R.kind[s] = (uint8_t)k;
      if (n > d->num || (s < 2 ? k != 2 && k != 3 : k != 4)) ok = 0; }
  }
  St.words += 10; St.cmds[1]++; V.cl = V.wl = 1; St.cmds[0x6c]++; XC(St.wd[0x6c] += 4);
  up_census(g->hunp); ud_eager(g->hunp, g->hdr, 4);
  vsrc0 = g->tadr + 16 * (uint32_t)nr + 16; usrc = 0;
  memcpy(R.row, V.row, 16);
  R.lane = 1;
  vcmd = g->mscal; vn = 0; dqn = 0; St.cmds[0x14]++;
  NS_MARK(NS_REG);
  call(g->mscal & 0xffff, 0, ok ? &R : 0);
  St.cmds[0x00] += 3; vcmd = 0;
  xl_lane_groups[1]++;
  return 1;
}
/* gl34 (xlate.h): xl_p04_core and call() for the one form, each step in their order.  What the general code does and this leaves out
   changes no state: tests that the form answers, the search for the entry's rule, the descriptor's kind read back, the A+D packet
   read back from the VU memory model (it was just copied there: the same bytes at g->dp), the GIF's general forms, the GS front end's
   vertex cases.  A register that needs one of those (PRIM .. XYZ3, FOG, TEX2, TRXDIR, HWREG) goes to gf_reg and gs_write as always */
long xl_nf_n[8] H39;
long xl_nk_n[16];
static const uint32_t *nf_blk[2]; static uint32_t nf_gen[2], nf_top[2];   /* by V.dbf: the block last copied to that buffer by a fast group */
static inline __attribute__((always_inline)) void nf_put(uint32_t a, const uint32_t *p, uint32_t num)   /* num quadwords to a, VM owns them */
{
  const ud_w32 *sp = (const ud_w32 *)p; ud_w32 *q = (ud_w32 *)(VM + (a << 4)), *e = q + 4 * num;
  for (; q < e; q += 4, sp += 4) { uint32_t x0 = sp[0], x1 = sp[1], x2 = sp[2], x3 = sp[3]; q[0] = x0; q[1] = x1; q[2] = x2; q[3] = x3; }
  own_fill_i(a, num, 0);
}
static inline __attribute__((always_inline)) void nf_copy(uint32_t a, const uint32_t *p, uint32_t num)   /* ud_eager's copy: num quadwords to a, VM owns them */
{
  nf_put(a, p, num);
  XC(St.eg_cmds++); XC(St.eg_vec += num);
}
/* gl36 (xlate.h xl_nr_*): the reuse's table and the run being walked.  nr_lw[q]: the group (j + 1) of the run being recorded that wrote
   VU quadword q last, for the quadwords a replay may skip (cleared at the run's end); nr_tv: the run's T values (its groups' VU buffers) */
static NrEnt *nr_t; int xl_nr_cap, xl_nr_ok = 1; static int nr_chk; long xl_nr_n[20];
static int nr_mode, nr_n, nr_i0, nr_j = -1, nr_bad; static long nr_nf0, nr_nfl; static const NatGrp *nr_g; static uint32_t nr_vbg, nr_tv[4]; static int nr_tn;
static uint16_t *nr_lw;   /* gl36 fix B: XL_NR_LW_N entries after the table's cap entries, in the host's block (not this file's bss) */
#ifndef __psp__
long xl_nr_why[16];   /* x86: why a run was not replayed: 0 buffers out of range, 1 length, 2 buffers, 3 entry, 4 head or not fast, 5 + k word k */
/* gl37 dev (x86): MGS2_NAT_RDUMP=<file>: a line per picture ("P vb") and per run that reaches the table's test ("R i0 n vbg df0 fast hash
   mode taken bad tn"), for the rule's study (build_gl37/nrsim.py).  Reads only */
extern long gl_rp_vb;
static FILE *nr_df; static int nr_dset, nr_dpend; static unsigned long long nr_dh; static uint32_t nr_dvbg; static int nr_ddf, nr_dfa;
static void nr_dopen(void) { if (!nr_dset) { const char *e = getenv("MGS2_NAT_RDUMP"); nr_dset = 1; if (e && *e) nr_df = fopen(e, "w"); } }
#endif
typedef char nr_size_check[sizeof(NrEnt) == 64 ? 1 : -1];
void xl_nr_table(NrEnt *t, int cap, int chk) { nr_t = t; xl_nr_cap = t ? cap : 0; nr_chk = chk; nr_lw = t ? (uint16_t *)(void *)(t + cap) : 0; }
static inline void nr_words(const NatGrp *g, uint32_t *w)
{
  w[0] = g->mat; w[1] = g->s[0]; w[2] = g->s[1]; w[3] = g->s[2];
  w[4] = (uint32_t)g->n[0] | (uint32_t)g->n[1] << 8 | (uint32_t)g->n[2] << 16 | (uint32_t)(g->flags & 0xf) << 24;
  w[5] = g->hdr[0]; w[6] = g->hdr[1]; w[7] = g->hdr[2]; w[8] = g->hdr[3]; w[9] = g->mscal;
}
/* gl37, the reuse only where it pays (attic/gl37/report.txt; the rule chosen on the twin's runs, build_gl37/nrsim.py): a run that does not
   match its kept entries is recorded only while each of the last two pictures had at least half of its tried groups in runs that replayed
   or were the same as in the picture before (a shadow: the run's length, buffers and a hash of its records' words kept at its head entry,
   which is then no head: hd 0xffff, rn 0, so no replay can use it).  Otherwise the run is fed plain and its shadow written.  Replays are
   the same as gl36's: the rule only chooses between recording a run and not.  xl_nr_rule 0: gl36 (every run that does not match is
   recorded).  xl_nr_r: [0] runs not recorded (shadows written), [1] their groups, [2] runs the same as their shadow, [3] their groups,
   [4] pictures with recording on, [5] pictures with recording off, [6] groups tried, [7] groups replayed or the same */
int xl_nr_rule = 1; long xl_nr_r[8] H39;
static int nr_pa, nr_pg, nr_ok0 = 1, nr_ok1 = 1, nr_rec = 1;
static void nr_sig(const NatGrp *g, int n, uint32_t *h0, uint32_t *h1)
{
  uint32_t a = 0x811c9dc5u, b = 0x9e3779b9u;
  for (int j = 0; j < n; j++) { uint32_t w[10]; nr_words(&g[j], w);
    for (int k = 0; k < 10; k++) { a = (a ^ w[k]) * 0x01000193u; b = (b + w[k]) * 0x85ebca6bu; b ^= b >> 13; } }
  *h0 = a; *h1 = b;
}
int xl_nr_begin(const void *grp, int n, int i0)
{
  const NatGrp *g = (const NatGrp *)grp;
  nr_mode = 0; nr_j = -1;
  if (!nr_t || n <= 0 || !xl_nr_ok) return 0;
  if (i0 < 0 || i0 + n > xl_nr_cap) { xl_nr_n[14]++; return 0; }
  NrEnt *e = nr_t + i0;
  uint32_t vbg = ~0u;
  if (V.tops < 1024 && V.base < 1024 && V.ofst < 1024) vbg = (uint32_t)V.tops | (uint32_t)V.base << 10 | (uint32_t)V.ofst << 20 | (uint32_t)(V.dbf & 1) << 30;
#ifndef __psp__
  nr_dopen();
  if (nr_df) { unsigned long long h = 1469598103934665603ull; int fa = 1;
    for (int j = 0; j < n; j++) { uint32_t w[10]; nr_words(&g[j], w); for (int k = 0; k < 10; k++) { h ^= w[k]; h *= 1099511628211ull; }
      if (!(g[j].flags & NATG_FAST)) fa = 0; }
    nr_dh = h; nr_dvbg = vbg; nr_ddf = VPsrc[0x1be] == XL_DF0; nr_dfa = fa; nr_dpend = 1; }
#endif
  int ok = vbg != ~0u && e->rn == n && e->vbg == vbg && VPsrc[0x1be] == XL_DF0;   /* (the fast path's entry, as when recorded) */
#ifndef __psp__
  int why = vbg == ~0u ? 0 : e->rn != n ? 1 : e->vbg != vbg ? 2 : VPsrc[0x1be] != XL_DF0 ? 3 : -1;
#endif
  for (int j = 0; j < n && ok; j++) {
    uint32_t w[10]; const NrEnt *f = e + j; nr_words(&g[j], w);
    if (f->hd != (uint16_t)i0 || !(g[j].flags & NATG_FAST)) ok = 0;
    else for (int k = 0; k < 10; k++) if (w[k] != f->w[k]) { ok = 0;
#ifndef __psp__
      why = 5 + k;
#endif
      break; }
#ifndef __psp__
    if (!ok && why < 0) why = 4;
#endif
  }
#ifndef __psp__
  if (!ok) xl_nr_why[why < 0 ? 15 : why]++;
#endif
  nr_n = n; nr_i0 = i0; nr_vbg = vbg; nr_nf0 = xl_nf_n[0]; nr_tn = 0; nr_bad = 0; nr_g = g;
  nr_pa += n; xl_nr_r[6] += n;
  if (ok) { nr_pg += n; xl_nr_r[7] += n; }
  else if (xl_nr_rule) {   /* gl37: the run against its shadow; recorded only while the last two pictures replayed well */
    const int sh = (e->bits & NR_SH) && e->hd == 0xffffu; uint32_t h0 = 0, h1 = 0;
    if (sh || !nr_rec) {
      nr_sig(g, n, &h0, &h1);
      if (sh && e->w[0] == (uint32_t)n && e->w[1] == vbg && e->w[2] == h0 && e->w[3] == h1) { nr_pg += n; xl_nr_r[2]++; xl_nr_r[3] += n; xl_nr_r[7] += n; }
    }
    if (!nr_rec) {   /* not recorded: the shadow at the head (no longer a head: no replay can take it) */
      e->hd = 0xffffu; e->rn = 0; e->bits = NR_SH; e->w[0] = (uint32_t)n; e->w[1] = vbg; e->w[2] = h0; e->w[3] = h1;
      xl_nr_r[0]++; xl_nr_r[1] += n; nr_mode = 0; return 0;
    }
  }
  if (ok && !nr_chk) { nr_mode = 1; xl_nr_n[0]++; xl_nr_n[1] += n; return 1; }
  nr_mode = ok ? 3 : 2;
  if (ok) { xl_nr_n[6]++; xl_nr_n[7] += n; } else { xl_nr_n[3]++; xl_nr_n[4] += n; }
  for (int j = 0; j < n; j++) { NrEnt *f = e + j; uint8_t b = f->bits; f->bits = ok ? (uint8_t)((b & NR_S ? NR_OS : 0) | (b & NR_BOX ? NR_OB : 0)) : 0; }
  return nr_mode;
}
int xl_f1_j, xl_f1_chk; long xl_f1_n[12];   /* gl38 F1 (xlate.h xl_f1_*) */
static int f1_on, f1_n, f1_bad; static uint32_t f1_T[2]; static uint8_t f1_s[(NAT_MAXGRP + 7) / 8];
static uint8_t f1_d[(NAT_MAXGRP + 7) / 8], f1_s2[(NAT_MAXGRP + 7) / 8]; static const NatGrp *f1_g;   /* (the check mode's) */
static void f1_plan(const NatGrp *g, int n, const uint8_t *xb, uint8_t *f1_s);
/* a recorded run's segment ends: each quadword's last writer so far is in S (NR_S2) and the notes are cleared */
static void nr_seg(void)
{
  NrEnt *e = nr_t + nr_i0; const uint32_t n = (uint32_t)nr_n;
  for (int t = 0; t < nr_tn; t++) {
    const uint32_t T = nr_tv[t];
    for (uint32_t q = T + 4; q < T + 0x1e0; q++) { const uint16_t l = nr_lw[q]; if (l) { if (l <= n) e[l - 1].bits |= NR_S2; nr_lw[q] = 0; } }
  }
}
/* the group before (nr_j) in a recorded run did not take the fast path: xl_p04_core (or the word-by-word feed) wrote all it writes,
   and a converter of another entry may read any quadword: a barrier.  It is in S (never skips), and the segment before it ends there,
   so every quadword it may read holds its last writer's value, which writes in full */
static void nr_after(void)
{
  if (nr_mode >= 2 && nr_j >= 0 && xl_nf_n[0] == nr_nfl) { nr_t[nr_i0 + nr_j].bits |= NR_S2; nr_seg(); xl_nr_n[19]++;
    if (f1_on == 2 && nr_j < f1_n) f1_d[nr_j >> 3] |= (uint8_t)(1u << (nr_j & 7)); }   /* gl38 F1's check: the group did not take the fast path */
}
void xl_nr_group(int j) { nr_after(); nr_j = j; nr_nfl = xl_nf_n[0]; }
void xl_nr_pic(void)   /* gl37: a picture starts (ge/gelive.c gl_begin): the rule's state from the picture before, if it tried a run */
{
#ifndef __psp__
  nr_dopen(); if (nr_df) fprintf(nr_df, "P %ld\n", gl_rp_vb);
#endif
  if (nr_pa) { nr_ok1 = nr_ok0; nr_ok0 = 2 * nr_pg >= nr_pa; nr_rec = nr_ok0 && nr_ok1; xl_nr_r[nr_rec ? 4 : 5]++; nr_pa = nr_pg = 0; }
}
void xl_nr_end(int taken)
{
#ifndef __psp__
  if (nr_dpend) { nr_dpend = 0; fprintf(nr_df, "R %d %d %08x %d %d %016llx %d %d %d %d\n", nr_i0, nr_n, nr_dvbg, nr_ddf, nr_dfa, nr_dh, nr_mode, taken, nr_bad, nr_tn); }
#endif
  if (!nr_mode) return;
  nr_after();
  NrEnt *e = nr_t + nr_i0; const int n = nr_n;
  if (nr_mode == 1) {
    if (taken != n) xl_nr_n[15]++;
    if (xl_nf_n[0] - nr_nf0 != n) xl_nr_n[17]++;   /* (a group the fast path declined: xl_p04_core wrote all it writes; exact) */
    nr_mode = 0; nr_j = -1; return;
  }
  int kept = taken == n && !nr_bad && nr_vbg != ~0u && nr_tn > 0;
#ifndef __psp__
  { static int dn = -1; if (dn < 0) { const char *ev = getenv("MGS2_NAT_RDBG"); dn = ev ? atoi(ev) : 0; }
    if (!kept && dn > 0) { dn--; fprintf(stderr, "nrdbg: n %d i0 %d taken %d fast %ld bad %d vbg %08x tn %d\n", n, nr_i0, taken, xl_nf_n[0] - nr_nf0, nr_bad, nr_vbg, nr_tn); } }
#endif
  nr_seg();   /* the S set from the notes of the last segment, which are cleared */
  /* gl38 F1's check: the plan's S against the notes'.  A group that the plan took for a fast one and that declined when it ran (the
     tables full: xl_nf_n[6], [7]) is a barrier in the notes and not in the plan.  The plan stays exact there (the declined group's
     xl_p04_core writes all the group's quadwords, and its converter reads the header, the matrix and the tag template, which a
     skipping group writes too: gl36's replays with a decline, xl_nr_n[17]), so the check plans the run again with those groups as
     barriers and compares that; the groups the first plan skips and the second does not are counted apart ([11]) */
  const uint8_t *f1p = f1_s;
  if (f1_on == 2 && f1_n == n) { int any = 0;
    for (int j = 0; j < (n + 7) >> 3; j++) any |= f1_d[j];
    if (any) { f1_plan(f1_g, n, f1_d, f1_s2); f1p = f1_s2;
      for (int j = 0; j < n; j++) if ((f1_s2[j >> 3] >> (j & 7) & 1) && !(f1_s[j >> 3] >> (j & 7) & 1)) xl_f1_n[11]++; } }
  for (int j = 0; j < n; j++) {
    NrEnt *f = e + j; const uint8_t b = f->bits; const int s2 = (b & NR_S2) != 0;
    if (nr_mode == 3) { if (((b & NR_OS) != 0) != s2) xl_nr_n[8]++; if ((b & NR_OB) && !(b & NR_BOX)) xl_nr_n[9]++; }
    if (f1_on == 2 && f1_n == n) { const int ps = f1p[j >> 3] >> (j & 7) & 1;
      xl_f1_n[5]++; if (!ps && s2) xl_f1_n[6]++; else if (ps && !s2) xl_f1_n[7]++; }
    if (s2) xl_nr_n[2]++;
    f->bits = (uint8_t)((s2 ? NR_S : 0) | (b & NR_BOX));
    if (kept) { nr_words(&nr_g[j], f->w); f->hd = (uint16_t)nr_i0; } else f->hd = 0xffffu;
  }
  if (kept) { e->rn = (uint16_t)n; e->vbg = nr_vbg; xl_nr_n[5]++; } else { e->rn = 0; xl_nr_n[13]++; }
  nr_mode = 0; nr_j = -1;
}
/* gl38 F1 (xlate.h xl_f1_*) */
int xl_f1_begin(const void *grp, int n)
{
  const NatGrp *g = (const NatGrp *)grp;
  const uint32_t T0 = V.tops, T1 = V.dbf ? V.base : V.base + V.ofst, T2 = V.dbf ? V.base + V.ofst : V.base;
  f1_on = 0;
  if (n < 2 || n > NAT_MAXGRP || !xl_nr_ok) return 0;   /* (a run of one group has nothing to skip) */
  if (vleft || V.mode || (V.mask & 0xff) != 0x10 || !ud_eager_on || !ud_mark_on || VPsrc[0x1be] != XL_DF0 || T0 != T2 || T0 < 64 || T1 < 64 ||
      T0 > 1024 - 0x1e0 || T1 > 1024 - 0x1e0 || (T0 < T1 ? T1 - T0 : T0 - T1) < 0x1e0) { xl_f1_n[3]++; xl_f1_n[8]++; return 0; }
  for (int j = 0; j < n; j++) if (g[j].mscal >> 24 != 0x14) { xl_f1_n[3]++; xl_f1_n[9]++; return 0; }   /* every group ends with an MSCAL: the buffers alternate */
  f1_plan(g, n, 0, f1_s);
  if (xl_f1_chk) { f1_g = g; for (int j = 0; j < (n + 7) >> 3; j++) f1_d[j] = 0; }
  f1_T[0] = T0; f1_T[1] = T1; f1_n = n; f1_bad = 0; f1_on = xl_f1_chk ? 2 : 1; xl_f1_n[0]++; xl_f1_n[1] += n;
  return f1_on;
}
/* the plan: S (bits of s) by a backward pass over the run's records; xb (the check mode): more barriers, by group */
static void f1_plan(const NatGrp *g, int n, const uint8_t *xb, uint8_t *f1_s)
{
  int lq0[2] = { 0, 0 }, lany[2] = { 0, 0 }; uint32_t mx[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
  for (int j = 0; j < (n + 7) >> 3; j++) f1_s[j] = 0;
  for (int j = n - 1; j >= 0; j--) { const NatGrp *q = &g[j]; const int p = j & 1, q0 = (q->flags & NATG_FULL) != 0;
    const uint32_t nm[3] = { 2u * q->n[0], 2u * q->n[1], 4u * q->n[2] }, h = q->hdr[0] & 0xffff;
    /* the fast form, by the record.  A group without it takes xl_p04_core, and the converter of another entry may read any quadword:
       a barrier as the notes' (nr_after): it is in S and the segment before it ends there, so the groups before it are planned
       against each other only */
    if ((xb && (xb[j >> 3] >> (j & 7) & 1)) ||
        q->mscal != 0x140001beu || !(q->flags & NATG_FAST) || !q->s[0] || !q->s[1] || !q->s[2] || !nm[0] || !nm[1] || !nm[2] || h > nm[0] ||
        h > nm[1] || h > nm[2] || 0x100 + 3 * (nm[0] - 1) >= 0x1e0 || 0x101 + 3 * (nm[1] - 1) >= 0x1e0 || 0x102 + 3 * (nm[2] - 1) >= 0x1e0) {
      f1_s[j >> 3] |= (uint8_t)(1u << (j & 7)); if (!xb) xl_f1_n[10]++;
      for (int k = 0; k < 2; k++) { lq0[k] = lany[k] = 0; mx[k][0] = mx[k][1] = mx[k][2] = 0; }
      continue; }
    const int s = (q0 && !lq0[p]) || !lany[p] || nm[0] > mx[p][0] || nm[1] > mx[p][1] || nm[2] > mx[p][2];
    if (s) f1_s[j >> 3] |= (uint8_t)(1u << (j & 7));
    lany[p] = 1; if (q0) lq0[p] = 1;
    for (int k = 0; k < 3; k++) if (nm[k] > mx[p][k]) mx[p][k] = nm[k]; }
}
void xl_f1_end(int ok)
{
  if (!f1_on) return;
  if (!ok || f1_bad) xl_f1_n[4]++;
  f1_on = 0;
}
int xl_p04_fast(const XlP04 *g)
{
  const int q0 = g->q0, nr = 4 + q0; const uint32_t T = V.tops, To = V.dbf ? V.base : V.base + V.ofst, n = g->hdr[0] & 0xffff;
  const uint32_t *const mat = g->dp[q0]; uint32_t num[3], nl, nreg;
  NS_MARK(NS_PTR);
  /* the form */
  if (vleft || V.mode || (V.mask & 0xff) != 0x10 || !ud_eager_on || !ud_mark_on) { xl_nf_n[1]++; return 0; }
  if (g->mscal != 0x140001beu || VPsrc[0x1be] != XL_DF0) { xl_nf_n[2]++; return 0; }
  if (T < 64 || To < 64 || T + 0x1e0 > 1024 || To + 0x1e0 > 1024 || (T < To ? To - T : T - To) < 0x1e0) { xl_nf_n[3]++; return 0; }
  num[0] = 2 * g->qwc[q0 + 1]; num[1] = 2 * g->qwc[q0 + 2]; num[2] = 4 * g->qwc[q0 + 3];
  if (n > num[0] || n > num[1] || n > num[2] || !num[0] || !num[1] || !num[2] ||
      0x100 + 3 * (num[0] - 1) >= 0x1e0 || 0x101 + 3 * (num[1] - 1) >= 0x1e0 || 0x102 + 3 * (num[2] - 1) >= 0x1e0) { xl_nf_n[4]++; return 0; }
  /* the material's tag: PACKED, EOP, no PRE, 1 .. 7 registers, each A+D (0xe) or a NOP (0xf), NLOOP x NREG <= 7 */
  nreg = mat[1] >> 28; nl = (mat[0] & 0x7fff) * nreg;
  if (nl < 1 || nl > 7 || (mat[0] & 0x8000u) == 0 || (mat[1] & 0x0c004000u) != 0) { xl_nf_n[5]++; return 0; }
  for (uint32_t k = 0; k < nreg; k++) if ((mat[2] >> (4 * k) & 0xe) != 0xe) { xl_nf_n[5]++; return 0; }
  if (ud_n + 3 > UD_N || (rc_dirty && rc_n == RC_N)) { xl_nf_n[6]++; return 0; }
  if (G[1].nloop) { xl_nf_n[7]++; return 0; }
  xl_nf_n[0]++;
  static XlRec R H39;
  const int kp = g->keep; xl_keep_bits = kp;   /* gl35 */
  /* gl36 (xl_nr_*): the group's reuse bits in a replayed run (XR_SKIP outside the S set, XR_BOX with a kept box), or its notes in a
     recorded or checked one */
  int ru = 0; NrEnt *ne = 0;
  if (nr_mode && (unsigned)nr_j < (unsigned)nr_n) {
    ne = nr_t + nr_i0 + nr_j;
    if (nr_mode == 1) ru = (ne->bits & NR_S ? 0 : XR_SKIP) | (ne->bits & NR_BOX ? XR_BOX : 0);
    else {
      const uint16_t lj = (uint16_t)(nr_j + 1); int t = 0;
      ru = XR_REC | (nr_mode == 3 ? XR_CHK : 0);
      while (t < nr_tn && nr_tv[t] != T) t++;
      if (t == nr_tn) { if (nr_tn < 4) nr_tv[nr_tn++] = T; else nr_bad = 1; }
      if (q0) { for (uint32_t q = T + 4; q < T + 0x14; q++) nr_lw[q] = lj; nr_lw[T + 0x16] = lj; nr_lw[T + 0x17] = lj; }
      for (uint32_t q = T + 0xe8; q < T + 0xf0; q++) nr_lw[q] = lj;
      for (uint32_t s = 0; s < 3; s++) for (uint32_t k = 0, q = T + 0x100 + s; k < num[s]; k++, q += 3) nr_lw[q] = lj;
    }
  }
  int f1s = 0;   /* gl38 F1: a group outside the plan's S skips as a replay's does */
  if (f1_on) { const unsigned j = (unsigned)xl_f1_j;
    if (j >= (unsigned)f1_n || T != f1_T[j & 1]) f1_bad = 1;
    else if (f1_on == 1 && !(ru & XR_SKIP) && !(f1_s[j >> 3] >> (j & 7) & 1)) { ru |= XR_SKIP; f1s = 1; } }
  R.ru = (uint8_t)ru; R.ne = ne;
  /* the REFs: STCYCL and the UNPACK (the block and the material copied, the streams' descriptors), as xl_p04_core counts them */
  St.words += 2u * (uint32_t)nr + 10u + (q0 ? 96u : 0u) + 32u + 4u * (g->qwc[q0 + 1] + g->qwc[q0 + 2] + g->qwc[q0 + 3]);
  St.cmds[1] += nr + 1; St.cmds[0x6c] += 2 + q0; St.cmds[0x6d] += 2; St.cmds[0x75]++; St.cmds[0x14]++;
#ifdef XL_CENSUS
  for (int j = 0; j < nr; j++) { V.cl = (int)(g->stc[j] & 0xff); V.wl = (int)(g->stc[j] >> 8 & 0xff); St.wd[g->unp[j] >> 24 & 0x7f] += 4 * g->qwc[j]; up_census(g->unp[j]); }
  V.cl = V.wl = 1; St.wd[0x6c] += 4; up_census(g->hunp);
#endif
  if (ru & XR_SKIP) {   /* gl36: a replayed run's group outside S: the matrix and the tag template only (the call reads them), the rest
                           skipped (a later group of the run writes each of those quadwords again); the counts as today */
    if (q0) { nf_put(T, g->dp[0], 4); nf_put(T + 0x15, g->dp[0] + 4 * 0x15, 1); XC(St.eg_cmds++); XC(St.eg_vec += 24);
      nf_blk[V.dbf & 1] = g->dp[0]; nf_gen[V.dbf & 1] = xl_vm_wgen; nf_top[V.dbf & 1] = T; }
    XC(St.eg_cmds++); XC(St.eg_vec += 8);
    if (rc_dirty) { memcpy(RC[rc_n], V.row, 16); memcpy(RC[rc_n] + 4, V.col, 16); rc_n++; rc_dirty = 0; }
    for (uint32_t s = 0; s < 3; s++) {
      XC(St.lz_cmds++); XC(St.lz_vec += num[s]);
      R.p[s] = g->dp[q0 + 1 + s]; R.num[s] = (uint16_t)num[s]; R.kind[s] = (uint8_t)(s == 2 ? 4 : 2);
    }
    if (f1s) xl_f1_n[2]++; else xl_nr_n[18]++;
  } else {
  if (q0) { nf_copy(T, g->dp[0], 24); nf_blk[V.dbf & 1] = g->dp[0]; nf_gen[V.dbf & 1] = xl_vm_wgen; nf_top[V.dbf & 1] = T; }
  nf_copy(T + 0xe8, mat, 8);
  if (rc_dirty) { memcpy(RC[rc_n], V.row, 16); memcpy(RC[rc_n] + 4, V.col, 16); rc_n++; rc_dirty = 0; }
  for (uint32_t s = 0; s < 3; s++) {   /* ud_new for CL 3, WL 1, MODE 0, not wrapping, above the constants */
    Ud *d = &UD[ud_n++]; const uint16_t id = (uint16_t)ud_n; const uint32_t a = T + 0x100 + s; uint16_t *q = own + a; int k = (int)num[s];
    d->p = g->dp[q0 + 1 + s]; d->addr = (uint16_t)a; d->num = (uint16_t)num[s]; d->nw = (uint16_t)(4 * g->qwc[q0 + 1 + s]); d->rc = (uint16_t)(rc_n - 1);
    d->fmt = (uint8_t)(s == 2 ? 0x15 : 0x0d); d->md = 0; d->cl = 3; d->wl = 1; d->mask = V.mask;
    XC(St.lz_cmds++); XC(St.lz_vec += num[s]); ud_vecs += num[s];
    for (; k >= 4; k -= 4, q += 12) { q[0] = id; q[3] = id; q[6] = id; q[9] = id; }
    for (; k > 0; k--, q += 3) *q = id;
    R.p[s] = d->p; R.num[s] = d->num; R.kind[s] = (uint8_t)(s == 2 ? 4 : 2);
  }
  }
  V.cl = V.wl = 1;
  nf_copy(T + 0x14, g->hdr, 1);   /* the CNT: the header */
  vsrc0 = g->tadr + 16 * (uint32_t)nr + 16; usrc = 0;
  memcpy(R.row, V.row, 16); R.lane = 1; R.nf = 1;
  R.xgen = xl_vm_wgen; R.xsrc = nf_gen[V.dbf & 1] == xl_vm_wgen && nf_top[V.dbf & 1] == T ? nf_blk[V.dbf & 1] : 0;
  vcmd = g->mscal; vn = 0; dqn = 0;
  NS_MARK(NS_REG);
  /* call(0x1be, 0, &R) */
  XlCall c;
  c.pc = 0x1be; c.cont = 0; c.top = T; c.itop = V.itops; c.src = 0;
  V.tops = To; V.dbf = !V.dbf;
  c.entry = last_entry = XL_DF0;
  St.calls++; last_cont = 0;
  { const XlRule *r; int i = ent_index(XL_DF0, &r); if (i < 64) St.ent_n[i]++; XC(if (i < 64) St.ent_uq[i] += St.uq_open; St.uq_open = 0); }
  c.ad_qa = (0xe8 + T) & 0x3ff; c.ad = 1; St.ad_kicks++;
  { Gif *gp = &G[1]; uint32_t ri = 0;   /* kick_vm(T + 0xe8): the tag, then nl quadwords (A+D or NOP), to the EOP */
    F.path = 1; XC(St.gq[1] += 1 + nl);
    gp->eop = 1; gp->flg = 0; gp->nreg = (int)nreg; gp->regs = mat[2] | (uint64_t)mat[3] << 32; gp->ri = 0; gp->nloop = 0;
    St.gif_tags[1]++; ME_BEAT();
    for (uint32_t k = 1; k <= nl; k++) {
      const uint32_t *q = mat + 4 * k; const uint32_t d = mat[2] >> (4 * ri) & 0xf, r = q[2] & 0xff; const uint64_t v = q[0] | (uint64_t)q[1] << 32;
      if (++ri == nreg) ri = 0;
      if (d != 0xe) continue;   /* a NOP */
      if (!(r < 0x18 ? 0x00f003c1u >> r & 1 : r == 0x42 || r == 0x43 || r == 0x7f)) xl_gs_gen++;   /* gl35: not PRIM, TEX0, CLAMP, TEX1, TEX2, ALPHA */
      if (!r && (kp & 2)) {   /* gl35 K2: gf_reg's case 0 and gs_write's store */
        F.gr[0] = v; F.vqn = 0; F.dirty = 1; F.unk &= ~(GF_U_PRIM | GF_U_QUEUE); gr[0] = v; xl_nk_n[8]++;
      } else if (r > 0x17 ? r != 0x53 && r != 0x54 : r >= 0x06 && r != 0x0a && r != 0x0c && r != 0x0d && r < 0x16) {   /* gf_reg's and gs_write's plain case */
        F.gr[r] = v; F.dirty = 1;
        if (r < 0x08 && F.up) {   /* gl35 K2: gu_clut_load returns at once unless the format is indexed and CLD is 1 .. 5 */
          const uint32_t psm = (uint32_t)(v >> 20 & 63), cld = (uint32_t)(v >> 61 & 7);
          if (!(kp & 2) || ((psm == 0x13 || psm == 0x14 || psm == 0x1b || psm == 0x24 || psm == 0x2c) && cld >= 1 && cld <= 5)) gu_clut_load(F.up, F.gr, (int)(r & 1));
          else xl_nk_n[7]++; }
        gr[r] = v;
      } else if (r == 0x16 || r == 0x17) {   /* TEX2: TEX0's CLUT fields */
        const uint64_t M = 0xFFFFFFE003F00000ull;
        F.gr[r - 0x10] = (F.gr[r - 0x10] & ~M) | (v & M); F.gr[r] = v; F.dirty = 1;
        if (F.up) gu_clut_load(F.up, F.gr, (int)(r & 1));
        gr[r - 0x10] = (gr[r - 0x10] & ~M) | (v & M); gr[r] = v;
      } else { gf_reg(&F, (int)r, v); gs_write((int)r, v); }
    }
  }
  NS_MARK(NS_AD);
  int dctx = 3;
  c.tag_qa = (0x15 + T) & 0x3ff;
  { const uint32_t *t = vmq(c.tag_qa);
    if (t[1] >> 14 & 1) {
      if (kp & 2) { const uint64_t v = t[1] >> 15 & 0x7ff; gr[0] = v; F.gr[0] = v; F.vqn = 0; F.dirty = 1; F.unk &= ~(GF_U_PRIM | GF_U_QUEUE); }   /* gl35 K2 */
      else { gs_write(0x00, t[1] >> 15 & 0x7ff); gf_reg(&F, 0x00, t[1] >> 15 & 0x7ff); }
      uint64_t at = (F.gr[0x1a] & 1) ? F.gr[0x00] : F.gr[0x1b]; dctx = 1 << (int)(at >> 9 & 1); } }
  gf_unknown(&F, GF_U_ALL);
  for (uint32_t k = 0; k < 4; k++) vmq(T + k);
  vmq(T + 0x14); for (uint32_t k = 2; k < 6; k++) vmq(k);
  c.rec = &R; c.lane = 1; c.vm = VM; c.gr = gr;
  NS_MARK(NS_PRE);
  xl_keep_nf = 1;
  if (call_fn) ME_CALLP(call_fn)(call_ctx, &c);
  xl_keep_nf = 0;
  NS_MARK(NS_BACK);
  if ((kp & 1) && F.up) {   /* gl35 K1: the drawn areas of the last full step again, while nothing it read has changed */
    static int kd_n, kd_ctx; static int16_t kd_k[2]; static uint32_t kd_gs, kd_dg;
    const int ok = kd_n > 0 && kd_gs == xl_gs_gen && kd_ctx == dctx && kd_dg == gu_dgen(F.up);
    if (ok && !xl_keep_chk) { gu_drawn_again(F.up, kd_n, kd_k); xl_nk_n[0]++; }
    else {
      int16_t k2[2]; const uint32_t mk = gu_keep_mark(F.up);
      gf_drew(&F, dctx);
      const int n2 = gu_keep_got(F.up, mk, k2);
      if (ok) { xl_nk_n[1]++; if (n2 != kd_n || k2[0] != kd_k[0] || (n2 > 1 && k2[1] != kd_k[1])) xl_nk_n[2]++; }
      kd_n = n2; kd_k[0] = k2[0]; kd_k[1] = k2[1]; kd_gs = xl_gs_gen; kd_dg = mk; kd_ctx = dctx;
    }
  } else gf_drew(&F, dctx);
  NS_MARK(NS_DREW);
  St.cmds[0x00] += 3; vcmd = 0;
  xl_lane_groups[1]++;
  return 1;
}
/* sb2, Stage B lane 2 (xlate.h) */
long xl_lane_groups[5];
int xl_lane_p06(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp)
{
  int nr = 9 + q0; const uint32_t *cn = t + 4 * nr;
  if (vleft || V.mode >= 2) return 0;
  { int cl = V.cl, wl = V.wl, ok = 1; V.cl = 6; V.wl = 1;   /* the streams' UNPACKs as xl_vif1_span would test them (the V4-32 ones always wait) */
    for (int j = q0 + 3; j < nr; j++) if (dp[j] && !ud_ok(t[4 * j + 3])) ok = 0;
    V.cl = cl; V.wl = wl; if (!ok) return 0; }
  static XlRec R H39;
  NS_PMARK(NS_PTR);   /* gl38: the people split */
  for (int j = 0; j < nr; j++) {   /* each slot: its TTE words (STCYCL, then the UNPACK or a NOP), then a sent slot's data, all waiting */
    const uint32_t *g = t + 4 * j; int s = j - q0 - 3;   /* s: stream slot 0 .. 5 (A .. F); below 0 the constant block and the materials */
    St.cmds[1]++; V.cl = (int)(g[2] & 0xff); V.wl = (int)(g[2] >> 8 & 0xff);
    if (!dp[j]) { St.words += 2; St.cmds[0]++; if (s >= 0) R.p[s] = 0; continue; }   /* not sent: a CNT of 0, STCYCL and a NOP */
    uint32_t w = g[3], nw = 4 * (g[0] & 0xffff); int c = (int)(w >> 24 & 0x7f);
    St.words += 2 + nw; St.cmds[c]++; XC(St.wd[c] += nw);
    up_census(w); if (s < 0) { ud_eager(w, dp[j], nw); continue; }   /* sb8: the block and the materials at once */
    ud_new(w, dp[j], nw);
    { const Ud *d = &UD[ud_n - 1]; R.p[s] = d->p; R.num[s] = d->num; R.kind[s] = (uint8_t)ud_kind(d); }
  }
  St.words += 10; St.cmds[1]++; V.cl = V.wl = 1; St.cmds[0x6c]++; XC(St.wd[0x6c] += 4);   /* the CNT: STCYCL, the header's UNPACK and data */
  up_census(cn[3]); ud_eager(cn[3], cn + 4, 4);   /* sb8: the header at once */
  vsrc0 = tadr + 16 * (uint32_t)nr + 16; usrc = 0;
  memcpy(R.row, V.row, 16); R.lane = 2;
  vcmd = cn[8]; vn = 0; dqn = 0; St.cmds[0x14]++;
  NS_PMARK(NS_REG);
  call(cn[8] & 0xffff, 0, &R);
  St.cmds[0x00] += 3; vcmd = 0;   /* the three NOPs */
  xl_lane_groups[2]++;
  return 1;
}
/* sb5, Stage B lane 4 (xlate.h): one P07 slot, tag k at quadword k (k < 7) or k + 4, as xl_vif1_span takes its words: each tag's TTE words
   (STCYCL or a NOP; the UNPACK, a NOP, FLUSHE or MSCAL), then a sent part's data, all waiting */
int xl_lane_p07(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp)
{
  if (vleft || V.mode >= 2) return 0;
  { int cl = V.cl, wl = V.wl, ok = 1; V.cl = 7;   /* the streams' UNPACKs as xl_vif1_span would test them (the V4-32 ones always wait) */
    for (int j = 0; j < 6; j++) if (dp[j]) { V.wl = j == 5 ? 2 : 1; if (!ud_ok(t[4 * j + 3])) ok = 0; }
    V.cl = cl; V.wl = wl; if (!ok) return 0; }
  static XlRec R H39; uint32_t last = 0;
  for (int k = 0; k < 19; k++) {
    const uint32_t *g = t + 4 * (k < 7 ? k : k + 4); uint32_t w = g[3];
    St.words += 2;
    if (k == 10) { St.cmds[0]++; St.cmds[w >> 24 & 0x7f]++; continue; }   /* NOP, then FLUSHE or NOP */
    St.cmds[1]++; V.cl = (int)(g[2] & 0xff); V.wl = (int)(g[2] >> 8 & 0xff);   /* the slot's STCYCL */
    const uint32_t *p = k == 6 ? t + 28 : dp[k]; uint32_t nw = k == 6 ? 16 : 4 * (g[0] & 0xffff);
    if (!p) { St.cmds[0]++; if (k < 6) { R.p[k] = 0; R.num[k] = 0; R.kind[k] = 0; } continue; }   /* not sent: a NOP */
    int c = (int)(w >> 24 & 0x7f);
    St.cmds[c]++; St.words += nw; XC(St.wd[c] += nw);
    up_census(w); if (k < 6) ud_new(w, p, nw); else ud_eager(w, p, nw);   /* sb8: the constants at once, the streams wait */
    last = k == 6 ? tadr + 112 : g[1] & 0xfffffff0u;   /* (xl_vif1_span's vsrc0: where the last data began) */
    if (k < 6) { const Ud *d = &UD[ud_n - 1];   /* a stream: A .. D (V4-16, kind 2), E, F (V4-8 unsigned: 5) */
      R.p[k] = d->p; R.num[k] = d->num; R.kind[k] = (uint8_t)(k < 5 ? ud_kind(d) : d->fmt == 0x0e && d->md == 1 ? 5 : 0); }
  }
  const uint32_t *m = t + 4 * 23;   /* the last tag: a NOP, then MSCAL */
  St.words += 2; St.cmds[0]++;
  vsrc0 = last; usrc = 0;
  memcpy(R.row, V.row, 16); R.lane = 4;
  vcmd = m[3]; vn = 0; dqn = 0; uvert = 0; St.cmds[0x14]++;
  call(m[3] & 0xffff, 0, &R);
  xl_lane_groups[4]++;
  return 1;
}
/* sb6, Stage B lane 3 (xlate.h): one effect prim, tag k at quadword k (k < 4) or k + 4, as xl_vif1_span takes its words: each tag's TTE
   words (STCYCL and the UNPACK; the last tag's a NOP and MSCAL), then a REF's or the CNT's data, all waiting */
int xl_lane_fx(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp)
{
  (void)tadr;   /* (the CNT's data at tadr + 0x40 is the prim's own: t + 16) */
  if (vleft || V.mode >= 2) return 0;
  { int cl = V.cl, wl = V.wl, ok = 1;   /* the streams' UNPACKs as xl_vif1_span would test them (the V4-32 ones in STCYCL 1 / 1 always wait) */
    for (int k = 0; k < 2; k++) { V.cl = (int)(t[4 * k + 2] & 0xff); V.wl = (int)(t[4 * k + 2] >> 8 & 0xff); if (!ud_ok(t[4 * k + 3])) ok = 0; }
    V.cl = cl; V.wl = wl; if (!ok) return 0; }
  static XlRec R H39;
  for (int k = 0; k < 5; k++) {
    const uint32_t *g = t + 4 * (k < 4 ? k : k + 4), *p = k == 3 ? t + 16 : dp[k]; uint32_t w = g[3], nw = 4 * (g[0] & 0xffff);
    int c = (int)(w >> 24 & 0x7f);
    St.words += 2 + nw; St.cmds[1]++; V.cl = (int)(g[2] & 0xff); V.wl = (int)(g[2] >> 8 & 0xff); St.cmds[c]++; XC(St.wd[c] += nw);
    up_census(w); if (k >= 2) { ud_eager(w, p, nw); continue; }   /* sb8: the constants at once, the streams wait */
    ud_new(w, p, nw);
    { const Ud *d = &UD[ud_n - 1]; R.p[k] = d->p; R.num[k] = d->num; R.kind[k] = (uint8_t)ud_kind(d); }   /* the positions (kind
                                                        1: V4-32), the attributes (2, 3: V4-16 signed) */
  }
  R.kind[2] = (t[2] & 0xffff) == 0x0103 && (t[6] & 0xffff) == 0x0203;   /* the layout P04+25f0 and +2870 read: cl 3 */
  St.words += 2; St.cmds[0]++;   /* the last tag: a NOP, then MSCAL */
  vsrc0 = t[33] & 0xfffffff0u; usrc = t[1] & 0xfffffff0u;   /* (xl_vif1_span's: where the last data began, the REF 3's; where the
                                                        positions' did, the UNPACK to T + 0x100: XlCall.src for the thinning) */
  memcpy(R.row, V.row, 16); R.lane = 3;
  vcmd = t[39]; vn = 0; dqn = 0; uvert = 0; St.cmds[0x14]++;
  call(t[39] & 0xffff, 0, &R);
  xl_lane_groups[3]++;
  return 1;
}
void xl_lane(int on) { vm_all(); lane_on = on; }
/* sb1 (xlate.h): what each converter reads (ge/vu1conv.c, ge/xconv.c), every vq() address of it: abs 0..0x3f (the pass constants, fog,
   viewport, alpha; P07's matrix, lights and ramp), T + 0..0x17 (the matrices, lights, skin, pass 3's matrix, the header), T + 0xfe and
   0xff (pass 3's scale and offset), the vertices from T + 0x100 (3, 6 or 7 quadwords each, the header's count of them, at most the whole
   memory), and P07's bone matrices (abs 32 + each vertex's offsets, four quadwords each, for its 1 to 4 bones) */
void xl_vm_need(const XlCall *c)
{
  uint32_t e = c->entry, t = c->top, st, h;
  if (e == XL_DF0 || e == XL_FX || e == XL_FX2) { st = 3; h = 0x14; }
  else if (e == 0x19fe08u || e == 0x1a0160u || e == 0x1a0540u || e == 0x1a08c0u) { st = 6; h = 0x14; }   /* P06 (vu1conv.h VC_P06..) */
  else if (e == 0x1a31e0u) { st = 7; h = 0; }   /* P07 (VC_P07) */
  else if (e == 0x19bb50u) { st = 6; h = 0x14; }   /* sb15 step 4: P04+3740's sprites (VC_SPR: 6 quadwords a record) */
  else if (e == 0x19be50u) { st = 12; h = 0x14; }  /* P04+3a38's quads (VC_QUAD: 12) */
  else if (e == 0x19af68u) { st = 3; h = 0x14; }   /* P04+2b60's streaks (VC_LINE: 3) */
  else { vm_all(); return; }
  XC(St.need_calls++);
  need_q(0, 0x40); need_q(t, 0x18); need_q(t + 0xfe, 2);
  if (c->rec) return;   /* lane 1: P04+df0's streams are read from the game's data */
  uint32_t n = vmw(t + h)[0] & 0xffff, m = st * n < 1024 ? st * n : 1024;
  need_q(t + 0x100, m);
  if (st == 7) {   /* P07: the bones of each vertex (T + 0x106 + 7 i lanes 0 .. nb - 1); nb outside 1..4 is a path vc_p07 refuses */
    uint32_t nb = vmw(t)[2] & 0xffff; if (nb > 4) nb = 4;
    for (uint32_t i = 0; i < n && i < 1024; i++) { const uint32_t *B = vmw(t + 0x106 + 7 * i);
      for (uint32_t b = 0; b < nb; b++) need_q((B[b] & 0xffffu) + 32u, 4); }
  }
}
void xl_need(int on) { vm_all(); need_on = on; }
static void (*flush_fn)(void *); static void *flush_ctx;   /* sb4: xl_on_flush */
void xl_on_flush(void (*fn)(void *ctx), void *ctx) { flush_fn = fn; flush_ctx = ctx; }
void xl_flush(void) { vm_all(); if (flush_fn) ME_CALLP(flush_fn)(flush_ctx); lo_all(); xl_vm_wgen++; }   /* (sb10 T2: the game's data may change after the transfer) */
void xl_vm_sync(void) { vm_all(); }
const uint32_t *xl_vm_q(uint32_t qa) { return vmq(qa); }
void xl_vm_logical(uint8_t *out)
{
  memcpy(out, VM, sizeof VM);
  if (ud_n) for (uint32_t qa = 0; qa < 1024; qa++) if (own[qa]) { const Ud *d = &UD[own[qa] - 1]; ud_vec(d, ud_k(d, qa), (uint32_t *)(out + (qa << 4))); }
}
#ifndef __psp__
/* gl34 (x86, the twin's "nats:" line): the translator's state folded, for a run against a run: h[0] the VU memory model as a read
   sees it (xl_vm_logical), h[1] the GS registers, h[2] the GS front end (its registers, vertex state and flags), h[3] the VIF and GIF
   state, h[4] the counters.  Not folded: which quadwords wait for their decode (own, the descriptors) */
static uint32_t sd_h(uint32_t h, const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; } return h; }
void xl_state_dig(uint32_t *h)
{
  static uint8_t lv[16384];
  for (int k = 0; k < 5; k++) h[k] = 2166136261u;
  xl_vm_logical(lv); h[0] = sd_h(h[0], lv, sizeof lv);
  h[1] = sd_h(h[1], gr, sizeof gr);
  h[2] = sd_h(h[2], F.gr, sizeof F.gr); h[2] = sd_h(h[2], &F.cur, sizeof F.cur); h[2] = sd_h(h[2], &F.q, sizeof F.q); h[2] = sd_h(h[2], F.vq, sizeof F.vq);
  h[2] = sd_h(h[2], &F.vqn, sizeof F.vqn); h[2] = sd_h(h[2], F.vqu, sizeof F.vqu); h[2] = sd_h(h[2], &F.curu, sizeof F.curu); h[2] = sd_h(h[2], &F.dirty, sizeof F.dirty);
  h[2] = sd_h(h[2], &F.path, sizeof F.path); h[2] = sd_h(h[2], &F.unk, sizeof F.unk);
  for (int k = 0; k < 4; k++) { h[3] = sd_h(h[3], &G[k].nloop, sizeof(int)); h[3] = sd_h(h[3], &G[k].flg, sizeof(int)); h[3] = sd_h(h[3], &G[k].nreg, sizeof(int));
    h[3] = sd_h(h[3], &G[k].ri, sizeof(int)); h[3] = sd_h(h[3], &G[k].eop, sizeof(int)); h[3] = sd_h(h[3], &G[k].regs, sizeof(uint64_t)); }
  h[3] = sd_h(h[3], &V.cl, sizeof V.cl); h[3] = sd_h(h[3], &V.wl, sizeof V.wl); h[3] = sd_h(h[3], &V.mode, sizeof V.mode); h[3] = sd_h(h[3], &V.mask, sizeof V.mask);
  h[3] = sd_h(h[3], V.row, sizeof V.row); h[3] = sd_h(h[3], V.col, sizeof V.col); h[3] = sd_h(h[3], &V.base, sizeof V.base); h[3] = sd_h(h[3], &V.ofst, sizeof V.ofst);
  h[3] = sd_h(h[3], &V.tops, sizeof V.tops); h[3] = sd_h(h[3], &V.itops, sizeof V.itops); h[3] = sd_h(h[3], &V.dbf, sizeof V.dbf);
  h[3] = sd_h(h[3], &vcmd, sizeof vcmd); h[3] = sd_h(h[3], &vsrc0, sizeof vsrc0); h[3] = sd_h(h[3], &vleft, sizeof vleft); h[3] = sd_h(h[3], &vn, sizeof vn);
  h[3] = sd_h(h[3], &dqn, sizeof dqn); h[3] = sd_h(h[3], &usrc, sizeof usrc); h[3] = sd_h(h[3], &last_entry, sizeof last_entry); h[3] = sd_h(h[3], &last_cont, sizeof last_cont);
  h[3] = sd_h(h[3], xl_vgen, sizeof xl_vgen);
  h[4] = sd_h(h[4], &St.words, sizeof St.words); h[4] = sd_h(h[4], St.cmds, sizeof St.cmds); h[4] = sd_h(h[4], &St.calls, sizeof St.calls);
  h[4] = sd_h(h[4], St.gif_tags, sizeof St.gif_tags); h[4] = sd_h(h[4], &St.ad_kicks, sizeof St.ad_kicks); h[4] = sd_h(h[4], &St.ad_bad, sizeof St.ad_bad);
  h[4] = sd_h(h[4], St.ent, sizeof St.ent); h[4] = sd_h(h[4], St.ent_n, sizeof St.ent_n); h[4] = sd_h(h[4], xl_lane_groups, sizeof xl_lane_groups);
}
#endif
void xl_reset(void)
{
  xl_gs_gen++;   /* gl35 */
  memset(&St, 0, sizeof St); memset(gr, 0, sizeof gr); memset(G, 0, sizeof G); memset(VM, 0, sizeof VM); memset(VPsrc, 0, sizeof VPsrc);
  memset(&V, 0, sizeof V); V.cl = V.wl = 1; vleft = vn = dqn = 0; last_entry = 0; last_cont = 0; usrc = 0; uvert = 0;
  lo_all();   /* sb10 T2 */
  xl_vm_wgen++;   /* gl34 */
  memset(own, 0, sizeof own); ud_n = rc_n = 0; rc_dirty = 1; ud_vecs = 0; memset(EC, 0, sizeof EC);
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
