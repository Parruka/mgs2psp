/* me_ep_me.c - the ME's eDRAM probe, the Media Engine's side (2026-11-03, task #150; mgs2/me_edram_probe_notes.md).  ep_work is the
   loop psp/me_ep.c starts on the ME with the port's bring-up (psp/me/hle_me_mgs2.c: Sony's start, kernel helpers at their kernel
   alias, as v30me6c and gl14mp).  Jobs the main CPU posts: the clock's calibration, gl14mp's kernels (psp/me_probe_kern.c) on main
   RAM or eDRAM addresses, the eDRAM filled with a pattern or checked against it, copies (main RAM -> main RAM, and main RAM -> eDRAM
   with plain stores, claimed lines or uncached stores), an eDRAM range compared with a main RAM range, a cache flush, and (only in a
   build made with -DEP_REG_B or -DEP_REG_C, each after Ffm's yes on the card of 2026-11-03) turn B's or turn C's register writes.
   The stream of the game's turns copies the 3,456 KB window of game RAM into the eDRAM's first MB (or into the main RAM ring, gl14mp's
   burst, for the same run's reference) as fast as it can, and after each 64 KB block checks 4 KB of the sentinel, the pattern in the
   rest of the eDRAM; with no stream it rests (psp/me_rest.S, as gl14mp).  Its own CPU register it writes: Count (once, when its loop
   starts).  Memory it writes: the eDRAM, its region (stack), the ring, the control block (uncached).  Game RAM it only reads, through
   its cache: those lines are never dirty there, so nothing of the game's is written back.  hle_me.h's rules: no syscall, no libc, no
   function pointer or jump table, shared words through the uncached alias only.  The eDRAM starts at address 0: the build gives this
   file -fno-delete-null-pointer-checks, so an access at offset 0 is made as written (and checks that the unit holds no trap).
   Probe 2 (a build made with -DEP_P2, task #163): the eDRAM through the kernel segments (me_ep.h), the progress word EpCtl.prog,
   and two more jobs: the one-word tests (EP_J_WORD) and reads of registers (EP_J_SNAP; reads only).  It writes no register more. */
#include <stdint.h>
#include "me_ep.h"

void me_rest_init(void);
void me_rest(void);
extern char me_rest_halt[];
#define MW(i) (((volatile uint32_t *)0xBFC00400u)[i])   /* me_rest.S's words ([12..15]) */
#define HW(a) (*(volatile uint32_t *)(uintptr_t)(a))
/* probe 2's progress word (nothing in a build without -DEP_P2) */
#ifdef EP_P2
#define EP_PROG(c, a) ((c)->prog = (uint32_t)(uintptr_t)(a))
#define EP_FILL_C , volatile EpCtl *c
#define EP_FILL_PASS , c
#else
#define EP_PROG(c, a) ((void)0)
#define EP_FILL_C
#define EP_FILL_PASS
#endif

static inline void dwbinv_range(const void *p, uint32_t n) { hle_me_dwb(p, n); hle_me_dinval(p, n); hle_me_sync(); }
static inline volatile uint32_t *edram(uint32_t off) { return (volatile uint32_t *)(uintptr_t)(EP_EDRAM + off); }

/* the compiler's own memset / memcpy calls, renamed by the build (objcopy --redefine-sym), as gl14mp */
void *me_memset(void *d, int c, unsigned n) { volatile uint8_t *p = (volatile uint8_t *)d; while (n--) *p++ = (uint8_t)c; return d; }
void *me_memcpy(void *d, const void *s, unsigned n)
{
  volatile uint8_t *p = (volatile uint8_t *)d; const volatile uint8_t *q = (const volatile uint8_t *)s;
  while (n--) *p++ = *q++;
  return d;
}

/* [off, off + n) of the eDRAM filled with the pattern of seed, then written back and dropped from the cache, so the words are in the
   eDRAM itself */
