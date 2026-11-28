/* nat_x86.c: the native renderer's step 1 on the x86 twin (container only).  Included by boot_x86.c after nat_stub.h (-DNAT_HOOK).
   The twin's one walk (runtime/ee_io.h vif1_chain_fast) calls nat_xfer at each chained VIF1 transfer and nat_walk at each tag:
     arm 3 (B): the groups the stub recorded are drawn by the walk's rule, the PSP's (ge/xlme.c lw_walk, xlme.h XmPic): before a
       top-level tag of the groups' packet buffer, every group whose key is at or below the tag's address; lanes 3 and 4 stop their
       runs at the next key (nat_stop).  The runtime's own VIF state is not touched: on the PSP the runtime's walk never sees a
       native group either, so the state hashes of the two stay comparable.
     arm 2 (shadow, drawn): the builder ran; a group's record is drawn at its packet's first tag and the walk steps over the packet.
     arm 1 (shadow, compared): each record's words against its packet's.
   MGS2_NAT_LW=1 / 2 (boot/xlive_chk.c): the transfer goes through the PSP's late walk instead (direct / plan, copy and view); this
   file's walk then only counts.
   Lines: "nat:" at the exit. */
static struct {
  long tags, tags_sub, xf, emit, core, fed, bad, state, late_seq, match, words_bad, wclass[8][4], unrec, unrec_y0, twice, stale_tags;
  long recs, min_margin; int margin_set;
} NX;
static int nat_xf_b = -1; static uint32_t nat_cmp[2]; static int nat_lw_mode = -1;
static int n6_xf_b = -1;   /* native step 2: the buffer whose people's groups the transfer draws */
static long nat_lw_n[8];   /* boot/xlive_chk.c's late-walk harness counts (it prints them) */

void nat_x86_rec(int b, uint32_t g0, int ng, uint32_t mesh, uint32_t dest, uint32_t end, uint32_t qwc)
{
  long mg = (long)(int32_t)end - (long)(int32_t)(dest + 16 * qwc);
  (void)b; (void)g0; (void)ng; (void)mesh;
  NX.recs++; if (!NX.margin_set || mg < NX.min_margin) { NX.min_margin = mg; NX.margin_set = 1; }
}
/* ---- gl33: static streams in place (ge/natsnap.h NATG_INPL), the twin's check.  MGS2_NAT_STCHK=<w>: every flagged group's three
   streams hashed as it is drawn and again at the start of each of the next w vblanks: "CHANGED" counts the groups whose bytes were
   others, by the vblanks since the draw (the PSP's ME reads them in RAM up to a step after the record).  A "natinpl:" line each 100
   vblanks with groups drawn, and "natst:" at the exit. ---- */
extern long g_nat_stc;
extern long g_nat_fst, xl_nf_n[8], lw_nh_n[3]; int rt_nat_fast_at(long vb);   /* gl34 */
extern long g_nat_kpt, xl_nk_n[16]; int rt_nat_keep_at(long vb);   /* gl35 */
extern long xl_nr_n[20], xl_nr_why[16]; extern int xl_nr_cap;   /* gl36 (ge/xlate.h) */
extern long xl_f1_n[12];   /* gl38 (ge/xlate.h) */
extern long xl_nr_r[8]; extern int xl_nr_rule;   /* gl37 (ge/xlate.h): the reuse's rule */
#include "../ge/vu1conv.h"
static long ninp_vb0 = -1, ninp_st0, ninp_drawn, ninp_drawn0;
typedef struct { uint32_t s[3]; uint8_t n[3], bad; long vb; uint64_t h; uint32_t seq; } StE;
#define ST_RING (1 << 16)
static StE *st_ring; static uint32_t st_head, st_tail; static long st_win = -1, st_n, st_bad, st_lost, st_age[65], st_done;
static uint64_t st_hash(const uint32_t *s, const uint8_t *n)
{
  uint64_t h = 1469598103934665603ull;
  for (int k = 0; k < 3; k++) { const uint32_t *p = (const uint32_t *)(const void *)(g_mem + (s[k] & (GUEST_SIZE - 1) & ~15u));
    for (uint32_t i = 0; i < 4u * n[k]; i++) { h ^= p[i]; h *= 1099511628211ull; } }
  return h;
}
/* every vblank's start (boot_x86.c k_vblank_hook): each flagged group drawn in the last st_win vblanks hashed again; one whose bytes are
   others is counted once, by the vblanks since it was drawn */
