#define _GNU_SOURCE
/* c3model.c (sb13, task #144; x86 only): Stage C step 3, the rooms' meshes (P1214a0 groups, VU1 entry P04+df0), sized before any GE
   code.  For each df0 call the picture code reads (xconv.c xc_call: vc_df0_rec from the lane's streams, or vc_df0 from the VU memory),
   with its exact results in hand, this file counts
   - the volumes: calls and vertices, from the streams or the VU memory, left out off the scissor (XcHost.cut), drawn (strips), and what
     today's frame data holds for them (GerXform, 16 bytes a vertex, 4 a strip, 2 an index when XcHost.noidx is 0): in all, by frame (the
     largest) and in the model windows;
   - the keys a kept GE copy would have (the streams' addresses and kinds, ROW where a kind adds it, the count): new keys, the same bytes
     again, other bytes at a known key (positions, UVs and flags; the colours; the alpha, each apart), the GE bytes of all keys and of the
     keys a window draws, where the streams lie in the EE's RAM;
   - an LRU cache of kept copies at eight sizes: the drawn calls and vertices it would serve from a copy;
   - the depth range (XcZr): the calls whose box's w range lies inside the range kept so far (the same projection, every corner in
     front), for which xc_zr_df0's loop over the vertices gives nothing new: the loop could be skipped with the same range and counts.
   It only reads: every run with it stays exact.  MGS2_C3_MODEL=1 turns it on; c3m_report prints the totals. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c3model.h"
#include <signal.h>
#include <sys/mman.h>
#include <execinfo.h>
#include <ucontext.h>

int c3m_on;
long c3m_vb;
const uint8_t *c3m_ram;

#define NWIN 3
static const long WIN[NWIN][2] = { { 30810, 30890 }, { 33410, 33490 }, { 31510, 31590 } };   /* gl14's model windows: the waiting deck and
                                                         inside (indoor route), the codec call (codec route) */
#define NB 8
static const long BUD[NB] = { 128, 256, 384, 512, 768, 1024, 1536, 2048 };   /* KB */
#define KN (1 << 17)

typedef struct {
  const void *a, *b, *c; uint32_t row[4]; int n; uint8_t ka, kb, kc, used;
  uint64_t hp, hc; uint32_t alpha;   /* the content: positions, UVs and flags; the colours (RGB); the alpha */
  int size;                          /* GE bytes: 16 a vertex, 4 a strip, 32 */
  long first, last, uses, drawn, wstamp, fstamp;
  long wcount, wvb; int wwriter;   /* MGS2_C3_WATCH: stores into its streams since its last use, the last one's vblank and writer */
  int prev[NB], next[NB]; uint8_t in[NB];
} Key;
static Key *K;
static struct {
  long calls, verts, rec, rec_v, vm, vm_v, vm_src, bad, cut, cut_v, drawn, drawn_v, drawn_st, empty, clip;
  long long bytes, bytes_x, bytes_v, bytes_s, bytes_i;
  long fr_vb, fr_bytes, fr_v, fr_max_bytes, fr_max_bytes_vb, fr_max_v, fr_max_v_vb, frames;
  long fr_vs, fr_ws, fr_drawn, fr_keys, fr_vm;   /* MGS2_C3_FRAMES: this frame's drawn vertex + strip bytes, its distinct drawn keys' bytes */
  long keys, same, other, oth_p, oth_c, oth_a, full, rowcalls, kbytes;
  long hist[3][34];
  long lru_c[NB], lru_v[NB], lru_n, lru_nv; long lbytes[NB]; int head[NB], tail[NB];
  long zr_calls, zr_v, zr_always, zr_skip, zr_skip_v, zr_newproj, zr_behind;
  long kc[NWIN][5], wc[NWIN], wv[NWIN], wd[NWIN], wdv[NWIN], wcut[NWIN], wnew[NWIN], wws[NWIN], wkeys[NWIN];
  long long wbytes[NWIN];
  long wlru_v[NWIN][NB], wzr[NWIN], wzr_v[NWIN], wzs[NWIN], wzs_v[NWIN];
  long kall;
} T;
static int first = 1;
static int fr_on;   /* MGS2_C3_FRAMES=1: a "c3 fr" line a frame */
/* ---- MGS2_C3_WATCH=1 (with MGS2_C3_MODEL=1; x86 dev): every store into a host page that holds a key's streams (A, B, C) is caught
   (the page is read-only from the vblank after the key is first seen; SIGSEGV), counted by its writer (the store's code address and the
   unwinder's next frames) and by where it lands (a stream's bytes or a neighbour on the page), and let through (the page is writable again
   until the next vblank).  The game's stores, the DMA and SIF copies and the CD reads (iop_host.h WR8) are user-space stores, so each is
   caught; the run's state stays as without it (its hash lines are compared). ---- */