static void fill(uint32_t off, uint32_t n, uint32_t seed EP_FILL_C)
{
  volatile uint32_t *p = edram(off); uint32_t i;
  EP_PROG(c, p);
  for (i = 0; i < n / 4u; i++) { p[i] = ep_pat(off + 4u * i, seed); if (!((i + 1u) & 0x3FFFu)) EP_PROG(c, p + i + 1u); }
  EP_PROG(c, (uint32_t)(uintptr_t)p + 1u);
  dwbinv_range((const void *)p, n);
}
/* [off, off + n) checked against the pattern of seed (its lines dropped from the cache first, so the words are read from the eDRAM):
   r[0] words changed, r[1] the first one's offset (0xFFFFFFFF: none), r[2] its value, r[3] the pattern's, r[4] / r[5] the 64 KB blocks
   with a change (bit b: offset b x 64 KB) */
static void check(volatile EpCtl *c, uint32_t off, uint32_t n, uint32_t seed)
{
  const volatile uint32_t *p = edram(off); uint32_t i, bad = 0, fo = 0xFFFFFFFFu, fv = 0, fe = 0, b0 = 0, b1 = 0;
  hle_me_dinval((const void *)p, n); hle_me_sync();
  EP_PROG(c, p);
  for (i = 0; i < n / 4u; i++) {
    uint32_t o = off + 4u * i, e = ep_pat(o, seed), v = p[i];
    if (v != e) {
      uint32_t b = o >> 16;
      if (!bad) { fo = o; fv = v; fe = e; }
      bad++;
      if (b < 32u) b0 |= 1u << b; else if (b < 64u) b1 |= 1u << (b - 32u);
    }
    if (!((i + 1u) & 0x3FFFu)) EP_PROG(c, p + i + 1u);
  }
  c->r[0] = bad; c->r[1] = fo; c->r[2] = fv; c->r[3] = fe; c->r[4] = b0; c->r[5] = b1;
}

typedef struct EpLoc { uint32_t off, ap, rp, sp; } EpLoc;   /* positions: source window, arena, ring, sentinel */

/* the sentinel: the next 4 KB checked against its pattern (all run counts in the control block) */
static void sentinel_step(volatile EpCtl *c, EpLoc *l)
{
  uint32_t n = c->sent_bytes, so = c->sent_off, seed = c->sent_seed, k = EP_SENT_PER_BLOCK, i, bad = 0;
  const volatile uint32_t *p;
  if (n < EP_SENT_PER_BLOCK) return;
  if (l->sp + k > n) l->sp = 0;
  p = edram(so + l->sp);
  EP_PROG(c, p);
  hle_me_dinval((const void *)p, k); hle_me_sync();
  for (i = 0; i < k / 4u; i++) {
    uint32_t o = so + l->sp + 4u * i, v = p[i];
    if (v != ep_pat(o, seed)) {
      uint32_t b = o >> 16;
      bad++;
      if (c->sent_first_off == 0xFFFFFFFFu) { c->sent_first_off = o; c->sent_first_val = v; }
      if (b < 32u) c->sent_blk[0] = c->sent_blk[0] | (1u << b); else if (b < 64u) c->sent_blk[1] = c->sent_blk[1] | (1u << (b - 32u));
    }
  }
  l->sp += k;
  c->sent_kb = c->sent_kb + (k >> 10); c->sent_bad = c->sent_bad + bad;
}

/* one block of the stream: 1,024 lines of the source window (64 KB) copied into the arena (the eDRAM's first MB, by c->kind) or the
   main RAM ring (EP_C_MAIN), then 4 KB of the sentinel checked */
