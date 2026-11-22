# gl21: the ring (MGS2_ME_PIC_RING=1): two or more frames share the arena and the extents, so a frame's walk on the ME runs while the
# next step plans its copies.  Main-CPU side only (psp/xl_psp.c); ge/ untouched.  Usage: python3 -I apply.py <tree>
import sys, hashlib
p = sys.argv[1] + '/psp/xl_psp.c'
s = open(p).read()
assert hashlib.md5(s.encode()).hexdigest().startswith('449d8317'), 'xl_psp.c is not gl20s'
def rep(old, new, n=1):
    global s
    c = s.count(old); assert c == n, (c, old[:80]); s = s.replace(old, new)

# 1. state
rep("""static uint32_t mq_walk_seq, mq_thin_seq; static int mq_walk_due, mq_thin_due;   /* the last WALK's and THIN's places in the queue */
""", """static uint32_t mq_walk_seq, mq_thin_seq; static int mq_walk_due, mq_thin_due;   /* the last WALK's and THIN's places in the queue */
/* gl21: the ring (MGS2_ME_PIC_RING=1; 0, the default: gl20's one frame at a time).  The arena's bytes and the extents are shared, in the
   order they came, by the frames whose walks are posted and the records not yet walked; a frame's walk runs on the ME while the next step
   plans its copies, so the step's first plan no longer waits for it (mq_chk_end).  A copy that finds no room at the heads goes to the
   start (a wrap) when the oldest live bytes leave room there, else the main CPU waits for the oldest walk and tries again; with no walk
   left it takes gl20's way (the records before walked now).  me_handoff_design.md 4.6: a frame's walk done by the next hook's FILL at the
   latest (the queue's order), as the design's x86 split had it (section 5: the picture as late as the end of the next step) */
#define MQ_RF 16
static int mq_ring; static struct { uint32_t seq, a; int x; } mq_rf[MQ_RF]; static int mq_rfn;   /* the walks posted and not seen done,
                                                                                                  the oldest first: place, bytes' start */
static uint32_t mq_ra, mq_ra0, mq_an; static int mq_rx, mq_rx0, mq_xn;   /* the heads, the unwalked records' start, the sizes */
static long mq_r_wrap[2], mq_r_wait, mq_r_late, mq_r_plans; static int mq_r_fmax, mq_r_xmax; static uint32_t mq_r_amax;
""")

