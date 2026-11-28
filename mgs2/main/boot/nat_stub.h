/* nat_stub.h: the native renderer's step 1, the main CPU's part (native_renderer_design.md 6.1 "stub"; native_step0_report.md's
   corrections).  Included by boot/boot.c (the PSP) and boot/boot_x86.c (the x86 twin) just before rt_dg_call: one text for both, so the
   stub's choices, which are part of the run (a stubbed call writes no packet), are the same on both.

   The builder 0x1214a0 (gen: P1214a0) is reached through the wrapper below (the generated function is linked as P1214a0_orig: objcopy
   in the build, gen.o itself is not compiled again).  In a full call of the draw pipeline (rt_dg_call: g_zt set, g_dg_lite clear) the
   wrapper asks rt_nat_p04: 1 = stubbed, the builder does not run; 0 = the builder runs as always.

   MGS2_NAT=<n>: the arm for the whole run.  MGS2_NAT_CYCLE=<a>,<b>,...: the arm in each turn of MGS2_OPT_CYCLE's schedule (its <from>
   and <n>: turn i = ((vb - from - 1) / n) % the count here, from vb > from; before that MGS2_NAT's).  The arm is taken at each pipeline
   call from its vblank.
     0  A  today's path: the builder runs, nothing is recorded.
     3  B  native: the call is stubbed and its groups are recorded (ge/natsnap.h NatGrp) in the array of the packet buffer the call
           writes; the picture's walk draws them, merged by key (ge/xlme.c on the PSP, boot/nat_x86.c on the twin).
     4  C  stubbed and not drawn: no record (timing only: the picture has no ship mesh).
     1, 2  x86 only (shadow): the builder runs and the groups are recorded with their packets' addresses; 2: the walk draws the record
           and steps over the packet.
   A call in arm B runs the builder instead (returns 0, counted) when: its buffer's array is full (NAT_MAXGRP); a stream's count is
   over the UNPACK's field; the packet control's destination is not in the buffer the pipeline call started in; its key is below the
   last one recorded (the walk merges by a cursor: keys must not fall; never on the routes [step 0]); or the builder's own overflow
   test could come out differently because the stubbed groups wrote nothing: the packet position with every stubbed group's quadwords
   added (the virtual position, vq) is within NAT_END_MARGIN of the buffer's end (least margin on the routes 1.27 MB [step 0]).
   These tests use the game's state and constants only, never the host's memory: with no array (g 0) the choices are the same and
   nothing is drawn.
   Each packet buffer has its array (the game builds a picture in one buffer while the one before is sent from the other; on the PSP the
   ME copies a sent picture's groups while the next pipeline call already records).  A host with room for one array only gives it to
   both (g_nat[0].g == g_nat[1].g): then a pipeline call's first recorded call ends the other buffer's groups too, after the host's
   g_nat_sync has had them copied.
   Host memory only is written here (the arrays, counts); the game's state is read. */
#include "../ge/natsnap.h"

#define NAT_END_MARGIN 0x10000
NatBuf g_nat[2];                 /* [1]: the lower packet buffer (NAT_BUF) */
uint32_t g_dg_seq;               /* the draw pipeline's zero-time calls so far (rt_dg_call) */
uint32_t g_nat_bseq[2];          /* the last such call that started with the packet write pointer [0x1e50a0] in each buffer */
int g_nat_arm = -1;              /* the arm of the last pipeline call that reached the builder (-1: none yet) */
int g_nat_cfg;                   /* 1: MGS2_NAT or MGS2_NAT_CYCLE names an arm other than A */
long g_nat_c[NC_N];              /* counts (the host prints them) */
void (*g_nat_sync)(int b);       /* the host's, before a buffer's array is written again (psp/xl_psp.c: its copies still owed) */
static int nat_set = -1, nat_all, nat_ncyc; static long nat_from = -1, nat_turn = 1; static uint8_t nat_cyc[32];
/* gl33, static streams read in place (ge/natsnap.h NATG_INPL).  MGS2_NAT_INPL=1 for the whole run, MGS2_NAT_INPL_CYCLE=<a>,<b>,... by
   MGS2_OPT_CYCLE's turns (as MGS2_NAT_CYCLE): 1: a native group with the P04+df0 entry whose three streams lie outside packet memory is
   flagged, and the PSP's copy plan leaves its streams in RAM (the ME's walk reads them there).  0 (the default): gl31's path.  Only
   arm 3 (native) flags its groups.  g_nat_stc: groups flagged */
int g_nat_inpl; long g_nat_stc;
static int nat_inpl_all, nat_inpl_ncyc; static uint8_t nat_inpl_cyc[32];
/* gl34, the fast native front end (ge/natsnap.h NATG_FAST; ge/xlate.h xl_p04_fast).  MGS2_NAT_FAST=1 for the whole run,
   MGS2_NAT_FAST_CYCLE=<a>,<b>,... by MGS2_OPT_CYCLE's turns (as MGS2_NAT_CYCLE): 1: every group arm 3 records is flagged, and the
   picture's walk hands it to the fast front end.  0 (the default): gl33's path.  g_nat_fst: groups flagged */
