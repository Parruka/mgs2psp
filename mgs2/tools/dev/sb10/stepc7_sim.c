/* stepc7_sim (task #133, Stage C step 1, T2's sizing): tools/dev/sb2/stepc_sim.c with the codec faces' calls (P07: vc_p07_rec or vc_p07
 * entered) as a third kind to hold back, on gl13's build, where step c already holds lane 2's P06 calls (their gl_call holds a record and
 * returns: kind 0 here, left where they were).  Variants: A as traced; B the P07 calls and their passes held to the transfer's end with the
 * texture lookup (xtex_cb) at the call; C also xc_gs at the call (step c's form); D as C, every kind held (df0, P06, P07: the df0 and P06
 * calls left in gl13's trace are those step c does not take); E the P07 calls held whole, the lookup too (a bound, not a form one can build).
 * stepc_sim's own description follows.
 *
 * stepc_sim: what step c (renderer_notes 11.24 "Next" 1) would save in I-misses, counted on a PspProf instruction trace before it is built.
 *
 *   stepc_sim <cfg> <trace> <steps per window> [<first window of the second group>]
 *
 * <cfg>: tools/dev/sb2/stepc_cfg.py's output for the traced build: the entries followed ("entry name hex"), the parts ("part i name"),
 *        every function's range with its part ("range lo hi part"), and the return address of every linking call in the text ("ret hex").
 * <trace>: PSPPROF_ITRACE runs (uint32 pc, uint32 count), {0xffffffff, 0} at each window's start (PSPPROF_ITRACE_ALL=1); read once, in
 *        order, so it can be a FIFO the emulator writes while this reads (no copy on disk).
 * Four I-caches as PspProf's (16 KB, 2-way, 64-byte lines, LRU; each window starts empty) are fed:
 *   A the trace as it is;
 *   B, C, D the trace in step c's order: the fetches of each converter call (gl_call entered by a linking call, from its entry to the
 *     run at its return address) and of the P06 passes that follow it (gl_pass) are held back and fetched at the transfer's end (the entry
 *     of xl_flush, which ends every VIF1 transfer of the one walk and the second walk's), the df0 calls' first (those that entered
 *     vc_df0_rec or vc_df0), then the P06 calls' (vc_p06_rec or vc_p06) each with its passes, each kind in the calls' order; a call of any
 *     other kind (the effects, P07, the empty ones) stays where it is.  What stays at the call (fetched where it was): B the texture lookup
 *     (xtex_cb and all it calls); C also xc_gs (the GS registers of that moment); D also vc_df0_rec / vc_df0 (df0's input, which the cut
 *     test needs before the texture lookup).  E: D with the df0 calls left where they were (only the P06 calls and their passes held back:
 *     the form step c takes when df0's work after the cut test, a copy of the vertices into the batch, stays at the call).
 * The code step c adds (the record at the call, the loop at the transfer's end) and every D-cache effect are not modelled.
 * Prints each window's misses of A..D, then by part for each group of windows (the first group up to the window given; e.g. the deck's
 * windows, then the rooms'), and the converter calls by kind.  A equals PspProf's count of each window (checked: 2026-10-30). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum { E_CALL, E_PASS, E_TEX, E_GS, E_DF0R, E_DF0, E_P06R, E_P06, E_P07R, E_P07, E_FLUSH, NE };
static const char *ENAME[NE] = { "gl_call", "gl_pass", "xtex_cb", "xc_gs", "vc_df0_rec", "vc_df0", "vc_p06_rec", "vc_p06", "vc_p07_rec", "vc_p07", "xl_flush" };
static uint32_t ent[NE];
#define NP 16
static char pname[NP + 1][64];
typedef struct { uint32_t lo, hi; int part; } Rg;
static Rg *rg, arena; static int nrg;
static uint32_t *ret; static int nret;
static int is_ret(uint32_t a) { int l = 0, h = nret; while (l < h) { int m = (l + h) / 2; if (ret[m] < a) l = m + 1; else h = m; } return l < nret && ret[l] == a; }
static int part_of(uint32_t pc)
{
  int l = 0, h = nrg - 1, best = -1;
  while (l <= h) { int m = (l + h) / 2; if (rg[m].lo <= pc) { best = m; l = m + 1; } else h = m - 1; }
  if (best >= 0 && pc < rg[best].hi) return rg[best].part;
  if (arena.lo <= pc && pc < arena.hi) return arena.part;   /* an overlay's code */
  return NP;
}

/* a cache: misses by part and by class (0 outside a converter call, 1 a converter call's fetch where it was, 2 one moved to the
   transfer's end) */