static void st_tick(long vb)
{
  if (st_win <= 0 || !st_ring) return;
  while (st_tail != st_head && vb - st_ring[st_tail & (ST_RING - 1)].vb > st_win) { st_tail++; st_done++; }
  for (uint32_t i = st_tail; i != st_head; i++) {
    StE *e = &st_ring[i & (ST_RING - 1)]; long age = vb - e->vb;
    if ((e->bad & 1) || age <= 0 || st_hash(e->s, e->n) == e->h) continue;
    e->bad |= 1; st_bad++; st_age[age > 64 ? 64 : age]++;
    if (st_bad <= 40) fprintf(stderr, "natst: CHANGED: streams %08x %08x %08x (%u %u %u qw) drawn at vb %ld, other bytes at the start of vb %ld\n",
                              e->s[0], e->s[1], e->s[2], e->n[0], e->n[1], e->n[2], e->vb, vb);
  }
}
/* The same by pipeline calls (boot_x86.c rt_dg_call, as g_dg_seq goes up): on the PSP the ME walks call S's groups while the main CPU
   may run on until its first copy plan after call S + 2 (two copy areas: psp/xl_psp.c two_post, two_wait), so a flagged group's streams
   must hold the same bytes until call S + 3 starts.  Each group drawn is hashed again at the start of each of the next ST_SEQW calls;
   one whose bytes are others is counted once, by the calls since its own. */