int g_nat_fast; long g_nat_fst;
/* gl35, the keeps for fast native groups (ge/natsnap.h NATG_KEEP; ge/xlate.h xl_p04_fast; attic/gl35/keep_design.txt).  MGS2_NAT_KEEP=<bits>
   for the whole run, MGS2_NAT_KEEP_CYCLE=<bits>,<bits>,... by the same turns: 1 the drawn areas, 2 the A+D packet, 4 the texture of the
   groups' own look-ups, 8 the texture of every look-up.  MGS2_NAT_KCHK=1: the check mode (the kept steps run in full and are compared).
   g_nat_kpt: groups flagged with any bit */
int g_nat_keep; long g_nat_kpt;
static int nat_keep_all, nat_keep_ncyc; static uint8_t nat_keep_cyc[32];
extern int xl_keep_chk;
static int nat_fast_all, nat_fast_ncyc; static uint8_t nat_fast_cyc[32];
/* gl36, the reuse of a run's translation from the last picture (ge/xlate.h xl_nr_*; attic/gl36/reuse_design.txt).  MGS2_NAT_REUSE=1 for
   the whole run, MGS2_NAT_REUSE_CYCLE=<a>,<b>,... by the same turns: 1: the picture's fast native runs are replayed when they match
   the kept entries, else recorded.  MGS2_NAT_RCHK=1: the check mode (a matching run is fed in full, recorded again and compared).
   Taken at each pipeline call with the other switches, kept in its buffer's NatBuf.ru */
int g_nat_reuse, g_nat_rchk, g_nat_rcfg;   /* g_nat_rcfg: 1 when MGS2_NAT_REUSE or MGS2_NAT_REUSE_CYCLE names a 1 (the host makes the table) */
struct NrEnt; void xl_nr_table(struct NrEnt *t, int cap, int chk);   /* (ge/xlate.h) */
extern int xl_nr_rule;   /* gl37 (ge/xlate.h): MGS2_NAT_RRULE, the reuse's rule (1 the default; 0 gl36's) */
static int nat_reuse_all, nat_reuse_ncyc; static uint8_t nat_reuse_cyc[32];
#ifndef __psp__
/* cull study (ge/xconv.h XcHost.cull; attic/cull/cull_v3.md): MGS2_NAT_CULL=<bits> for the whole run, MGS2_NAT_CULL_CYCLE=<bits>,...
   by the same turns: bit 1 the effect strips (25f0 / 2870) that would draw nothing left out before their conversion.  MGS2_NAT_CULLCHK=1:
   the check mode (such a call converted anyway and compared).  Taken at each picture's start (rt_nat_cull_at).  The twin's; the PSP's is
   psp/xl_psp.c rt_nat_cull_at (the same keys and turns), so that the PSP's boot_rt.o is gl36's.  Its own reader, as the PSP's, not
   nat_setup: that at the first picture would set the native switches up early and start the twin walk's counts there */
static int nat_cull_set, nat_cull_all, nat_cull_ncyc, nat_cull_chk; static long nat_cull_from, nat_cull_turn = 1; static uint8_t nat_cull_cyc[32];
#endif
static uint32_t nat_seq_seen;
#ifndef __psp__
static int nat_cap_dev;          /* dev (x86): MGS2_NAT_CAP=<n>: the array's entries (the fallback's test) */
#define NAT_CAP (nat_cap_dev ? (uint32_t)nat_cap_dev : (uint32_t)NAT_MAXGRP)
#else
#define NAT_CAP ((uint32_t)NAT_MAXGRP)
#endif
/* the packet buffer index of the write pointer wp, in arithmetic only (rt_dg_call's control flow stays as it was, and its profile) */
#define NAT_WPB(wp) (1u ^ (((((uint32_t)(wp)) & (GUEST_SIZE - 1)) - NAT_PKT0) >> 21 & 1u))

static inline uint32_t nat_r32(uint32_t a) { if ((a >> 28) == 7) return *(const uint32_t *)(g_spr + (a & 0x3ffc)); return *(const uint32_t *)(g_mem + (a & (GUEST_SIZE - 1) & ~3u)); }
static inline uint32_t nat_r16(uint32_t a) { if ((a >> 28) == 7) return *(const uint16_t *)(g_spr + (a & 0x3ffe)); return *(const uint16_t *)(g_mem + (a & (GUEST_SIZE - 1) & ~1u)); }
static inline uint32_t nat_r8(uint32_t a) { if ((a >> 28) == 7) return g_spr[a & 0x3fff]; return g_mem[a & (GUEST_SIZE - 1)]; }
static inline uint32_t nat_spr32(uint32_t o) { return *(const uint32_t *)(g_spr + o); }