static int w_on; static uintptr_t w_base; static int w_np;
static uint8_t *w_watch, *w_prot, *w_kind;   /* per page: holds a stream, read-only now; per EE word: 1 A, 2 B, 3 C */
static int32_t *w_keyid;                     /* per EE word: the last key whose streams lie there (K index + 1) */
static int w_step[4], w_nstep;               /* pages made writable for the one store being single-stepped (TF) */
static long w_live_same, w_live_changed, w_traps;
static int *w_list, w_nl;                     /* pages to make read-only at the next vblank */
#define WEN 4096
typedef struct { uintptr_t pc[4]; uint8_t kind, used; long n, first, last; uint32_t ee; long ls, lc; } WEv;   /* ls, lc: keys used again after its store, same / other bytes */
static WEv *W; static long w_faults, w_full, w_kept;
static void w_seg(int sig, siginfo_t *si, void *ucp)
{
  uintptr_t a = (uintptr_t)si->si_addr; (void)sig;
  int pg = a >= w_base ? (int)((a - w_base) >> 12) : -1;
  if (pg < 0 || pg >= w_np || !w_prot[pg]) { signal(SIGSEGV, SIG_DFL); return; }   /* not ours: the store faults again, as without us */
  ucontext_t *uc = (ucontext_t *)ucp; void *bt[8]; int nb = backtrace(bt, 8);
  uintptr_t pc[4] = { (uintptr_t)uc->uc_mcontext.gregs[REG_RIP], nb > 3 ? (uintptr_t)bt[3] : 0, nb > 4 ? (uintptr_t)bt[4] : 0, nb > 5 ? (uintptr_t)bt[5] : 0 };
  uint32_t ee = c3m_ram && a >= (uintptr_t)c3m_ram && a < (uintptr_t)c3m_ram + (32u << 20) ? (uint32_t)(a - (uintptr_t)c3m_ram) : 0xffffffffu;
  uint8_t kind = ee != 0xffffffffu ? w_kind[ee >> 2] : 0;
  w_faults++;
  uint64_t h = (pc[0] * 0x9e3779b97f4a7c15ull) ^ (pc[1] * 0xc2b2ae3d27d4eb4full) ^ pc[2] ^ kind;
  int wi = -1;
  for (uint32_t i = (uint32_t)(h ^ h >> 31) & (WEN - 1), t = 0; t < WEN; t++, i = (i + 1) & (WEN - 1)) {
    WEv *e = &W[i];
    if (!e->used) { e->used = 1; memcpy(e->pc, pc, sizeof pc); e->kind = kind; e->n = 1; e->first = e->last = c3m_vb; e->ee = ee; w_kept++; wi = (int)i; break; }
    if (!memcmp(e->pc, pc, sizeof pc) && e->kind == kind) { e->n++; e->last = c3m_vb; wi = (int)i; break; }
    if (t == WEN - 1) w_full++;
  }
  if (kind && wi >= 0) { int32_t kid = w_keyid[ee >> 2]; if (kid > 0) { Key *k = &K[kid - 1]; k->wcount++; k->wvb = c3m_vb; k->wwriter = wi; } }
  /* the page writable for this one store: TF set, the trap (w_trap) makes it read-only again */
  mprotect((void *)(w_base + ((uintptr_t)pg << 12)), 4096, PROT_READ | PROT_WRITE); w_prot[pg] = 0;
  if (w_nstep < 4) w_step[w_nstep++] = pg;
  uc->uc_mcontext.gregs[REG_EFL] |= 0x100;
}
static void w_trap(int sig, siginfo_t *si, void *ucp)
{
  (void)sig; (void)si; ucontext_t *uc = (ucontext_t *)ucp;
  w_traps++;
  for (int i = 0; i < w_nstep; i++) { int pg = w_step[i]; if (!w_prot[pg]) { mprotect((void *)(w_base + ((uintptr_t)pg << 12)), 4096, PROT_READ); w_prot[pg] = 1; } }
  w_nstep = 0;
  uc->uc_mcontext.gregs[REG_EFL] &= ~0x100;
}
static void w_init(void)
{
  w_on = getenv("MGS2_C3_WATCH") && atoi(getenv("MGS2_C3_WATCH"));
  if (!w_on || !c3m_ram) { w_on = 0; return; }
  w_base = (uintptr_t)c3m_ram & ~(uintptr_t)4095; w_np = (int)((((uintptr_t)c3m_ram + (32u << 20) - w_base) + 4095) >> 12);
  w_watch = calloc((size_t)w_np, 1); w_prot = calloc((size_t)w_np, 1); w_kind = calloc((32u << 20) / 4, 1);
  w_list = calloc((size_t)w_np, sizeof *w_list); W = calloc(WEN, sizeof *W); w_keyid = calloc((32u << 20) / 4, sizeof *w_keyid);
  void *bt[4]; backtrace(bt, 4);   /* (the unwinder loaded before any fault) */
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = w_seg; sa.sa_flags = SA_SIGINFO | SA_NODEFER; sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, 0);
  struct sigaction st; memset(&st, 0, sizeof st); st.sa_sigaction = w_trap; st.sa_flags = SA_SIGINFO; sigemptyset(&st.sa_mask); sigaction(SIGTRAP, &st, 0);
  fprintf(stderr, "c3 watch: on, EE RAM at %p, %d host pages; c3m_report at %p\n", (const void *)c3m_ram, w_np, (void *)c3m_report);
}
/* a key's streams: their words marked, their pages watched from the next vblank */
static void w_key(const XlRec *r, int kid)
{
  for (int s = 0; s < 3; s++) {
    const uint8_t *p = (const uint8_t *)r->p[s];
    if (!p || p < c3m_ram || p >= c3m_ram + (32u << 20)) continue;
    uint32_t ee = (uint32_t)(p - c3m_ram), len = (uint32_t)r->num[s] * (r->kind[s] == 4 ? 4u : 8u);
    if (ee + len > (32u << 20)) len = (32u << 20) - ee;
    for (uint32_t k = ee >> 2; k < (ee + len + 3) >> 2; k++) { w_kind[k] = (uint8_t)(s + 1); w_keyid[k] = kid + 1; }
    for (uintptr_t pa = ((uintptr_t)p - w_base) >> 12; pa <= ((uintptr_t)p + len - 1 - w_base) >> 12; pa++)
      if (!w_watch[pa]) { w_watch[pa] = 1; w_list[w_nl++] = (int)pa; }
  }
}
void c3m_vblank(void)
{
  if (!w_on) return;
  for (int i = 0; i < w_nl; i++) { int pg = w_list[i]; if (!w_prot[pg]) { mprotect((void *)(w_base + ((uintptr_t)pg << 12)), 4096, PROT_READ); w_prot[pg] = 1; } }
  w_nl = 0;
}
static void w_report(FILE *f)
{
  if (!w_on) return;
  long pages = 0; for (int i = 0; i < w_np; i++) pages += w_watch[i];
  fprintf(f, "c3 watch: %ld faults, %ld pages watched, %ld writers kept (table full %ld); host c3m_report %p\n", w_faults, pages, w_kept, w_full, (void *)c3m_report);
  fprintf(f, "c3 watch: single steps %ld; keys used again after a store into their streams: %ld with the same bytes, %ld with other bytes\n", w_traps, w_live_same, w_live_changed);
  for (int i = 0; i < WEN; i++) { const WEv *e = &W[i];
    if (e->used && (e->ls || e->lc))
      fprintf(f, "c3 watch live: %s used again %ld same, %ld other; %ld stores, vb %ld-%ld, pc %p %p %p %p\n", e->kind == 1 ? "A" : e->kind == 2 ? "B" : "C",
              e->ls, e->lc, e->n, e->first, e->last, (void *)e->pc[0], (void *)e->pc[1], (void *)e->pc[2], (void *)e->pc[3]); }
  for (int k = 3; k >= 0; k--)
    for (int i = 0; i < WEN; i++) { const WEv *e = &W[i];
      if (e->used && e->kind == k)
        fprintf(f, "c3 watch writer: %s %ld stores, vb %ld-%ld, first at EE %07x, pc %p %p %p %p\n", k == 1 ? "A (positions)" : k == 2 ? "B (colours)" : k == 3 ? "C (UVs)" : "page neighbour",
                e->n, e->first, e->last, e->ee, (void *)e->pc[0], (void *)e->pc[1], (void *)e->pc[2], (void *)e->pc[3]); }
}
static void init(void)
{
  first = 0;
  K = calloc(KN, sizeof *K);
  for (int b = 0; b < NB; b++) T.head[b] = T.tail[b] = -1;
  T.fr_vb = -1;
  fr_on = getenv("MGS2_C3_FRAMES") && atoi(getenv("MGS2_C3_FRAMES"));
  w_init();
}
static uint64_t fnv(uint64_t h, const void *p, size_t len) { const uint8_t *q = (const uint8_t *)p; for (size_t i = 0; i < len; i++) { h ^= q[i]; h *= 0x100000001b3ull; } return h; }
static int win(void) { for (int j = 0; j < NWIN; j++) if (c3m_vb >= WIN[j][0] && c3m_vb < WIN[j][1]) return j; return -1; }

