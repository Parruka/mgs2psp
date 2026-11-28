/* layout_sim: code placement against the PSP I-cache (16 KB, 2-way, 64-byte lines, LRU: the PspProf model).
 *
 *   layout_sim <trace> <funcs> [-and <trace2> <funcs2>] [-wl n] [-wd n] [-base0 a] [-base1 a] [-steps lo:hi] [-opt iterations]
 *              [-seed n] [-j threads] [-test n] [-out order.txt] [-in order.txt] [-new funcs2] [-fix0]
 *
 * <trace>: PspProf PSPPROF_ITRACE output, runs of sequential instruction fetches (uint32 pc, uint32 length), in fetch order.  Bit 31 of
 *          the length marks a fetch the profile paused for (the draw pipeline or the GS walk; PSPPROF_ITRACE_MARK=1), a record
 *          {0xffffffff, 0} starts a profiled window (PSPPROF_ITRACE_ALL=1: all windows of one run in one trace).
 * <funcs>: "region start size name" per movable function (region 0 = the main ELF's text, 1 = the overlay's), from tools/layout_funcs.py.
 *          A function's code moves as a unit; fetches outside every listed function keep their address.
 * -and: a second route's trace and function list: the same main ELF (its region 0 lines must equal the first list's), its own overlay,
 *       which is region 2 here (in -in / -out files too).  The main ELF's order serves both routes, each overlay has its own; the search
 *       minimises the sum of both routes' (weighted) misses.
 * -wl / -wd: weight of a miss in an unmarked (logic, IOP) / a marked fetch (default 1 / 1).  At 25 fps a displayed frame runs two logic
 *       steps and one draw once undisplayed steps skip their draw: -wl 2 -wd 1.
 * A layout is, per region, the hot functions (those a trace fetches) in order from the region's base (-base0: the main ELF's .text
 * start, where ld --section-ordering-file puts them; -base1: the overlay code's start after its header, for regions 1 and 2; default: the
 * first hot address of the current placement), each at 4-byte alignment (gcc's function sections on the PSP), followed by everything else.
 * The tool prints the I-misses of the current placement (exactly the PspProf count when the trace starts with the profile window) and of
 * the layout given with -in; -opt searches (simulated annealing on the simulated misses, moves: swap two hot functions, move one, from the
 * best of: current order, first-fetch order, or the -in order) and writes the best order with -out ("region name" per line, hot functions
 * only).  -steps restricts the search to a slice of each window by run index fraction (lo:hi in 0..1000) to make each evaluation cheaper;
 * the final numbers are always on the whole traces.  Each window starts with an empty cache, as PspProf's count does, so the windows are
 * simulated apart, on -j threads (default 4).  -test n: the last n windows of each trace are left out of the search and reported apart,
 * to see whether an order found on the other windows holds on play it was not fitted to.
 * -new: the function list of a relinked build (same objects, new placement; one trace only): prints the misses of its real addresses.
 * -fix0: the main ELF's functions keep their current addresses; only the overlays' orders are searched.
 * gl36 (build_gl36/layout_sim36.c, a copy of tools/layout_sim.c; the shared tool is unchanged):
 * -norm: the search's objective is the sum over the training windows of each window's misses divided by its misses under the start
 *       order (-in, else the better of current and first-fetch), so every window counts the same: a 1 % loss on one window weighs as
 *       much as a 1 % gain on another, whatever their sizes.  -guard p: a move is refused when any training window would end more than
 *       p per mille above its start-order misses (-guard 10: 1 %).  -cmp order.txt (up to 4): further orders evaluated window by window.
 * -inall: the functions -in lists count as hot even when the traces do not fetch them, so they keep their slot (an order taken on
 *       other traces then places every other function where it was).
 * At the end, a table of every window's misses (training and test): current placement, the start order, each -cmp order, the result.
 * gl40 (build_gl40/layout_sim40.c, a copy of build_gl38/layout_sim38.c): -bind k: a window's -wf cap (> 0) becomes a ceiling that
 *       binds: each miss above it adds k * 1e8 / cap to the objective (k 20: 1 % over the cap costs 20 % of a normalized window), so
 *       the search first brings every capped window under its cap; the start temperature is taken from the objective without that
 *       term; the end prints every capped window's result against its cap.
 *
 * gl41 (build_gl41/layout_sim41.c, a copy of layout_sim40.c): -pin name:ref:off: region 0's function name is not searched; it is
 *       placed after the last hot function of region 0 so that its start = (the address just before ref, i.e. the end of the function
 *       ref follows) + off modulo 8192, then aligned to its own alignment: the linker's `_l41_f = .;` before ref and
 *       `. = . + ((_l41_f + off - .) & 8191);` before name at the end of the hot list (attic/gl41s/results.md: P3's copy on the box
 *       cull form's sets, two lines in).  Its fetches count at that address; -in / -out / the seed leave it out.
 * Loading compresses each trace without changing any count: consecutive runs inside one function whose bytes lie within 8064 bytes of
 * each other touch at most 128 consecutive lines, one per cache set, wherever the function is placed; nothing else runs between them, so
 * each of those lines hits or misses once and ends most recently used, in any order.  They become the union of their byte ranges, ranges
 * less than 64 bytes apart joined (such a gap holds no whole line). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>

typedef struct { uint32_t start, size, align; int region, hot; char name[96]; long fetch; long first; } Fn;
static Fn *fn; static int nfn, cap;
typedef struct { long n, raw, nmark, win; uint32_t *pc, *len, *off; int *fid; uint8_t *mk; int ovl; long *ws; } Trace;   /* ws: window starts, ws[win] = n */
static Trace tr[2]; static int ntr = 1;
static int *ridx[3], nridx[3];   /* per region: function indices sorted by start (lookup) */
static uint32_t base[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };   /* first hot address per region (kept) */
static int *ord[3], nord[3];
static uint32_t *nb;   /* new start per function */
static int pin_id = -1, pin_ref = -1; static uint32_t pin_off; static const char *pin_arg;   /* gl41: -pin */
static int fix0;   /* -fix0: region 0 stays where it is */
static long wl = 1, wd = 1; static double t0f = 0.002; static const char *wfile;   /* gl38: -t0 x (the start temperature as a part of the start objective), -wf file (lines: trace window weight cap) */