static void stream_block(volatile EpCtl *c, EpLoc *l)
{
  const uint8_t *src = (const uint8_t *)(uintptr_t)c->src; uint8_t *ring = (uint8_t *)(uintptr_t)c->ring;
  uint32_t sb = c->src_bytes, rb = c->ring_bytes, kind = c->kind, off = l->off, ap = l->ap, rp = l->rp, i, t0 = hle_me_count();
  if (sb < 65536u || rb < 4096u) { c->hdr.fault = 3; return; }
  EP_PROG(c, kind == EP_C_MAIN ? (uint32_t)(uintptr_t)(ring + rp) : (kind == EP_C_UNC ? EP_EDRAM_UNC : EP_EDRAM) + ap);
  for (i = 0; i < EP_LINES_PER_BLOCK; i++) {
    const uint32_t *s = (const uint32_t *)(src + off);
    if (kind == EP_C_MAIN) { mep_k_copy((uint32_t *)(ring + rp), s, 64u); rp += 64u; if (rp >= rb) rp = 0; }
    else {
      if (kind == EP_C_UNC) mep_k_copy((uint32_t *)(uintptr_t)(EP_EDRAM_UNC + ap), s, 64u);
      else if (kind == EP_C_CDE) mep_k_copy_cde((uint32_t *)(uintptr_t)(EP_EDRAM + ap), s, 64u);
      else mep_k_copy((uint32_t *)(uintptr_t)(EP_EDRAM + ap), s, 64u);
      ap += 64u; if (ap >= EP_ARENA) ap = 0;
    }
    off += 64u; if (off >= sb) off = 0;
  }
  l->off = off; l->ap = ap; l->rp = rp;
  c->s_rd = c->s_rd + EP_LINES_PER_BLOCK; c->s_wr = c->s_wr + EP_LINES_PER_BLOCK;
  c->s_busy = c->s_busy + (hle_me_count() - t0); c->s_blocks = c->s_blocks + 1;
  sentinel_step(c, l);
}