void c3m_kind(int k)
{
  if (first) init();
  int w = win(); if (w >= 0 && k >= 0 && k < 5) T.kc[w][k]++;
}

/* the LRU lists, one a size */
static void l_unlink(int b, int i)
{
  Key *e = &K[i];
  if (e->prev[b] >= 0) K[e->prev[b]].next[b] = e->next[b]; else T.head[b] = e->next[b];
  if (e->next[b] >= 0) K[e->next[b]].prev[b] = e->prev[b]; else T.tail[b] = e->prev[b];
  e->in[b] = 0; T.lbytes[b] -= e->size;
}
static void l_front(int b, int i)
{
  Key *e = &K[i];
  e->prev[b] = -1; e->next[b] = T.head[b];
  if (T.head[b] >= 0) K[T.head[b]].prev[b] = i; else T.tail[b] = i;
  T.head[b] = i; e->in[b] = 1; T.lbytes[b] += e->size;
}
/* a drawn call of key i (its content as now: changed 1 when it differs from the kept one): served from a copy, or (re)made */
static void l_use(int i, int changed, int oldsize, int n, int w)
{
  Key *e = &K[i];
  T.lru_n++; T.lru_nv += n;
  for (int b = 0; b < NB; b++) {
    if (e->in[b]) {
      if (!changed) { T.lru_c[b]++; T.lru_v[b] += n; if (w >= 0) T.wlru_v[w][b] += n; }
      int s = e->size; e->size = oldsize; l_unlink(b, i); e->size = s;
    }
    if ((long)e->size <= BUD[b] << 10) {
      l_front(b, i);
      while (T.lbytes[b] > BUD[b] << 10 && T.tail[b] >= 0 && T.tail[b] != i) l_unlink(b, T.tail[b]);
    }
  }
}
static void frame_end(void)
{
  if (T.fr_vb < 0) return;
  if (fr_on) fprintf(stderr, "c3 fr: vb %ld drawn %ld vs %ld ws %ld keys %ld vm %ld\n", T.fr_vb, T.fr_drawn, T.fr_vs, T.fr_ws, T.fr_keys, T.fr_vm);
  T.fr_vs = T.fr_ws = T.fr_drawn = T.fr_keys = T.fr_vm = 0;
  T.frames++;
  if (T.fr_bytes > T.fr_max_bytes) { T.fr_max_bytes = T.fr_bytes; T.fr_max_bytes_vb = T.fr_vb; }
  if (T.fr_v > T.fr_max_v) { T.fr_max_v = T.fr_v; T.fr_max_v_vb = T.fr_vb; }
  T.fr_bytes = T.fr_v = 0;
}