#define ST_SEQW 6
static long st_sbad, st_sage[ST_SEQW + 1]; static uint32_t st_stail;
static void st_seq_tick(uint32_t seq)
{
  if (st_win <= 0 || !st_ring) return;
  if ((int32_t)(st_stail - st_tail) < 0) st_stail = st_tail;
  while (st_stail != st_head && seq - st_ring[st_stail & (ST_RING - 1)].seq > ST_SEQW) st_stail++;
  for (uint32_t i = st_stail; i != st_head; i++) {
    StE *e = &st_ring[i & (ST_RING - 1)]; uint32_t age = seq - e->seq;
    if ((e->bad & 2) || !age || age > ST_SEQW || st_hash(e->s, e->n) == e->h) continue;
    e->bad |= 2; st_sbad++; st_sage[age]++;
    if (st_sbad <= 40) fprintf(stderr, "natst: CHANGED (calls): streams %08x %08x %08x (%u %u %u qw) of pipeline call %u drawn at vb %ld, other bytes at "
                               "the start of call %u, vb %ld\n", e->s[0], e->s[1], e->s[2], e->n[0], e->n[1], e->n[2], e->seq, e->vb, seq, k_vblanks);
  }
}
static uint32_t st_cur_seq;
static void st_group(const NatGrp *g)
{
  if (st_win < 0) { const char *e = getenv("MGS2_NAT_STCHK"); st_win = e ? atol(e) : 0; if (st_win > 0) st_ring = calloc(ST_RING, sizeof *st_ring); }
  if (!(g->flags & NATG_INPL)) return;
  ninp_drawn++;
  if (st_win <= 0 || !st_ring) return;
  if (st_head - st_tail == ST_RING) { st_lost++; st_tail++; }
  { StE *e = &st_ring[st_head++ & (ST_RING - 1)]; memcpy(e->s, g->s, sizeof e->s); memcpy(e->n, g->n, 3); e->bad = 0; e->vb = k_vblanks; e->seq = st_cur_seq; e->h = st_hash(e->s, e->n); st_n++; }
}
static void ninp_line(long vb, int last)
{
  long turn = -1; int arm = rt_nat_arm_at(ninp_vb0 + 1, &turn);
  fprintf(stderr, "natinpl: vb %ld-%ld%s arm %d inpl %d turn %ld: groups flagged static %ld (drawn %ld)\n", ninp_vb0, vb, last ? " (exit)" : "", arm,
          rt_nat_inpl_at(ninp_vb0 + 1), turn, g_nat_stc - ninp_st0, ninp_drawn - ninp_drawn0);
  ninp_st0 = g_nat_stc; ninp_drawn0 = ninp_drawn;
  { static long f0, n0[8], k0[4]; if (vc_km_chk < 0) { const char *e = getenv("MGS2_NAT_XMCHK"); vc_km_chk = e ? atoi(e) : 0; }   /* gl34 */
    fprintf(stderr, "natfast: vb %ld-%ld%s arm %d fast %d turn %ld: groups flagged %ld; the fast front end took %ld, declined: state %ld, entry %ld, "
            "buffers %ld, stream counts %ld, material tag %ld, tables full %ld, path 1 busy %ld; transforms taken from the last keyed call %ld, "
            "computed and kept %ld, taken ones checked %ld, differing %ld\n", ninp_vb0, vb, last ? " (exit)" : "", arm, rt_nat_fast_at(ninp_vb0 + 1), turn,
            g_nat_fst - f0, xl_nf_n[0] - n0[0], xl_nf_n[1] - n0[1], xl_nf_n[2] - n0[2], xl_nf_n[3] - n0[3], xl_nf_n[4] - n0[4], xl_nf_n[5] - n0[5],
            xl_nf_n[6] - n0[6], xl_nf_n[7] - n0[7], vc_km_n[0] - k0[0], vc_km_n[1] - k0[1], vc_km_n[2] - k0[2], vc_km_n[3] - k0[3]);
    f0 = g_nat_fst; for (int k = 0; k < 8; k++) n0[k] = xl_nf_n[k]; for (int k = 0; k < 4; k++) k0[k] = vc_km_n[k]; }
  { static long f0, n0[9];   /* gl35 */
    fprintf(stderr, "natkeep: vb %ld-%ld%s arm %d keep %d turn %ld: groups flagged %ld; drawn areas replayed %ld, checked %ld, differing %ld; textures "
            "replayed %ld, checked %ld, differing %ld, records %ld; CLUT calls not made %ld, PRIM writes in line %ld\n", ninp_vb0, vb, last ? " (exit)" : "", arm,
            rt_nat_keep_at(ninp_vb0 + 1), turn, g_nat_kpt - f0, xl_nk_n[0] - n0[0], xl_nk_n[1] - n0[1], xl_nk_n[2] - n0[2], xl_nk_n[3] - n0[3], xl_nk_n[4] - n0[4],
            xl_nk_n[5] - n0[5], xl_nk_n[6] - n0[6], xl_nk_n[7] - n0[7], xl_nk_n[8] - n0[8]);
    f0 = g_nat_kpt; for (int k = 0; k < 9; k++) n0[k] = xl_nk_n[k]; }
  { static long f0[12]; long d[12]; int ch = 0; int rt_nat_f1_at(long vb);   /* gl38 (ge/xlate.h xl_f1_n) */
    for (int k = 0; k < 12; k++) { d[k] = xl_f1_n[k] - f0[k]; f0[k] = xl_f1_n[k]; ch |= d[k] != 0; }
    if (ch) fprintf(stderr, "natf1: vb %ld-%ld%s arm %d F1 %d turn %ld: runs planned %ld (groups %ld, of them not written %ld, without the fast form %ld), runs not planned %ld (state %ld, a group without an MSCAL %ld), planned "
            "runs with a problem %ld; check mode: groups compared %ld, DIFFERING %ld, written by the plan and not by the notes %ld\n", ninp_vb0, vb,
            last ? " (exit)" : "", arm, rt_nat_f1_at(ninp_vb0 + 1), turn, d[0], d[1], d[2], d[10], d[3], d[8], d[9], d[4], d[5], d[6], d[7]); }
  { static long r0[20]; long d[20]; int rt_nat_reuse_at(long vb);   /* gl36 (ge/xlate.h xl_nr_n) */
    for (int k = 0; k < 20; k++) { d[k] = xl_nr_n[k] - r0[k]; r0[k] = xl_nr_n[k]; }
    if (xl_nr_cap) fprintf(stderr, "natreuse: vb %ld-%ld%s arm %d reuse %d turn %ld: runs replayed %ld (groups %ld, skipping %ld, in S %ld), recorded %ld "
            "(groups %ld; kept %ld, not kept %ld), checked %ld (groups %ld; S differing %ld, boxes differing %ld); boxes recorded %ld, kept boxes taken %ld, "
            "cut %ld, positions then needed %ld; no table room %ld; replays with a group not taken %ld, with a decline %ld\n", ninp_vb0, vb, last ? " (exit)" : "",
            arm, rt_nat_reuse_at(ninp_vb0 + 1), turn, d[0], d[1], d[18], d[2], d[3], d[4], d[5], d[13], d[6], d[7], d[8], d[9], d[16], d[10], d[11], d[12],
            d[14], d[15], d[17]);
    { static long w0[16]; if (xl_nr_cap) { fprintf(stderr, "natreuse: vb %ld-%ld why not replayed:", ninp_vb0, vb); for (int k = 0; k < 16; k++) { fprintf(stderr, " %ld", xl_nr_why[k] - w0[k]); w0[k] = xl_nr_why[k]; } fprintf(stderr, "; barriers %ld\n", d[19]); } }
    { static long q0[8]; long q[8]; for (int k = 0; k < 8; k++) { q[k] = xl_nr_r[k] - q0[k]; q0[k] = xl_nr_r[k]; }   /* gl37 */
      if (xl_nr_cap) fprintf(stderr, "natreuse: vb %ld-%ld%s rule %d (gl37): groups tried %ld, replayed or the same as the picture before %ld; runs not "
                             "recorded %ld (groups %ld), the same as their shadow %ld (groups %ld); pictures recording %ld, not recording %ld\n", ninp_vb0, vb,
                             last ? " (exit)" : "", xl_nr_rule, q[6], q[7], q[0], q[1], q[2], q[3], q[4], q[5]); } }
}
static void nat_report(void);
static void nat_xfer(uint32_t tadr)
{
  static int once; (void)tadr;
  if (!once) { once = 1; atexit(nat_report); }
  nat_xf_b = -1; n6_xf_b = -1; NX.xf++;
  { static long t0 = -2, t1; if (t0 == -2) { const char *e = getenv("MGS2_NAT_TRACE"); t0 = -1; if (e) sscanf(e, "%ld:%ld", &t0, &t1); }
    if (t0 >= 0 && k_vblanks >= t0 && k_vblanks <= t1)
      fprintf(stderr, "natx: vb %ld transfer tadr %08x; pipeline call %u (lite now %d, arm %d), last started in buffer0 %u buffer1 %u; groups b0 seq %u n %u "
              "cur %u draw %u; b1 seq %u n %u cur %u draw %u; wp %08x\n", k_vblanks, tadr, g_dg_seq, g_dg_lite, g_nat_arm, g_nat_bseq[0], g_nat_bseq[1],
              g_nat[0].seq, g_nat[0].n, g_nat[0].cur, g_nat[0].draw, g_nat[1].seq, g_nat[1].n, g_nat[1].cur, g_nat[1].draw, *(const uint32_t *)(g_mem + 0x1e50a0)); }
}
static inline const uint32_t *nat_ptr(uint32_t a, uint32_t n)   /* the DMAC's view of guest address a, n quadwords, or 0 */
{
  if (a & 0x80000000u) { uint32_t o = a & 0x3ff0u; return o + 16u * n <= 0x4000u ? (const uint32_t *)(const void *)(g_spr + o) : 0; }
  { uint32_t b = a & (GUEST_SIZE - 1u) & ~15u; return b + 16u * n <= GUEST_SIZE ? (const uint32_t *)(const void *)(g_mem + b) : 0; }
}
#define NATP_PTR(a, n) nat_ptr((a), (n))
#define NATP_SPAN(p, nw, a) xl_vif1_span((p), (nw), (a))
#include "../ge/natp04.h"
#include "../ge/natp06.h"   /* native step 2: the people's records */
/* ---- native step 2 (boot/np6_stub.h): the people's records in the twin walk, beside the ship's: arm 3 drawn by the ship's rule (before a
   top-level tag of the groups' buffer, every group whose key is at or below the tag's address; the people first when a ship group waits
   at a later key, counted when they interleave); arm 2 drawn at its packet's first tag, the packet stepped over; arm 1 each record's
   words against its packet's.  Lines: "np6:" at the exit. ---- */
