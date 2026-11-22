# patch_b4.py <xl_psp.c of b3 (c278d9e6)> <out>: task #180 step 4b-2, diagnostic b4: the check by 16-byte quadword (a line piece's quadwords
# the transfer itself reads, from the second walk, against its other quadwords: the margin), and the watch printed per line with each
# changed word marked read (r) or margin (m), its counts split the same way
import sys, hashlib
src, out = sys.argv[1], sys.argv[2]
t = open(src).read()
assert hashlib.md5(t.encode()).hexdigest().startswith('c278d9e6'), 'unexpected source'
def rep(old, new, cnt=1):
    global t
    assert t.count(old) == cnt, (old[:80], t.count(old)); t = t.replace(old, new)
rep("""static XmCopy xmr; static uint8_t *mp_chr;   /* the check's second walk (rd_only: the bytes the transfer itself reads) and its flag
                                               per line: 1 the line holds such bytes, 0 only the margin after a tag */""",
"""static XmCopy xmr; static uint8_t *mp_chr;   /* the check's second walk (rd_only: the bytes the transfer itself reads) and its flag
                                               per line piece (b4): bits 0-3 the piece's quadwords the transfer reads (by place in
                                               their line), bits 4-7 a 4-bit hash of its other quadwords (the margin) when it has both */""")
rep("""static uint32_t mp_hash(const uint8_t *p, uint32_t n)   /* the check's: FNV-1a over the words of n bytes (16-byte aligned pieces) */
{
  const uint32_t *w = (const uint32_t *)p; uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n / 4; i++) h = (h ^ w[i]) * 16777619u;
  return h ^ n;
}""", """static uint32_t mp_hash(const uint8_t *p, uint32_t n)   /* the check's: FNV-1a over the words of n bytes (16-byte aligned pieces) */
{
  const uint32_t *w = (const uint32_t *)p; uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n / 4; i++) h = (h ^ w[i]) * 16777619u;
  return h ^ n;
}
static uint32_t mp_hashq(const uint8_t *p, uint32_t a, uint32_t f, unsigned qm, int rd)   /* b4: FNV-1a over the quadwords of [a, f) (p its
                                                                                           bytes) whose bit in qm is rd */
{
  uint32_t h = 2166136261u, n = 0;
  for (uint32_t q = a; q < f; q += 16) if ((int)((qm >> ((q >> 4) & 3)) & 1) == rd) {
    const uint32_t *w = (const uint32_t *)(p + (q - a));
    h = (h ^ w[0]) * 16777619u; h = (h ^ w[1]) * 16777619u; h = (h ^ w[2]) * 16777619u; h = (h ^ w[3]) * 16777619u; n++; }
  return h ^ n;
}""")
# the plan: the piece's read quadwords
rep("""      if (mp_chr) {   /* (both lists sorted by address) */
        int rd = 1;
        if (r) { while (j < rnx && r[j].a1 <= a) j++; rd = j < rnx && r[j].a0 < f; }
        mp_chr[mp_chn + n] = (uint8_t)rd; }
#ifdef ME_WATCH
      if (mpw_on && mpw_bit(a >> 6)) mpw_arm(e, a >> 6);
#endif
      mp_ch[mp_chn + n++] = mp_hash(M.mem + a, f - a); a = f; }""",
"""      unsigned qm = 15;   /* b4: the piece's quadwords the transfer reads (all of them without the second walk) */
      if (mp_chr) {   /* (both lists sorted by address) */
        if (r) { qm = 0; while (j < rnx && r[j].a1 <= a) j++;
          for (int jj = j; jj < rnx && r[jj].a0 < f; jj++) { uint32_t s = r[jj].a0 > a ? r[jj].a0 : a, u = r[jj].a1 < f ? r[jj].a1 : f;
            for (uint32_t q = s & ~15u; q < u; q += 16) qm |= 1u << ((q >> 4) & 3); } }
        mp_chr[mp_chn + n] = (uint8_t)(qm | (qm ? (mp_hashq(M.mem + a, a, f, qm, 0) & 15) << 4 : 0)); }
#ifdef ME_WATCH
      if (mpw_on && mpw_bit(a >> 6)) mpw_arm(e, a >> 6, qm);
#endif
      mp_ch[mp_chn + n++] = qm ? mp_hashq(M.mem + a, a, f, qm, 1) : mp_hash(M.mem + a, f - a); a = f; }""")