static void run_job(volatile EpCtl *c, uint32_t job)
{
  uint32_t a0 = c->a[0], a1 = c->a[1], a2 = c->a[2], a3 = c->a[3], t0, t1, x = 0, i;
  for (i = 0; i < 8u; i++) c->r[i] = 0;
  if (job == EP_J_CAL) {                /* Count into the block every ~3 us while the main CPU reads it against its own clock */
    while (c->cal_run) {
      c->cal = hle_me_count();
      for (uint32_t k = 0; k < 1024u; k++) __asm__ volatile("nop");
    }
  } else if (job == EP_J_KERN) {        /* a0 kernel, a1.. its arguments (main RAM or eDRAM addresses) */
    EP_PROG(c, a1);
    t0 = hle_me_count();
    if (a0 == MEP_K_ALU) x = mep_k_alu(a1);
    else if (a0 == MEP_K_LINES) x = mep_k_lines((const uint32_t *)(uintptr_t)a1, a2);
    else if (a0 == MEP_K_CHASE) x = mep_k_chase((const uint32_t *)(uintptr_t)a1, a2, a3);
    t1 = hle_me_count();
    c->r[0] = t1 - t0; c->r[1] = x;
  } else if (job == EP_J_FILL) {        /* a0 eDRAM offset, a1 bytes, a2 seed */
    t0 = hle_me_count();
    fill(a0, a1, a2 EP_FILL_PASS);
    c->r[6] = hle_me_count() - t0;
  } else if (job == EP_J_CHECK) {       /* a0 eDRAM offset, a1 bytes, a2 seed */
    t0 = hle_me_count();
    check(c, a0, a1, a2);
    c->r[6] = hle_me_count() - t0;
  } else if (job == EP_J_COPY) {        /* a0 kind, a1 destination (an eDRAM offset; a main RAM address for EP_C_MAIN), a2 source, a3
                                           bytes; then written back (timed; not for uncached stores) and dropped */
    uint32_t *d = a0 == EP_C_MAIN ? (uint32_t *)(uintptr_t)a1 : a0 == EP_C_UNC ? (uint32_t *)(uintptr_t)(EP_EDRAM_UNC + a1)
                                                                               : (uint32_t *)(uintptr_t)(EP_EDRAM + a1);
    EP_PROG(c, d);
    t0 = hle_me_count();
    if (a0 == EP_C_CDE) mep_k_copy_cde(d, (const uint32_t *)(uintptr_t)a2, a3);
    else mep_k_copy(d, (const uint32_t *)(uintptr_t)a2, a3);
    t1 = hle_me_count();
    EP_PROG(c, (uint32_t)(uintptr_t)d + 1u);
    if (a0 != EP_C_UNC) hle_me_dwb(d, a3); else hle_me_sync();
    c->r[0] = t1 - t0; c->r[1] = hle_me_count() - t1;
    if (a0 != EP_C_UNC) hle_me_dinval(d, a3);
    hle_me_dinval((const void *)(uintptr_t)a2, a3); hle_me_sync();
  } else if (job == EP_J_CMP) {         /* a0 eDRAM offset, a1 main RAM address, a2 bytes: r[0] words different, r[1] the first one's
                                           offset in the range (0xFFFFFFFF: none) */
    const volatile uint32_t *p = edram(a0), *q = (const volatile uint32_t *)(uintptr_t)a1; uint32_t bad = 0, fo = 0xFFFFFFFFu;
    hle_me_dinval((const void *)p, a2); hle_me_dinval((const void *)q, a2); hle_me_sync();
    EP_PROG(c, p);
    for (i = 0; i < a2 / 4u; i++) if (p[i] != q[i]) { if (!bad) fo = 4u * i; bad++; }
    c->r[0] = bad; c->r[1] = fo;
  } else if (job == EP_J_FLUSH) {       /* the whole D-cache written back and dropped (timed) */
    t0 = hle_me_count();
    hle_me_dwbinv_all();
    c->r[0] = hle_me_count() - t0;
#ifdef EP_P2
  } else if (job == EP_J_WORD) {        /* probe 2's one-word tests: a0 the test, a1 an eDRAM offset (test 5: an address), a2 the value
                                           written: 1 an uncached read; 2 an uncached write read back; 3 a cached write (its line
                                           dropped first), written back, read uncached; 4 an uncached write read cached (its line
                                           dropped first); 5 a read at a1 as it is (0x00000100: a cached read through the user segment,
                                           the way probe 1's fill reached the eDRAM).  r[0] the value read, r[7] the test (when done) */
    volatile uint32_t *u = (volatile uint32_t *)(uintptr_t)(EP_EDRAM_UNC + a1), *k = (volatile uint32_t *)(uintptr_t)(EP_EDRAM + a1);
    if (a0 == 1u) { EP_PROG(c, u); hle_me_sync(); x = *u; }
    else if (a0 == 2u) { EP_PROG(c, u); hle_me_sync(); *u = a2; hle_me_sync(); x = *u; }
    else if (a0 == 3u) {
      hle_me_dinval((const void *)k, 64u); hle_me_sync();
      EP_PROG(c, k); hle_me_sync(); *k = a2;
      EP_PROG(c, (uint32_t)(uintptr_t)k + 1u); hle_me_sync(); hle_me_dwb((const void *)k, 64u); hle_me_sync();
      EP_PROG(c, u); hle_me_sync(); x = *u;
    } else if (a0 == 4u) {
      EP_PROG(c, u); hle_me_sync(); *u = a2; hle_me_sync();
      hle_me_dinval((const void *)k, 64u); hle_me_sync();
      EP_PROG(c, k); hle_me_sync(); x = *k;
    } else if (a0 == 5u) { EP_PROG(c, a1); hle_me_sync(); x = HW(a1); }
    c->r[0] = x; c->r[7] = a0;
  } else if (job == EP_J_SNAP) {        /* probe 2: a1 words (at most 8) read from the address a0 on into r[0..] (registers; reads
                                           only) */
    for (i = 0; i < a1 && i < 8u; i++) { EP_PROG(c, a0 + 4u * i); hle_me_sync(); c->r[i] = HW(a0 + 4u * i); }
#endif
  } else if (job == EP_J_REG) {         /* a0 = 0: 0xBC100050 read (the ME's own; its start code reads it too); EP_R_B / EP_R_C: the
                                           turn's writes, each value read before and after (only in a build made with the turn's flag) */
    c->r[0] = HW(0xBC100050u);
#ifdef EP_REG_B
    if (a0 == EP_R_B) { HW(0xBC100050u) = 0x0fu; hle_me_sync(); c->r[1] = HW(0xBC100050u); c->r[7] = 1; }
#endif
#ifdef EP_REG_C
    if (a0 == EP_R_C) {
      c->r[1] = HW(0xBCC00000u); c->r[2] = HW(0xBCC00030u); c->r[3] = HW(0xBCC00040u);
      HW(0xBCC00000u) = 0xFFFFFFFFu; HW(0xBCC00030u) = 1u; HW(0xBCC00040u) = 1u; hle_me_sync();
      c->r[4] = HW(0xBCC00000u); c->r[5] = HW(0xBCC00030u); c->r[6] = HW(0xBCC00040u); c->r[7] = 2;
    }
#endif
  }
  /* every job ends with the D-cache written back and empty (as gl14mp): the main CPU then reuses its buffers, and a line left here
     could be read later in place of what it writes there */
  EP_PROG(c, 0xFFFFFFFCu);
  hle_me_dwbinv_all();
}