#define NV 5
typedef struct { uint32_t tag[128][2]; uint8_t lru[128]; long miss[3][NP + 1], wmiss[3][NP + 1]; } Cache;
static Cache cv[NV];
static const uint32_t VSET[NV] = { 0, 1u << E_TEX, 1u << E_TEX | 1u << E_GS, 1u << E_TEX | 1u << E_GS, 0 };
static const int VHOLD[NV] = { 0, 8, 8, 14, 8 };   /* the call kinds each variant holds back (bit 1 df0, bit 2 P06, bit 3 P07) */
static void c_reset(Cache *c) { for (int i = 0; i < 128; i++) { c->tag[i][0] = c->tag[i][1] = 0xffffffffu; c->lru[i] = 0; } }
static void c_run(Cache *c, uint32_t pc, uint32_t n, int cls)
{
  uint32_t l0 = (pc & 0x1fffffffu) >> 6, l1 = ((pc + 4 * n - 4) & 0x1fffffffu) >> 6;
  for (uint32_t line = l0; line <= l1; line++) {
    uint32_t s = line & 127;
    if (c->tag[s][0] == line) { c->lru[s] = 1; continue; }
    if (c->tag[s][1] == line) { c->lru[s] = 0; continue; }
    int x = c->lru[s]; c->tag[s][x] = line; c->lru[s] = (uint8_t)(x ^ 1);
    c->wmiss[cls][part_of(line << 6 | (pc & 0xe0000000u))]++;
  }
}

/* the runs of the transfer so far that belong to converter calls, each with the set of followed entries it lies in (bit E_TEX ...) */
typedef struct { uint32_t pc, n, in; } Run;
static Run *ar; static long nar, car;
static void ar_add(uint32_t pc, uint32_t n, uint32_t in)
{
  if (nar == car) { car = car ? 2 * car : 1 << 16; ar = realloc(ar, (size_t)car * sizeof *ar); if (!ar) { perror("realloc"); exit(1); } }
  ar[nar].pc = pc; ar[nar].n = n; ar[nar].in = in; nar++;
}
/* a held-back call: its kind and its runs (the call's and its passes'), as ranges of ar */
typedef struct { int kind; long r[3][2]; int nr; } Held;
static Held *hd; static int nhd, chd;
static long st_calls[4], st_pass, st_pass_inline, st_tail, st_bad, st_flush, st_maxrun, st_maxheld;

static void emit_range(long a, long b, int all, int cls, int kind)
{
  for (int v = 1; v < NV; v++) {
    int al = all || !(VHOLD[v] >> kind & 1);   /* a kind this variant does not hold: all of it where it was */
    for (long i = a; i < b; i++) {
      int at = (ar[i].in & VSET[v]) != 0;
      if (al || at == (cls == 1)) c_run(&cv[v], ar[i].pc, ar[i].n, al ? 1 : cls);
    }
  }
}
/* the end of a transfer: the held-back fetches, df0 calls first, then P06, then P07 */
static void flush_held(void)
{
  for (int k = 1; k <= 3; k++)
    for (int j = 0; j < nhd; j++) if (hd[j].kind == k)
      for (int q = 0; q < hd[j].nr; q++)
        for (int v = 1; v < NV; v++) if (VHOLD[v] >> k & 1)
          for (long i = hd[j].r[q][0]; i < hd[j].r[q][1]; i++)
            if (!(ar[i].in & VSET[v])) c_run(&cv[v], ar[i].pc, ar[i].n, 2);
  if (nar > st_maxrun) st_maxrun = nar;
  if (nhd > st_maxheld) st_maxheld = nhd;
  nhd = 0; nar = 0;
}