# the fill: read quadwords and margin apart
rep("""      int rd = mp_chr ? mp_chr[e->h0 + n] : 1;
      mp_chk_lrd += rd;
      if (mp_hash(xmc.arena + x[i].off + (a - x[i].a0), f - a) != mp_ch[e->h0 + n]) {
        mp_chk_diff++; bad = 1; if (rd) mp_chk_drd++; else mp_chk_dmg++;""",
"""      unsigned c = mp_chr ? mp_chr[e->h0 + n] : 15u, qm = c & 15u; const uint8_t *cp = xmc.arena + x[i].off + (a - x[i].a0);
      int rd = 0, dm = 0;   /* b4: rd the read quadwords differ, dm the margin's (in a piece with both: by its 4-bit hash) */
      if (qm) { rd = mp_hashq(cp, a, f, qm, 1) != mp_ch[e->h0 + n]; dm = mp_chr && (mp_hashq(cp, a, f, qm, 0) & 15) != (c >> 4); }
      else dm = mp_hash(cp, f - a) != mp_ch[e->h0 + n];
      mp_chk_lrd += qm != 0;
      if (rd && dm) mp_chk_dboth++;
      if (rd || dm) {
        mp_chk_diff++; bad = 1; if (rd) mp_chk_drd++; else mp_chk_dmg++;""")
rep("""          fprintf(stderr, "me picture check: vb %ld: record %d (ch %d, chcr %08x, tadr %08x): line %07x+%u of extent %07x..%07x (%s) differs "
                  "from the bytes as its transfer ended\\n", me_vb, e->t, e->ch, (unsigned)e->chcr, (unsigned)e->tadr, (unsigned)a,
                  (unsigned)(f - a), (unsigned)x[i].a0, (unsigned)x[i].a1, rd ? "bytes the transfer reads" : "only the margin after a tag"); } }""",
"""          fprintf(stderr, "me picture check: vb %ld: record %d (ch %d, chcr %08x, tadr %08x): line %07x+%u of extent %07x..%07x (read quadwords "
                  "%u%u%u%u: %s) differs from the bytes as its transfer ended\\n", me_vb, e->t, e->ch, (unsigned)e->chcr, (unsigned)e->tadr,
                  (unsigned)a, (unsigned)(f - a), (unsigned)x[i].a0, (unsigned)x[i].a1, qm & 1, (qm >> 1) & 1, (qm >> 2) & 1, (qm >> 3) & 1,
                  rd ? (dm ? "in bytes the transfer reads and in the margin" : "in bytes the transfer reads") : "only in the margin"); } }""")
rep("static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd;",
    "static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd, mp_chk_dboth;")
rep("""                          "holding bytes the transfers read, the others only the margins after tags), %ld differ (in %ld copies): %ld in bytes "
                          "the transfers read, %ld only in a margin; %ld copies not checked (hashes full); second walk failed %ld times\\n",
                          mp_chk_lines, mp_chk_lrd, mp_chk_diff, mp_chk_rec, mp_chk_drd, mp_chk_dmg, mp_chk_skip, mp_chk_rdfail);""",
"""                          "holding bytes the transfers read, the others only the margins after tags), %ld differ (in %ld copies): %ld in bytes "
                          "the transfers read (b4: by 16-byte quadword; %ld of them in the margin too), %ld only in a margin; %ld copies not "
                          "checked (hashes full); second walk failed %ld times\\n",
                          mp_chk_lines, mp_chk_lrd, mp_chk_diff, mp_chk_rec, mp_chk_drd, mp_chk_dboth, mp_chk_dmg, mp_chk_skip, mp_chk_rdfail);""")
# the watch: the read mask per line, prints per line, counts split
rep("typedef struct { uint32_t l; int armed, letgo, t, ch, seen; uint32_t tadr; long vb; uint8_t b[64]; } MpW;",
    "typedef struct { uint32_t l; int armed, letgo, t, ch, seen, prn; unsigned rmask; uint32_t tadr; long vb; uint8_t b[64]; } MpW;   /* b4: prn, rmask */")