static void nat_setup(void)
{
  const char *e = getenv("MGS2_NAT"), *c = getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0;
  nat_set = 1; nat_all = e ? atoi(e) : 0;
  if ((e = getenv("MGS2_NAT_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
    char *p = (char *)e;
    while (nat_ncyc < 32 && *p) { nat_cyc[nat_ncyc++] = (uint8_t)strtoul(p, &p, 10); if (*p != ',') break; p++; }
    if (nat_ncyc) { nat_from = f; nat_turn = n; } }
  g_nat_cfg = nat_all != 0; for (int i = 0; i < nat_ncyc; i++) if (nat_cyc[i]) g_nat_cfg = 1;
  nat_inpl_all = (e = getenv("MGS2_NAT_INPL")) ? atoi(e) & 1 : 0;
  if ((e = getenv("MGS2_NAT_INPL_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
    char *p = (char *)e;
    while (nat_inpl_ncyc < 32 && *p) { nat_inpl_cyc[nat_inpl_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 1); if (*p != ',') break; p++; }
    if (nat_inpl_ncyc && !nat_ncyc) { nat_from = f; nat_turn = n; } }
  if (g_nat_cfg) fprintf(stderr, "nat: gl33 static streams in place: MGS2_NAT_INPL %d, MGS2_NAT_INPL_CYCLE %s (1: a static group's streams stay in "
                         "RAM for the ME's walk; by the same turns)\n", nat_inpl_all, nat_inpl_ncyc ? getenv("MGS2_NAT_INPL_CYCLE") : "none");
  nat_fast_all = (e = getenv("MGS2_NAT_FAST")) ? atoi(e) & 1 : 0;
  if ((e = getenv("MGS2_NAT_FAST_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
    char *p = (char *)e;
    while (nat_fast_ncyc < 32 && *p) { nat_fast_cyc[nat_fast_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 1); if (*p != ',') break; p++; }
    if (nat_fast_ncyc && !nat_ncyc && !nat_inpl_ncyc) { nat_from = f; nat_turn = n; } }
  nat_keep_all = (e = getenv("MGS2_NAT_KEEP")) ? atoi(e) & 15 : 0;
  if ((e = getenv("MGS2_NAT_KEEP_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
    char *p = (char *)e;
    while (nat_keep_ncyc < 32 && *p) { nat_keep_cyc[nat_keep_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 15); if (*p != ',') break; p++; }
    if (nat_keep_ncyc && !nat_ncyc && !nat_inpl_ncyc && !nat_fast_ncyc) { nat_from = f; nat_turn = n; } }
  xl_keep_chk = (e = getenv("MGS2_NAT_KCHK")) ? atoi(e) & 1 : 0;
  nat_reuse_all = (e = getenv("MGS2_NAT_REUSE")) ? atoi(e) & 1 : 0;   /* gl36 */
  if ((e = getenv("MGS2_NAT_REUSE_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
    char *p = (char *)e;
    while (nat_reuse_ncyc < 32 && *p) { nat_reuse_cyc[nat_reuse_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 1); if (*p != ',') break; p++; }
    if (nat_reuse_ncyc && !nat_ncyc && !nat_inpl_ncyc && !nat_fast_ncyc && !nat_keep_ncyc) { nat_from = f; nat_turn = n; } }
  g_nat_rchk = (e = getenv("MGS2_NAT_RCHK")) ? atoi(e) & 1 : 0;
  g_nat_rcfg = nat_reuse_all; for (int i = 0; i < nat_reuse_ncyc; i++) if (nat_reuse_cyc[i]) g_nat_rcfg = 1;
  xl_nr_rule = (e = getenv("MGS2_NAT_RRULE")) ? atoi(e) & 1 : 1;   /* gl37 */
  if (g_nat_cfg && g_nat_rcfg) fprintf(stderr, "nat: gl37 reuse rule: MGS2_NAT_RRULE %d (1: a run that does not match its kept entries is recorded only "
                         "while each of the last two pictures had half of its tried groups replayed or the same as in the picture before; else its shadow "
                         "is kept; 0: gl36, every such run recorded)\n", xl_nr_rule);
  if (g_nat_cfg) fprintf(stderr, "nat: gl36 reuse of a run's translation from the last picture: MGS2_NAT_REUSE %d, MGS2_NAT_REUSE_CYCLE %s (1: a fast run "
                         "that matches its kept entries is replayed; by the same turns), MGS2_NAT_RCHK %d (1: such a run fed in full and compared)\n",
                         nat_reuse_all, nat_reuse_ncyc ? getenv("MGS2_NAT_REUSE_CYCLE") : "none", g_nat_rchk);
  if (g_nat_cfg) fprintf(stderr, "nat: gl35 keeps for fast native groups: MGS2_NAT_KEEP %d, MGS2_NAT_KEEP_CYCLE %s (bits: 1 drawn areas, 2 A+D "
                         "packet, 4 texture of the groups' look-ups, 8 texture of every look-up; by the same turns), MGS2_NAT_KCHK %d\n",
                         nat_keep_all, nat_keep_ncyc ? getenv("MGS2_NAT_KEEP_CYCLE") : "none", xl_keep_chk);
  if (g_nat_cfg) fprintf(stderr, "nat: gl34 fast native front end: MGS2_NAT_FAST %d, MGS2_NAT_FAST_CYCLE %s (1: arm 3's groups go to the "
                         "translator's fast front end; by the same turns)\n", nat_fast_all, nat_fast_ncyc ? getenv("MGS2_NAT_FAST_CYCLE") : "none");
#ifndef __psp__
  if ((e = getenv("MGS2_NAT_CAP"))) { nat_cap_dev = atoi(e); if (nat_cap_dev < 0 || nat_cap_dev > NAT_MAXGRP) nat_cap_dev = 0; }
  if (g_nat_cfg && g_nat_rcfg) {   /* gl36: the reuse's table (the PSP's: psp/xl_psp.c nat_psp_setup) */
    int cap = (e = getenv("MGS2_NAT_RCAP")) ? atoi(e) : NAT_MAXGRP; if (cap < 0 || cap > NAT_MAXGRP) cap = NAT_MAXGRP;
    struct NrEnt *t = cap ? calloc((size_t)cap * 64u + 2048u, 1) : 0;   /* gl36 fix B: + ge/xlate.h XL_NR_LW_N uint16_t */
    xl_nr_table(t, t ? cap : 0, g_nat_rchk);   /* (ge/xlate.h NrEnt: 64 B) */
    fprintf(stderr, "nat: gl36 reuse table: %d entries of 64 B (%u KB)%s\n", t ? cap : 0, (unsigned)((size_t)(t ? cap : 0) * 64u >> 10),
            g_nat_rchk ? ", check mode" : ""); }
  if (g_nat_cfg && !((e = getenv("MGS2_NAT_NOMEM")) && atoi(e)))   /* (dev: MGS2_NAT_NOMEM=1: no arrays, the PSP's case when its block is refused) */
  { g_nat[0].g = calloc(NAT_MAXGRP, sizeof(NatGrp));   /* (dev: MGS2_NAT_ONE=1: one array for both buffers, the PSP's case with one block) */
    g_nat[1].g = (e = getenv("MGS2_NAT_ONE")) && atoi(e) ? g_nat[0].g : calloc(NAT_MAXGRP, sizeof(NatGrp)); }
#endif
  if (g_nat_cfg) fprintf(stderr, "nat: MGS2_NAT %d, MGS2_NAT_CYCLE %s (turns of %ld vblanks from vb %ld; 0 the builder, 3 native, 4 stubbed and not drawn); "
                         "%d groups a picture, %u B each\n", nat_all, nat_ncyc ? getenv("MGS2_NAT_CYCLE") : "none", nat_turn, nat_from + 1, (int)NAT_CAP,
                         (unsigned)sizeof(NatGrp));
}
/* the arm at vblank vb, and (host: the turn lines) the turn's index, -1 before the cycle */
int rt_nat_arm_at(long vb, long *turn)
{
  if (nat_set < 0) nat_setup();
  if (nat_ncyc && vb > nat_from) { long t = (vb - nat_from - 1) / nat_turn; if (turn) *turn = t; return nat_cyc[t % nat_ncyc]; }
  if (turn) *turn = -1;
  return nat_all;
}
int rt_nat_inpl_at(long vb)   /* gl33: the in-place switch at vblank vb */
{
  if (nat_set < 0) nat_setup();
  if (nat_inpl_ncyc && vb > nat_from) return nat_inpl_cyc[((vb - nat_from - 1) / nat_turn) % nat_inpl_ncyc];
  return nat_inpl_all;
}
int rt_nat_keep_at(long vb)   /* gl35: the keep bits at vblank vb */
{
  if (nat_set < 0) nat_setup();
  if (nat_keep_ncyc && vb > nat_from) return nat_keep_cyc[((vb - nat_from - 1) / nat_turn) % nat_keep_ncyc];
  return nat_keep_all;
}
int rt_nat_reuse_at(long vb)   /* gl36: the reuse's switch at vblank vb */
{
  if (nat_set < 0) nat_setup();
  if (nat_reuse_ncyc && vb > nat_from) return nat_reuse_cyc[((vb - nat_from - 1) / nat_turn) % nat_reuse_ncyc];
  return nat_reuse_all;
}
#ifndef __psp__
int rt_nat_cull_at(long vb)   /* cull study: XcHost.cull at vblank vb (its bits, and 0x80 in the check mode) */
{
  if (!nat_cull_set) {
    const char *e, *c = getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0; nat_cull_set = 1;
    nat_cull_all = (e = getenv("MGS2_NAT_CULL")) ? atoi(e) & 1 : 0;
    if ((e = getenv("MGS2_NAT_CULL_CYCLE")) && c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0) {
      char *p = (char *)e;
      while (nat_cull_ncyc < 32 && *p) { nat_cull_cyc[nat_cull_ncyc++] = (uint8_t)(strtoul(p, &p, 10) & 1); if (*p != ',') break; p++; }
      if (nat_cull_ncyc) { nat_cull_from = f; nat_cull_turn = n; } }
    nat_cull_chk = (e = getenv("MGS2_NAT_CULLCHK")) ? atoi(e) & 1 : 0;
    if (nat_cull_all || nat_cull_ncyc || nat_cull_chk) fprintf(stderr, "nat: cull study v3: MGS2_NAT_CULL %d, MGS2_NAT_CULL_CYCLE %s (bit 1: the effect strips "
                           "that would draw nothing left out before their conversion; by the same turns), MGS2_NAT_CULLCHK %d\n", nat_cull_all,
                           nat_cull_ncyc ? getenv("MGS2_NAT_CULL_CYCLE") : "none", nat_cull_chk);
  }
  int k = nat_cull_ncyc && vb > nat_cull_from ? nat_cull_cyc[((vb - nat_cull_from - 1) / nat_cull_turn) % nat_cull_ncyc] : nat_cull_all;
  return k ? k | (nat_cull_chk ? 0x80 : 0) : 0;
}
#endif
int rt_nat_fast_at(long vb)   /* gl34: the fast front end's switch at vblank vb */
{
  if (nat_set < 0) nat_setup();
  if (nat_fast_ncyc && vb > nat_from) return nat_fast_cyc[((vb - nat_from - 1) / nat_turn) % nat_fast_ncyc];
  return nat_fast_all;
}
/* gl38, the bound turns (ge/bound.h): MGS2_BOUND_CYCLE=<digits>, one kind (0..7) a turn of MGS2_OPT_CYCLE's <n> vblanks from its
   <from>; past the list kind 0.  rt_bound_at(vb): the mask (BD_*) of the frame that begins at hook vb, i.e. of the picture the pipeline
   call of vblank vb - 1 makes: turn (vb - from) / n, so the frames that begin at hooks A .. A + n - 1 are the turn's that the PSP's
   turn line "vb A-(A + n)" times.  Its own reader (as rt_nat_cull_at): the PSP's host reads it before any pipeline call.
   gl38, F1 (attic/gl37/fix_design.md option A; ge/xlate.h xl_f1_*): MGS2_NAT_F1=1 for the whole run, MGS2_NAT_F1_CYCLE=<digits> one
   digit a turn by the bound turns' rule (past the list MGS2_NAT_F1's): 1: a fast native run that is not replayed skips the VU writes of
   the groups a later group of the run overwrites.  MGS2_NAT_F1CHK=1: the check mode (the plan made, not applied, compared with the
   run's own notes).  Taken at each pipeline call for its picture (NatBuf.ru bit 1) */
#include "../ge/bound.h"
#define BD_CYC_N 128
extern int xl_f1_chk;   /* (ge/xlate.h) */
static int bd_set, bd_ncyc, f1_all, f1_ncyc, bd_rain_all; static long bd_from, bd_turn = 1; static uint16_t bd_cyc[BD_CYC_N]; static uint8_t f1_cyc[BD_CYC_N];   /* (step 2: 16 bits, BD_NATIVE) */
/* gl39 (attic/gl39/gl39_plan.md): MGS2_DIG_CYCLE=<digits 0/1>, a digit a turn by the bound turns' rule: 1 = no frame digest for the frames
   whose LAST vblank lies in the turn (rt_dig_off_at; MGS2_GL_DIG's range is tested with that vblank too; past the list 0).
   MGS2_LOAD_CYCLE=<digits 0..3>, a digit a turn: the ME's load loop in a kind 8 turn (rt_load_at; PSP only, psp/xl_psp.c: 1 arithmetic,
   2 cached reads of the program's code (read-only), 3 uncached writes to its own 256-byte block).  Neither changes the game's state */
static int dg_ncyc, ld_ncyc; static uint8_t dg_cyc[BD_CYC_N], ld_cyc[BD_CYC_N]; int g_dig_cfg, g_load_cfg;
/* gl40 (attic/gl40/gl40_plan.md 1d, 6): MGS2_XP_CYCLE=<digits 0..7>, a digit a turn by the bound turns' rule: the copy plan's lever XP
   (ge/xlme.h c->xp: 1 the merge sort's ping-pong, 2 the look-back join in xc_add, 4 the check against gl39's plan); 0 gl39's plan, also
   before the cycle and past the list.  MGS2_XP_CHECK=1: every plan of the run with 7 (both parts and the check), the cycle ignored (the
   twin's and the emulator's whole-route proof); MGS2_XP_ALL=<digit>: that digit in every plan (the twin's whole-route arms).  Neither changes the game's state: the plan decides what the picture's copy holds */
static int xp_ncyc, xp_all; static uint8_t xp_cyc[BD_CYC_N]; int g_xp_cfg;
int g_bd_cfg, g_nat_f1cfg, g_nat_f1chk, g_bd_rain;   /* 1: a kind other than 0 is named; F1 is named on; its check mode */
static void bd_setup(void)
{
  static const uint16_t mask[BD_KINDS] = BD_MASKS;
  const char *e, *c = getenv("MGS2_OPT_CYCLE"); long f = -1, n = 0; int ok = c && sscanf(c, "%ld:%ld", &f, &n) == 2 && n > 0 && f >= 0;
  bd_set = 1;
  if (ok) { bd_from = f; bd_turn = n; }
  if ((e = getenv("MGS2_BOUND_CYCLE")) && ok)
    for (; bd_ncyc < BD_CYC_N && *e >= '0' && *e < '0' + BD_KINDS; e++) { bd_cyc[bd_ncyc] = mask[*e - '0']; if (bd_cyc[bd_ncyc++]) g_bd_cfg = 1; }
  if (!g_bd_cfg) bd_ncyc = 0;
  bd_rain_all = (e = getenv("MGS2_GL_RAIN_CUT")) && atoi(e) ? BD_RAIN : 0; g_bd_rain = bd_rain_all != 0;   /* the rain's cut for the whole run, its own switch */
  if (bd_rain_all) fprintf(stderr, "bound: MGS2_GL_RAIN_CUT 1: in every picture the rain owners' effect calls are left out but the drop sheets' near "
                           "calls (MGS2_GL_RAIN_NEAR; picture only)\n");
  f1_all = (e = getenv("MGS2_NAT_F1")) ? atoi(e) & 1 : 0; g_nat_f1cfg = f1_all;
  if ((e = getenv("MGS2_NAT_F1_CYCLE")) && ok)
    for (; f1_ncyc < BD_CYC_N && (*e == '0' || *e == '1'); e++) { f1_cyc[f1_ncyc] = (uint8_t)(*e - '0'); if (f1_cyc[f1_ncyc++]) g_nat_f1cfg = 1; }
  g_nat_f1chk = (e = getenv("MGS2_NAT_F1CHK")) ? atoi(e) & 1 : 0; xl_f1_chk = g_nat_f1chk;
  if ((e = getenv("MGS2_DIG_CYCLE")) && ok)
    for (; dg_ncyc < BD_CYC_N && (*e == '0' || *e == '1'); e++) { dg_cyc[dg_ncyc] = (uint8_t)(*e - '0'); if (dg_cyc[dg_ncyc++]) g_dig_cfg = 1; }
  if ((e = getenv("MGS2_LOAD_CYCLE")) && ok)
    for (; ld_ncyc < BD_CYC_N && *e >= '0' && *e <= '3'; e++) { ld_cyc[ld_ncyc] = (uint8_t)(*e - '0'); if (ld_cyc[ld_ncyc++]) g_load_cfg = 1; }
  if ((e = getenv("MGS2_XP_CYCLE")) && ok)
    for (; xp_ncyc < BD_CYC_N && *e >= '0' && *e <= '7'; e++) { xp_cyc[xp_ncyc] = (uint8_t)(*e - '0'); if (xp_cyc[xp_ncyc++]) g_xp_cfg = 1; }
  xp_all = (e = getenv("MGS2_XP_CHECK")) && atoi(e) ? 7 : 0;
  if (!xp_all && (e = getenv("MGS2_XP_ALL")) && *e >= '0' && *e <= '7') xp_all = 0x100 | (*e - '0');   /* (0x100: set, even to 0) */
  if (xp_all) g_xp_cfg = 1;
  if (g_xp_cfg) fprintf(stderr, "bound: gl40 MGS2_XP_CYCLE %s, MGS2_XP_CHECK %d (the copy plan's lever XP a turn: 1 the merge sort's ping-pong, "
                        "2 the look-back join of %d, 4 the check against gl39's plan; 0 gl39's plan); MGS2_XP_ALL %s\n", xp_ncyc ? getenv("MGS2_XP_CYCLE") : "none",
                        xp_all == 7, 8, xp_all & 0x100 ? getenv("MGS2_XP_ALL") : "not set");
  if (g_dig_cfg) fprintf(stderr, "bound: gl39 MGS2_DIG_CYCLE %s (1: no frame digest for the frames whose last vblank is in the turn)\n", getenv("MGS2_DIG_CYCLE"));
  if (g_load_cfg) fprintf(stderr, "bound: gl39 MGS2_LOAD_CYCLE %s (the ME's load loop in a kind 8 turn: 1 arithmetic, 2 cached reads, 3 uncached writes; PSP only)\n", getenv("MGS2_LOAD_CYCLE"));
  if (g_bd_cfg) fprintf(stderr, "bound: gl38 bound turns: MGS2_BOUND_CYCLE %s (%d turns of %ld vblanks from vb %ld; kinds: 0 all drawn, 1 the "
                        "ship out, 2 the people (chain 2), 3 chain 6 but the rain, 4 the rain but the near drop sheets, 5 the stage's program "
                        "(chain 10), 6 the ship, the people and chain 6 whole, 7 nothing drawn, the ME resting, 8 nothing drawn in an ME turn; past the list 0)\n",
                        getenv("MGS2_BOUND_CYCLE"), bd_ncyc, bd_turn, bd_from);
  if (g_nat_f1cfg || g_nat_f1chk) fprintf(stderr, "nat: gl38 F1 (the VU writes of a run's groups that a later group overwrites left out, for runs not "
                        "replayed): MGS2_NAT_F1 %d, MGS2_NAT_F1_CYCLE %s (by the bound turns' rule), MGS2_NAT_F1CHK %d\n", f1_all,
                        f1_ncyc ? getenv("MGS2_NAT_F1_CYCLE") : "none", g_nat_f1chk);
}
int rt_bound_at(long vb, long *turn)   /* the mask of the frame that begins at hook vb; the turn's index (-1 before the cycle) */
{
  if (!bd_set) bd_setup();
  if (turn) *turn = vb >= bd_from && bd_turn > 0 ? (vb - bd_from) / bd_turn : -1;
  if (bd_ncyc && vb >= bd_from) { long t = (vb - bd_from) / bd_turn; return (t < bd_ncyc ? bd_cyc[t] : 0) | bd_rain_all; }
  return bd_rain_all;
}
int rt_dig_off_at(long fvb)   /* gl39: 1 = no frame digest for the frame whose last vblank is fvb */
{
  if (!bd_set) bd_setup();
  if (dg_ncyc && fvb >= bd_from) { long t = (fvb - bd_from) / bd_turn; return t < dg_ncyc ? dg_cyc[t] : 0; }
  return 0;
}
int rt_xp_at(long vb)   /* gl40: the copy plan's XP bits for the turn of hook vb (MGS2_XP_CHECK: 7 everywhere) */
{
  if (!bd_set) bd_setup();
  if (xp_all) return xp_all & 7;
  if (xp_ncyc && vb >= bd_from) { long t = (vb - bd_from) / bd_turn; return t < xp_ncyc ? xp_cyc[t] : 0; }
  return 0;
}
int rt_load_at(long vb)   /* gl39: the ME's load flavour (0 none) for the turn of hook vb */
{
  if (!bd_set) bd_setup();
  if (ld_ncyc && vb >= bd_from) { long t = (vb - bd_from) / bd_turn; return t < ld_ncyc ? ld_cyc[t] : 0; }
  return 0;
}
int rt_nat_f1_at(long vb)   /* F1 for the frame that begins at hook vb (1 | 2 in the check mode: the plan made and compared only) */
{
  int k;
  if (!bd_set) bd_setup();
  k = f1_all;
  if (f1_ncyc && vb >= bd_from) { long t = (vb - bd_from) / bd_turn; if (t < f1_ncyc) k = f1_cyc[t]; }
  return g_nat_f1chk ? 1 : k;
}
int g_nat_f1;
#include "nge_stub.h"                             /* native step 1 (MGS2_NGE): the cooked ship meshes */
#include "np6_stub.h"                             /* native step 2: the people's records (MGS2_NP6; MGS2_NGE_CYCLE's digit 2) */
#ifndef __psp__
void nat_x86_rec(int b, uint32_t g0, int ng, uint32_t mesh, uint32_t dest, uint32_t end, uint32_t qwc);   /* dev: boot/nat_x86.c's counts */
#endif

/* the builder's call (R[4] the mesh): 1 = stubbed.  The builder's loop in C (gen_elf.h P1214a0, 0x121550 .. 0x121a58; step 0 checked
   every word of it against the packets: native_step0_report.md) */
int rt_nat_p04(void)
{
  uint32_t mesh, desc, qwc, cap, dest, end, d, blk, fog, clip, pre, cur[3], key = 0, vq = 0, kq; int32_t ng; int b, arm, i, r30, stub; NatBuf *s; NatGrp *g;
  static NatGrp tmp;
  if (nat_seq_seen != g_dg_seq || g_nat_arm < 0) { nat_seq_seen = g_dg_seq; g_nat_arm = rt_nat_arm_at(k_vblanks, 0); g_nat_inpl = rt_nat_inpl_at(k_vblanks); g_nat_fast = rt_nat_fast_at(k_vblanks); g_nat_keep = rt_nat_keep_at(k_vblanks); g_nat_reuse = rt_nat_reuse_at(k_vblanks);
    g_nat_f1 = rt_nat_f1_at(k_vblanks + 1);                                           /* gl38 */
    if (g_nat_arm == 3 && (rt_bound_at(k_vblanks + 1, 0) & BD_SHIP)) g_nat_arm = 4;   /* gl38 K1, K6: the picture's ship stubbed and not drawn */
    if (g_nge) nge_pic_begin(); }                                                     /* native step 1 (nge_stub.h) */
  arm = g_nat_arm;
  if (arm <= 0) return 0;
  if (arm == 4) { g_nat_c[NC_STUBC]++; return 1; }
#ifdef __psp__
  if (arm != 3) return 0;
#endif
  stub = arm == 3;
  mesh = (uint32_t)R[4];
  dest = nat_spr32(0xc70); d = dest & (GUEST_SIZE - 1);
  if (d < NAT_PKT0 || d >= NAT_PKT1) { g_nat_c[NC_FB_OTHER]++; return 0; }
  b = NAT_BUF(d);
  if (g_nat_bseq[b] != g_dg_seq) { g_nat_c[NC_FB_OTHER]++; return 0; }
  s = &g_nat[b];
  if (s->seq != g_dg_seq) {   /* this pipeline call's first: the array starts again.  When the host could give one array only (g the
                                 same for both buffers) the other buffer's groups go with it (on the routes every transfer has taken
                                 its groups before the next pipeline call begins: the twin's "after a later pipeline call" count) */
    NatBuf *o = &g_nat[b ^ 1];
    if (s->draw && s->cur < s->n) { g_nat_c[NC_LEFT] += (long)(s->n - s->cur); g_nat_c[NC_LEFT_PICS]++; }   /* (the host's cursor: a count only) */
    if (o->g == s->g && o->draw && o->cur < o->n) { g_nat_c[NC_LEFT] += (long)(o->n - o->cur); g_nat_c[NC_LEFT_PICS]++; }
    if (g_nat_sync) g_nat_sync(b);
    if (o->g == s->g) { o->n = o->cur = 0; o->draw = 0; }
    s->seq = g_dg_seq; s->n = s->cur = s->vq = s->last_key = 0; s->draw = arm >= 2; g_nat_c[NC_PICS]++;
    s->ru = (uint32_t)(stub && g_nat_fast && g_nat_reuse) | (stub && g_nat_fast && g_nat_f1 ? 2u : 0u); }   /* gl36; gl38: bit 1 F1 */
  ng = (int32_t)(int16_t)nat_r16(mesh + 0xcc);
  if (ng <= 0) { g_nat_c[NC_EMPTY]++; return stub; }
  if (ng > 1024 || s->n + (uint32_t)ng > NAT_CAP) { g_nat_c[ng > 1024 ? NC_FB_OTHER : NC_FB_FULL]++; return 0; }
  pre = nat_r32(mesh + 0xc0) & 1; clip = (uint32_t)(int16_t)nat_r16(mesh + 0xc8) & 1; desc = nat_r32(mesh + 0xf4);
  cur[0] = nat_r32(mesh + 0xdc); cur[1] = nat_r32(mesh + 0xe0); cur[2] = nat_r32(mesh + 0xe4);
  qwc = nat_spr32(0xc64); cap = nat_spr32(0xc68); end = nat_spr32(0xc74); blk = nat_spr32(0xc54); fog = nat_spr32(0xb90);
  if (stub) {
    key = dest + 16 * qwc; kq = key & (GUEST_SIZE - 1) & ~15u;
    if (kq < s->last_key) { g_nat_c[NC_FB_ORDER]++; return 0; }
    if ((int32_t)end - (int32_t)(dest + 16u * (qwc + s->vq + 8u * (uint32_t)ng)) < NAT_END_MARGIN) { g_nat_c[NC_FB_END]++; return 0; }
  }
  for (i = 0, r30 = 2; i < ng; i++, r30--) {
    uint32_t dd = desc + 32u * (uint32_t)i, d0 = nat_r32(dd), idx, m = r30 > 0 ? 8 : 7; int k;
    g = s->g ? &s->g[s->n + (uint32_t)i] : &tmp;
    for (k = 0; k < 3; k++) {
      uint32_t n = nat_r8(dd + 14 + (uint32_t)k);
      if (cur[k] && n) { if (n > (k == 2 ? 64u : 128u)) { g_nat_c[NC_FB_OTHER]++; return 0; }   /* (nothing committed yet: n stays) */
        g->s[k] = cur[k]; g->n[k] = (uint8_t)n; cur[k] += 16 * n; }
      else { g->s[k] = 0; g->n[k] = 0; }
    }
    g->hdr[0] = nat_r8(dd + 12); g->hdr[1] = 0; g->hdr[2] = fog; g->hdr[3] = clip; g->mat = nat_r32(dd + 20); g->blk = blk;
    idx = 2 * (d0 & 1) + clip;
    if (pre) { idx += 4; if (d0 & 4) idx += 2; else if ((d0 & 2) && clip) idx += 2; }
    g->mscal = (uint32_t)(int32_t)(int16_t)nat_r16(0x70000b9cu + 2 * idx) | 0x14000000u;
    g->flags = r30 > 0 ? NATG_FULL : 0;
    if (stub && g_nat_inpl && g->mscal == NAT_DF0_MSCAL && g->s[0] && g->s[1] && g->s[2]) {   /* gl33: a static group (ge/natsnap.h NATG_INPL) */
      for (k = 0; k < 3; k++) { uint32_t a = g->s[k] & (GUEST_SIZE - 1); if ((g->s[k] >> 28) || (a >= NAT_PKT0 && a < NAT_PKT1)) break; }
      if (k == 3) { g->flags |= NATG_INPL; g_nat_stc++; }
    }
    if (stub && g_nat_fast) { g->flags |= NATG_FAST; g_nat_fst++;   /* gl34 */
      if (g_nat_keep) { g->flags |= (uint8_t)(g_nat_keep << 4); g_nat_kpt++; } }   /* gl35 */
    if (stub) g->key = key;
#ifndef __psp__
    else {   /* shadow: the group's own packet address, as the builder places it */
      if ((int32_t)end < (int32_t)(dest + qwc)) { g->flags |= 0x80; g->key = dest + 16 * qwc; continue; }   /* the builder skips it (never seen): marked, left out below */
      if ((int32_t)cap < (int32_t)(qwc + m)) { dest += 16 * qwc; qwc = 0; }
      g->key = dest + 16 * qwc; g->flags |= NATG_PKT; qwc += m; }
#endif
    vq += m;
  }
#ifndef __psp__
  if (!stub && s->g) {   /* shadow: skipped groups out (none on the routes) */
    int o = 0; for (i = 0; i < ng; i++) if (!(s->g[s->n + (uint32_t)i].flags & 0x80)) s->g[s->n + (uint32_t)o++] = s->g[s->n + (uint32_t)i];
    ng = o; }
  nat_x86_rec(b, s->n, ng, mesh, nat_spr32(0xc70), end, nat_spr32(0xc64));
#endif
  if (g_nge && s->g) {   /* native step 1 (nge_stub.h): the groups' references (0: today's path) */
    if (stub && g_nge_pic) nge_rec(b, s, s->n, ng, mesh, blk); else for (i = 0; i < ng; i++) xc_nge_ref[b][s->n + (uint32_t)i].g = 0; }
  if (!s->g) g_nat_c[NC_NOMEM]++;
  s->n += (uint32_t)ng; s->vq += vq; if (stub) s->last_key = key & (GUEST_SIZE - 1) & ~15u;
  g_nat_c[NC_REC]++; g_nat_c[NC_GRP] += ng; if ((long)s->n > g_nat_c[NC_GRP_MAX]) g_nat_c[NC_GRP_MAX] = (long)s->n;
  return stub;
}
void P1214a0_orig(void);
void P1214a0(void) __attribute__((section(".text.nat_P1214a0")));
void P1214a0(void)
{
  if (__builtin_expect(nat_set < 0, 0)) nat_setup();
  if (g_nat_cfg && g_zt && !g_dg_lite && rt_nat_p04()) return;   /* (no arm named: the builder, as before the wrapper) */
  P1214a0_orig();
}