static struct { long emit, core, fed, bad, late_seq, stale, inter, match, words_bad, wclass[13][4], unrec, recs, step; } N6X;
static uint32_t n6_cmp[2];
void np6_x86_rec(int b, uint32_t g0, int ng, uint32_t mesh) { (void)b; (void)g0; (void)ng; (void)mesh; N6X.recs++; }
static int np6_tag(const uint32_t *t)   /* a tag that starts a P06 group (lane_p06's first words) */
{
  return (t[0] >> 28 == 3 && (t[0] & 0xffff) == 24 && t[2] == NP6T_S11 && t[3] == NP6T_BLK && t[6] == NP6T_S11 && t[10] == NP6T_S11 && t[14] == NP6T_S11 && t[18] == NP6T_S61) ||
         (t[2] == NP6T_S11 && t[6] == NP6T_S11 && t[10] == NP6T_S11 && t[14] == NP6T_S61 && (t[3] == 0 || t[3] == NP6T_MAT));
}
/* the people's groups of buffer b with keys at or below lim (the walk's tag a); 1 when one was drawn */
static void np6_emit(NatP6Buf *q, uint32_t lim)
{
  while (q->cur < q->n && (q->g[q->cur].key & (GUEST_SIZE - 1) & ~15u) <= lim) {
    const NatP6 *g = &q->g[q->cur++]; int rc;
    if (!g_xl_one) continue;   /* (a transfer the translator is not fed) */
    if (q->seq != g_dg_seq) N6X.late_seq++;
    nge_walk6(q == &g_np6[0] ? 0 : 1, q->cur - 1); rc = nat_p06_group(g); nge_walk6_end(); N6X.emit++;   /* step 2b: the group's reference (ge/nge.h) */
    if (rc == NATP_CORE) N6X.core++; else if (rc == NATP_FED) N6X.fed++; else N6X.bad++;
  }
}
static uint32_t np6_next(const NatP6Buf *q) { return q->cur < q->n ? q->g[q->cur].key & (GUEST_SIZE - 1) & ~15u : 0xffffffffu; }
/* the group's quadwords as the builder copies them to the packet (8 from q0 with NATG_FULL, else 7 from q1; a CNT tag's ADDR 0) */
static int nat_words(const NatGrp *g, uint32_t *w)
{
  int q = 0;
#define Q4(a, b, c, d) do { w[4 * q] = (a); w[4 * q + 1] = (b); w[4 * q + 2] = (c); w[4 * q + 3] = (d); q++; } while (0)
  if (g->flags & NATG_FULL) Q4(0x30000000u | 24u, g->blk, NATT_S11, NATT_BLK);
  Q4(0x30000000u | 8u, g->mat, NATT_S11, NATT_MAT);
  for (int k = 0; k < 3; k++) { uint32_t n = g->n[k];
    if (g->s[k] && n) Q4(0x30000000u | n, g->s[k], NATT_S31, k == 2 ? NATT_C | ((4 * n) & 0xffu) << 16 : (k ? NATT_B : NATT_A) | ((2 * n) & 0xffu) << 16);
    else Q4(0x10000000u, 0, NATT_S31, 0); }
  Q4(0x10000000u | 2u, 0, NATT_S11, NATT_HDR);
  Q4(g->hdr[0], g->hdr[1], g->hdr[2], g->hdr[3]);
  Q4(g->mscal, 0, 0, 0);
#undef Q4
  return q;
}
static int nat_p04_tag(const uint32_t *t)   /* a tag that starts a P04 group, by its words */
{
  return (t[2] == 0x01000101u && t[3] == 0x6c188000u && t[6] == 0x01000101u && t[7] == 0x6c0880e8u && t[10] == 0x01000103u) ||
         (t[2] == 0x01000101u && t[3] == 0x6c0880e8u && t[6] == 0x01000103u);
}
static int nat_walk(const uint32_t *t, uint32_t tadr, VifLoc *vs, uint32_t *next, uint32_t chcr, uint32_t depth)
{
  uint32_t a; int b; NatBuf *s;
  nat_stop = 0xffffffffu;
  { extern long gl_rp_vb; gl_rp_vb = k_vblanks; }   /* gl36 probe */
  if (tadr & 0x80000000u) return 0;
  a = tadr & (GUEST_SIZE - 1) & ~15u;
  if (a < NAT_PKT0 || a >= NAT_PKT1) return 0;
  b = NAT_BUF(a); s = &g_nat[b];
  NX.tags++; if (depth) NX.tags_sub++;
  if (nat_lw_mode < 0) { const char *e = getenv("MGS2_NAT_LW"); nat_lw_mode = e ? atoi(e) : 0; }
  if (nat_lw_mode) return 0;   /* the late walk made the picture of this transfer (boot/xlive_chk.c) */
  if (depth || !(chcr & 0x40)) return 0;
  { NatP6Buf *q = &g_np6[b];   /* native step 2: the people's records of this buffer */
    if (q->g && q->n) {
      if (q->seq != g_nat_bseq[b]) { if (q->draw && q->cur < q->n) N6X.stale++; }
      else if (!q->draw) {   /* arm 1: the record's words against the packet's, by key */
        { static uint32_t cs[2]; if (cs[b] != q->seq) { cs[b] = q->seq; n6_cmp[b] = 0; } }
        while (n6_cmp[b] < q->n && (q->g[n6_cmp[b]].key & (GUEST_SIZE - 1) & ~15u) < a) n6_cmp[b]++;
        if (n6_cmp[b] < q->n && (q->g[n6_cmp[b]].key & (GUEST_SIZE - 1) & ~15u) == a) {
          const NatP6 *g = &q->g[n6_cmp[b]++]; uint32_t w[4 * 16]; int nq = np6_words(g, w), bad = 0, f = (g->flags & NP6G_FULL) != 0;
          N6X.match++;
          if (a + 16u * (uint32_t)nq > GUEST_SIZE) bad = 1;
          else for (int j = 0; j < nq; j++) for (int k = 0; k < 4; k++) {
            if (k == 1 && w[4 * j] >> 28 == 1) continue;   /* (a CNT tag's ADDR: the builder leaves an older pointer there) */
            if (t[4 * j + k] != w[4 * j + k]) { N6X.wclass[j + !f][k]++; bad = 1; } }
          if (bad) { N6X.words_bad++;
            if (N6X.words_bad <= 8) { fprintf(stderr, "np6 arm 1: vb %ld group at %08x differs:", k_vblanks, a);
              for (int j = 0; j < nq; j++) fprintf(stderr, " q%d %08x %08x %08x %08x / %08x %08x %08x %08x", j + !f, t[4 * j], t[4 * j + 1], t[4 * j + 2], t[4 * j + 3],
                                                   w[4 * j], w[4 * j + 1], w[4 * j + 2], w[4 * j + 3]);
              fputc('\n', stderr); } } }
        else if (a + 192 <= GUEST_SIZE && np6_tag(t)) N6X.unrec++; }
      else {   /* arms 2 and 3: drawn */
        if (n6_xf_b < 0 && q->cur < q->n) n6_xf_b = b;
        if (b == n6_xf_b) {
          uint32_t ks = s->g && s->seq == g_nat_bseq[b] && s->draw && (nat_xf_b < 0 || nat_xf_b == b) && s->cur < s->n ?
                        s->g[s->cur].key & (GUEST_SIZE - 1) & ~15u : 0xffffffffu, kp = np6_next(q);
          int pk = kp == a && (q->g[q->cur].flags & NP6G_PKT);   /* arm 2: this tag is the group's own packet */
          np6_emit(q, pk || a < ks ? a : ks);   /* (arm 2: the group at its own packet first, always) */
          if (np6_next(q) <= a || (pk && ks < a)) N6X.inter++;   /* (a ship group waits at a lower key: the people's after it, below) */
          if (pk) {   /* arm 2: the packet stepped over: the runtime's VIF state as lane 2 leaves it */
            int q0 = t[3] == NP6T_BLK, nr = 9 + q0;
            vs->left = 0; vs->cmd = 0; vs->cl = 1; vs->wl = 1; N6X.step++;
            *next = tadr + 16u * (uint32_t)(nr + 3); return nr + 3; }
          if (np6_next(q) < nat_stop) nat_stop = np6_next(q); } } } }
  if (!s->g || s->seq != g_nat_bseq[b]) { if (s->n && s->cur < s->n) NX.stale_tags++; return 0; }   /* another pipeline call wrote this buffer since */
  if (!s->draw) {   /* arm 1: the record's words against the packet's, by key (the groups come in address order) */
    { static uint32_t cs[2]; if (cs[b] != s->seq) { cs[b] = s->seq; nat_cmp[b] = 0; } }
    while (nat_cmp[b] < s->n && (s->g[nat_cmp[b]].key & (GUEST_SIZE - 1) & ~15u) < a) nat_cmp[b]++;
    if (s->seq != g_dg_seq) NX.late_seq++;
    if (nat_cmp[b] < s->n && (s->g[nat_cmp[b]].key & (GUEST_SIZE - 1) & ~15u) == a) {
      const NatGrp *g = &s->g[nat_cmp[b]++]; uint32_t w[32]; int nq = nat_words(g, w), bad = 0;
      NX.match++;
      if (a + 16u * (uint32_t)nq > GUEST_SIZE) bad = 1;
      else for (int j = 0; j < nq; j++) for (int k = 0; k < 4; k++) {
        if (k == 1 && w[4 * j] >> 28 == 1 && j < nq - 2) continue;   /* (a CNT tag's ADDR: the builder leaves an older pointer there) */
        if (t[4 * j + k] != w[4 * j + k]) { NX.wclass[j + !(g->flags & NATG_FULL)][k]++; bad = 1; } }
      if (bad) NX.words_bad++; }
    else if (a + 48 <= GUEST_SIZE && nat_p04_tag(t)) { NX.unrec++; if (!(t[3] == 0x6c188000u ? t[25] : t[21])) NX.unrec_y0++; }
    return 0; }
  if (nat_xf_b < 0 && s->cur < s->n) nat_xf_b = b;
  if (b != nat_xf_b) return 0;
  { int nq = 0, nr = 0, nrt = 0, nr0 = (int)s->cur, f1 = 0; long bad0 = NX.bad;   /* gl36 (ge/xlate.h xl_nr_*): the run's reuse, when its picture has the switch; gl38: F1 */
    if ((s->ru & 3) && g_xl_one && s->cur < s->n && (s->g[s->cur].key & (GUEST_SIZE - 1) & ~15u) <= a) {
      uint32_t e = s->cur + 1; while (e < s->n && (s->g[e].key & (GUEST_SIZE - 1) & ~15u) <= a) e++;
      if ((s->ru & 1) && xl_nr_cap) nr = xl_nr_begin(s->g + s->cur, (int)(e - s->cur), (int)s->cur);
      if ((s->ru & 2) && nr != 1) f1 = xl_f1_begin(s->g + s->cur, (int)(e - s->cur)); }
    while (s->cur < s->n && (s->g[s->cur].key & (GUEST_SIZE - 1) & ~15u) <= a) {
      if (nr) xl_nr_group((int)s->cur - nr0);
      xl_f1_j = (int)s->cur - nr0;
      const NatGrp *g = &s->g[s->cur++]; int rc;
      st_cur_seq = s->seq; st_group(g);   /* gl33 */
      if (ninp_vb0 < 0) ninp_vb0 = k_vblanks - k_vblanks % 100;
      if (k_vblanks >= ninp_vb0 + 100) { ninp_line(ninp_vb0 + 100, 0); ninp_vb0 = k_vblanks - k_vblanks % 100; }
      if (!g_xl_one) { NX.state++; continue; }   /* (a transfer the translator is not fed: nothing drawn) */
      if (s->seq != g_dg_seq) NX.late_seq++;
      nge_walk(b, (int)(g - s->g)); rc = nat_p04_group(g); nge_walk_end(); NX.emit++;   /* native step 1: the group's reference (ge/nge.h) */
      if (rc == NATP_CORE) { NX.core++; nrt++; } else if (rc == NATP_FED) NX.fed++; else NX.bad++;
      nq = (g->flags & NATG_PKT) && (g->key & (GUEST_SIZE - 1) & ~15u) == a ? 7 + (g->flags & NATG_FULL) : 0;
    }
    if (nr) xl_nr_end(nrt);
    if (f1) xl_f1_end(NX.bad == bad0);
    if (s->cur < s->n && (s->g[s->cur].key & (GUEST_SIZE - 1) & ~15u) < nat_stop) nat_stop = s->g[s->cur].key & (GUEST_SIZE - 1) & ~15u;
    { NatP6Buf *q = &g_np6[b]; if (q->g && q->draw && q->seq == g_nat_bseq[b] && b == n6_xf_b && np6_next(q) <= a) np6_emit(q, a); }   /* (interleaved: after the ship's) */
    if (nq) {   /* arm 2: the packet stepped over: the runtime's VIF state as lane 1 leaves it */
      int q0 = nq == 8, nr = 4 + q0;
      vs->left = 0; vs->cmd = 0; vs->cl = 1; vs->wl = 1;
      vif1_n_cmd[0x01] += nr + 1; vif1_n_cmd[0x6c] += 2 + q0; vif1_n_cmd[0x6d] += 2; vif1_n_cmd[0x75]++; vif1_n_cmd[0x14]++; vif1_n_cmd[0x00] += 3;
      *next = tadr + 16u * (uint32_t)nq; return nq; }
    if (a + 48 <= GUEST_SIZE && nat_p04_tag(t)) { NX.unrec++; if (!(t[3] == 0x6c188000u ? t[25] : t[21])) NX.unrec_y0++; }
  }
  return 0;
}
static void nat_report(void)
{
  if (g_nat_cfg && ninp_vb0 >= 0) ninp_line(k_vblanks, 1);
  nge_end_line();   /* native step 1 */
  if (g_nat_cfg) fprintf(stderr, "natinpl: totals: groups flagged static %ld (drawn %ld)\n", g_nat_stc, ninp_drawn);
  if (g_nat_cfg) fprintf(stderr, "natfast: totals: groups flagged %ld; taken %ld, declined %ld %ld %ld %ld %ld %ld %ld; transforms taken %ld, kept %ld, checked %ld, DIFFERING %ld\n",
                         g_nat_fst, xl_nf_n[0], xl_nf_n[1], xl_nf_n[2], xl_nf_n[3], xl_nf_n[4], xl_nf_n[5], xl_nf_n[6], xl_nf_n[7], vc_km_n[0], vc_km_n[1], vc_km_n[2], vc_km_n[3]);
  if (g_nat_cfg) fprintf(stderr, "natkeep: totals: groups flagged %ld; drawn areas replayed %ld, checked %ld, DIFFERING %ld; textures replayed %ld, checked %ld, DIFFERING %ld, records %ld; CLUT calls not made %ld, PRIM writes in line %ld\n",
                         g_nat_kpt, xl_nk_n[0], xl_nk_n[1], xl_nk_n[2], xl_nk_n[3], xl_nk_n[4], xl_nk_n[5], xl_nk_n[6], xl_nk_n[7], xl_nk_n[8]);
  if (g_nat_cfg) fprintf(stderr, "natf1: totals: runs planned %ld (groups %ld, of them not written %ld, without the fast form %ld), runs not planned %ld (state %ld, a group without an MSCAL %ld), planned runs with a problem %ld; "
                         "check mode: groups compared %ld, DIFFERING %ld, written by the plan and not by the notes %ld, skipped by the plan ahead of a group that declined when it ran %ld\n", xl_f1_n[0], xl_f1_n[1], xl_f1_n[2], xl_f1_n[10],
                         xl_f1_n[3], xl_f1_n[8], xl_f1_n[9], xl_f1_n[4], xl_f1_n[5], xl_f1_n[6], xl_f1_n[7], xl_f1_n[11]);
  if (g_nat_cfg && xl_nr_cap) fprintf(stderr, "natreuse: totals: runs replayed %ld (groups %ld, skipping %ld), recorded %ld (groups %ld; kept %ld, not kept %ld), "
                         "checked %ld (groups %ld; S DIFFERING %ld, boxes DIFFERING %ld); in S %ld; boxes recorded %ld, taken %ld, cut %ld, positions then needed %ld; "
                         "no table room %ld; replays with a group not taken %ld (PROBLEM if not 0), with a decline %ld\n", xl_nr_n[0], xl_nr_n[1], xl_nr_n[18], xl_nr_n[3],
                         xl_nr_n[4], xl_nr_n[5], xl_nr_n[13], xl_nr_n[6], xl_nr_n[7], xl_nr_n[8], xl_nr_n[9], xl_nr_n[2], xl_nr_n[16], xl_nr_n[10], xl_nr_n[11],
                         xl_nr_n[12], xl_nr_n[14], xl_nr_n[15], xl_nr_n[17]);
  if (g_nat_cfg && xl_nr_cap) fprintf(stderr, "natreuse: totals: rule %d (gl37): groups tried %ld, replayed or the same as the picture before %ld; runs not "
                         "recorded %ld (groups %ld), the same as their shadow %ld (groups %ld); pictures recording %ld, not recording %ld\n", xl_nr_rule,
                         xl_nr_r[6], xl_nr_r[7], xl_nr_r[0], xl_nr_r[1], xl_nr_r[2], xl_nr_r[3], xl_nr_r[4], xl_nr_r[5]);
  if (g_nat_cfg) fprintf(stderr, "natfast: totals: pointers by their own hints (MGS2_NAT_LW=2's views) %ld, checked %ld, DIFFERING %ld\n", lw_nh_n[0], lw_nh_n[2], lw_nh_n[1]);
  if (st_win > 0) {
    fprintf(stderr, "natst: static groups drawn %ld, each hashed again at every vblank's start for %ld vblanks: CHANGED %ld; by the vblanks since the draw:",
            st_n, st_win, st_bad);
    for (int i = 1; i <= 64; i++) if (st_age[i]) fprintf(stderr, " %d: %ld", i, st_age[i]);
    fprintf(stderr, "; left the window unchanged or counted %ld, not followed (ring full) %ld\n", st_done, st_lost);
    fprintf(stderr, "natst: the same by pipeline calls: each hashed again at the start of each of the next %d calls: CHANGED %ld; by the calls since "
            "the group's own:", ST_SEQW, st_sbad);
    for (int i = 1; i <= ST_SEQW; i++) fprintf(stderr, " %d: %ld", i, st_sage[i]);
    fprintf(stderr, " (the PSP's ME may read a group until call + 3 starts)\n"); }
  fprintf(stderr, "nat: stub: calls recorded %ld, groups %ld (most in a picture %ld), pictures %ld; stubbed without a record (arm C) %ld; calls with "
          "no group %ld; the builder run instead: array full %ld, key below the last %ld, near the buffer's end %ld, other %ld; recorded "
          "with no array %ld; groups not taken when their array was written again %ld (pictures %ld); least margin end - (dest + 16 qwc) at a "
          "recorded call %ld bytes\n", g_nat_c[NC_REC], g_nat_c[NC_GRP], g_nat_c[NC_GRP_MAX], g_nat_c[NC_PICS], g_nat_c[NC_STUBC], g_nat_c[NC_EMPTY],
          g_nat_c[NC_FB_FULL], g_nat_c[NC_FB_ORDER], g_nat_c[NC_FB_END], g_nat_c[NC_FB_OTHER], g_nat_c[NC_NOMEM], g_nat_c[NC_LEFT],
          g_nat_c[NC_LEFT_PICS], NX.min_margin);
  fprintf(stderr, "nat: twin walk: transfers %ld, packet-buffer tags %ld (inside a CALL %ld); groups drawn from records %ld (core %ld, fed word by "
          "word %ld, dropped %ld), not drawn (the translator not fed) %ld; drawn or compared after a later pipeline call began %ld; tags met "
          "while their buffer still held untaken groups of an older pipeline call (stale: not drawn) %ld; run lanes stopped at a key %ld; P04 packets met with no record %ld "
          "(header y 0: %ld)\n", NX.xf, NX.tags, NX.tags_sub, NX.emit, NX.core, NX.fed, NX.bad, NX.state, NX.late_seq, NX.stale_tags, nat_stop_hit,
          NX.unrec, NX.unrec_y0);
  if (NX.match || NX.words_bad) { fprintf(stderr, "nat: arm 1: records matched to their packets %ld, with a differing word %ld; by quadword q0..q7 (w0 w1 w2 w3):",
                                          NX.match, NX.words_bad);
    for (int q = 0; q < 8; q++) fprintf(stderr, " q%d %ld %ld %ld %ld", q, NX.wclass[q][0], NX.wclass[q][1], NX.wclass[q][2], NX.wclass[q][3]);
    fputc('\n', stderr); }
  if (g_np6_cfg) {
    fprintf(stderr, "np6: stub: calls recorded %ld, groups %ld (most in a picture %ld), pictures %ld; stubbed without a record (arm 4) %ld; calls with no group %ld; "
            "the builder run instead: array full %ld, key below the last %ld, near the buffer's end %ld, other %ld; of the fallbacks with the game's material memo "
            "behind the stub's %ld; recorded with no array %ld; groups not taken when their array was written again %ld; template set-ups %ld; pipeline calls in arm 3 "
            "that ran the builder (before the arrays or the people's start) %ld, of them from the start on (the growth late) %ld\n",
            g_np6_c[N6_REC], g_np6_c[N6_GRP], g_np6_c[N6_GRP_MAX], g_np6_c[N6_PICS], g_np6_c[N6_STUBC], g_np6_c[N6_EMPTY], g_np6_c[N6_FB_FULL], g_np6_c[N6_FB_ORDER],
            g_np6_c[N6_FB_END], g_np6_c[N6_FB_OTHER], g_np6_c[N6_FB_MEMO], g_np6_c[N6_NOMEM], g_np6_c[N6_LEFT], g_np6_c[N6_MEMO_RESET],
            g_np6_c[N6_NOARR], g_np6_c[N6_LATE]);
    fprintf(stderr, "np6: twin walk: groups drawn from records %ld (lane 2 %ld, fed word by word %ld, dropped %ld), drawn after a later pipeline call began %ld; "
            "tags met with untaken groups of an older call %ld; tags where the people's and the ship's groups interleaved %ld; arm 2 packets stepped over %ld; "
            "P06 packets met with no record %ld; the most groups in a picture before vb 24,000 %ld (vb %ld), from it %ld (vb %ld)\n", N6X.emit, N6X.core, N6X.fed, N6X.bad, N6X.late_seq, N6X.stale, N6X.inter, N6X.step, N6X.unrec, np6_gmax[0], np6_gmax_vb[0], np6_gmax[1], np6_gmax_vb[1]);
    if (N6X.match || N6X.words_bad) { fprintf(stderr, "np6: arm 1: records matched to their packets %ld, with a differing word %ld; by quadword q0..q12 (w0 w1 w2 w3):",
                                              N6X.match, N6X.words_bad);
      for (int q = 0; q < 13; q++) fprintf(stderr, " q%d %ld %ld %ld %ld", q, N6X.wclass[q][0], N6X.wclass[q][1], N6X.wclass[q][2], N6X.wclass[q][3]);
      fputc('\n', stderr); } }
  fprintf(stderr, "nat: guest thread stacks (gate 1: the dead frames below a stack pointer differ with a stubbed builder):");
  for (int i = 0; i < K_THREADS; i++) if (k_th[i].used) fprintf(stderr, " %d: %08x..%08x", i, k_th[i].stack, k_th[i].stack + k_th[i].ssize);
  fputc('\n', stderr);
}
