/* reuse_probe.h (gl36, x86 twin only; included by ge/gelive.c under #ifndef __psp__): the twin's probe for the whole-group reuse
   (attic/gl36/reuse_design.txt section 2).  MGS2_REUSE_PROBE=1: every fast native ship group's converter call (XlRec.nf) is
   hashed by its inputs and its output, and matched with the same group of the picture before (gl_begin to gl_begin):
     id    the group's identity: its material address, stream counts, header count, and the streams' addresses (static groups,
           NATG_INPL) or their bytes (the others), with its occurrence number among equal ids in the picture;
     in    what the translation reads: the VU memory model at T + 0 .. 0x17 (the constant block: mtx = T + 0 .. 3, the matrix; rest
           = T + 4 .. 0x13, 0x15 .. 0x17), the header (T + 0x14), the material (T + 0xe8 .. 0xef), abs 2 .. 5, the GS registers
           xc_gs / xc_state / gl_tex read after the material packet, the streams' bytes, T itself (the VU buffer);
     out   the converter's result: r, the batch (first 0, the texture index as the frame's texture key), the GerXform, the rest of
           the blob (vertices, strips, indices).
   "rprobe:" lines a 100 vblanks and at the exit.  Reads only; changes nothing of the run. */
#ifndef MGS2_REUSE_PROBE_H
#define MGS2_REUSE_PROBE_H
#include <stdio.h>
const NatGrp *gl_rp_g;   /* the group being walked (ge/natp04.h, x86) */
long gl_rp_vb;          /* the vblank (boot/nat_x86.c nat_walk) */
static int rp_on = -1;
typedef struct { uint64_t id, mtx, rest, hdr, mat, abs_, gs, str, out_b, out_x, out_v; uint32_t top; int r, used; } RpE;
#define RP_N 8192
static RpE rp_t[2][RP_N]; static int rp_cur, rp_n[2]; static long rp_pic;
static long rp_c[40], rp_vb0 = -1;
/* calls (the groups of one P1214a0 call share its key): by the first group's id, the count and the ids' sequence; 24 calls; 25 their
   groups; 26 calls whose count and sequence equal the picture before's call with that first id; 27 their groups; 28 of 27 cut; 29 groups
   in calls of one group */
typedef struct { uint64_t id, seq; int n, used; } RpC;
static RpC rp_ct[2][RP_N]; static uint32_t rp_ckey; static uint64_t rp_cid, rp_cseq; static int rp_cn, rp_ccut, rp_copen;
static int rp_cfind(int t, uint64_t id)
{
  uint32_t h = (uint32_t)(id * 0x9e3779b97f4a7c15ull >> 51) & (RP_N - 1);
  for (int k = 0; k < RP_N; k++, h = (h + 1) & (RP_N - 1)) { RpC *e = &rp_ct[t][h]; if (!e->used) return -(int)h - 1; if (e->id == id) return (int)h; }
  return -RP_N - 1;
}
static void rp_cend(void)
{
  if (!rp_copen) return;
  rp_copen = 0; rp_c[24]++; rp_c[25] += rp_cn; if (rp_cn == 1) rp_c[29]++;
  int s = rp_cfind(rp_cur, rp_cid); if (s < -RP_N) return;
  if (s < 0) { RpC *e = &rp_ct[rp_cur][-s - 1]; e->id = rp_cid; e->seq = rp_cseq; e->n = rp_cn; e->used = 1; }
  int p = rp_cfind(rp_cur ^ 1, rp_cid);
  if (p >= 0 && rp_ct[rp_cur ^ 1][p].n == rp_cn && rp_ct[rp_cur ^ 1][p].seq == rp_cseq) { rp_c[26]++; rp_c[27] += rp_cn; rp_c[28] += rp_ccut; }
}
/* counts: 0 groups; 1 id found in the picture before; 2 all inputs equal; 3 of 2 output equal; 4 of 2 output differs; 5 id found, the
   matrix differs, the rest of the inputs equal; 6 of 5 vertices part equal; 7 of 5 cut outcome equal; 8 of 5 batch equal; 9 id found, the
   matrix equal but something else differs; 10 the GS registers differ (of 1); 11 the texture key differs (of 1, in the batch); 12 T differs
   (of 1); 13 rest of block differs (of 1); 14 streams differ (of 1); 15 static (INPL) groups; 16 of 15 id found; 17 of 15 all inputs equal;
   18 cut (r 0) groups; 19 mat differs (of 1); 20 hdr differs; 21 abs differs; 22 of 2 with output differing only in batch;
   23 of 1: output equal (whatever the inputs) */