# 2. the ring's functions, before mq_arena_wait; mq_arena_wait empties the ring
rep("""static void mq_arena_wait(void)   /* the walks posted last are done: the arena and its extents are the main CPU's again */
{
  if (!mq_walk_due) return;
  mq_wait(mq_walk_seq, MW_ARENA);
  if (xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes */
  if (xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }
  mq_walk_due = 0; xmc.next = 0; xmc.used = 0;
}
""", """static void mq_ring_pop(void)   /* gl21: the walks seen done leave the ring; nothing live: the heads back at the start */
{
  int k = 0;
  while (k < mq_rfn && mq_done(mq_rf[k].seq)) k++;
  if (k) { for (int i = k; i < mq_rfn; i++) mq_rf[i - k] = mq_rf[i]; mq_rfn -= k; }
  if (!mq_rfn && mq_ra == mq_ra0 && mq_rx == mq_rx0) { mq_ra = mq_ra0 = 0; mq_rx = mq_rx0 = 0; }
}
static void mq_ring_reset(void) { mq_rfn = 0; mq_ra = mq_ra0 = 0; mq_rx = mq_rx0 = 0; }
static void mq_ring_push(uint32_t seq)   /* gl21: a WALK posted: the records so far are its region; the next ones start at the heads */
{
  if (mq_rfn == MQ_RF) { mq_wait(mq_rf[0].seq, MW_ARENA); mq_ring_pop(); }   /* (not expected: a step posts one or two walks) */
  if (mq_rfn == MQ_RF) { mq_wait(seq - 1, MW_ARENA); mq_rfn = 0; }   /* (a take-back meanwhile) */
  mq_rf[mq_rfn].seq = seq; mq_rf[mq_rfn].a = mq_ra0; mq_rf[mq_rfn].x = mq_rx0; mq_rfn++;
  if (mq_rfn > mq_r_fmax) mq_r_fmax = mq_rfn;
  mq_ra0 = mq_ra; mq_rx0 = mq_rx;
}
/* gl21: xm_plan in the ring: at the heads, below the oldest live bytes (or the end); then at the start below them (a wrap); else the
   oldest walk waited for and the plan tried again.  The return: xm_plan's (2 or 3: no room with no walk left).  xm_plan run again on
   the same transfer gives the same plan (its marks are kept: mq_chk_end's no-room path runs it twice already) */
static int mq_ring_plan(const MpRec *e, int *x0, int *nx)
{
  long tb = mq_tbk_cold; int r;
  for (;;) {
    uint32_t ta = mq_rfn ? mq_rf[0].a : mq_ra0; int tx = mq_rfn ? mq_rf[0].x : mq_rx0, wa = 0, wx = 0;
    for (;;) {
      uint32_t ha = wa ? 0 : mq_ra; int hx = wx ? 0 : mq_rx;
      xmc.used = ha; xmc.arena_n = ha >= ta && !wa ? mq_an : ta - 1;
      xmc.next = hx; xmc.ext_cap = hx >= tx && !wx ? mq_xn : tx - 1;
      r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, x0, nx); mq_r_plans++;
      if (r == 2 && !wx && mq_rx >= tx && tx > 1) { xmc.fails[2]--; wx = 1; continue; }   /* the extents at the start */
      if (r == 3 && !wa && mq_ra >= ta && ta > 64) { xmc.fails[3]--; wa = 1; continue; }   /* the bytes at the start */
      break;
    }
    if (!r) {
      uint32_t ua; int ux;
      if (wa) mq_r_wrap[0]++;
      if (wx) mq_r_wrap[1]++;
      mq_ra = xmc.used; mq_rx = xmc.next;
      ua = mq_ra >= ta ? mq_ra - ta : mq_an - ta + mq_ra; ux = mq_rx >= tx ? mq_rx - tx : mq_xn - tx + mq_rx;   /* (live, at most) */
      if (ua > mq_r_amax) { mq_r_amax = ua; mq_arena_max = ua; mq_arena_max_vb = me_vb; }
      if (ux > mq_r_xmax) { mq_r_xmax = ux; mq_ext_max = ux; mq_ext_max_vb = me_vb; }
      break;
    }
    if ((r != 2 && r != 3) || !mq_rfn) break;
    xmc.fails[r]--; mq_r_wait++;
    mq_wait(mq_rf[0].seq, MW_ARENA);   /* the oldest walk: its bytes and extents free */
    if (mq_tbk_cold != tb) break;
    if (!mq_done(mq_rf[0].seq)) break;   /* (a warm take-back: the main CPU's turn now) */
    mq_ring_pop();
  }
  xmc.arena_n = mq_an; xmc.ext_cap = mq_xn;
  return r;
}
static void mq_arena_wait(void)   /* the walks posted last are done: the arena and its extents are the main CPU's again */
{
  if (!mq_walk_due) return;
  mq_wait(mq_walk_seq, MW_ARENA);
  if (mq_ring) { mq_walk_due = 0; mq_ring_reset(); xmc.next = 0; xmc.used = 0; return; }   /* gl21: every walk done: the ring empty */
  if (xmc.next > mq_ext_max) { mq_ext_max = xmc.next; mq_ext_max_vb = me_vb; }   /* step 4d: for 4d-3's sizes */
  if (xmc.used > mq_arena_max) { mq_arena_max = xmc.used; mq_arena_max_vb = me_vb; }
  mq_walk_due = 0; xmc.next = 0; xmc.used = 0;
}
""")

# 3. the cfg line and the sizes
rep("""  mq_turn = (e = mgs2_getenv("MGS2_ME_PIC_TURN")) && atoi(e) > 0 ? atoi(e) : 0;
""", """  mq_turn = (e = mgs2_getenv("MGS2_ME_PIC_TURN")) && atoi(e) > 0 ? atoi(e) : 0;
  mq_ring = (e = mgs2_getenv("MGS2_ME_PIC_RING")) && atoi(e); mq_an = xmc.arena_n; mq_xn = xmc.ext_cap; mq_ring_reset();   /* gl21 */
""")

# 4. take-backs and the stop: the ring empty
rep("""  g_g0_on = 0; g_xl_img = g_xl_end = 0; mp_nrec = 0; mp_unfilled = 0; mp_nlog = 0; mq_walk_due = 0; mq_thin_due = 0; thin_due = 0;
""", """  g_g0_on = 0; g_xl_img = g_xl_end = 0; mp_nrec = 0; mp_unfilled = 0; mp_nlog = 0; mq_walk_due = 0; mq_thin_due = 0; thin_due = 0;
  mq_ring_reset();   /* gl21 */
""")
rep("""  xmc.next = 0; xmc.used = 0; mq_walk_due = 0; mq_thin_due = 0; g_g0_on = 0;
""", """  xmc.next = 0; xmc.used = 0; mq_walk_due = 0; mq_thin_due = 0; g_g0_on = 0; mq_ring_reset();   /* (gl21) */
""")