static int find_in(int r, uint32_t pc)
{
  int lo = 0, hi = nridx[r] - 1;
  while (lo <= hi) { int m = (lo + hi) / 2; Fn *f = &fn[ridx[r][m]]; if (pc < f->start) hi = m - 1; else if (pc >= f->start + f->size) lo = m + 1; else return ridx[r][m]; }
  return -1;
}
static int find_fn(uint32_t pc, int ovl) { int id = find_in(0, pc); return id >= 0 ? id : find_in(ovl, pc); }
static void place(void)
{
  for (int r = fix0; r < 3; r++) {
    uint32_t a = base[r], pre = 0;
    for (int i = 0; i < nord[r]; i++) { Fn *f = &fn[ord[r][i]]; if (ord[r][i] == pin_ref) pre = a; a = (a + f->align - 1) & ~(f->align - 1); nb[ord[r][i]] = a; a += f->size; }
    if (r == 0 && pin_id >= 0) {   /* gl41: the pinned function after the hot list, on ref's sets + off */
      if (fn[pin_ref].region != 0 || !fn[pin_ref].hot) pre = nb[pin_ref];
      uint32_t b = a + ((pre + pin_off - a) & 8191u); b = (b + fn[pin_id].align - 1) & ~(fn[pin_id].align - 1); nb[pin_id] = b; }
  }
}
/* weighted misses of runs r0..r1 of t from an empty cache; *count: the plain count */
static long simulate(const Trace *t, long r0, long r1, long *count)
{
  uint32_t tag[128][2]; uint8_t lru[128];
  long miss = 0, wmiss = 0;
  for (int i = 0; i < 128; i++) { tag[i][0] = tag[i][1] = 0xffffffffu; lru[i] = 0; }
  for (long k = r0; k < r1; k++) {
    uint32_t a = t->fid[k] >= 0 ? nb[t->fid[k]] + t->off[k] : t->pc[k];
    uint32_t l0 = (a & 0x1fffffffu) >> 6, l1 = ((a + 4 * t->len[k] - 4) & 0x1fffffffu) >> 6; long w = t->mk[k] ? wd : wl;
    for (uint32_t line = l0; line <= l1; line++) {
      uint32_t s = line & 127;
      if (tag[s][0] == line) { lru[s] = 1; continue; }
      if (tag[s][1] == line) { lru[s] = 0; continue; }
      int x = lru[s]; tag[s][x] = line; lru[s] = (uint8_t)(x ^ 1); miss++; wmiss += w;
    }
  }
  if (count) *count = miss;
  return wmiss;
}
static int cmp_first(const void *a, const void *b) { long x = fn[*(const int *)a].first, y = fn[*(const int *)b].first; return x < y ? -1 : x > y; }
static int cmp_cur(const void *a, const void *b) { uint32_t x = fn[*(const int *)a].start, y = fn[*(const int *)b].start; return x < y ? -1 : x > y; }
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

