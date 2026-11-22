# gl21 debug (not for delivery): MGS2_ME_PIC_RCHK=1: the image data left in RAM (marks outside xc_keep's two ranges), hashed as each
# transfer ends; checked at the next step's first plan (gl20's walk time) and at the next hook (the ring's latest walk time)
import sys
p = sys.argv[1] + '/psp/xl_psp.c'; s = open(p).read()
def rep(old, new):
    global s
    assert s.count(old) == 1, old[:70]; s = s.replace(old, new)
rep("""/* gl21: xm_plan in the ring:""", """/* gl21 debug (MGS2_ME_PIC_RCHK=1, the emulator only): the image data the walk reads from RAM (marks outside xm_plan's two copied
   ranges), hashed as the transfer ends; checked again at the step after's first plan (gl20's walk) and at the hook after (the ring's) */
#define RC_N 16384
typedef struct { uint32_t k0, k1; uint64_t h; } RcP;
static int rc_on, rc_cn, rc_pn, rc_due, rc_logn; static RcP *rc_c, *rc_p; static long rc_bad[2], rc_chk[2], rc_over; static uint64_t rc_badb[2];
static uint64_t rc_hash(uint32_t a0, uint32_t a1)
{
  uint64_t h = 0xcbf29ce484222325ull; const uint8_t *b = M.mem;
  for (uint32_t a = a0; a < a1; a += 4) { uint32_t w; memcpy(&w, b + a, 4); h = (h ^ w) * 0x100000001b3ull; }
  return h;
}
static void rc_add(uint32_t a, uint32_t b) { if (b <= a) return; if (rc_cn == RC_N) { rc_over++; return; } rc_c[rc_cn].k0 = a; rc_c[rc_cn].k1 = b; rc_c[rc_cn].h = rc_hash(a, b); rc_cn++; }
static void rc_note(void)   /* a transfer's end, before its plan: its marks less the two ranges */
{
  static const uint32_t kp[2][2] = { { 0x12c0000u, 0x1430000u }, { 0x1ed0000u, 0x1ee0000u } };
  if (xmc.mk_full) return;   /* (its image data copied whole) */
  for (int i = 0; i < xmc.nmk; i++) {
    uint32_t a = xmc.mk[i].k0, b = xmc.mk[i].k1;
    for (int j = 0; j < 2 && a < b; j++) {
      if (kp[j][1] <= a || kp[j][0] >= b) continue;
      if (kp[j][0] > a) rc_add(a, kp[j][0]);
      a = kp[j][1];
    }
    if (a < b) rc_add(a, b);
  }
}
static void rc_check(int late)
{
  for (int i = 0; i < rc_pn; i++) {
    const RcP *q = &rc_p[i]; rc_chk[late]++;
    if (rc_hash(q->k0, q->k1) == q->h) continue;
    rc_bad[late]++; rc_badb[late] += q->k1 - q->k0;
    if (rc_logn < 80) { rc_logn++; fprintf(stderr, "rchk: vb %ld at the %s: image data %07x-%07x (%u bytes) changed since its transfer\\n", me_vb,
                                          late ? "hook after (the ring's walk)" : "next step's first plan (gl20's walk)", (unsigned)q->k0,
                                          (unsigned)q->k1, (unsigned)(q->k1 - q->k0)); }
  }
}
static void rc_hook(void) { RcP *t; if (!rc_on) return; rc_check(1); t = rc_p; rc_p = rc_c; rc_c = t; rc_pn = rc_cn; rc_cn = 0; rc_due = 1; }
/* gl21: xm_plan in the ring:""")
rep("""  if (mq_ring) mq_ring_pop();            /* gl21: the walks before may still run (their bytes stay theirs) */""",
    """  if (rc_on) { if (rc_due) { rc_check(0); rc_due = 0; } rc_note(); }   /* (gl21 debug) */
  if (mq_ring) mq_ring_pop();            /* gl21: the walks before may still run (their bytes stay theirs) */""")
rep("""  if (mq_ring && mq_m4) mq_ring_pop();   /* gl21:""", """  if (rc_on && mq_m4 && !mq_self) rc_hook();   /* (gl21 debug) */
  if (mq_ring && mq_m4) mq_ring_pop();   /* gl21:""")
rep("""  mq_turn_k = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0; mq_ring_reset();   /* (gl21) */""",
    """  mq_turn_k = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0; mq_ring_reset();   /* (gl21) */
  rc_cn = rc_pn = 0; rc_due = 0;   /* (gl21 debug) */""")
rep("""  mq_ring = mq_ring_cfg == 1; mq_an""", """  if ((e = mgs2_getenv("MGS2_ME_PIC_RCHK")) && atoi(e) && mp_want == 4) {   /* (gl21 debug) */
    rc_c = malloc(RC_N * sizeof *rc_c); rc_p = malloc(RC_N * sizeof *rc_p); rc_on = rc_c && rc_p;
    fprintf(stderr, "rchk (gl21 debug): %s\\n", rc_on ? "on" : "no memory: off"); }
  mq_ring = mq_ring_cfg == 1; mq_an""")
rep("""  if (mq_m4)
    fprintf(f, "me picture ring (gl21, MGS2_ME_PIC_RING):""", """  if (rc_on)
    fprintf(f, "rchk (gl21 debug): image pieces checked at the next first plan %ld (changed %ld, %llu KB), at the hook after %ld (changed %ld, "
            "%llu KB); pieces not kept (full) %ld\\n", rc_chk[0], rc_bad[0], (unsigned long long)(rc_badb[0] >> 10), rc_chk[1], rc_bad[1],
            (unsigned long long)(rc_badb[1] >> 10), rc_over);
  if (mq_m4)
    fprintf(f, "me picture ring (gl21, MGS2_ME_PIC_RING):""")
open(p, 'w').write(s); print('ok')
