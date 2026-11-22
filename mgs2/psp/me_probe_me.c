/* me_probe_me.c - gl14mp (2026-11-02, task #132; mgs2/me_probe_notes.md): the Media Engine's side of the probe.  mep_work is the loop
   psp/me_probe.c starts on the ME with the port's bring-up (psp/me/hle_me_mgs2.c: Sony's start, kernel helpers at their kernel alias,
   as v30me6c).  It runs the jobs the main CPU posts (the clock's calibration, the float ops, the kernels of psp/me_probe_kern.c, the
   copies, a cache flush) and the two streams of the game's turns (paced: one 64-byte line of game RAM read and 291 integer
   instructions, one line in 11 written to the ring; burst: game RAM copied into the ring as fast as it can); with neither, it rests
   (psp/me_rest.S: HALT until the main CPU's interrupt, the handshake of psp/me_mix_me.c).  Its own CPU registers it writes: Count
   (once, when its loop starts: it reads a constant until written, media-engine.md section 2) and FCR31 (for the float job only).  Memory it
   writes: its region (stack), the ring, the jobs' destinations the main CPU gives it, the control block (uncached).  Game RAM it only
   reads, through its cache: those lines are never dirty there, so nothing of the game's is written back.  hle_me.h's rules: no syscall,
   no libc, no function pointer or jump table (the one call through a pointer is to the generated code of the I-cache kernel, at its
   kernel alias), shared words through the uncached alias only. */
#include <stdint.h>
#include "me_probe.h"

void me_rest_init(void);
void me_rest(void);
extern char me_rest_halt[];
#define MW(i) (((volatile uint32_t *)0xBFC00400u)[i])   /* me_rest.S's words ([12..15]) */

static inline uint32_t fcr31_get(void) { uint32_t v; __asm__ volatile("cfc1 %0, $31" : "=r"(v)); return v; }
static inline void fcr31_set(uint32_t v) { __asm__ volatile(".set push\n.set noreorder\nctc1 %0, $31\nnop\nnop\n.set pop" :: "r"(v)); }
static inline void icache_inval_all(void)       /* index invalidate, both ways (the start stub's loop) */
{
  for (uint32_t i = 0; i < 8192u; i += 64u) {
    __asm__ volatile("cache 0x04, 0(%0)" :: "r"(i) : "memory");
    __asm__ volatile("cache 0x04, 0(%0)" :: "r"(i) : "memory");
  }
  __asm__ volatile("sync" ::: "memory");
}
static inline void dwbinv_range(const void *p, uint32_t n)   /* write back, then drop: the main CPU reads (or frees) it next */
{
  hle_me_dwb(p, n);
  hle_me_dinval(p, n);
  hle_me_sync();
}

/* the compiler's own memset / memcpy calls, renamed by psp/gl14mp.sh (objcopy --redefine-sym) */
void *me_memset(void *d, int c, unsigned n) { volatile uint8_t *p = (volatile uint8_t *)d; while (n--) *p++ = (uint8_t)c; return d; }
void *me_memcpy(void *d, const void *s, unsigned n)
{
  volatile uint8_t *p = (volatile uint8_t *)d; const volatile uint8_t *q = (const volatile uint8_t *)s;
  while (n--) *p++ = *q++;
  return d;
}

typedef struct MepLoc { uint32_t off, rp, k, acc; } MepLoc;   /* the streams' positions (source, ring), lines since a write, work value */

/* the paced stream's work per line: 97 x 3 dependent integer instructions */
static inline uint32_t pace(uint32_t a, uint32_t v)
{
  uint32_t b = v | 1u;
  __asm__ volatile(".set push\n.set noreorder\n.rept 97\naddu %0, %0, %1\nxor %1, %1, %0\nsll %0, %0, 1\n.endr\n.set pop" : "+r"(a), "+r"(b));
  return a ^ b;
}

/* one block of a stream: 1,024 lines of the source window read (64 KB); paced: work and every 11th line one line written to the ring;
   burst: every line copied to the ring (claimed in the cache first when c->cde) */
static void stream_block(volatile MepCtl *c, MepLoc *l, uint32_t mode)
{
  const uint8_t *src = (const uint8_t *)c->src; uint8_t *ring = (uint8_t *)c->ring;
  uint32_t sb = c->src_bytes, rb = c->ring_bytes, off = l->off, rp = l->rp, i, wr = 0, t0 = hle_me_count();
  if (sb < 65536u || rb < 4096u) { c->hdr.fault = 3; return; }
  if (mode == MEP_M_PACED) {
    uint32_t a = l->acc, k = l->k;
    for (i = 0; i < MEP_LINES_PER_BLOCK; i++) {
      uint32_t v = *(const volatile uint32_t *)(src + off);
      a = pace(a, v);
      off += 64u; if (off >= sb) off = 0;
      if (++k == 11u) {
        volatile uint32_t *d = (volatile uint32_t *)(ring + rp);
        d[0] = a; d[1] = a + 1; d[2] = a + 2; d[3] = a + 3; d[4] = a + 4; d[5] = a + 5; d[6] = a + 6; d[7] = a + 7;
        d[8] = a + 8; d[9] = a + 9; d[10] = a + 10; d[11] = a + 11; d[12] = a + 12; d[13] = a + 13; d[14] = a + 14; d[15] = a + 15;
        rp += 64u; if (rp >= rb) rp = 0;
        k = 0; wr++;
      }
    }
    l->acc = a; l->k = k;
  } else {
    uint32_t cde = c->cde;
    for (i = 0; i < MEP_LINES_PER_BLOCK; i++) {
      if (cde) mep_k_copy_cde((uint32_t *)(ring + rp), (const uint32_t *)(src + off), 64u);
      else mep_k_copy((uint32_t *)(ring + rp), (const uint32_t *)(src + off), 64u);
      off += 64u; if (off >= sb) off = 0;
      rp += 64u; if (rp >= rb) rp = 0;
    }
    wr = MEP_LINES_PER_BLOCK;
  }
  l->off = off; l->rp = rp;
  c->s_rd = c->s_rd + MEP_LINES_PER_BLOCK; c->s_wr = c->s_wr + wr;
  c->s_busy = c->s_busy + (hle_me_count() - t0); c->s_blocks = c->s_blocks + 1;
}