static void add_fn(Fn *x) { if (nfn == cap) fn = realloc(fn, (cap = cap ? cap * 2 : 1 << 16) * sizeof *fn); fn[nfn++] = *x; }
/* which 0: regions 0 and 1 as listed; which 1 (-and): region 0 lines must match the first list, region 1 becomes region 2 */
static int load_funcs(const char *path, int which)
{
  FILE *f = fopen(path, "r"); if (!f) { perror(path); return -1; }
  char line[512]; int n0 = 0, n0first = 0, bad = 0;
  if (which) for (int i = 0; i < nfn; i++) n0first += fn[i].region == 0;
  while (fgets(line, sizeof line, f)) {
    Fn x = {0}; if (sscanf(line, "%d %x %x %95s", &x.region, &x.start, &x.size, x.name) != 4 || !x.size) continue;
    x.align = 4; x.first = -1;
    if (which && x.region == 0) {
      int ok = 0; n0++;   /* an equal entry (two names can share a start: scan all of region 0) */
      for (int k = 0; k < nridx[0] && !ok; k++) { Fn *y = &fn[ridx[0][k]]; ok = y->start == x.start && y->size == x.size && !strcmp(y->name, x.name); }
      bad += !ok;
      continue;
    }
    if (which) x.region = 2;
    add_fn(&x);
  }
  fclose(f);
  if (which && (bad || n0 != n0first)) { fprintf(stderr, "%s: region 0 differs from the first list (%d lines, %d differ, first list %d)\n", path, n0, bad, n0first); return -1; }
  return 0;
}
static void index_regions(void)
{
  for (int r = 0; r < 3; r++) { free(ridx[r]); ridx[r] = malloc((nfn + 1) * sizeof(int)); nridx[r] = 0; for (int i = 0; i < nfn; i++) if (fn[i].region == r) ridx[r][nridx[r]++] = i; }
  for (int r = 0; r < 3; r++) qsort(ridx[r], nridx[r], sizeof(int), cmp_cur);
}