rep("static long mpw_prn, mpw_chg, mpw_logd, mpw_d_chg, mpw_d_same, mpw_d_ram, mpw_d_new, mpw_d_logd, mpw_d_other, mpw_iv[20][20];",
    "static long mpw_prn, mpw_chg, mpw_logd, mpw_d_chg, mpw_d_same, mpw_d_ram, mpw_d_new, mpw_d_logd, mpw_d_other, mpw_iv[20][20], mpw_ivm[20][20], mpw_chg_rd;")
rep("""  for (int k = 0; k < 16; k++) if (o[k] != n[k]) { if (k0 < 0) k0 = k; nd++; }
  mpw_chg++; w->seen++; mpw_iv[mpw_lid][id]++;
  if (mpw_prn < 400) {
    mpw_prn++;""", """  int inr = 0;
  for (int k = 0; k < 16; k++) if (o[k] != n[k]) { if (k0 < 0) k0 = k; nd++; if ((w->rmask >> (k >> 2)) & 1) inr = 1; }
  mpw_chg++; w->seen++; if (inr) { mpw_iv[mpw_lid][id]++; mpw_chg_rd++; } else mpw_ivm[mpw_lid][id]++;
  if (w->prn < 8 && mpw_prn < 1000) {   /* b4: at most 8 a line */
    mpw_prn++; w->prn++;""")
rep("""    for (int k = 0, m = 0; k < 16 && m < 6; k++) if (o[k] != n[k]) { fprintf(stderr, " +%02x %08x>%08x", 4 * k, (unsigned)o[k], (unsigned)n[k]); m++; }""",
    """    for (int k = 0, m = 0; k < 16 && m < 6; k++) if (o[k] != n[k]) { fprintf(stderr, " +%02x%c %08x>%08x", 4 * k, (w->rmask >> (k >> 2)) & 1 ? 'r' : 'm',
                                                                                    (unsigned)o[k], (unsigned)n[k]); m++; }""")
rep("""static void mpw_arm(const MpRec *e, uint32_t l)   /* mp_chk_plan: a watched line in a copy planned now (as its transfer ends) */
{
  MpW *w = mpw_find(l);
  if (!w || w->armed) return;
  w->armed = 1; w->letgo = 0; g_mpw_armed++; w->t = e->t; w->ch = e->ch; w->tadr = e->tadr; w->vb = me_vb; w->seen = 0;""",
"""static void mpw_arm(const MpRec *e, uint32_t l, unsigned qm)   /* mp_chk_plan: a watched line in a copy planned now (as its transfer ends);
                                                                 qm: the piece's quadwords the transfer reads */
{
  MpW *w = mpw_find(l);
  if (!w) return;
  if (w->armed) { if (w->t == e->t) w->rmask |= qm; return; }
  w->armed = 1; w->letgo = 0; g_mpw_armed++; w->t = e->t; w->ch = e->ch; w->tadr = e->tadr; w->vb = me_vb; w->seen = 0; w->rmask = qm;""")
rep("""        for (int i = 0; i < 20; i++) for (int j = 0; j < 20; j++) if (mpw_iv[i][j])
          fprintf(f, "me picture watch: changes between %s and %s: %ld\\n", mpw_name[i], mpw_name[j], mpw_iv[i][j]);""",
"""        fprintf(f, "me picture watch (b4): %ld of the changes in quadwords the transfer reads, %ld only in the margin\\n", mpw_chg_rd, mpw_chg - mpw_chg_rd);
        for (int i = 0; i < 20; i++) for (int j = 0; j < 20; j++) if (mpw_iv[i][j] | mpw_ivm[i][j])
          fprintf(f, "me picture watch: changes between %s and %s: %ld in read quadwords, %ld only in the margin\\n", mpw_name[i], mpw_name[j],
                  mpw_iv[i][j], mpw_ivm[i][j]);""")
open(out, 'w').write(t)
print('ok', hashlib.md5(t.encode()).hexdigest()[:8])