int main(int argc, char **argv)
{
  if (argc < 4) { fprintf(stderr, "usage: stepc_sim <cfg> <trace> <steps per window> [windows to show]\n"); return 1; }
  double steps = atof(argv[3]); long g2 = argc > 4 ? atol(argv[4]) : 1L << 30;
  FILE *f = fopen(argv[1], "r"); char ln[256]; if (!f) { perror(argv[1]); return 1; }
  int cr = 0, cre = 0;
  while (fgets(ln, sizeof ln, f)) {
    char w[64], x[64]; unsigned a, b; int p;
    if (sscanf(ln, "entry %63s %x", w, &a) == 2) { for (int i = 0; i < NE; i++) if (!strcmp(w, ENAME[i])) ent[i] = a; }
    else if (sscanf(ln, "part %d %63[^\n]", &p, x) == 2 && p >= 0 && p < NP) strcpy(pname[p], x);
    else if (sscanf(ln, "range %x %x %d", &a, &b, &p) == 3) {   /* the first: the overlay arena; then the functions, sorted */
      if (!arena.hi) { arena.lo = a; arena.hi = b; arena.part = p; continue; }
      if (nrg == cr) { cr = cr ? 2 * cr : 4096; rg = realloc(rg, (size_t)cr * sizeof *rg); if (!rg) { perror("realloc"); return 1; } }
      if (nrg && a < rg[nrg - 1].lo) { fprintf(stderr, "cfg: ranges not sorted\n"); return 1; }
      rg[nrg].lo = a; rg[nrg].hi = b; rg[nrg].part = p; nrg++; }
    else if (sscanf(ln, "ret %x", &a) == 1) { if (nret == cre) { cre = cre ? 2 * cre : 65536; ret = realloc(ret, (size_t)cre * sizeof *ret); } ret[nret++] = a; }
  }
  fclose(f); strcpy(pname[NP], "(no function)");
  for (int i = 0; i < NE; i++) if (!ent[i]) { fprintf(stderr, "cfg: no entry %s\n", ENAME[i]); return 1; }
  FILE *t = strcmp(argv[2], "-") ? fopen(argv[2], "rb") : stdin; if (!t) { perror(argv[2]); return 1; }
  static uint32_t buf[1 << 16]; size_t bn = 0, bi = 0;
  long win = -1, nwin[2] = { 0, 0 }; static long tot[2][NV][3][NP + 1];
  uint32_t prev_end = 0;
  int incall = 0, inpass = 0, kind = 0; uint32_t call_ret = 0, pass_ret = 0, sub_ret[NE], in = 0; long cstart = 0;
  memset(sub_ret, 0, sizeof sub_ret);
  long runs = 0;
  for (;;) {
    uint32_t pc, n; int eof = 0;
    if (bi == bn) { bn = fread(buf, 8, sizeof buf / 8, t); bi = 0; }
    if (bn == 0) { eof = 1; pc = 0xffffffffu; n = 0; } else { pc = buf[2 * bi]; n = buf[2 * bi + 1] & 0x7fffffffu; bi++; }
    if (pc == 0xffffffffu && n == 0) {   /* a window's start, or the trace's end: the window before is closed */
      if (nhd || nar) flush_held();
      if (win >= 0) {
        int g = win >= g2; nwin[g]++;
        for (int v = 0; v < NV; v++) for (int c = 0; c < 3; c++) for (int p = 0; p <= NP; p++) tot[g][v][c][p] += cv[v].wmiss[c][p];
        printf("window %ld:", win);
        for (int v = 0; v < NV; v++) { long s = 0; for (int c = 0; c < 3; c++) for (int p = 0; p <= NP; p++) s += cv[v].wmiss[c][p];
          printf("  %c %ld (%.1f k a step)", 'A' + v, s, s / steps / 1000); }
        printf("\n"); fflush(stdout);
      }
      if (eof) break;
      win++;
      for (int v = 0; v < NV; v++) { c_reset(&cv[v]); memset(cv[v].wmiss, 0, sizeof cv[v].wmiss); }
      incall = inpass = 0; in = 0; nhd = 0; nar = 0; prev_end = 0;
      continue;
    }
    runs++;
    /* a sub-extent ends where its caller's code is fetched again */
    if (in) for (int e = E_TEX; e < E_FLUSH; e++) if ((in & 1u << e) && pc == sub_ret[e]) in &= ~(1u << e);
    /* the followed entries: a transfer ends, a converter call or a pass begins or ends, a sub-extent begins */
    if (!incall && !inpass) {
      if (pc == ent[E_FLUSH]) { flush_held(); st_flush++; }
      else if (pc == ent[E_CALL]) {
        if (is_ret(prev_end)) { incall = 1; call_ret = prev_end; kind = 0; in = 0; cstart = nar; }
        else st_tail++;   /* on_call's tail call: not a converter's entry (gl_call returns at once) */
      } else if (pc == ent[E_PASS]) {
        if (is_ret(prev_end)) { inpass = 1; pass_ret = prev_end; in = 0; cstart = nar; }
        else st_bad++;
      }
    } else if (pc == (incall ? call_ret : pass_ret)) {   /* the call or the pass returned: this run is the caller's */
      if (incall) {
        incall = 0; st_calls[kind]++;
        if (kind >= 1) {   /* held back: what stays at the call is fetched now, the rest at the transfer's end */
          emit_range(cstart, nar, 0, 1, kind);
          if (nhd == chd) { chd = chd ? 2 * chd : 256; hd = realloc(hd, (size_t)chd * sizeof *hd); if (!hd) { perror("realloc"); exit(1); } }
          hd[nhd].kind = kind; hd[nhd].r[0][0] = cstart; hd[nhd].r[0][1] = nar; hd[nhd].nr = 1; nhd++;
        } else { emit_range(cstart, nar, 1, 1, 0); nar = cstart; }
      } else {
        inpass = 0; st_pass++;
        if (nhd && hd[nhd - 1].kind >= 2 && hd[nhd - 1].nr < 3 && hd[nhd - 1].r[hd[nhd - 1].nr - 1][1] == cstart) {   /* right after its P06 or P07 call */
          emit_range(cstart, nar, 0, 1, hd[nhd - 1].kind); Held *h = &hd[nhd - 1]; h->r[h->nr][0] = cstart; h->r[h->nr][1] = nar; h->nr++;
        } else { st_pass_inline++; emit_range(cstart, nar, 1, 1, 0); nar = cstart; }
      }
      in = 0;
    } else {
      for (int e = E_TEX; e < E_FLUSH; e++)
        if (pc == ent[e] && !(in & 1u << e)) {
          if (incall && (e == E_DF0R || e == E_DF0)) kind = 1; else if (incall && (e == E_P06R || e == E_P06)) kind = 2;
          else if (incall && (e == E_P07R || e == E_P07)) kind = 3;
          if (is_ret(prev_end)) { in |= 1u << e; sub_ret[e] = prev_end; } else st_bad++;
        }
    }
    /* the run: A at once; B..D at once outside a converter call, else held in ar until the call returns */
    if (incall || inpass) { c_run(&cv[0], pc, n, 1); ar_add(pc, n, in); }
    else for (int v = 0; v < NV; v++) c_run(&cv[v], pc, n, 0);
    prev_end = pc + 4 * n;
  }

  if (t != stdin) fclose(t);
  printf("\n%ld windows of %.0f steps, %ld runs; converter calls: df0 %ld, P06 %ld, P07 %ld; others: %ld; passes %ld (%ld not after a "
         "P06 or P07 call); gl_call tail calls (not converters) %ld; entries not from a linking call %ld; transfer ends %ld; most runs held "
         "in a transfer %ld, most calls %ld\n", win + 1, steps, runs, st_calls[1], st_calls[2], st_calls[3], st_calls[0], st_pass, st_pass_inline,
         st_tail, st_bad, st_flush, st_maxrun, st_maxheld);
  for (int g = 0; g < 2; g++) {
    if (!nwin[g]) continue;
    double ns = steps * (double)nwin[g];
    printf("\nI-misses a step (k), %s (%ld windows):  A as traced         B          C          D          E\n", g ? "second group" : "first group", nwin[g]);
    for (int p = 0; p <= NP; p++) {
      long s[NV]; int any = 0;
      for (int v = 0; v < NV; v++) { s[v] = 0; for (int c = 0; c < 3; c++) s[v] += tot[g][v][c][p]; any |= s[v] != 0; }
      if (!any) continue;
      printf("  %-36s", pname[p]);
      for (int v = 0; v < NV; v++) printf(" %10.2f", s[v] / ns / 1000);
      printf("\n");
    }
    long S[NV]; for (int v = 0; v < NV; v++) { S[v] = 0; for (int c = 0; c < 3; c++) for (int p = 0; p <= NP; p++) S[v] += tot[g][v][c][p]; }
    printf("  %-36s", "all"); for (int v = 0; v < NV; v++) printf(" %10.2f", S[v] / ns / 1000); printf("\n");
    static const char *CL[3] = { "  outside converter calls", "  converter calls, where they were", "  moved to the transfer's end" };
    for (int c = 0; c < 3; c++) {
      printf("  %-36s", CL[c]);
      for (int v = 0; v < NV; v++) { long x = 0; for (int p = 0; p <= NP; p++) x += tot[g][v][c][p]; printf(" %10.2f", x / ns / 1000); }
      printf("\n");
    }
    printf("  %-36s", "ms a step (125 cycles a miss)"); for (int v = 0; v < NV; v++) printf(" %10.2f", S[v] / ns * 125 / 333e3); printf("\n");
    printf("  %-36s", "ms a step (68.2: 11.25's refit)"); for (int v = 0; v < NV; v++) printf(" %10.2f", S[v] / ns * 68.2 / 333e3); printf("\n");
  }
  return 0;
}
