import hashlib, sys
p='/home/claude/me18c/psp/xl_psp.c'
s=open(p).read()
assert hashlib.md5(s.encode()).hexdigest()=='00456f7b2676b8fcada40d79868eac2c', 'xl_psp.c changed'
def rep(a, b, n=1):
    global s
    assert s.count(a)==n, (s.count(a), a[:80])
    s=s.replace(a,b)

# 1. the statics: window counts and the watch
rep("static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd;\n",
"""static long mp_chk_drd, mp_chk_dmg, mp_chk_prnm, mp_chk_rdfail, mp_chk_lrd;
static long mpc_w = -1, mpc_rd, mpc_mg, mpc_rec;   /* me18c b3: the check's differing lines by window of 100 vblanks (printed as each ends) */
static void mpc_flush(void)
{
  if (mpc_w >= 0 && (mpc_rd | mpc_mg))
    fprintf(stderr, "me picture check window: vb %ld-%ld: %ld lines differ (%ld in bytes the transfers read, %ld only in a margin) in %ld copies\\n",
            mpc_w * 100, mpc_w * 100 + 99, mpc_rd + mpc_mg, mpc_rd, mpc_mg, mpc_rec);
  mpc_rd = mpc_mg = mpc_rec = 0;
}
#ifdef ME_WATCH
/* ---- task #180 step 4b-2, diagnostic b3 (built with -DME_WATCH; MGS2_ME_PIC_WATCH=1 with MGS2_ME_PIC_CHK=1): the read lines the check
   finds changed (the first MPW_N) are watched from then on.  When a later copy's plan holds one, its 64 bytes as the transfer ends are
   kept, and every checkpoint (runtime/ee_io.h ME_WCK: a transfer's start and end, a guarded write's logged line, a kernel call, a thread
   switch, an interrupt handler or alarm before and after, the vblank hook's parts, the native qword copy before and after, the deadline)
   compares the line with them: a change is printed with the checkpoints around it and the words that changed.  A line a guard logs is
   covered by the log from there on and is let go; at the deadline every line is let go.  A line the check finds changed although RAM
   still holds its bytes as the transfer ended is printed with the log's entries for it. ---- */
#define MPW_N 32
typedef struct { uint32_t l; int armed, letgo, t, ch, seen; uint32_t tadr; long vb; uint8_t b[64]; } MpW;
static MpW mpw[MPW_N]; static int mpw_n, mpw_on; static uint8_t *mpw_bm; int g_mpw_armed;
static int mpw_lid; static uint32_t mpw_la, mpw_lb; static long mpw_lvb;
static long mpw_prn, mpw_chg, mpw_logd, mpw_d_chg, mpw_d_same, mpw_d_ram, mpw_d_new, mpw_d_logd, mpw_d_other, mpw_iv[20][20];
static const char *const mpw_name[20] = { "the plan", "a transfer's start (ch, madr)", "a transfer's end (ch, record)",
  "a guarded write's logged line (who, line)", "a kernel call (number, pc)", "a thread switch (from, to)", "a handler's start (fn, cause)",
  "a handler's end (fn, cause)", "an alarm's start (fn, alarm)", "an alarm's end (fn, alarm)", "the hook: before io_vblank (start, vb)",
  "the hook: after io_vblank (start, vb)", "the hook: after opt_window (-, vb)", "the hook: after sif_vblank (-, vb)",
  "the hook: before host_vb_end (-, vb)", "the deadline: before the fill (why, records)", "the native qword copy: before (dst, qwords)",
  "the native qword copy: after (dst, qwords)", "?", "?" };
static void mpw_change(MpW *w, const uint8_t *p, int id, uint32_t a, uint32_t b)
{
  const uint32_t *o = (const uint32_t *)w->b, *n = (const uint32_t *)p; int nd = 0, k0 = -1;
  for (int k = 0; k < 16; k++) if (o[k] != n[k]) { if (k0 < 0) k0 = k; nd++; }
  mpw_chg++; w->seen++; mpw_iv[mpw_lid][id]++;
  if (mpw_prn < 400) {
    mpw_prn++;
    fprintf(stderr, "me picture watch: vb %ld: line %07x (record %d, ch %d, tadr %08x, planned at vb %ld) changed between %s %x %x (vb %ld) and %s "
            "%x %x: %d words, from +%02x:", me_vb, (unsigned)(w->l << 6), w->t, w->ch, (unsigned)w->tadr, w->vb, mpw_name[mpw_lid], (unsigned)mpw_la,
            (unsigned)mpw_lb, mpw_lvb, mpw_name[id], (unsigned)a, (unsigned)b, nd, 4 * k0);
    for (int k = 0, m = 0; k < 16 && m < 6; k++) if (o[k] != n[k]) { fprintf(stderr, " +%02x %08x>%08x", 4 * k, (unsigned)o[k], (unsigned)n[k]); m++; }
    fprintf(stderr, "\\n"); }
  memcpy(w->b, p, 64);
}
void rt_me_watch(int id, uint32_t a, uint32_t b)   /* runtime/ee_io.h ME_WCK: a checkpoint (called only while a line is armed) */
{
  for (int i = 0; i < mpw_n; i++) {
    MpW *w = &mpw[i]; const uint8_t *p;
    if (!w->armed) continue;
    p = M.mem + ((size_t)w->l << 6);
    if (memcmp(p, w->b, 64)) mpw_change(w, p, id, a, b);
  }
  mpw_lid = id; mpw_la = a; mpw_lb = b; mpw_lvb = me_vb;
}
static MpW *mpw_find(uint32_t l) { for (int i = 0; i < mpw_n; i++) if (mpw[i].l == l) return &mpw[i]; return 0; }
static int mpw_bit(uint32_t l) { return (mpw_bm[l >> 3] >> (l & 7)) & 1; }
static void mpw_arm(const MpRec *e, uint32_t l)   /* mp_chk_plan: a watched line in a copy planned now (as its transfer ends) */
{
  MpW *w = mpw_find(l);
  if (!w || w->armed) return;
  w->armed = 1; w->letgo = 0; g_mpw_armed++; w->t = e->t; w->ch = e->ch; w->tadr = e->tadr; w->vb = me_vb; w->seen = 0;
  memcpy(w->b, M.mem + ((size_t)l << 6), 64);
  mpw_lid = 2; mpw_la = (uint32_t)e->ch; mpw_lb = (uint32_t)e->t; mpw_lvb = me_vb;   /* (xlive_chk_end checked the lines armed before) */
}
static void mpw_letgo(void) { for (int i = 0; i < mpw_n; i++) mpw[i].armed = 0; g_mpw_armed = 0; }
static void mpw_logged(uint32_t l, int who)   /* mg_lines: a guard logs line l (in the filter) before it writes */
{
  MpW *w;
  if (!mpw_bit(l) || !(w = mpw_find(l)) || !w->armed) return;
  rt_me_watch(3, (uint32_t)who, l << 6);
  w->armed = 0; w->letgo = 1; g_mpw_armed--; mpw_logd++;
}
static void mpw_diff(const MpRec *e, uint32_t a, uint32_t f, const uint8_t *cp)   /* mp_chk_fill: a read line of e's copy differs */
{
  uint32_t l = a >> 6; MpW *w = mpw_bit(l) ? mpw_find(l) : 0;
  if (!w) {
    if (mpw_n < MPW_N) { w = &mpw[mpw_n++]; memset(w, 0, sizeof *w); w->l = l; mpw_bm[l >> 3] |= (uint8_t)(1u << (l & 7)); }
    mpw_d_new++; return; }
  if (w->t != e->t || (!w->armed && !w->letgo)) { mpw_d_other++; return; }
  if (w->letgo) { mpw_d_logd++; return; }
  if (w->seen) { mpw_d_chg++; return; }
  { const uint8_t *r = M.mem + a; const uint32_t *cw = (const uint32_t *)cp, *rw = (const uint32_t *)r; int same = !memcmp(M.mem + ((size_t)l << 6), w->b, 64), k0 = -1;
    if (same) mpw_d_same++; else mpw_d_ram++;
    for (uint32_t k = 0; k < (f - a) / 4; k++) if (cw[k] != rw[k]) { k0 = (int)k; break; }
    if (mpw_prn < 400) {
      mpw_prn++;
      fprintf(stderr, "me picture watch: vb %ld: line %07x+%u (record %d, ch %d, tadr %08x) differs in the deadline copy with no change seen at a "
              "checkpoint; RAM now %s the bytes as the transfer ended; the copy against RAM: %s", me_vb, (unsigned)a, (unsigned)(f - a), e->t, e->ch,
              (unsigned)e->tadr, same ? "holds" : "differs from", k0 < 0 ? "equal" : "differs");
      if (k0 >= 0) fprintf(stderr, " from +%02x (%08x, RAM %08x)", (unsigned)(4 * k0 + (a & 63)), (unsigned)cw[k0], (unsigned)rw[k0]);
      fprintf(stderr, "; the log's entries for the line:");
      for (int i = mp_nlog - 1, m = 0; i >= 0 && m < 6; i--) if (mp_log[i].l == l) {
        m++; fprintf(stderr, " %d (t %d, its bytes %s)", i, (int)mp_log[i].t, memcmp(mp_log[i].old, w->b, 64) ? "differ from the plan's" : "= the plan's"); }
      fprintf(stderr, "\\n"); } }
}
#endif
""")