void c3m_call(const XlCall *c, const XcGs *d, const XcHost *h, int n, const VcVert *v, const uint16_t *fl, const VcXform *x, int off)
{
  if (first) init();
  if (!K) return;
  if (c3m_vb != T.fr_vb) { frame_end(); T.fr_vb = c3m_vb; }
  const int w = win();
  T.calls++;
  if (n <= 0) { T.bad++; return; }
  T.verts += n;
  if (w >= 0) { T.wc[w]++; T.wv[w] += n; }
  if (x->clip) T.clip++;
  const XlRec *r = c->rec;
  if (r) { T.rec++; T.rec_v += n; } else { T.vm++; T.vm_v += n; if (c->src) T.vm_src++; }
  /* the strips (vc_strips's count) and today's frame data for a drawn call (xc_df0) */
  int nst = 0, ni = 0;
  for (int i = 2; i < n; ) {
    if (fl[i] & 0x8000) { i++; continue; }
    int j = i; while (j + 1 < n && !(fl[j + 1] & 0x8000)) j++;
    ni += (nst ? 2 : 0) + (j - i + 3); nst++; i = j + 1;
  }
  if (h->noidx) ni = 0;
  if (off) { T.cut++; T.cut_v += n; if (w >= 0) T.wcut[w]++; }
  else if (!nst) T.empty++;
  else {
    long by = (long)sizeof(GerXform) + 16L * n + 4L * nst + 2L * ni;
    T.drawn++; T.drawn_v += n; T.drawn_st += nst;
    T.bytes += by; T.bytes_x += sizeof(GerXform); T.bytes_v += 16L * n; T.bytes_s += 4L * nst; T.bytes_i += 2L * ni;
    T.fr_bytes += by; T.fr_v += n; T.fr_drawn++; T.fr_vs += 16L * n + 4L * nst; if (!r) T.fr_vm += 16L * n + 4L * nst;
    if (w >= 0) { T.wd[w]++; T.wdv[w] += n; T.wbytes[w] += by; }
  }
  /* the depth range: what xc_df0_zr (left out) or xc_df0 (drawn) gives the frame's XcZr, and whether the box tells it first */
  if (h->zr && d->zte && !d->zmsk && !(d->ate && d->atst == 0 && d->afail == 1) && nst) {
    if (d->ztst < 2) T.zr_always++;
    else {
      const XcZr *z = h->zr;
      int lo[3] = { 32767, 32767, 32767 }, hi[3] = { -32768, -32768, -32768 };
      for (int i = 0; i < n; i++) {
        if (v[i].x < lo[0]) lo[0] = v[i].x;
        if (v[i].x > hi[0]) hi[0] = v[i].x;
        if (v[i].y < lo[1]) lo[1] = v[i].y;
        if (v[i].y > hi[1]) hi[1] = v[i].y;
        if (v[i].z < lo[2]) lo[2] = v[i].z;
        if (v[i].z > hi[2]) hi[2] = v[i].z;
      }
      float a = x->world[2] / 32768, b = x->world[5] / 32768, cc = x->world[8] / 32768, t = x->world[11], wl = 3.0e38f, wh = -3.0e38f;
      for (int k = 0; k < 8; k++) {
        float X = (float)(k & 1 ? hi[0] : lo[0]), Y = (float)(k & 2 ? hi[1] : lo[1]), Z = (float)(k & 4 ? hi[2] : lo[2]);
        float ww = X * a + Y * b + Z * cc + t;
        if (ww < wl) wl = ww;
        if (ww > wh) wh = ww;
      }
      /* a vertex's w as xc_zr_df0 computes it lies within the corners' up to rounding: 3 roundings of terms no larger than S */
      float S = 32768.0f * (fabsf(a) + fabsf(b) + fabsf(cc)) + fabsf(t), eps = 1e-6f * S;
      T.zr_calls++; T.zr_v += n; if (w >= 0) { T.wzr[w]++; T.wzr_v[w] += n; }
      int same = z->on && x->pa == z->pa && x->pb == z->pb && x->vs[2] == z->vs2 && x->vo[2] == z->vo2;
      if (!same) T.zr_newproj++;
      else if (wl - eps <= 0) T.zr_behind++;
      else if (wl - eps >= z->wmin && wh + eps <= z->wmax) { T.zr_skip++; T.zr_skip_v += n; if (w >= 0) { T.wzs[w]++; T.wzs_v[w] += n; } }
    }
  }
  if (!r) return;
  /* where the streams lie (guest MB) */
  for (int s = 0; s < 3; s++) {
    const uint8_t *p = (const uint8_t *)r->p[s]; long mb = 33;
    if (c3m_ram && p >= c3m_ram && p < c3m_ram + (32u << 20)) mb = (long)((p - c3m_ram) >> 20);
    T.hist[s][mb]++;
  }
  /* the key and its content */
  uint32_t row[4] = { 0, 0, 0, 0 };
  if (r->kind[0] == 3 || r->kind[1] == 3) { memcpy(row, r->row, 16); T.rowcalls++; }
  uint64_t hp = 0xcbf29ce484222325ull, hc = 0xcbf29ce484222325ull;
  for (int i = 0; i < n; i++) {
    uint16_t q[6] = { v[i].u, v[i].v, (uint16_t)v[i].x, (uint16_t)v[i].y, (uint16_t)v[i].z, fl[i] };
    uint32_t col = v[i].c & 0xffffffu;
    hp = fnv(hp, q, sizeof q); hc = fnv(hc, &col, 4);
  }
  uint32_t alpha = v[0].c >> 24;
  int size = 16 * n + 4 * nst + 32;
  uint64_t k = (uint64_t)(uintptr_t)r->p[0] * 0x9e3779b97f4a7c15ull ^ (uint64_t)(uintptr_t)r->p[1] * 0xc2b2ae3d27d4eb4full ^
               (uint64_t)(uintptr_t)r->p[2] ^ (uint64_t)n << 40 ^ (uint64_t)(r->kind[0] | r->kind[1] << 4) << 56 ^ row[0] * 31ull ^ row[1] * 37ull ^
               row[2] * 41ull ^ row[3] * 43ull;
  for (uint32_t i = (uint32_t)(k ^ k >> 29) & (KN - 1), tries = 0; tries < KN; tries++, i = (i + 1) & (KN - 1)) {
    Key *e = &K[i];
    if (!e->used) {   /* a new key */
      e->used = 1; e->a = r->p[0]; e->b = r->p[1]; e->c = r->p[2]; memcpy(e->row, row, 16); e->n = n;
      e->ka = r->kind[0]; e->kb = r->kind[1]; e->kc = r->kind[2]; e->hp = hp; e->hc = hc; e->alpha = alpha; e->size = size;
      e->first = e->last = c3m_vb; e->uses = 1; e->drawn = 0; e->wstamp = -1; e->fstamp = -1;
      for (int b = 0; b < NB; b++) { e->prev[b] = e->next[b] = -1; e->in[b] = 0; }
      T.keys++; T.kbytes += size; if (w >= 0) T.wnew[w]++;
      if (w_on) { e->wcount = 0; w_key(r, (int)i); }
      if (!off && nst) { e->drawn++; e->size = size; l_use((int)i, 1, size, n, w); if (w >= 0) { e->wstamp = w; T.wws[w] += size; T.wkeys[w]++; }
        e->fstamp = c3m_vb; T.fr_ws += size - 32; T.fr_keys++; }
      return;
    }
    if (e->a == r->p[0] && e->b == r->p[1] && e->c == r->p[2] && e->n == n && e->ka == r->kind[0] && e->kb == r->kind[1] && e->kc == r->kind[2] &&
        !memcmp(e->row, row, 16)) {
      int changed = e->hp != hp || e->hc != hc || e->alpha != alpha, old = e->size;
      if (w_on && e->wcount) { WEv *we = &W[e->wwriter]; if (changed) { we->lc++; w_live_changed++; } else { we->ls++; w_live_same++; } e->wcount = 0; }
      if (!changed) T.same++;
      else { T.other++; T.oth_p += e->hp != hp; T.oth_c += e->hc != hc; T.oth_a += e->alpha != alpha; e->hp = hp; e->hc = hc; e->alpha = alpha; }
      e->uses++; e->last = c3m_vb;
      if (!off && nst) {
        e->drawn++; e->size = size; l_use((int)i, changed, old, n, w);
        if (w >= 0 && e->wstamp != w) { e->wstamp = w; T.wws[w] += size; T.wkeys[w]++; }
        if (e->fstamp != c3m_vb) { e->fstamp = c3m_vb; T.fr_ws += size - 32; T.fr_keys++; }
      }
      return;
    }
  }
  T.full++;
}