/* rest until the main CPU's interrupt (gl14mp's rest_once): sleeps counted, asleep written, then one more look at the job number and
   the mode, and the halt only if nothing came */
static void rest_once(volatile EpCtl *c, uint32_t done)
{
  uint32_t w0 = MW(13);
  c->sleeps = c->sleeps + 1;
  hle_me_sync();
  c->asleep = 1;
  hle_me_sync();
  if (c->job_seq == done && c->mode == EP_M_REST && !c->hdr.exit_req) {
    me_rest();
    if (MW(13) != w0) {
      uint32_t e = MW(15), hl = (uint32_t)(uintptr_t)me_rest_halt & 0x7FFFFFFFu;
      if (e == hl || e == hl + 4u) c->halted = c->halted + 1; else c->pending = c->pending + 1;
      c->flags_last = MW(12); c->flags_or = MW(14);
    } else c->nowake = c->nowake + 1;
  } else c->late = c->late + 1;
  c->asleep = 0;
  hle_me_sync();
}

void ep_work(volatile HleMeHdr *h)
{
  volatile EpCtl *c = (volatile EpCtl *)h;
  uint32_t done = c->done_seq, rest_set = 0;
  EpLoc l;
  l.off = 0; l.ap = 0; l.rp = 0; l.sp = 0;
  __asm__ volatile("mtc0 $zero, $9\n\tsync" ::: "memory");   /* Count starts (it reads a constant until written) */
  c->hdr.boots = c->hdr.boots + 1;
  c->hdr.state = 1;
  for (;;) {
    uint32_t seq, m;
    c->hdr.heartbeat = c->hdr.heartbeat + 1;
    c->hdr.count = hle_me_count();
    if (c->hdr.exit_req) break;
    seq = c->job_seq;
    if (seq != done) {
      run_job(c, c->job);
      done = seq;
      hle_me_sync();
      c->done_seq = seq;
      continue;
    }
    m = c->mode;
    if (m != c->s_mode) c->s_mode = m;
    if (m == EP_M_COPY) { stream_block(c, &l); continue; }
    if (c->rest) {
      if (!rest_set) { me_rest_init(); rest_set = 1; }
      rest_once(c, done);
      continue;
    }
    for (uint32_t i = 0; i < 4096u; i++) __asm__ volatile("nop");   /* about 12 us between looks (before the main CPU allows rests) */
  }
  hle_me_dwbinv_all();                  /* its stack, the ring and the arena's lines out of its cache before the halt (hle_me.h) */
  c->hdr.state = 2;
  hle_me_halt();
}