# 2. the second walk's arrays: xmc's work arrays (memory), and the watch's set-up
rep("""      xmr.ra = malloc((size_t)cap * sizeof(XcR)); xmr.rb = malloc((size_t)cap * sizeof(XcR)); xmr.mk = malloc((size_t)cap * sizeof(XcR));
      xmr.tmp = malloc((size_t)cap * sizeof(XcR)); xmr.ext = malloc((size_t)cap * sizeof(XvExt)); mp_chr = malloc((size_t)mp_chcap);
      if (!xmr.ra || !xmr.rb || !xmr.mk || !xmr.tmp || !xmr.ext || !mp_chr) {
        fprintf(stderr, "me picture check: no memory for its second walk: every line counted as read\\n");
        free(xmr.ra); free(xmr.rb); free(xmr.mk); free(xmr.tmp); free(xmr.ext); free(mp_chr); mp_chr = 0; } }
""", """      xmr.ra = xmc.ra; xmr.rb = xmc.rb; xmr.mk = xmc.mk; xmr.tmp = xmc.tmp;   /* b3: xmc's work arrays, free once its own plan is made (nmk
                                                                                  0 here: mk is not read), 512 KB less than b2 */
      xmr.ext = malloc((size_t)cap * sizeof(XvExt)); mp_chr = malloc((size_t)mp_chcap);
      if (!xmr.ext || !mp_chr) {
        fprintf(stderr, "me picture check: no memory for its second walk: every line counted as read\\n");
        free(xmr.ext); free(mp_chr); mp_chr = 0; } }
#ifdef ME_WATCH
    mpw_on = mp_chk && (e = mgs2_getenv("MGS2_ME_PIC_WATCH")) && atoi(e) && (mpw_bm = calloc(XL_GSZ / 64 / 8, 1));
    if (mpw_on) fprintf(stderr, "me picture watch (diagnostic b3): on: the first %d read lines the check finds changed are watched at the "
                        "checkpoints from then on (MGS2_ME_PIC_WATCH)\\n", MPW_N);
#endif
""")