static uint64_t rp_h(uint64_t h, const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
#define RP_H0 1469598103934665603ull
static inline const uint8_t *rp_q(const XlCall *c, uint32_t qa) { return c->vm + ((qa & 0x3ffu) << 4); }
static int rp_find(int t, uint64_t id)
{
  uint32_t h = (uint32_t)(id * 0x9e3779b97f4a7c15ull >> 51) & (RP_N - 1);
  for (int k = 0; k < RP_N; k++, h = (h + 1) & (RP_N - 1)) { RpE *e = &rp_t[t][h]; if (!e->used) return -(int)h - 1; if (e->id == id) return (int)h; }
  return -RP_N - 1;
}
static void rp_line(long vb, int last)
{
  static long c0[40];
  fprintf(stderr, "rprobe: vb %ld-%ld%s:", rp_vb0, vb, last ? " (exit)" : "");
  for (int k = 0; k < 30; k++) fprintf(stderr, " %ld", rp_c[k] - c0[k]);
  fputc('\n', stderr);
  for (int k = 0; k < 40; k++) c0[k] = rp_c[k];
}
static void rp_exit(void)
{
  if (rp_vb0 >= 0) rp_line(gl_rp_vb, 1);
  fprintf(stderr, "rprobe: totals:"); for (int k = 0; k < 30; k++) fprintf(stderr, " %ld", rp_c[k]); fprintf(stderr, " (pictures %ld)\n", rp_pic);
}
static void rp_begin(void)
{
  if (rp_on < 0) { const char *e = getenv("MGS2_REUSE_PROBE"); rp_on = e ? atoi(e) : 0; if (rp_on) atexit(rp_exit); }
  if (!rp_on) return;
  rp_cend();
  rp_cur ^= 1; memset(rp_t[rp_cur], 0, sizeof rp_t[rp_cur]); memset(rp_ct[rp_cur], 0, sizeof rp_ct[rp_cur]); rp_n[rp_cur] = 0; rp_pic++;
  long vb = gl_rp_vb;
  if (rp_vb0 < 0) rp_vb0 = vb - vb % 100;
  if (vb >= rp_vb0 + 100) { rp_line(rp_vb0 + 100, 0); rp_vb0 = vb - vb % 100; }
}
static void rp_call(const GlFrame *f, const XlCall *c, int r, const GerBatch *b, const uint8_t *blob, uint32_t size)
{
  if (rp_on <= 0 || !c->rec || !c->rec->nf || !gl_rp_g) return;
  const NatGrp *g = gl_rp_g; const XlRec *R = c->rec; RpE e; memset(&e, 0, sizeof e);
  int inpl = (g->flags & NATG_INPL) != 0;
  e.str = RP_H0; for (int k = 0; k < 3; k++) e.str = rp_h(e.str, R->p[k], 16u * g->n[k]);
  uint64_t id = rp_h(RP_H0, &g->mat, 4); id = rp_h(id, g->n, 3); id = rp_h(id, &g->hdr[0], 4);
  if (inpl) id = rp_h(id, g->s, 12); else id = rp_h(id, &e.str, 8);
  e.mtx = rp_h(RP_H0, rp_q(c, c->top), 64);
  e.rest = rp_h(RP_H0, rp_q(c, c->top + 4), 16 * 16); e.rest = rp_h(e.rest, rp_q(c, c->top + 0x15), 48);
  e.hdr = rp_h(RP_H0, rp_q(c, c->top + 0x14), 16);
  e.mat = rp_h(RP_H0, rp_q(c, c->top + 0xe8), 128);
  e.abs_ = rp_h(RP_H0, rp_q(c, 2), 64);
  { static const uint8_t RG[] = { 0x00, 0x1a, 0x1b, 0x18, 0x19, 0x40, 0x41, 0x4e, 0x4f, 0x47, 0x48, 0x42, 0x43, 0x06, 0x07, 0x08, 0x09, 0x3b, 0x3d, 0x4c, 0x4d, 0x14, 0x15 };
    e.gs = RP_H0; for (size_t k = 0; k < sizeof RG; k++) e.gs = rp_h(e.gs, &c->gr[RG[k]], 8); }
  e.top = c->top; e.r = r; e.used = 1;
  if (r > 0) {
    GerBatch bb = *b; bb.first = 0; uint64_t tk = bb.tex != 0xffff && bb.tex < (uint32_t)f->o.nt ? f->tkey[bb.tex] : 0; bb.tex = 0;
    e.out_b = rp_h(RP_H0, &bb, sizeof bb); e.out_b = rp_h(e.out_b, &tk, 8);
    e.out_x = rp_h(RP_H0, blob, sizeof(GerXform) < size ? sizeof(GerXform) : size);
    e.out_v = size > sizeof(GerXform) ? rp_h(RP_H0, blob + sizeof(GerXform), size - sizeof(GerXform)) : 0;
  }
  /* the occurrence number among equal ids */
  if (!rp_copen || g->key != rp_ckey) { rp_cend(); rp_copen = 1; rp_ckey = g->key; rp_cn = 0; rp_ccut = 0; rp_cid = 0; rp_cseq = RP_H0; }
  int t = rp_cur, o = 0, s;
  for (;;) { uint64_t k = id + 0x100000001b3ull * (uint64_t)o; s = rp_find(t, k); if (s < 0) { e.id = k; break; } o++; }
  if (s >= -RP_N) rp_t[t][-s - 1] = e;
  if (!rp_cn) { rp_cid = e.id; } rp_cn++; rp_ccut += r == 0; rp_cseq = rp_h(rp_cseq, &e.id, 8);
  rp_c[0]++; if (inpl) rp_c[15]++; if (r == 0) rp_c[18]++;
  int p = rp_find(t ^ 1, e.id); if (p < 0) return;
  const RpE *q = &rp_t[t ^ 1][p];
  rp_c[1]++; if (inpl) rp_c[16]++;
  int dm = q->mtx != e.mtx, dr = q->rest != e.rest, dh = q->hdr != e.hdr, dma = q->mat != e.mat, da = q->abs_ != e.abs_, dg = q->gs != e.gs, ds = q->str != e.str, dt = q->top != e.top;
  int oeq = q->r == e.r && q->out_b == e.out_b && q->out_x == e.out_x && q->out_v == e.out_v;
  if (oeq) rp_c[23]++;
  rp_c[10] += dg; rp_c[12] += dt; rp_c[13] += dr; rp_c[14] += ds; rp_c[19] += dma; rp_c[20] += dh; rp_c[21] += da;
  if (e.r > 0 && q->r > 0 && q->out_b != e.out_b) rp_c[11]++;
  if (!dm && !dr && !dh && !dma && !da && !dg && !ds && !dt) {
    rp_c[2]++; if (inpl) rp_c[17]++;
    if (oeq) rp_c[3]++; else { rp_c[4]++; if (q->r == e.r && q->out_x == e.out_x && q->out_v == e.out_v) rp_c[22]++; } }
  else if (dm && !dr && !dh && !dma && !da && !dg && !ds && !dt) {
    rp_c[5]++; if (q->r == e.r) rp_c[7]++;
    if (q->r > 0 && e.r > 0) { if (q->out_v == e.out_v) rp_c[6]++; if (q->out_b == e.out_b) rp_c[8]++; } }
  else if (!dm) rp_c[9]++;
}
#endif