/* the compressed trace under construction */
typedef struct { uint32_t s, e; } Iv;
static Iv *iv; static int niv, civ; static int sfid = -1, smk; static uint32_t slo, shi;
static long tcap;
static void push(Trace *t, uint32_t pc, uint32_t len, int fid, uint32_t off, int mk)
{
  if (t->n == tcap) {
    tcap = tcap ? tcap * 2 : 1 << 20;
    t->pc = realloc(t->pc, tcap * 4); t->len = realloc(t->len, tcap * 4); t->off = realloc(t->off, tcap * 4); t->fid = realloc(t->fid, tcap * sizeof(int)); t->mk = realloc(t->mk, tcap);
  }
  t->pc[t->n] = pc; t->len[t->n] = len; t->fid[t->n] = fid; t->off[t->n] = off; t->mk[t->n] = (uint8_t)mk; t->n++;
}
static int cmp_iv(const void *a, const void *b) { uint32_t x = ((const Iv *)a)->s, y = ((const Iv *)b)->s; return x < y ? -1 : x > y; }
static void merge_iv(void)
{
  qsort(iv, niv, sizeof *iv, cmp_iv);
  int m = 0;
  for (int i = 0; i < niv; i++) { if (m && iv[i].s < iv[m - 1].e + 64) { if (iv[i].e > iv[m - 1].e) iv[m - 1].e = iv[i].e; } else iv[m++] = iv[i]; }
  niv = m;
}
static void flush_seg(Trace *t)
{
  if (sfid < 0) return;
  merge_iv();
  for (int i = 0; i < niv; i++) push(t, fn[sfid].start + iv[i].s, (iv[i].e - iv[i].s) / 4, sfid, iv[i].s, smk);
  sfid = -1; niv = 0;
}
static int load_trace(const char *path, int ti, int ovl, long first0)
{
  Trace *t = &tr[ti]; t->ovl = ovl; tcap = 0;
  FILE *f = fopen(path, "rb"); if (!f) { perror(path); return -1; }
  static uint32_t buf[2 * 65536]; size_t n; long k = 0;
  while ((n = fread(buf, 8, 65536, f)) > 0) for (size_t i = 0; i < n; i++, k++) {
    uint32_t pc = buf[2 * i], len = buf[2 * i + 1] & 0x7fffffffu; int mk = buf[2 * i + 1] >> 31;
    if (!len) { flush_seg(t); t->ws = realloc(t->ws, (t->win + 2) * sizeof(long)); t->ws[t->win++] = t->n; continue; }
    t->raw++; t->nmark += mk;
    int id = find_fn(pc, ovl);
    if (id >= 0 && pc + 4 * len > fn[id].start + fn[id].size) id = -1;   /* a run leaving the function: keep it fixed */
    if (id < 0) { flush_seg(t); push(t, pc, len, -1, 0, mk); continue; }
    fn[id].fetch += len; if (fn[id].first < 0) fn[id].first = first0 + k; fn[id].hot = 1;
    uint32_t s = pc - fn[id].start, e = s + 4 * len;
    if (id == sfid && mk == smk && (e > shi ? e : shi) - (s < slo ? s : slo) <= 8064) { if (s < slo) slo = s; if (e > shi) shi = e; }
    else { flush_seg(t); sfid = id; smk = mk; slo = s; shi = e; }
    if (niv == civ) iv = realloc(iv, (civ = civ ? civ * 2 : 1024) * sizeof *iv);
    iv[niv].s = s; iv[niv].e = e; niv++;
    if (niv >= 4096) merge_iv();
  }
  flush_seg(t); fclose(f);
  /* ws[0] = 0, ws[1..win-1]: the later windows' starts, ws[win] = n; a trace without separators is one window */
  { long *w = malloc((t->win + 2) * sizeof(long)); int m = 0; w[m++] = 0; for (long i = 0; i < t->win; i++) if (t->ws[i] > w[m - 1] && t->ws[i] < t->n) w[m++] = t->ws[i];
    w[m] = t->n; free(t->ws); t->ws = w; t->win = m; }
  return 0;
}
/* the windows of all traces as work items, evaluated on -j threads: item i is window w of trace t */
typedef struct { int t; long r0, r1; long e, raw, seed; int w; double wt; long cap; } Item;   /* gl38: w the window's index in its trace, wt its weight in the objective, cap an absolute guard limit (-wf) */
static Item *items; static int nitems, nthr = 4, ntest; static int norm_on; static long guard_pm = -1; static long *newcol; static long slo_ = 0, shi_ = 1000;
static double bind_k; static long pen_last;   /* gl40: -bind k, the last total()'s cap term */
static int job_which; static volatile int job_next; static pthread_mutex_t job_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t bar_go, bar_done; static int job_quit;
static int item_wanted(const Item *it, int which) { return which < 0 || which == 0 || tr[it->t].ovl == which; }
static void run_items(void)
{
  for (;;) {
    pthread_mutex_lock(&job_mx); int i = job_next++; pthread_mutex_unlock(&job_mx);
    if (i >= nitems) return;
    Item *it = &items[i];
    if (!item_wanted(it, job_which)) continue;
    long n = it->r1 - it->r0;
    it->raw = simulate(&tr[it->t], it->r0 + n * slo_ / 1000, it->r0 + n * shi_ / 1000, 0);
    it->e = norm_on && it->seed > 0 ? (long)((double)it->raw * 1.0e8 * it->wt / (double)it->seed + 0.5) : (long)(it->raw * it->wt + 0.5);
  }
}
static void *worker(void *arg)
{
  (void)arg;
  for (;;) { pthread_barrier_wait(&bar_go); if (job_quit) return 0; run_items(); pthread_barrier_wait(&bar_done); }
}
static void make_items(void)
{
  for (int t = 0; t < ntr; t++) for (long w = 0; w < tr[t].win - ntest; w++) {
    items = realloc(items, (nitems + 1) * sizeof *items);
    items[nitems].t = t; items[nitems].r0 = tr[t].ws[w]; items[nitems].r1 = tr[t].ws[w + 1]; items[nitems].e = items[nitems].raw = items[nitems].seed = 0; items[nitems].w = (int)w; items[nitems].wt = 1.0; items[nitems].cap = 0; nitems++;
  }
  /* largest first, so the threads finish together */
  for (int i = 1; i < nitems; i++) for (int j = i; j > 0 && items[j].r1 - items[j].r0 > items[j - 1].r1 - items[j - 1].r0; j--) { Item x = items[j]; items[j] = items[j - 1]; items[j - 1] = x; }
  if (nthr > 1) {
    pthread_barrier_init(&bar_go, 0, nthr); pthread_barrier_init(&bar_done, 0, nthr);
    for (int i = 1; i < nthr; i++) { pthread_t th; pthread_create(&th, 0, worker, 0); pthread_detach(th); }
  }
}
/* weighted misses of the search slice: which -1 all traces, else only the windows a move in region `which` changes (the others keep e) */
static long total(int which)
{
  job_which = which; job_next = 0;
  if (nthr > 1) { pthread_barrier_wait(&bar_go); run_items(); pthread_barrier_wait(&bar_done); }
  else run_items();
  long s = 0; for (int i = 0; i < nitems; i++) s += items[i].e;
  long pen = 0; if (bind_k > 0) for (int i = 0; i < nitems; i++) if (items[i].cap > 0 && items[i].raw > items[i].cap) pen += (long)(bind_k * 1.0e8 * (double)(items[i].raw - items[i].cap) / (double)items[i].cap + 0.5);
  pen_last = pen; s += pen;   /* gl40: the binding caps */
  if (guard_pm >= 0) for (int i = 0; i < nitems; i++) if (items[i].seed > 0 && items[i].raw * 1000 > items[i].seed * (1000 + guard_pm) && items[i].raw > items[i].cap) return s + (1L << 50);   /* gl36: the guard */
  return s;
}
static void save_e(long *k) { for (int i = 0; i < nitems; i++) { k[2 * i] = items[i].e; k[2 * i + 1] = items[i].raw; } }
static void load_e(const long *k) { for (int i = 0; i < nitems; i++) { items[i].e = k[2 * i]; items[i].raw = k[2 * i + 1]; } }
/* gl36: every window's plain misses under the current ord[] (whole windows) into col[] */
static void wins_now(long *col) { int q = 0; place(); for (int t = 0; t < ntr; t++) for (long k = 0; k < tr[t].win; k++) { long c; simulate(&tr[t], tr[t].ws[k], tr[t].ws[k + 1], &c); col[q++] = c; } }
static int load_order(const char *path, int **o)
{
  FILE *g = fopen(path, "r"); if (!g) { perror(path); return -1; }
  int n[3] = {0, 0, 0}; char nm[96], line[512]; int r;
  while (fgets(line, sizeof line, g)) if (sscanf(line, "%d %95s", &r, nm) == 2 && r >= 0 && r < 3) for (int i = 0; i < nfn; i++) if (fn[i].hot && fn[i].region == r && !strcmp(fn[i].name, nm)) { o[r][n[r]++] = i; break; }
  fclose(g);
  if (n[0] != nord[0] || n[1] != nord[1] || n[2] != nord[2]) { fprintf(stderr, "%s lists %d + %d + %d of the %d + %d + %d hot functions\n", path, n[0], n[1], n[2], nord[0], nord[1], nord[2]); return -1; }
  return 0;
}
static void report(const char *what)
{
  long all = 0;
  printf("%s:", what);
  long allt = 0;
  for (int t = 0; t < ntr; t++) {
    long c = 0, w = 0, ct = 0, wt = 0;
    for (long k = 0; k < tr[t].win; k++) { long c1, w1 = simulate(&tr[t], tr[t].ws[k], tr[t].ws[k + 1], &c1); if (k < tr[t].win - ntest) { w += w1; c += c1; } else { wt += w1; ct += c1; } }
    all += w; allt += wt; printf(" trace %d %ld I-misses (weighted %ld)", t + 1, c, w);
    if (ntest) printf(" test windows %ld (weighted %ld)", ct, wt);
  }
  printf("%s%ld", ntr > 1 ? ", weighted sum " : ", ", all);
  if (ntest) printf(", test %ld", allt);
  printf("\n");
}

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: layout_sim <trace> <funcs> [-and trace2 funcs2] [-wl n] [-wd n] [-steps lo:hi] [-opt n] [-seed n] [-out f] [-in f] [-new f] [-fix0]\n"); return 1; }
  const char *cmpf[4]; int ncmp = 0; int inall = 0;
  long iters = 0; const char *out = 0, *in = 0, *nw = 0, *t2 = 0, *f2 = 0; uint32_t sb[2] = { 0, 0 };
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "-opt") && i + 1 < argc) iters = atol(argv[++i]);
    else if (!strcmp(argv[i], "-steps") && i + 1 < argc) sscanf(argv[++i], "%ld:%ld", &slo_, &shi_);
    else if (!strcmp(argv[i], "-seed") && i + 1 < argc) rs ^= (uint64_t)atoll(argv[++i]) * 0x9e3779b97f4a7c15ull;
    else if (!strcmp(argv[i], "-out") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i], "-in") && i + 1 < argc) in = argv[++i];
    else if (!strcmp(argv[i], "-new") && i + 1 < argc) nw = argv[++i];
    else if (!strcmp(argv[i], "-and") && i + 2 < argc) { t2 = argv[++i]; f2 = argv[++i]; }
    else if (!strcmp(argv[i], "-wl") && i + 1 < argc) wl = atol(argv[++i]);
    else if (!strcmp(argv[i], "-wd") && i + 1 < argc) wd = atol(argv[++i]);
    else if (!strcmp(argv[i], "-fix0")) fix0 = 1;
    else if (!strcmp(argv[i], "-t0") && i + 1 < argc) t0f = atof(argv[++i]);
    else if (!strcmp(argv[i], "-wf") && i + 1 < argc) wfile = argv[++i];
    else if (!strcmp(argv[i], "-norm")) norm_on = 1;
    else if (!strcmp(argv[i], "-bind") && i + 1 < argc) bind_k = atof(argv[++i]);
    else if (!strcmp(argv[i], "-inall")) inall = 1;
    else if (!strcmp(argv[i], "-pin") && i + 1 < argc) pin_arg = argv[++i];
    else if (!strcmp(argv[i], "-guard") && i + 1 < argc) guard_pm = atol(argv[++i]);
    else if (!strcmp(argv[i], "-cmp") && i + 1 < argc && ncmp < 4) cmpf[ncmp++] = argv[++i];
    else if (!strcmp(argv[i], "-j") && i + 1 < argc) { nthr = atoi(argv[++i]); if (nthr < 1) nthr = 1; }
    else if (!strcmp(argv[i], "-test") && i + 1 < argc) ntest = atoi(argv[++i]);
    else if (!strncmp(argv[i], "-base", 5) && (argv[i][5] == '0' || argv[i][5] == '1') && i + 1 < argc) { int r = argv[i][5] - '0'; sb[r] = (uint32_t)strtoul(argv[++i], 0, 16); }   /* r first: the index and the ++i are unsequenced */
    else { fprintf(stderr, "unknown or incomplete option %s\n", argv[i]); return 1; }
  }
  if (nw && t2) { fprintf(stderr, "-new takes one trace\n"); return 1; }
  if (load_funcs(argv[2], 0)) return 1;
  index_regions();
  if (t2) { if (load_funcs(f2, 1)) return 1; index_regions(); ntr = 2; }
  if (load_trace(argv[1], 0, 1, 0)) return 1;
  if (t2 && load_trace(t2, 1, 2, 1L << 40)) return 1;
  if (inall && in) {   /* gl36 -inall: every function the -in order lists counts as hot (keeps its slot), fetched or not */
    FILE *g = fopen(in, "r"); char l2[512], nm2[96]; int r2, k2 = 0; if (!g) { perror(in); return 1; }
    while (fgets(l2, sizeof l2, g)) if (sscanf(l2, "%d %95s", &r2, nm2) == 2) for (int i = 0; i < nfn; i++) if (fn[i].region == r2 && !fn[i].hot && !strcmp(fn[i].name, nm2)) { fn[i].hot = 1; k2++; break; }
    fclose(g); fprintf(stderr, "-inall: %d functions the traces do not fetch kept in their slots\n", k2); }
  if (pin_arg) {   /* gl41: -pin name:ref:off */
    char pn[96], rn[96]; unsigned po; const char *c1 = strchr(pin_arg, ':'), *c2 = c1 ? strchr(c1 + 1, ':') : 0;
    if (!c1 || !c2 || c1 - pin_arg >= 96 || c2 - c1 - 1 >= 96) { fprintf(stderr, "-pin name:ref:off\n"); return 1; }
    memcpy(pn, pin_arg, c1 - pin_arg); pn[c1 - pin_arg] = 0; memcpy(rn, c1 + 1, c2 - c1 - 1); rn[c2 - c1 - 1] = 0; po = (unsigned)strtoul(c2 + 1, 0, 0);
    for (int i = 0; i < nfn; i++) if (fn[i].region == 0 && !strcmp(fn[i].name, pn)) pin_id = i; else if (fn[i].region == 0 && !strcmp(fn[i].name, rn)) pin_ref = i;
    if (pin_id < 0 || pin_ref < 0) { fprintf(stderr, "-pin: %s or %s not in region 0's list\n", pn, rn); return 1; }
    pin_off = po; fprintf(stderr, "-pin: %s (fetched %ld bytes) placed after the hot list on %s's sets + 0x%x (not searched)\n", pn, fn[pin_id].fetch, rn, po);
    fn[pin_id].hot = 0; }
  nb = malloc(nfn * sizeof *nb);
  for (int i = 0; i < nfn; i++) { nb[i] = fn[i].start; if (fn[i].hot && fn[i].start < base[fn[i].region]) base[fn[i].region] = fn[i].start; }
  if (sb[0]) base[0] = sb[0];
  if (sb[1]) base[1] = base[2] = sb[1];
  for (int r = 0; r < 3; r++) { ord[r] = malloc((nfn + 1) * sizeof(int)); nord[r] = 0; for (int i = 0; i < nfn; i++) if (fn[i].hot && fn[i].region == r) ord[r][nord[r]++] = i; }
  for (int t = 0; t < ntr; t++) {
    long fixed = 0; for (long k = 0; k < tr[t].n; k++) if (tr[t].fid[k] < 0) fixed += tr[t].len[k];
    fprintf(stderr, "trace %d: %ld runs (%ld marked), %ld windows, %ld runs after compression, %ld fetches outside the functions\n", t + 1, tr[t].raw, tr[t].nmark, tr[t].win, tr[t].n, fixed);
  }
  { long hs[3] = {0, 0, 0}; for (int r = 0; r < 3; r++) for (int i = 0; i < nord[r]; i++) hs[r] += fn[ord[r][i]].size;
    fprintf(stderr, "%d functions, fetched: region 0 %d (%ld bytes), 1 %d (%ld), 2 %d (%ld)\n", nfn, nord[0], hs[0], nord[1], hs[1], nord[2], hs[2]); }
  report("current placement");
  /* the current placement in "hot first" form: the cold functions between hot ones close up */
  for (int r = 0; r < 3; r++) qsort(ord[r], nord[r], sizeof(int), cmp_cur);
  place(); report("current order, hot functions packed");
  char line[512];
  if (nw) {
    FILE *g = fopen(nw, "r"); if (!g) { perror(nw); return 1; }
    for (int i = 0; i < nfn; i++) nb[i] = 0xffffffffu;
    int r; uint32_t a, sz; char nm[96]; int found = 0, bad = 0;
    while (fgets(line, sizeof line, g)) if (sscanf(line, "%d %x %x %95s", &r, &a, &sz, nm) == 4)
      for (int i = 0; i < nfn; i++) if (fn[i].region == r && !strcmp(fn[i].name, nm)) { nb[i] = a; found++; if (sz != fn[i].size && fn[i].hot) bad++; break; }
    fclose(g);
    int miss = 0; for (int i = 0; i < nfn; i++) if (fn[i].hot && nb[i] == 0xffffffffu) { miss++; nb[i] = fn[i].start; }
    printf("%s: %d functions matched, %d hot ones missing, %d hot ones changed size; ", nw, found, miss, bad); report("I-misses");
    { long nw2 = 0; for (int t = 0; t < ntr; t++) nw2 += tr[t].win; newcol = malloc(nw2 * sizeof(long)); int q = 0;   /* gl36: the relinked build's own addresses, window by window */
      for (int t = 0; t < ntr; t++) for (long k = 0; k < tr[t].win; k++) { long c; simulate(&tr[t], tr[t].ws[k], tr[t].ws[k + 1], &c); newcol[q++] = c; } }
  }
  if (in) {
    FILE *g = fopen(in, "r"); if (!g) { perror(in); return 1; }
    int n[3] = {0, 0, 0}; char nm[96]; int r;
    while (fgets(line, sizeof line, g)) if (sscanf(line, "%d %95s", &r, nm) == 2 && r >= 0 && r < 3) for (int i = 0; i < nfn; i++) if (fn[i].hot && fn[i].region == r && !strcmp(fn[i].name, nm)) { ord[r][n[r]++] = i; break; }
    fclose(g);
    if (n[0] != nord[0] || n[1] != nord[1] || n[2] != nord[2]) { fprintf(stderr, "%s lists %d + %d + %d of the %d + %d + %d hot functions\n", in, n[0], n[1], n[2], nord[0], nord[1], nord[2]); return 1; }
    place(); report(in);
  }
  if (iters > 0) {
    int *best[3], *keep[3], *init[3]; make_items(); long *k_t = malloc(2 * nitems * sizeof(long));
    setvbuf(stdout, 0, _IOLBF, 0);
    if (wfile) { FILE *g = fopen(wfile, "r"); if (!g) { fprintf(stderr, "no %s\n", wfile); return 1; } int a, b; double c; long d;
      while (fscanf(g, "%d %d %lf %ld", &a, &b, &c, &d) == 4) { int hit = 0; for (int i = 0; i < nitems; i++) if (items[i].t == a - 1 && items[i].w == b) { items[i].wt = c; items[i].cap = d; hit = 1; }
        printf("-wf: trace %d window %d weight %g cap %ld%s\n", a, b, c, d, hit ? "" : " (NO SUCH TRAINING WINDOW)"); } fclose(g); }
    printf("-t0 %g\n", t0f);
    for (int r = 0; r < 3; r++) { best[r] = malloc((nord[r] + 1) * sizeof(int)); keep[r] = malloc((nord[r] + 1) * sizeof(int)); init[r] = malloc((nord[r] + 1) * sizeof(int)); memcpy(init[r], ord[r], nord[r] * sizeof(int)); }
    /* start: the -in order, else the better of the current order and first-fetch order (on the slice) */
    for (int r = 0; r < 3; r++) qsort(ord[r], nord[r], sizeof(int), cmp_cur);
    place(); long ec = total(-1);
    for (int r = 0; r < 3; r++) memcpy(keep[r], ord[r], nord[r] * sizeof(int));
    for (int r = fix0; r < 3; r++) qsort(ord[r], nord[r], sizeof(int), cmp_first);
    place(); long ef = total(-1);
    printf("slice %ld..%ld: current order %ld, first-fetch order %ld\n", slo_, shi_, ec, ef);
    if (ec < ef) for (int r = 0; r < 3; r++) memcpy(ord[r], keep[r], nord[r] * sizeof(int));
    if (in) for (int r = 0; r < 3; r++) memcpy(ord[r], init[r], nord[r] * sizeof(int));
    place(); long e = total(-1), eb = e;
    if (in) printf("slice: %s %ld\n", in, e);
    if (norm_on || guard_pm >= 0) { for (int i = 0; i < nitems; i++) items[i].seed = items[i].raw; e = eb = total(-1);
      printf("gl36 objective: %s, guard %s; start %ld\n", norm_on ? "each window by its start-order misses (1e8 = the start order's)" : "plain misses", guard_pm >= 0 ? "on" : "off", e); }
    for (int r = 0; r < 3; r++) memcpy(best[r], ord[r], nord[r] * sizeof(int));
    long nmov = 0; for (int r = fix0; r < 3; r++) nmov += nord[r];
    double T0 = (e - pen_last) * t0f, T;   /* gl40: without the cap term */
    if (bind_k > 0) printf("gl40 -bind %g: the caps' term at the start %ld (objective without it %ld)\n", bind_k, pen_last, e - pen_last);
    for (long it = 0; it < iters; it++) {
      T = T0 * (1.0 - (double)it / iters) + 1e-9;
      long pick = (long)(rnd() % (uint64_t)nmov); int r = fix0; while (pick >= nord[r]) pick -= nord[r++];
      if (nord[r] < 2) continue;
      int i = rnd() % nord[r], j = rnd() % nord[r]; if (i == j) continue;
      int mv = rnd() & 1;
      memcpy(keep[r], ord[r], nord[r] * sizeof(int)); save_e(k_t);
      if (mv) { int t = ord[r][i]; ord[r][i] = ord[r][j]; ord[r][j] = t; }
      else { int t = ord[r][i]; if (i < j) memmove(&ord[r][i], &ord[r][i + 1], (j - i) * sizeof(int)); else memmove(&ord[r][j + 1], &ord[r][j], (i - j) * sizeof(int)); ord[r][j] = t; }
      place(); long e2 = total(r);
      if (e2 <= e || exp((e - e2) / T) * 4294967296.0 > (double)(rnd() & 0xffffffffu)) {
        e = e2; if (e < eb) { eb = e; for (int q = 0; q < 3; q++) memcpy(best[q], ord[q], nord[q] * sizeof(int)); }
      } else { memcpy(ord[r], keep[r], nord[r] * sizeof(int)); load_e(k_t); }
      if ((it + 1) % 1000 == 0) { fprintf(stderr, "it %ld: %ld (best %ld)\n", it + 1, e, eb); fflush(stderr); }
    }
    for (int r = 0; r < 3; r++) memcpy(ord[r], best[r], nord[r] * sizeof(int));
    place(); printf("optimized order: slice %ld\n", eb); report("optimized order, whole traces");
    if (bind_k > 0) { total(-1); for (int i = 0; i < nitems; i++) if (items[i].cap > 0) printf("bind: trace %d window %d: cap %ld, result %ld (%+.2f%% of the cap) %s\n", items[i].t + 1, items[i].w, items[i].cap, items[i].raw, 100.0 * (items[i].raw - items[i].cap) / (double)items[i].cap, items[i].raw <= items[i].cap ? "under" : "OVER"); }
    if (out) { FILE *g = fopen(out, "w"); for (int r = 0; r < 3; r++) for (int i = 0; i < nord[r]; i++) fprintf(g, "%d %s\n", r, fn[ord[r][i]].name); fclose(g); }
  }
  { /* gl36: the per-window table */
    long nw = 0; for (int t = 0; t < ntr; t++) nw += tr[t].win;
    long *col[8]; const char *lab[8]; int nc = 0; int *sv[3];
    for (int r = 0; r < 3; r++) { sv[r] = malloc((nord[r] + 1) * sizeof(int)); memcpy(sv[r], ord[r], nord[r] * sizeof(int)); }
    int *tmp[3]; for (int r = 0; r < 3; r++) tmp[r] = malloc((nord[r] + 1) * sizeof(int));
    if (in && !load_order(in, tmp)) { for (int r = 0; r < 3; r++) memcpy(ord[r], tmp[r], nord[r] * sizeof(int)); col[nc] = malloc(nw * sizeof(long)); wins_now(col[nc]); lab[nc++] = "start (-in)"; }
    for (int c = 0; c < ncmp; c++) if (!load_order(cmpf[c], tmp)) { for (int r = 0; r < 3; r++) memcpy(ord[r], tmp[r], nord[r] * sizeof(int)); col[nc] = malloc(nw * sizeof(long)); wins_now(col[nc]); lab[nc++] = cmpf[c]; }
    if (newcol) { col[nc] = newcol; lab[nc++] = "the relinked build (-new)"; }
    if (iters > 0 || (!in && !ncmp && !newcol)) { for (int r = 0; r < 3; r++) memcpy(ord[r], sv[r], nord[r] * sizeof(int)); col[nc] = malloc(nw * sizeof(long)); wins_now(col[nc]); lab[nc++] = iters > 0 ? "result" : "current order"; }
    printf("per-window table (I-misses of each whole window; the columns:"); for (int c = 0; c < nc; c++) printf(" [%d] %s", c, lab[c]); printf(")\n");
    int q = 0; long sum[8][2] = {{0}};
    for (int t = 0; t < ntr; t++) for (long k = 0; k < tr[t].win; k++, q++) {
      int test = k >= tr[t].win - ntest; printf("  trace %d window %ld%s:", t + 1, k, test ? " (test)" : "        ");
      for (int c = 0; c < nc; c++) { printf(" %9ld", col[c][q]); sum[c][test] += col[c][q]; if (c) printf(" (%+6.2f%%)", 100.0 * (col[c][q] - col[0][q]) / (double)col[0][q]); }
      printf("\n"); }
    printf("  sums, training windows:"); for (int c = 0; c < nc; c++) printf(" %ld", sum[c][0]); printf("; test windows:"); for (int c = 0; c < nc; c++) printf(" %ld", sum[c][1]); printf("\n");
  }
  return 0;
}