# 3. arming in mp_chk_plan
rep("""      mp_ch[mp_chn + n++] = mp_hash(M.mem + a, f - a); a = f; }
  e->nh = n; mp_chn += n;
""", """#ifdef ME_WATCH
      if (mpw_on && mpw_bit(a >> 6)) mpw_arm(e, a >> 6);
#endif
      mp_ch[mp_chn + n++] = mp_hash(M.mem + a, f - a); a = f; }
  e->nh = n; mp_chn += n;
""")

# 4. mp_chk_fill: window counts and the watch
rep("""        mp_chk_diff++; bad = 1; if (rd) mp_chk_drd++; else mp_chk_dmg++;
""", """        mp_chk_diff++; bad = 1; if (rd) mp_chk_drd++; else mp_chk_dmg++;
        { long w = me_vb / 100; if (w != mpc_w) { mpc_flush(); mpc_w = w; } if (rd) mpc_rd++; else mpc_mg++; }   /* b3 */
#ifdef ME_WATCH
        if (mpw_on && rd) mpw_diff(e, a, f, xmc.arena + x[i].off + (a - x[i].a0));
#endif
""")
rep("  mp_chk_rec += bad;\n", "  mp_chk_rec += bad; mpc_rec += bad;\n")

# 5. mp_fill_all: the deadline's checkpoint first, every line let go after
rep("""static void mp_fill_all(int why)
{
  int n = 0;
""", """static void mp_fill_all(int why)
{
  int n = 0;
#ifdef ME_WATCH
  if (g_mpw_armed) rt_me_watch(15, (uint32_t)why, (uint32_t)mp_nrec);
#endif
""")
rep("""  if (why) mp_fill_early[why - 1] += n; else mp_fill_dl += n;
""", """  if (why) mp_fill_early[why - 1] += n; else mp_fill_dl += n;
#ifdef ME_WATCH
  if (mpw_on) mpw_letgo();
#endif
""")

# 6. mg_lines: a watched line a guard logs
rep("""    { XmLogE *e = &mp_log[mp_nlog++]; e->l = l; e->t = mp_seg; memcpy(e->old, M.mem + ((size_t)l << 6), 64); }
""", """#ifdef ME_WATCH
    if (mpw_on && g_mpw_armed) mpw_logged(l, who);
#endif
    { XmLogE *e = &mp_log[mp_nlog++]; e->l = l; e->t = mp_seg; memcpy(e->old, M.mem + ((size_t)l << 6), 64); }
""")

# 7. checkpoints at a transfer's end and start
rep("""  MpRec *e = &mp_rec[mp_nrec - 1]; int r, x0 = 0, nx = 0;
  (void)ch; g_xl_img = g_xl_end = 0;
""", """  MpRec *e = &mp_rec[mp_nrec - 1]; int r, x0 = 0, nx = 0;
  (void)ch; g_xl_img = g_xl_end = 0;
#ifdef ME_WATCH
  if (g_mpw_armed) rt_me_watch(2, (uint32_t)ch, (uint32_t)e->t);
#endif
""")
rep("""void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
  if (!(g_opt & OPT_XL)) { if (state == 1) state = 2; return; }
""", """void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
#if defined(ME_WATCH) && defined(MGS2_GL)
  if (g_mpw_armed) rt_me_watch(1, (uint32_t)ch, madr);
#endif
  if (!(g_opt & OPT_XL)) { if (state == 1) state = 2; return; }
""")

# 8. the report
rep("""      if (mp_chk) fprintf(f, "me picture check: %ld lines""", """      if (mp_chk) mpc_flush();   /* b3: the last window */
      if (mp_chk) fprintf(f, "me picture check: %ld lines""")
open(p,'w').write(s)
print('ok', hashlib.md5(s.encode()).hexdigest())