# 5. mq_late: the walk joins the ring
rep("""  mq_walk_seq = mq_post(&e); mq_walk_due = 1;
  mp_nrec = 0; mp_late_runs++; mp_chn = 0;
  if (wait) mq_arena_wait();
""", """  mq_walk_seq = mq_post(&e); mq_walk_due = 1;
  if (mq_ring && !mq_self) { mq_ring_push(mq_walk_seq); if (wait) mq_r_late++; }   /* gl21 */
  mp_nrec = 0; mp_late_runs++; mp_chn = 0;
  if (wait) mq_arena_wait();
""")

# 6. mq_chk_end: no wait at the step's first plan; the ring's plan
rep("""  { uint64_t ov = mq_now(); mq_ov_us += (ov - mq_ov_t) & (0 - (uint64_t)mq_ov_f); mq_ov_n += mq_ov_f; mq_ov_f = 0; }   /* gl20: the overlap (no branch) */
  mq_arena_wait();                       /* (no plan while the ME walks the records before) */
  if (mq_tbk_cold != tb || !mp_nrec) return;   /* (a take-back: the record dropped) */
  e = &mp_rec[mp_nrec - 1];
  r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before laid and walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mq_late(3, 1);
    if (mq_tbk_cold != tb) return;
    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  }
""", """  { uint64_t ov = mq_now(); mq_ov_us += (ov - mq_ov_t) & (0 - (uint64_t)mq_ov_f); mq_ov_n += mq_ov_f; mq_ov_f = 0; }   /* gl20: the overlap (no branch) */
  if (mq_ring) mq_ring_pop();            /* gl21: the walks before may still run (their bytes stay theirs) */
  else mq_arena_wait();                  /* (no plan while the ME walks the records before) */
  if (mq_tbk_cold != tb || !mp_nrec) return;   /* (a take-back: the record dropped) */
  e = &mp_rec[mp_nrec - 1];
  r = mq_ring ? mq_ring_plan(e, &x0, &nx) : xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
  if (mq_tbk_cold != tb) return;
  if ((r == 2 || r == 3) && mp_nrec > 1) {   /* no room: the records before laid and walked now, the copy planned again */
    MpRec keep = *e; mp_drain_room++; mp_nrec--; mq_late(3, 1);
    if (mq_tbk_cold != tb) return;
    mp_rec[0] = keep; mp_nrec = 1; e = &mp_rec[0];
    r = mq_ring ? mq_ring_plan(e, &x0, &nx) : xm_plan(&xmc, e->chcr, e->madr, e->qwc, e->tadr, &x0, &nx);
    if (mq_tbk_cold != tb) return;
  }
""")

# 7. the turns: the ring empty at each change; the hook: no wait for the walk before in the ring
rep("""  mq_turn_k = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0;
}""", """  mq_turn_k = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0; mq_ring_reset();   /* (gl21) */
}""")
rep("""  mq_probe();
  mq_arena_wait();
  if (!mq_m4 && mq_turn""", """  mq_probe();
  if (mq_ring && mq_m4) mq_ring_pop();   /* gl21: the walk before may run on (the FILL below waits for it in the queue's order) */
  else mq_arena_wait();
  if (!mq_m4 && mq_turn""")

# 8. the report
rep("""  if (mq_m4)
    fprintf(f, "me picture loop (step 4d-4): the loop's cache steps through the hook:""", """  if (mq_m4)
    fprintf(f, "me picture ring (gl21, MGS2_ME_PIC_RING): %s; plans %ld, wraps to the start %ld (bytes) and %ld (extents), waits for the "
            "oldest walk %ld, records before walked now (no walk left) %ld; walks in flight at most %d; live at most %u KB of %u and %d extents "
            "of %d (the 4d line above: the same, in the ring)\\n", mq_ring ? "on" : "off (gl20's one frame at a time)", mq_r_plans, mq_r_wrap[0],
            mq_r_wrap[1], mq_r_wait, mq_r_late, mq_r_fmax, (unsigned)(mq_r_amax >> 10), (unsigned)(mq_an >> 10), mq_r_xmax, mq_xn);
  if (mq_m4)
    fprintf(f, "me picture loop (step 4d-4): the loop's cache steps through the hook:""")
open(p, 'w').write(s)
print('applied:', hashlib.md5(s.encode()).hexdigest())