void c3m_report(FILE *f)
{
  w_report(f);
  if (!c3m_on || first) return;
  frame_end();
  fprintf(f, "c3 census: df0 calls %ld (%ld vertices; bad %ld): from the streams %ld (%ld), from the VU memory %ld (%ld; with an upload "
          "address %ld); header clip flag %ld; left out off the scissor %ld (%ld vertices); nothing drawn %ld; drawn %ld (%ld vertices, %ld strips)\n",
          T.calls, T.verts, T.bad, T.rec, T.rec_v, T.vm, T.vm_v, T.vm_src, T.clip, T.cut, T.cut_v, T.empty, T.drawn, T.drawn_v, T.drawn_st);
  fprintf(f, "c3 census: today's frame data for the drawn calls %lld bytes (GerXform %lld at %u, vertices %lld, strips %lld, indices %lld); "
          "frames %ld; the largest frame %ld bytes (vb %ld); the most drawn vertices in a frame %ld (vb %ld)\n", T.bytes, T.bytes_x,
          (unsigned)sizeof(GerXform), T.bytes_v, T.bytes_s, T.bytes_i, T.frames, T.fr_max_bytes, T.fr_max_bytes_vb, T.fr_max_v, T.fr_max_v_vb);
  long once = 0, nod = 0, longest = 0; double span = 0;
  for (int i = 0; i < KN; i++) if (K[i].used) {
    once += K[i].uses == 1; nod += !K[i].drawn; long s = K[i].last - K[i].first; span += s; if (s > longest) longest = s;
  }
  fprintf(f, "c3 keys: %ld keys (GE bytes %ld at 16 a vertex, 4 a strip, 32 a key), used once %ld, never drawn %ld, mean span %.0f vblanks "
          "(longest %ld); the same bytes again %ld, other bytes %ld (positions, UVs or flags %ld, colours %ld, alpha %ld), table full %ld; calls "
          "with ROW in the key %ld\n", T.keys, T.kbytes, once, nod, T.keys ? span / T.keys : 0, longest, T.same, T.other, T.oth_p, T.oth_c, T.oth_a,
          T.full, T.rowcalls);
  static const char *SN[3] = { "A (positions)", "B (colours)", "C (UVs)" };
  for (int s = 0; s < 3; s++) {
    fprintf(f, "c3 streams %s by guest MB:", SN[s]);
    for (int m = 0; m < 34; m++) if (T.hist[s][m]) fprintf(f, " %s%d x%ld", m == 33 ? "outside " : "", m == 33 ? 0 : m, T.hist[s][m]);
    fprintf(f, "\n");
  }
  fprintf(f, "c3 lru: drawn calls from the streams %ld (%ld vertices); served from a kept copy at", T.lru_n, T.lru_nv);
  for (int b = 0; b < NB; b++) fprintf(f, " %ld KB %ld (%ld v)", BUD[b], T.lru_c[b], T.lru_v[b]);
  fprintf(f, "\n");
  fprintf(f, "c3 depth: calls into the range %ld (%ld vertices), depth writers with ZTST ALWAYS %ld; a new projection %ld, a corner not in front "
          "%ld; the box inside the kept range (the loop gives nothing new) %ld (%ld vertices)\n", T.zr_calls, T.zr_v, T.zr_always, T.zr_newproj,
          T.zr_behind, T.zr_skip, T.zr_skip_v);
  for (int w = 0; w < NWIN; w++) {
    fprintf(f, "c3 window vb %ld-%ld: converter calls df0 %ld, 25f0 %ld, 2870 %ld, P06 %ld, P07 %ld; df0 %ld calls (%ld vertices), left out %ld, "
            "drawn %ld (%ld vertices), today's data %lld bytes; new keys %ld; keys drawn %ld (GE bytes %ld); depth range %ld calls (%ld v), box "
            "inside %ld (%ld v); kept copies serve (vertices)", WIN[w][0], WIN[w][1], T.kc[w][0], T.kc[w][1], T.kc[w][2], T.kc[w][3], T.kc[w][4],
            T.wc[w], T.wv[w], T.wcut[w], T.wd[w], T.wdv[w], T.wbytes[w], T.wnew[w], T.wkeys[w], T.wws[w], T.wzr[w], T.wzr_v[w], T.wzs[w], T.wzs_v[w]);
    for (int b = 0; b < NB; b++) fprintf(f, " %ld KB %ld", BUD[b], T.wlru_v[w][b]);
    fprintf(f, "\n");
  }
}