static void run_job(volatile MepCtl *c, uint32_t job)
{
  uint32_t a0 = c->a[0], a1 = c->a[1], a2 = c->a[2], a3 = c->a[3], t0, t1, x = 0;
  c->r[0] = c->r[1] = c->r[2] = 0;
  if (job == MEP_J_CAL) {               /* Count into the block every ~3 us while the main CPU reads it against its own clock */
    while (c->cal_run) {
      c->cal = hle_me_count();
      for (uint32_t k = 0; k < 1024u; k++) __asm__ volatile("nop");
    }
  } else if (job == MEP_J_FP) {         /* a0 op, a1 input pairs, a2 count, a3 output */
    fcr31_set(c->fcr31);
    t0 = hle_me_count();
    x = mep_fp_run(a0, (const uint32_t *)a1, a2, (uint32_t *)a3);
    t1 = hle_me_count();
    dwbinv_range((const void *)a3, a2 * 4u);
    c->r[0] = x; c->r[1] = t1 - t0; c->r[2] = fcr31_get();
  } else if (job == MEP_J_KERN) {       /* a0 kernel, a1.. its arguments */
    t0 = hle_me_count();
    if (a0 == MEP_K_ALU) x = mep_k_alu(a1);
    else if (a0 == MEP_K_FALU) x = mep_k_falu(a1);
    else if (a0 == MEP_K_LINES) x = mep_k_lines((const uint32_t *)a1, a2);
    else if (a0 == MEP_K_CHASE) x = mep_k_chase((const uint32_t *)a1, a2, a3);
    else if (a0 == MEP_K_STORES) mep_k_stores((uint32_t *)a1, a2, a3);
    else if (a0 == MEP_K_CODE) {        /* the main CPU's generated code (written back to memory), at its kernel alias */
      icache_inval_all();
      t0 = hle_me_count();
      ((void (*)(void))(0x80000000u | a1))();
    }
    t1 = hle_me_count();
    c->r[0] = t1 - t0; c->r[1] = x;
    if (a0 == MEP_K_STORES) { t0 = hle_me_count(); dwbinv_range((const void *)a1, a2 * 64u); c->r[2] = hle_me_count() - t0; }
  } else if (job == MEP_J_COPY) {       /* a0 kind, a1 destination, a2 source, a3 bytes; then written back (timed) and dropped */
    t0 = hle_me_count();
    if (a0 == MEP_C_CDE) mep_k_copy_cde((uint32_t *)a1, (const uint32_t *)a2, a3);
    else mep_k_copy((uint32_t *)a1, (const uint32_t *)a2, a3);
    t1 = hle_me_count();
    hle_me_dwb((const void *)a1, a3);
    c->r[0] = t1 - t0; c->r[1] = hle_me_count() - t1;
    hle_me_dinval((const void *)a1, a3); hle_me_dinval((const void *)a2, a3); hle_me_sync();
  } else if (job == MEP_J_FLUSH) {      /* the whole D-cache written back and dropped (timed) */
    t0 = hle_me_count();
    hle_me_dwbinv_all();
    c->r[0] = hle_me_count() - t0;
  }
  /* every job ends with the D-cache written back and empty: the main CPU then frees and reuses the buffers, and a line left here
     could be read later in place of what it writes there (a chase over a stale line would follow a wrong index) */
  hle_me_dwbinv_all();
}

/* rest until the main CPU's interrupt (psp/me_mix_me.c's me_rest_once): sleeps counted, asleep written, then one more look at the
   job number and the mode, and the halt only if nothing came; the main CPU writes them, then looks at asleep (each side syncs between
   its write and its read, so at least one of the two sees the other's write) */
static void rest_once(volatile MepCtl *c, uint32_t done)
{
  uint32_t w0 = MW(13);
  c->sleeps = c->sleeps + 1;
  hle_me_sync();
  c->asleep = 1;
  hle_me_sync();
  if (c->job_seq == done && c->mode == MEP_M_REST && !c->hdr.exit_req) {
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

void mep_work(volatile HleMeHdr *h)
{
  volatile MepCtl *c = (volatile MepCtl *)h;
  uint32_t done = c->done_seq, rest_set = 0;
  MepLoc l;
  l.off = 0; l.rp = 0; l.k = 0; l.acc = 0x12345678u;
  c->fcr31_found = fcr31_get();
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
    if (m == MEP_M_PACED || m == MEP_M_BURST) { stream_block(c, &l, m); continue; }
    if (c->rest) {
      if (!rest_set) { me_rest_init(); rest_set = 1; }
      rest_once(c, done);
      continue;
    }
    for (uint32_t i = 0; i < 4096u; i++) __asm__ volatile("nop");   /* about 12 us between looks (before the main CPU allows rests) */
  }
  hle_me_dwbinv_all();                  /* its stack and the ring to memory before the halt (hle_me.h) */
  c->hdr.state = 2;
  hle_me_halt();
}
