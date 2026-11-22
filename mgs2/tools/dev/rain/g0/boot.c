/* x86 boot probe for the recompiled MGS2 ELF: loads SLES_503.83 into the guest memory, runs the entry with the kernel HLE (runtime/kernel.h)
   and logs every kernel call and the EE / GS register ranges the game touches.  Stops at the first kernel call not handled yet.
   Build (from a directory holding gen_elf.h made by recomp/survey.py and the IOP translations, see runtime/iop_host.h); RT is
   third_party/ps2rc/ps2rc/iop/rt:
     gcc -O1 -w -I../runtime -I$RT -fwrapv -fno-strict-aliasing -DGEN='"gen_elf.h"' boot.c jsifman.c cdbios.c iop_main.c
         $RT/iop_rt.c $RT/iop_rt_libs.c $RT/iop_rt_ucontext.c -o boot -lm
   Split build (the generated code compiled once, the runtime alone after a runtime change), from /home/claude/mgs2/boot:
     gcc -O1 -c -w -I../runtime -fwrapv -fno-strict-aliasing -DGEN='"../gen_elf.h"' -DSTAGES='"../stages.h"' gen.c -o gen.o
     gcc -O1 -w -I../runtime -I$RT -fwrapv -fno-strict-aliasing -DGUEST_SPLIT_RT boot.c gen.o <IOP objects> -o boot -lm
   Run: MGS2_DISC=<dir with MODULES\, MGS2\ ...> ./boot SLES_503.83 [max vblanks] */
#if !defined(__psp__)
#include <xmmintrin.h>                  /* MGS2_FPU_RZ */
#endif
#include <unistd.h>
#define IRQ_POLL() k_poll()
#ifdef K_COUNTDOWN   /* ee_io.h: the slow path first checks that the clamped countdown met k_poll_at itself */
#define IRQ_POLLF(flush) ((k_left -= K_POLL_CYCLES) <= 0 ? ((void)(flush), K_NOW() >= k_poll_at ? k_poll_rest() : K_SYNC()) : (void)0)
#else
#define IRQ_POLLF(flush) ((k_time += K_POLL_CYCLES) >= k_poll_at ? ((void)(flush), k_poll_rest()) : (void)0)   /* tools/poll_fast.py (gen_pre.h) */
#endif
static void k_poll(void);
#if defined(DG_EXP) && !defined(__psp__)
extern int g_dgx_in; extern long g_dgx_cnt[8];
#define DGX_NOTE(k) (g_dgx_in ? (void)g_dgx_cnt[k]++ : (void)0)
#endif
#include "ee_io.h"
#include "mmi.h"
#include "kernel.h"
#include "sif.h"
#include "iop_host.h"

static long n_sys, max_vblanks = 600, verbose_from = -10;   /* BOOT_VERBOSE=<vblank>: log every kernel call for 3 vblanks from there */
uint32_t g_opt = ~0u;                                             /* runtime/host_time.h OPT_*: all on (OPT_LITE since v21, speed_notes §34) */
uint32_t g_opt2 = OPT2_DEFAULT;                                   /* sb9: runtime/host_time.h OPT2_*: all on (sb11: but C1; sb12: but C2) */
long g_hash_vb = -1; SPRS uint64_t g_hash_ee, g_hash_iop;                /* the last state hash (the PSP shows it on screen) */
#ifdef __psp__
void host_vb_begin(long vb);    /* psp/main_psp.c: a guest vblank started: the host time since the last one is one game step */
void host_vb_end(void);         /* the hash / dump work of the hook is not the game's: the next step is timed from here */
#elif defined(MGS2_CALLGRIND)    /* dev: valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no; MGS2_PROF_VB=<a>:<b> profiles vblanks a..b-1, the hook's own work left out */
#include <valgrind/callgrind.h>
static long prof_a = -2, prof_b, prof_vb; static int prof_on;
static void host_vb_begin(long vb)
{ if (prof_a == -2) { const char *e = getenv("MGS2_PROF_VB"); prof_a = -1; if (e && sscanf(e, "%ld:%ld", &prof_a, &prof_b) != 2) prof_a = -1; }
  prof_vb = vb; if (prof_on) { CALLGRIND_TOGGLE_COLLECT; prof_on = 0; }
  if (vb == prof_a) CALLGRIND_START_INSTRUMENTATION;
  if (vb == prof_b) { CALLGRIND_DUMP_STATS; CALLGRIND_STOP_INSTRUMENTATION; fprintf(stderr, "prof: vblanks %ld..%ld dumped\n", prof_a, prof_b - 1); } }
static void host_vb_end(void) { if (prof_a >= 0 && prof_vb >= prof_a && prof_vb < prof_b) { CALLGRIND_TOGGLE_COLLECT; prof_on = 1; } }
#else
#define host_vb_begin(vb) ((void)0)
#define host_vb_end() ((void)0)
#endif
static void iop_watch(void)               /* dev: MGS2_IOP_WATCH=<hex addr>:<hex bytes>[,...] prints the IOP RAM words of those ranges that changed */
{
  static uint32_t a[8], n[8], *last[8]; static int init, nr; int r; uint32_t i; const char *e = getenv("MGS2_IOP_WATCH"); const uint8_t *m;
  if (!e) return;
  if (!init) { init = 1; while (nr < 8 && sscanf(e, "%x:%x", &a[nr], &n[nr]) == 2) { n[nr] = (n[nr] + 3) / 4; last[nr] = calloc(n[nr], 4); nr++;
                                                                                      e = strchr(e, ','); if (!e) break; e++; } }
  m = iop_rt_mem(g_iop);
  for (r = 0; r < nr; r++) for (i = 0; i < n[r]; i++) { uint32_t v; memcpy(&v, m + ((a[r] + 4 * i) & 0x1ffffc), 4);
    if (v != last[r][i]) { fprintf(stderr, "iopw: vb %ld %06x: %08x -> %08x\n", k_vblanks, a[r] + 4 * i, last[r][i], v); last[r][i] = v; } }
}
static uint64_t fnv64(const uint8_t *p, size_t n, uint64_t h) { size_t i; for (i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; } return h; }
/* dev: MGS2_GS_MASK=<file>: a game-state hash ("gs"), defined at the end of the file (gs_load, gs_hash) */
static int gs_n = -1; static void gs_load(void); static uint64_t gs_hash(void);
static void state_hash(void)             /* MGS2_HASH_EVERY=<n>[,<vb>:<m>]: state hash 2 of EE RAM + scratchpad, and of IOP RAM, every n vblanks (and every m from vb on) */
{
  static long every = -1, from, from2 = -1, every2; uint64_t ee, iop; int hash_dense(void); void hash_ref(uint64_t ee); void state_hash2(uint64_t *ee, uint64_t *iop);   /* dev: MGS2_HASH_FROM=<vb> hashes from that vblank on only; hash_dense / hash_ref / state_hash2: end of this file */
  if (every < 0) { const char *e = getenv("MGS2_HASH_EVERY"), *c; every = e ? atol(e) : 0; from = getenv("MGS2_HASH_FROM") ? atol(getenv("MGS2_HASH_FROM")) : 0;
    if (e && (c = strchr(e, ',')) && (sscanf(c + 1, "%ld:%ld", &from2, &every2) != 2 || every2 <= 0)) from2 = -1; }   /* audit M5: the deck every 100 at the cost of a few hashes */
  if ((!every || k_vblanks % every || k_vblanks < from) && (from2 < 0 || k_vblanks < from2 || k_vblanks % every2) && !hash_dense()) return;
  state_hash2(&ee, &iop);
  fprintf(stderr, "hash: vb %ld ee %016llx iop %016llx", k_vblanks, (unsigned long long)ee, (unsigned long long)iop);
  if (gs_n < 0) gs_load();
  if (gs_n > 0) { extern int gs_dn; fprintf(stderr, " gs %016llx gsd %d", (unsigned long long)gs_hash(), gs_dn); }   /* sbg0: and the ranges added by the native code */
  { extern uint32_t g_vgrp_fb; static int vs = -1; if (vs < 0) vs = getenv("MGS2_VFPU_STAT") != 0; if (vs) fprintf(stderr, " vfb %u", g_vgrp_fb); }   /* dev: VFPU group fallbacks so far */
  fputc('\n', stderr);
  g_hash_vb = k_vblanks; g_hash_ee = ee; g_hash_iop = iop; hash_ref(ee);
  if (getenv("MGS2_HASH_TIME")) fprintf(stderr, "time: vb %ld k_time %llu\n", k_vblanks, (unsigned long long)K_NOW());   /* dev: virtual time (polls, Count reads) */
}
SPRS void (*g_fn_hook)(uint32_t);           /* set when MGS2_FN_TRACE is: only the boot_hook build calls it (gen.c GEN_FN_HOOK) */
static struct rt_cov_e { const char *u; uint32_t a; } *rt_cov_t; static long rt_cov_n;   /* dev: gen_pre.h GEN_COVER, printed at the stop */
static unsigned long rt_mm_n[16], rt_st_n[16]; static long rt_mm_from = -1;   /* dev: MGS2_MM_STATS=<from vblank>: runtime accesses of the generated code by address top nibble */
static uint32_t fn_tr[16]; static int fn_ntr, fn_all; static long fn_from, fn_to = 0x7fffffff;
static void fn_trace(uint32_t a)       /* dev: MGS2_FN_TRACE=<hex addr>[,...] [MGS2_FN_TRACE_VB=<from>:<to>] prints the arguments at entry */
{
  int i; if (k_vblanks < fn_from || k_vblanks > fn_to) return;
  if (fn_all) fprintf(stderr, "fa %06x t %llu\n", a, (unsigned long long)K_NOW());   /* dev: MGS2_FN_ALL: every function entry with the virtual time */
  if (a == 0x14d860 && getenv("MGS2_VU_DBG")) { vu_dbg = (uint32_t)R[5] == strtoul(getenv("MGS2_VU_DBG"), 0, 16);  /* dev: VU0 dumps for one control's floor test */
    if (vu_dbg) { printf("vudbg query vb %ld ra %06x\n", k_vblanks, (uint32_t)R[31]); fflush(stdout); } }
  { static int nx; if (a == 0x14d510 && vu_dbg) { printf("vudbg send\n"); fflush(stdout); nx = 6; }
    else if (nx > 0) { nx--; printf("vudbg next %06x ra %06x v0 %x r8 %x r20 %x flags %x %x\n", a, (uint32_t)R[31], (uint32_t)R[2], (uint32_t)R[8], (uint32_t)R[20], *(uint32_t *)mm_map(0x11004320), *(uint32_t *)(VUMEM + 0x320)); fflush(stdout); } }
  for (i = 0; i < fn_ntr; i++) if (fn_tr[i] == a) {
    fprintf(stderr, "fn: vb %ld %06x ra %06x a0 %08x a1 %08x a2 %08x a3 %08x v0 %08x spr %08x %08x %08x\n", k_vblanks, a, (uint32_t)R[31], (uint32_t)R[4],
            (uint32_t)R[5], (uint32_t)R[6], (uint32_t)R[7], (uint32_t)R[2], RD32(0x70000000), RD32(0x70000004), RD32(0x70000008)); return; }
}
static void fn_trace_init(void)
{
  const char *e = getenv("MGS2_FN_TRACE"), *v = getenv("MGS2_FN_TRACE_VB");
  while (e && *e && fn_ntr < 16) { char *n; fn_tr[fn_ntr++] = strtoul(e, &n, 16); e = *n ? n + 1 : n; }
  if (v) sscanf(v, "%ld:%ld", &fn_from, &fn_to);
  fn_all = getenv("MGS2_FN_ALL") != 0;
  if (getenv("MGS2_MM_STATS")) rt_mm_from = atol(getenv("MGS2_MM_STATS"));
  if (fn_ntr || fn_all || getenv("MGS2_VU_DBG")) g_fn_hook = fn_trace;
}
static void ee_dump(void)               /* dev: MGS2_EE_DUMP=<prefix>:<from>:<every> writes EE RAM to <prefix>.<vblank> from that vblank on */
{
  static char pre[256]; static long from = -1, every; char fn[300]; FILE *f;
  if (from < 0) { const char *e = getenv("MGS2_EE_DUMP"); from = 0x7fffffff; if (e && sscanf(e, "%255[^:]:%ld:%ld", pre, &from, &every) != 3) from = 0x7fffffff;
    /* game RAM stays in the container: a relative prefix once wrote two dumps into the shared project folder (deleted 2026-10-23, audit L11) */
    if (from != 0x7fffffff && (pre[0] != '/' || !strncmp(pre, "/mnt/", 5))) { fprintf(stderr, "MGS2_EE_DUMP: %s refused, give an absolute path in the container\n", pre); from = 0x7fffffff; } }
  if (k_vblanks < from || (every > 0 && (k_vblanks - from) % every) || (every <= 0 && k_vblanks != from)) return;
  snprintf(fn, sizeof fn, "%s.%ld", pre, k_vblanks); if ((f = fopen(fn, "wb"))) { fwrite(g_mem, 1, GUEST_SIZE, f); fclose(f); }
}
static void ee_poke(void)               /* dev: MGS2_POKE=<vb>:<addr>:<word>[,...] stores a 32-bit word into EE RAM at the start of that vblank
                                           (measurement only, e.g. another stage's name in the script that loads the next stage) */
{
  static int n = -1; static long vb[8]; static unsigned ad[8], wd[8];
  if (n < 0) { const char *e = getenv("MGS2_POKE"); n = 0; while (e && n < 8 && sscanf(e, "%ld:%x:%x", &vb[n], &ad[n], &wd[n]) == 3) { n++; e = strchr(e, ','); if (e) e++; } }
  for (int i = 0; i < n; i++) if (k_vblanks == vb[i]) { uint32_t w = wd[i]; memcpy(g_mem + (ad[i] & (GUEST_SIZE - 1)), &w, 4); fprintf(stderr, "poke: vb %ld %08x = %08x\n", k_vblanks, ad[i], wd[i]); }
}
/* MGS2_OPT=<hex>: g_opt for the whole run; MGS2_OPT_CYCLE=<vb>:<n>[:<hex>,<hex>...]: from that vblank on, the configurations take turns,
   n vblanks each (without a list: all on, then each speedup off alone, then all off), so one run times each speedup against the same scenes.
   sb9: MGS2_OPT2=<hex>: g_opt2 for the whole run; MGS2_OPT2_CYCLE=<hex>,<hex>...: g_opt2 in each of those configurations, in order */
static void opt_window(long vb)
{
  static uint32_t cfg[16] = { ~0u, ~OPT_GS_BLOCK, ~OPT_POLL_DUE, ~OPT_VIF0_FAST, ~OPT_SPR_BULK, ~OPT_NATIVE, 0 }; static int ncfg = 7;
  static uint32_t cfg2[16];   /* sb9: g_opt2 in each configuration (MGS2_OPT2_CYCLE; else the run's g_opt2) */
  static long from = -2, n; const char *e;
  if (from == -2) { from = -1; if ((e = getenv("MGS2_OPT"))) g_opt = (uint32_t)strtoul(e, 0, 16);
    if ((e = getenv("MGS2_OPT2"))) g_opt2 = (uint32_t)strtoul(e, 0, 16);
    for (int i = 0; i < 16; i++) cfg2[i] = g_opt2;
    if ((e = getenv("MGS2_OPT_CYCLE"))) { int k = 0; char *p;
      if (sscanf(e, "%ld:%ld%n", &from, &n, &k) != 2 || n <= 0) from = -1;
      else if (e[k] == ':') for (p = (char *)e + k, ncfg = 0; (*p == ':' || *p == ',') && ncfg < 16; ) cfg[ncfg++] = (uint32_t)strtoul(p + 1, &p, 16);
      if (!ncfg) from = -1; }
    if ((e = getenv("MGS2_OPT2_CYCLE"))) { char *p = (char *)e;
      for (int i = 0; i < 16 && *p; i++) { cfg2[i] = (uint32_t)strtoul(p, &p, 16); if (*p != ',') break; p++; } } }
  if (from >= 0 && vb > from) { int i = (int)(((vb - from - 1) / n) % ncfg); g_opt = cfg[i]; g_opt2 = cfg2[i]; }
  K_POLL_KICK();                                 /* (OPT_POLL_DUE may have changed) */
}
static void k_vblank_hook(int start)
{
  io_vblank(start);
#if !defined(__psp__)
  if (start) { void vb_time(long vb); vb_time(k_vblanks); void pkt_vblank(long vb); pkt_vblank(k_vblanks); }
#ifdef MGS2_XLIVE
  if (start) { void xlive_chk_vblank(long vb); xlive_chk_vblank(k_vblanks); }   /* dev: boot/xlive_chk.c */
#endif
#ifdef GEN_GI_COUNT
  if (start) { void gi_window(long vb); gi_window(k_vblanks); }
#endif
#endif
  if (start) { opt_window(k_vblanks); sif_vblank(); host_vb_begin(k_vblanks); iop_watch(); state_hash(); ee_dump(); ee_poke(); host_vb_end(); }
  if (k_vblanks >= max_vblanks) {
    int t; fprintf(stderr, "stop: %ld vblanks\n", k_vblanks);
    stack_report();
    if (rt_cov_n) { FILE *cf = fopen(getenv("MGS2_COV") ? getenv("MGS2_COV") : "cov.txt", "w"); for (t = 0; t < (1 << 20); t++) if (rt_cov_t[t].u) fprintf(cf, "%s %06x\n", rt_cov_t[t].u, rt_cov_t[t].a); fclose(cf); }
    if (rt_mm_from >= 0) for (t = 0; t < 16; t++) if (rt_mm_n[t] | rt_st_n[t]) fprintf(stderr, "mm stats: %xxxxxxxx loads %lu stores %lu (%.1f per vblank)\n", t, rt_mm_n[t], rt_st_n[t], (double)(rt_mm_n[t] + rt_st_n[t]) / (k_vblanks - rt_mm_from));
    if (getenv("MGS2_VU_UNSUP")) {     /* dev: which words of the VU0 micro memory the interpreter does not support (each run alone on a saved state) */
      static VuSnap sv; int pc; vu_snap(&sv);
      for (pc = 0; pc < 512; pc++) { uint32_t lo = VUPROG[pc * 2], hi = VUPROG[pc * 2 + 1]; int u = g_unsup_vu;
        vu_upper(hi & 0x07ffffff); if (g_unsup_vu != u) fprintf(stderr, "vu0 unsupported: pc %d upper %08x\n", pc, hi);
        u = g_unsup_vu; if (!(hi & 0x80000000u)) vu_lower(lo, pc); if (g_unsup_vu != u) fprintf(stderr, "vu0 unsupported: pc %d lower %08x\n", pc, lo);
        vu_restore(&sv); }
      { FILE *vf = fopen(getenv("MGS2_VU_UNSUP"), "wb"); if (vf) { fwrite(VUPROG, 4, 1024, vf); fwrite(VUMEM, 1, 4096, vf); fclose(vf); } }
    }
#ifdef VU_PROFILE
    { void vu_prof_report(void); vu_prof_report(); }
#endif
#if defined(GEN_DG_TRACE) && !defined(__psp__)
    { void rt_dgt_report(void); rt_dgt_report(); }
#endif
    fprintf(stderr, "vu0: %ld micro calls, %ld steps, unsupported ops %d; vif0 unsupported codes %u\n", vu_calls, vu_steps, g_unsup_vu, vif0.unsup);
#if defined(GUEST_SPLIT_RT) && defined(GEN_MM_FAST) && defined(GUEST_SPR0)
    { extern long rt_idiom_n[2]; fprintf(stderr, "idioms: qcopy %ld native, %ld left to the translation\n", rt_idiom_n[0], rt_idiom_n[1]); }
    fprintf(stderr, "vfpu groups: %u fell back to the C forms\n", g_vgrp_fb);
#ifdef VGRP_TRACE
    { void vgrp_report(void); vgrp_report(); }
#endif
#ifdef GEN_NCHK
    { void rt_nchk_report(void); rt_nchk_report(); }
#endif
#ifdef QCOPY_STAT
    { extern uint32_t qs_ra[256], qs_ra2[256]; extern long qs_calls[256], qs_qw[256]; extern int qs_n; for (int i = 0; i < qs_n; i++) fprintf(stderr, "qcopy caller %08x (sp word %08x): %ld calls, %ld qwords\n", qs_ra[i], qs_ra2[i], qs_calls[i], qs_qw[i]); }
#endif
#endif
#ifdef VU_CHECK
    fprintf(stderr, "VU_CHECK: %ld compiled calls checked against the interpreter, %ld differ\n", vu_check_calls, vu_check_bad);
#endif
    if (getenv("MGS2_IOP_DUMP")) { FILE *df = fopen(getenv("MGS2_IOP_DUMP"), "wb"); if (df) { fwrite(iop_rt_mem(g_iop), 1, iop_rt_mem_size(g_iop), df); fclose(df); } }   /* dev: IOP RAM at the stop */
    for (t = 1; t < K_THREADS; t++) if (k_th[t].used)          /* where each thread waits (saved ra / sp of its last switch) */
      fprintf(stderr, "  thread %d: status %d prio %d wait %d/%d entry %06x ra %06x sp %08x\n", t, k_th[t].status, k_th[t].prio, k_th[t].wait_type,
              k_th[t].wait_id, k_th[t].entry, (uint32_t)k_th[t].c.R[31], (uint32_t)k_th[t].c.R[29]);
    exit(0);
  }
}
static const char *sys_name(int32_t n)
{
  switch (n) {
  case 0x3c: return "SetupThread"; case 0x3d: return "SetupHeap"; case 0x3e: return "EndOfHeap"; case 0x64: return "FlushCache";
  case 0x20: return "CreateThread"; case 0x22: return "StartThread"; case 0x23: return "ExitThread"; case 0x29: return "ChangeThreadPriority";
  case 0x2b: return "RotateThreadReadyQueue"; case 0x2f: return "GetThreadId"; case 0x30: return "ReferThreadStatus"; case 0x32: return "SleepThread";
  case 0x33: return "WakeupThread"; case -0x34: return "iWakeupThread"; case 0x40: return "CreateSema"; case 0x42: return "SignalSema";
  case -0x43: return "iSignalSema"; case 0x44: return "WaitSema"; case 0x45: return "PollSema"; case 0x10: return "AddIntcHandler";
  case 0x12: return "AddDmacHandler"; case 0x14: return "EnableIntc"; case 0x16: return "EnableDmac"; case 0x5c: return "EnableIntcHandler";
  case 0x5e: return "EnableDmacHandler"; case 0x77: return "SifSetDma"; case 0x76: return "SifDmaStat"; case 0x79: return "SifSetReg";
  case 0x7a: return "SifGetReg"; case 0x71: return "GsPutIMR"; case 0x70: return "GsGetIMR"; case 0x02: return "SetGsCrt";
  }
  return "";
}
static int quiet_sys(int32_t n) { return n == 0x44 || n == 0x42 || n == -0x43 || n == 0x45 || n == 0x64 || n == 0x2f || n == 0x2b || n == 0x32 || n == -0x34 || n == 0x33
                                    || n == 0x70 || n == 0x71 || n == 0x7c || n == 0xfc || n == 0xfd || n == 0x35 || n == -0x2f || n == 0x77 || n == -0x77 || n == -0x78; }
static void SYSCALL(uint32_t pc)
{
  int32_t n = (int32_t)R[3]; uint32_t a0 = R[4], a1 = R[5], a2 = R[6], a3 = R[7];
  n_sys++; k_take_interrupts(); if (k_need_resched && !k_in_intr && !k_dispatch_off) k_resched();
  if (!quiet_sys(n) || n_sys < 400 || (k_vblanks >= verbose_from && k_vblanks < verbose_from + 3))
    fprintf(stderr, "[%ld vb, th %d] sys %s%02x %-20s pc %06x a0 %08x a1 %08x a2 %08x a3 %08x ra %06x\n", k_vblanks, k_cur, n < 0 ? "-" : " ",
            n < 0 ? -n : n, sys_name(n), pc, a0, a1, a2, a3, (uint32_t)R[31]);
  if (k_syscall(n) || sif_syscall(n)) return;
  switch (n) {
  case 0x02: return;                                                           /* SetGsCrt: the display side is not modelled yet */
  case 0x71: case -0x71: gs_imr = a0; return;                                  /* GsPutIMR */
  case 0x70: case -0x70: R[2] = (int32_t)gs_imr; return;                       /* GsGetIMR */
  case 0x4b:                                                                   /* GetOsdConfigParam: the console settings word */
    /* bit 0 spdif, 1-2 screen type (0 = 4:3), 3 video output (0 = RGB), 4 "japLanguage" (1 = English), 5-12 ps1drv, 13-15 version,
       16-20 language (1 = English: Ffm's port is English), 21-31 time zone offset in minutes (0) */
    WR32(a0, (1u << 4) | (1u << 13) | (1u << 16)); return;
  case 0x7c:                                                                   /* Deci2Call: 0x10 kputs { char *s } is the game's printf output */
    if (a0 == 0x10) { uint32_t sp = RD32(a1), i; char b[256];
      for (i = 0; i < sizeof b - 1 && RD8(sp + i); i++) b[i] = (char)RD8(sp + i); b[i] = 0; fprintf(stderr, "kputs: %s%s", b, i && b[i - 1] == '\n' ? "" : "\n");
      R[2] = 0; return; }   /* (retail return value not checked: the printf sink at 0x174e20 discards it) */
    break;
  case 0x6f: {                                                                 /* GetOsdConfigParam2 (buffer, size, offset): the extended settings */
    /* n_title reads byte 1 (bits 6-7, bit 4) when the version field above is nonzero.  Answered as all zero: a console choice like the
       language, not taken from a real console's NVRAM */
    uint32_t i; for (i = 0; i < a1; i++) WR8(a0 + i, 0); return; }
  }
  fprintf(stderr, "unhandled syscall %s%x\n", n < 0 ? "-" : "", n < 0 ? -n : n); exit(2);
}
static void BREAK(uint32_t pc) { fprintf(stderr, "break at %06x\n", pc); exit(3); }
/* performance counters (COP0 25): PCCR (sel 0) bit 31 enables, EVENT0 bits 5-9 / EVENT1 bits 15-19 = 1 count processor cycles
   (the game's profiler: PCCR 0x80000038, PCR0 = 0, ..., PCCR 0, read PCR0).  Cycles here are the virtual time. */
static uint32_t pc_ccr, pc_r[2]; static uint64_t pc_t0;
static void pc_sync(void)
{
  uint32_t d = (uint32_t)(K_READ() - pc_t0); pc_t0 = K_READ();
  if (pc_ccr >> 31) { if (((pc_ccr >> 5) & 31) == 1) pc_r[0] += d; if (((pc_ccr >> 15) & 31) == 1) pc_r[1] += d; }
}
static uint32_t COP0R(int r, int s)
{
  (void)s;
  if (r == 9) { DGX_NOTE(6); K_ADV(64); return (uint32_t)K_READ(); }         /* Count: a poll advances virtual time */
  if (r == 12) return k_status;                                                /* Status: EIE (bit 16) follows ei / di */
  if (r == 15) return 0x2e20;                                                  /* PRId: R5900 */
  if (r == 25) { pc_sync(); return s == 0 ? pc_ccr : pc_r[(s >> 1) & 1]; }
  if (r == 28) return 0;                                                       /* TagLo after a cache index load (SyncDCache): no cache here */
  fprintf(stderr, "mfc0 %d.%d at ra %06x\n", r, s, (uint32_t)R[31]); return 0;
}
static void COP0W(int r, int s, uint32_t v)
{
  if (r == 25) { pc_sync(); if (s == 0) pc_ccr = v; else if (s == 1 || s == 3) pc_r[s >> 1] = v; return; }
  fprintf(stderr, "mtc0 %d.%d %08x\n", r, s, v);
}
static void COP0OP(int f, uint32_t pc)                                         /* ei / di set / clear Status.EIE; interrupts are delivered only at idle */
{
  if (f == 0x38) { k_status |= 0x10000u; K_POLL_KICK(); } else if (f == 0x39) k_status &= ~0x10000u;
  else { fprintf(stderr, "cop0 op %x at %06x\n", f, pc); exit(6); }
}
#ifdef GUEST_SPLIT_RT
/* the runtime unit of the split build: the generated unit (gen.c) reaches the machine through these */
int gen_guest_call(uint32_t a); void gen_start_x(void);
static int guest_call(uint32_t a) { return gen_guest_call(a); }
static void gen_start(void) { gen_start_x(); }
uint8_t *rt_mm(uint32_t a) { if (rt_mm_from >= 0 && k_vblanks >= rt_mm_from) rt_mm_n[a >> 28]++; return mm_map(a); }
uint32_t rt_wr_note(uint32_t a) { return io_wr_note(a); }
void rt_poll(void) { k_poll(); }
SPRS uint32_t g_vgrp_fb;                            /* vu0.h vu_vgrp: VFPU macro-op runs that fell back to the C forms */
#if defined(VGRP_TRACE) && !defined(__psp__)
static const void *vg_pc[4096]; static long vg_n[4096];
void vgrp_note(const void *pc, const uint32_t (*sv)[4])
{
  static long a = -2, b, dn; static uintptr_t dpc; uint32_t h;
  if (a == -2) { const char *e = getenv("MGS2_VGRP_TRACE"); a = -1; if (e) sscanf(e, "%ld:%ld", &a, &b);
    if ((e = getenv("MGS2_VGRP_DUMP"))) sscanf(e, "%lx:%ld", (unsigned long *)&dpc, &dn); }   /* <pc>:<count>: VF / vu_sv at that site's first fallbacks */
  if (a < 0 || (long)k_vblanks < a || (long)k_vblanks > b) return;
  if (dn > 0 && (uintptr_t)pc == dpc) { int r; dn--; fprintf(stderr, "vgdump vb %ld", (long)k_vblanks);
    for (r = 1; r < 17; r++) fprintf(stderr, " VF%d=%08x,%08x,%08x,%08x sv=%08x,%08x,%08x,%08x", r, VF[r].u[0], VF[r].u[1], VF[r].u[2], VF[r].u[3],
      sv[r][0], sv[r][1], sv[r][2], sv[r][3]);
    fprintf(stderr, " ACC=%08x,%08x,%08x,%08x\n", VACC[0], VACC[1], VACC[2], VACC[3]); }
  for (h = ((uintptr_t)pc >> 2) & 4095; vg_pc[h] && vg_pc[h] != pc; h = (h + 1) & 4095) {}
  vg_pc[h] = pc; vg_n[h]++;
}
void vgrp_report(void) { for (int i = 0; i < 4096; i++) if (vg_pc[i]) fprintf(stderr, "vgrp pc %p %ld\n", vg_pc[i], vg_n[i]); }
#endif
SPRS uint32_t g_vu_ext;                             /* gen_pre.h VH_CHK: the slow paths that can change VF / ACC behind generated code (tools/vu_hoist.py) */
void rt_poll_slow(void)                         /* gen_pre.h IRQ_POLLF: time already advanced, k_poll_at reached */
{
#ifdef K_COUNTDOWN
  if (K_NOW() < k_poll_at) { K_SYNC(); return; }   /* the countdown was clamped (ee_io.h): nothing due yet, re-arm */
#endif
  g_vu_ext++; k_poll_rest();
}
#if defined(GEN_MM_FAST) && defined(GUEST_SPR0)
/* recomp/idioms.py QCOPY (gen_pre.h GEN_IDIOM_QCOPY): the game's qword copy, a0 dst, a1 src, a2 qwords.  The translation copies one qword
   at a time up to a 64-byte boundary of dst, then four at a time while more than four remain, then one at a time, polling at each loop
   back-edge.  Here: the same loads and stores in the same order (qword i of src, then qword i of dst), the same registers at the end (v0 / v1:
   the last qwords loaded into them, with their upper halves, or the counters the translation leaves there) and the same virtual time.  Only
   when every access is plain RAM (gen_pre.h GEN_FAST / GEN_RAM, without a wrap of the 32 MB mask) and no poll would reach k_poll_at;
   otherwise 0, and the translation runs. */
SPRS long rt_idiom_n[2];                             /* native / left to the translation (printed at the stop) */
#ifdef GEN_NCHK   /* dev (guest.h NCHK): the loads of 32-bit register locals from a non-sign-extended R[], per function and register */
#define NCHK_TAB 16384                                /* per C function, register and kind (entry load / RCLD / RCMV), hashed */
static long nchk_n, nchk_lost; static struct { const char *fn; uint32_t fa; int n; long cnt; int64_t v; long vb; } nchk_tab[NCHK_TAB]; static int nchk_k;
void rt_nchk_bad(int n, uint32_t fa, int64_t v, const char *fn)
{
  uint32_t h = ((uint32_t)(uintptr_t)fn * 2654435761u ^ fa * 40503u ^ (uint32_t)n * 97u) & (NCHK_TAB - 1);
  nchk_n++;
  while (nchk_tab[h].fn && (nchk_tab[h].fn != fn || nchk_tab[h].fa != fa || nchk_tab[h].n != n)) h = (h + 1) & (NCHK_TAB - 1);
  if (nchk_tab[h].fn) { nchk_tab[h].cnt++; return; }
  if (nchk_k >= NCHK_TAB * 3 / 4) { nchk_lost++; return; }
  nchk_tab[h].fn = fn; nchk_tab[h].fa = fa; nchk_tab[h].n = n; nchk_tab[h].cnt = 1; nchk_tab[h].v = v; nchk_tab[h].vb = k_vblanks; nchk_k++;
}
void rt_nchk_report(void)
{
  fprintf(stderr, "NCHK: %ld loads of a 32-bit local from a non-sign-extended register, %d places (%ld not recorded: table full)\n", nchk_n, nchk_k, nchk_lost);
  for (int i = 0; i < NCHK_TAB; i++) if (nchk_tab[i].fn)
    fprintf(stderr, "NCHK %s %s r%d: %ld times, first %016llx at vb %ld\n", nchk_tab[i].fn, nchk_tab[i].fa == 0 ? "RCLD" : nchk_tab[i].fa == 1 ? "RCMV" : "entry",
            nchk_tab[i].n, nchk_tab[i].cnt, (unsigned long long)nchk_tab[i].v, nchk_tab[i].vb);
}
#endif
#ifdef QCOPY_STAT
uint32_t qs_ra[256], qs_ra2[256]; long qs_calls[256], qs_qw[256]; int qs_n;   /* qs_ra2: the word at the guest sp (a wrapper's saved ra) */
#endif
int rt_idiom_qcopy(void)
{
  int64_t a0 = R[4], a1 = R[5], a2 = R[6];
  rt_idiom_n[1]++;
  if (a0 != (int32_t)a0 || a1 != (int32_t)a1 || a2 != (int32_t)a2) return 0;
  int32_t dst = (int32_t)a0, src = (int32_t)a1, r6 = (int32_t)a2, r4 = (int32_t)((uint32_t)dst + 15u), r3 = ((dst < 0 ? r4 : dst) >> 4) & 3, r2s = 0;
  int64_t T = 0, P = 0, i2 = -1, i3 = -1; int s2 = 0, s3 = 1;       /* T qwords, P polls; i2 / i3: the last qword loaded into v0 / v1 */
  if (r3) {                                     /* one at a time up to the 64-byte boundary: lq v0 before the loop, then in the delay slot */
    r3 = 4 - r3;
    for (;;) { i2 = T++; r6--; if (r6 <= 0) goto end; if (--r3 > 0) { P++; continue; } break; }
  }
  r2s = r6 < 5; s2 = 1;                         /* slti v0, a2, 5 */
  if (!r2s) {                                   /* four at a time: v1, v0, v1, v0 */
    int64_t k = ((int64_t)r6 - 1) / 4;
    T += 4 * k; i3 = T - 2; i2 = T - 1; s2 = s3 = 0; P += k - 1; r6 -= (int32_t)(4 * k); r4 = 1;
  }
  if (r6 > 0) { T += r6; i2 = T - 1; s2 = 0; P += r6 - 1; r6 = 0; }
end:
  if (T) {
    uint32_t s0 = (uint32_t)src & ~15u, d0 = (uint32_t)dst & ~15u;
    if (s0 >> 28 == 1 || d0 >> 28 == 1 || T > (1 << 21) || (s0 & (GUEST_SIZE - 1)) + 16 * T > GUEST_SIZE || (d0 & (GUEST_SIZE - 1)) + 16 * T > GUEST_SIZE) return 0;
    if (K_NOW() + (uint64_t)P * K_POLL_CYCLES >= k_poll_at) return 0;
    const uint64_t *s = (const uint64_t *)(g_mem + (s0 & (GUEST_SIZE - 1))); uint64_t *d = (uint64_t *)(g_mem + (d0 & (GUEST_SIZE - 1)));
#if defined(__psp__) && !defined(QCOPY_NO_VFPU)
    /* PSP: each qword through a VFPU register (lv.q / sv.q: 16 bytes in one instruction, g_mem is 64-byte aligned), still qword i loaded
       then stored before qword i + 1, so an overlap copies as in the translation; C000-C030 are scratch (nothing else uses the VFPU).
       v18: when the two ranges share no 64-byte line the order does not matter, and rt_qcopy_nov skips reading the lines it overwrites */
    if (((uintptr_t)d >> 6) > (((uintptr_t)s + 16 * (uintptr_t)T - 1) >> 6) || (((uintptr_t)d + 16 * (uintptr_t)T - 1) >> 6) < ((uintptr_t)s >> 6))
      rt_qcopy_nov((uint8_t *)d, (const uint8_t *)s, 16u * (uint32_t)T);
    else
    { const uint64_t *sq = s; uint64_t *dq = d; int32_t n = (int32_t)T;   /* T <= 1 << 21 (checked above) */
      for (; n >= 4; n -= 4, sq += 8, dq += 8)
        __asm__ volatile ("lv.q C000, 0(%0)\n\tsv.q C000, 0(%1)\n\tlv.q C010, 16(%0)\n\tsv.q C010, 16(%1)\n\t"
                          "lv.q C020, 32(%0)\n\tsv.q C020, 32(%1)\n\tlv.q C030, 48(%0)\n\tsv.q C030, 48(%1)" :: "r"(sq), "r"(dq) : "memory");
      for (; n > 0; n--, sq += 2, dq += 2) __asm__ volatile ("lv.q C000, 0(%0)\n\tsv.q C000, 0(%1)" :: "r"(sq), "r"(dq) : "memory"); }
#else
    for (int64_t i = 0; i < 2 * T; i += 2) { uint64_t lo = s[i], hi = s[i + 1]; d[i] = lo; d[i + 1] = hi; }
#endif
    if (i2 >= 0) { RH[2] = (int64_t)d[2 * i2 + 1]; if (!s2) R[2] = (int64_t)d[2 * i2]; }   /* qword i of dst holds what was loaded */
    if (i3 >= 0) { RH[3] = (int64_t)d[2 * i3 + 1]; if (!s3) R[3] = (int64_t)d[2 * i3]; }
  }
  K_ADV((uint64_t)P * K_POLL_CYCLES);
  if (s2) R[2] = r2s; if (s3) R[3] = r3;
  R[4] = r4; R[5] = (int32_t)((uint32_t)src + 16u * (uint32_t)T); R[6] = r6; R[7] = (int32_t)((uint32_t)dst + 16u * (uint32_t)T);
#ifdef QCOPY_STAT   /* dev: qwords copied per caller over MGS2_QCOPY_STAT=<first vb>:<last vb> (printed at the stop) */
  { static long a = -2, b; if (a == -2) { const char *e = getenv("MGS2_QCOPY_STAT"); a = -1; if (e) sscanf(e, "%ld:%ld", &a, &b); }
    if (a >= 0 && (long)k_vblanks >= a && (long)k_vblanks <= b) { uint32_t ra = (uint32_t)R[31], ra2 = *(uint32_t *)(g_mem + ((uint32_t)R[29] & (GUEST_SIZE - 4))); int i;
      for (i = 0; i < qs_n && (qs_ra[i] != ra || qs_ra2[i] != ra2); i++) {}
      if (i == qs_n && qs_n < 256) { qs_ra[qs_n] = ra; qs_ra2[qs_n++] = ra2; } if (i < 256) { qs_calls[i]++; qs_qw[i] += T; } } }
#endif
  rt_idiom_n[1]--; rt_idiom_n[0]++; return 1;
}
#endif
void rt_sp_bad(uint32_t sp, int k) { static long n; if (n++ < 20) fprintf(stderr, "sp access outside the invariant: vb %ld sp %08x + %d ra %06x\n", k_vblanks, sp, k, (uint32_t)R[31]); }   /* gen_pre.h GEN_SP_CHECK */
void rt_mm_odd(uint32_t a) { static long n; if (n++ < 20) fprintf(stderr, "mm odd: vb %ld address %08x ra %06x\n", k_vblanks, a, (uint32_t)R[31]); }   /* gen_pre.h GEN_MM_CHECK */
#ifndef __psp__
void rt_mb_bad(uint32_t b, int k) { static long n; if (n++ < 20) fprintf(stderr, "mm base invariant error: vb %ld base %08x + %d ra %06x\n", k_vblanks, b, k, (uint32_t)R[31]); }   /* gen_pre.h GEN_MB_CHECK */
#endif
void rt_store(uint32_t a, const void *p, int n)   /* gen_pre.h GEN_MM_FAST: a store outside RAM; a register write takes effect at once */
{ g_vu_ext++; if (rt_mm_from >= 0 && k_vblanks >= rt_mm_from) rt_st_n[a >> 28]++; memcpy(mm_map(io_wr_note(a)), p, n); if (g_io_arm) { g_io_pend = g_io_arm; g_io_arm = 0; } if (g_io_pend) io_flush(); }
uint32_t rt_hwr32(uint32_t a) { return HWR32(a); }
#ifdef GEN_DG_TRACE
static int ct_dg_inside(void);
#endif
void rt_cov(const char *u, uint32_t a)
{ if (!rt_cov_t) rt_cov_t = calloc(1 << 20, sizeof *rt_cov_t);
  uint32_t h = (a * 2654435761u ^ (uint32_t)(uintptr_t)u) & ((1u << 20) - 1);
  while (rt_cov_t[h].u && (rt_cov_t[h].u != u || rt_cov_t[h].a != a)) h = (h + 1) & ((1u << 20) - 1);
  if (!rt_cov_t[h].u) { rt_cov_t[h].u = u; rt_cov_t[h].a = a; rt_cov_n++; }
  /* dev: MGS2_CALLTRACE=<vblank>:<file>: every function entry during that vblank, with $sp, $ra and a0-a3 (and, with GEN_DG_TRACE, whether
     inside the draw pipeline): the pipeline's dynamic call structure.  Addresses and register values only */
  { static long ct_vb = -2; static FILE *ct_f;
    if (ct_vb == -2) { const char *e = getenv("MGS2_CALLTRACE"); char fn[256]; ct_vb = -1; if (e && sscanf(e, "%ld:%255s", &ct_vb, fn) == 2) ct_f = fopen(fn, "w"); else ct_vb = -1; }
    if (ct_f && k_vblanks == ct_vb)
      fprintf(ct_f, "%s %06x sp %08x ra %06x a %08x %08x %08x %08x th %d in %d\n", u, a, (uint32_t)R[29], (uint32_t)R[31], (uint32_t)R[4], (uint32_t)R[5], (uint32_t)R[6], (uint32_t)R[7], k_cur,
#ifdef GEN_DG_TRACE
              ct_dg_inside()
#else
              -1
#endif
              );
    else if (ct_f && k_vblanks > ct_vb) { fclose(ct_f); ct_f = 0; } } }
#ifdef VU_PROFILE
/* dev (vu0.h): the interpreter's calls and steps per micro memory image and entry, counted from MGS2_VU_PROF=<dir>:<first vblank>; every image
   met is saved as <dir>/<hash>.bin and its entries listed in <dir>/entries.txt (recomp/vurecomp.py input); the table is printed at the stop */
static struct { uint32_t h; int pc; long calls, steps; } vp_t[256]; static int vp_n, vpp_cur = -1; static long vp_s0, vp_from = -2; static char vp_dir[200];
void vu_prof(uint32_t h, int pc, int end)
{
  int i;
  if (vp_from == -2) { const char *e = getenv("MGS2_VU_PROF"); vp_from = -1; if (e && sscanf(e, "%199[^:]:%ld", vp_dir, &vp_from) != 2) vp_from = -1; }
  if (vp_from < 0) return;
  if (end) { if (vpp_cur >= 0) vp_t[vpp_cur].steps += vu_steps - vp_s0; vpp_cur = -1; return; }
  for (i = 0; i < vp_n && (vp_t[i].h != h || vp_t[i].pc != pc); i++) {}
  if (i == vp_n) {
    char fn[260]; FILE *f; int k, seen = 0;
    if (vp_n == 256) return;
    for (k = 0; k < vp_n; k++) seen |= vp_t[k].h == h;
    if (!seen) { snprintf(fn, sizeof fn, "%s/%08x.bin", vp_dir, h); if ((f = fopen(fn, "wb"))) { fwrite(VUPROG, 4, 1024, f); fclose(f); } }
    snprintf(fn, sizeof fn, "%s/entries.txt", vp_dir); if ((f = fopen(fn, "a"))) { fprintf(f, "%08x %d\n", h, pc); fclose(f); }
    vp_t[vp_n].h = h; vp_t[vp_n].pc = pc; vp_n++;
  }
  if (k_vblanks < vp_from) return;
  vp_t[i].calls++; vpp_cur = i; vp_s0 = vu_steps;
}
void vu_prof_report(void)
{
  int i; if (vp_from < 0) return;
  fprintf(stderr, "vu0 profile from vb %ld (image, entry, calls, steps, steps/call):\n", vp_from);
  for (i = 0; i < vp_n; i++) if (vp_t[i].calls) fprintf(stderr, "  vu %08x pc %4d calls %9ld steps %11ld (%.1f)\n", vp_t[i].h, vp_t[i].pc, vp_t[i].calls, vp_t[i].steps, (double)vp_t[i].steps / vp_t[i].calls);
}
#endif
/* dev (dispatch.h GEN_STUB_HOOK): MGS2_STUB=<hex addr>[,<hex addr>...]:<first vblank>: from that vblank the calls through a register to those
   functions return at once (what the game logic costs without, e.g., the DG draw pipeline; the game state then differs) */
int rt_stub(uint32_t a)
{
  static uint32_t st[8]; static int n = -1; static long from;
  if (n < 0) { const char *e = getenv("MGS2_STUB"); n = 0; if (e) { char *p; do { st[n++] = (uint32_t)strtoul(e, &p, 16); e = p + 1; } while (*p == ',' && n < 8); from = *p == ':' ? atol(p + 1) : 0; } }
  if (!n || k_vblanks < from) return 0;
  for (int i = 0; i < n; i++) if (st[i] == a) return 1;
  return 0;
}
#if !defined(__psp__)
#include <time.h>
/* dev: MGS2_PKT_HASH=<from vb>:<to vb>: a line "pkt: vb <n> <hash>" per vblank of that range: ee_io.h's FNV-1a 64 of the VIF1 and GIF DMA
   (tags and data) since the previous vblank, i.e. what reached the GS in the frame that just ended */
void pkt_vblank(long vb)
{
  static long a = -2, b; static const char *dp; extern int g_pkt_on; extern uint64_t g_pkt_h; extern FILE *g_pkt_df;
  if (a == -2) { const char *e = getenv("MGS2_PKT_HASH"); a = -1; if (e && sscanf(e, "%ld:%ld", &a, &b) != 2) a = -1; dp = getenv("MGS2_PKT_DUMP"); }
  if (a < 0) return;
  if (g_pkt_on) fprintf(stderr, "pkt: vb %ld %016llx\n", vb - 1, (unsigned long long)g_pkt_h);
  g_pkt_on = vb >= a && vb < b; g_pkt_h = 0xcbf29ce484222325ull;
  if (g_pkt_df) { fclose(g_pkt_df); g_pkt_df = 0; }
  if (g_pkt_on && dp) { char fn[300]; snprintf(fn, sizeof fn, "%s.%ld", dp, vb); g_pkt_df = fopen(fn, "wb"); }   /* MGS2_PKT_DUMP=<prefix>: the hashed
                                                                         bytes of each vblank of the window as records (ee_io.h pkt_fnv) in <prefix>.<vb> */
}
/* dev: MGS2_VB_TIME=<first vblank>: the host time of every 100 vblanks from there */
void vb_time(long vb)
{
  static long from = -2; static double t0; struct timespec ts; double t;
  if (from == -2) { const char *e = getenv("MGS2_VB_TIME"); from = e ? atol(e) : -1; }
  if (from < 0 || vb < from || (vb - from) % 100) return;
  clock_gettime(CLOCK_MONOTONIC, &ts); t = ts.tv_sec + ts.tv_nsec * 1e-9;
  if (vb > from) fprintf(stderr, "vbtime: vb %ld: %.3f ms per vblank\n", vb, (t - t0) * 1000 / 100);
  t0 = t;
}
#endif
#ifdef GEN_GI_COUNT
/* dev (x86): MGS2_GI=<a>:<b>:<file>: how many times each guest instruction ran in vblanks a+1 .. b, outside the draw pipeline and inside
   it apart, from headers rewritten by tools/dev/gi_count.py (GI_(pc) at each instruction line, P11da90 wrapped); after b the non-zero
   counts go to <file> as "pc count dg_count" (to set against PspProf's per-function PSP instructions) */
uint32_t g_gi_cnt[1 << 22], g_gi_dgc[1 << 22]; int g_gi_on, g_gi_dg;
void gi_window(long vb)
{
  static long a = -2, b; static char out[256];
  if (a == -2) { const char *e = getenv("MGS2_GI"); a = -1; if (e && sscanf(e, "%ld:%ld:%255s", &a, &b, out) != 3) a = -1; }
  if (a < 0) return;
  g_gi_on = vb > a && vb <= b;
  if (vb == b + 1) {
    FILE *f = fopen(out, "w"); uint32_t i;
    if (f) { for (i = 0; i < (1u << 22); i++) if (g_gi_cnt[i] | g_gi_dgc[i]) fprintf(f, "%06x %u %u\n", i << 2, g_gi_cnt[i], g_gi_dgc[i]); fclose(f); }
    fprintf(stderr, "gi: counts of vblanks %ld-%ld written to %s\n", a + 1, b, out); a = -1;
  }
}
#endif
#if defined(GEN_DG_TRACE) && !defined(__psp__)
/* dev: MGS2_DG_TRACE=<first vblank>:<out file>: which RAM words the draw pipeline (the call of 0x11da90, dispatch.h) writes that code
   outside it then reads (what the GE renderer, which replaces it, must still provide).  Per word the last writer (inside the pipeline or
   not, any thread switch inside it counts as outside); every read outside it of a word last written inside it is counted from that vblank
   to the stop, by reader: the host return address of the access (a generated function: tools/dgt_report.py names it with nm) or a DMA
   channel (0x100 + channel; scratchpad copies 0x200 + channel).  Also: micro calls outside the pipeline whose micro code it uploaded, and
   for the words game code reads, which pipeline code wrote them ("wr" lines). */
#ifdef GEN_ACTOR_PROF
/* dev: MGS2_ACT_TRACE=<first vblank>:<end vblank>:<out file> (an x86 GEN_DG_TRACE + GEN_ACTOR_PROF build): who reads what each actor
   writes.  Per RAM word: the last writer (the actor whose act the scheduler's call is running, dispatch.h, on the thread that called it;
   "main" = that thread outside any actor; "thread" = any other thread; a scratchpad DMA copy keeps the source word's writer) and the
   vblank of that write.  From the first vblank to the end one, every read by another reader, or by the same actor in a later vblank, is
   counted per (writer, reader, same / previous / older vblank): reads, and words (a word counts once per reader until it is written
   again).  Readers are also the DMA channels (VIF1 = the GS packets).  Ids: 0 written before the trace, 1 main, 2 thread, 3 + channel. */
volatile uint32_t g_emuactor;
static long act_a = -2, act_b; static char act_out[256];
static uint16_t *act_lw, *act_lr; static uint32_t *act_lv;
static uint32_t act_addr[4096]; static int act_n = 16;
static uint32_t act_last = 0xffffffffu; static int act_th = -1, act_curid = 1;
struct act_p { uint32_t k; uint64_t n, w; }; static struct act_p act_t[1 << 16]; static int act_tn;
/* MGS2_ACT_DETAIL=<act>[,<act>...] (up to 8): for those writers, the words other readers (and the writer itself in a later vblank) read,
   per (word, reader, class): "d" lines in the report */
static uint32_t act_det[8]; static int act_ndet = -1; struct act_d { uint32_t w; uint16_t wr, r; uint8_t c; uint64_t n; }; static struct act_d *act_dt; static long act_dn;
static int act_on(void)
{
  if (act_a == -2) { const char *e = getenv("MGS2_ACT_TRACE"); act_a = -1; if (e && sscanf(e, "%ld:%ld:%255s", &act_a, &act_b, act_out) != 3) act_a = -1; }
  return act_a >= 0 && k_vblanks < act_b;
}
static int act_id(uint32_t a)
{
  int i; for (i = 16; i < act_n; i++) if (act_addr[i] == a) return i;
  if (act_n == 4096) return 1;
  act_addr[act_n] = a; return act_n++;
}
static inline int act_who(void)
{
  if (g_emuactor != act_last) { act_last = g_emuactor; act_th = k_cur; act_curid = act_last ? act_id(act_last) : 1; }   /* the first access after a change is the changer's */
  return k_cur != act_th ? 2 : act_curid;
}
static void act_count(uint32_t w, int r)
{
  int wr = act_lw[w], c; uint32_t k, h;
  if (wr == r && act_lv[w] == (uint32_t)k_vblanks) return;   /* an actor reading what it wrote in this vblank */
  c = act_lv[w] == (uint32_t)k_vblanks ? 0 : act_lv[w] + 1 == (uint32_t)k_vblanks ? 1 : 2;
  if (k_vblanks < act_a) return;
  k = (uint32_t)wr << 14 | (uint32_t)r << 2 | (uint32_t)c; h = (k * 2654435761u) >> 16;
  while (act_t[h].n && act_t[h].k != k) h = (h + 1) & 0xffff;
  if (!act_t[h].n) { if (act_tn > 0xc000) return; act_t[h].k = k; act_tn++; }
  act_t[h].n++; if (act_lr[w] != r) { act_lr[w] = (uint16_t)r; act_t[h].w++; }
  if (act_ndet < 0) { const char *e = getenv("MGS2_ACT_DETAIL"); act_ndet = 0; while (e && *e && act_ndet < 8) { act_det[act_ndet++] = (uint32_t)strtoul(e, (char **)&e, 16); if (*e == ',') e++; else break; } }
  if (act_ndet && wr >= 16) {
    int i; for (i = 0; i < act_ndet && act_det[i] != act_addr[wr]; i++) ;
    if (i < act_ndet) {
      if (!act_dt) act_dt = calloc(1 << 20, sizeof *act_dt);
      h = ((w * 2654435761u) ^ (uint32_t)(r * 40503u) ^ (uint32_t)c) & ((1 << 20) - 1);
      while (act_dt[h].n && (act_dt[h].w != w || act_dt[h].r != r || act_dt[h].c != c || act_dt[h].wr != wr)) h = (h + 1) & ((1 << 20) - 1);
      if (!act_dt[h].n) { if (act_dn > (1 << 20) * 3 / 4) return; act_dt[h].w = w; act_dt[h].r = (uint16_t)r; act_dt[h].c = (uint8_t)c; act_dt[h].wr = (uint16_t)wr; act_dn++; }
      act_dt[h].n++;
    }
  }
}
static void act_rd(uint32_t a) { if (!act_on() || !act_lw) return; act_count((a & (GUEST_SIZE - 1)) >> 2, act_who()); }
static void act_wr(uint32_t a, int n)
{
  uint32_t w, e; int who;
  if (!act_on()) return;
  if (!act_lw) { act_lw = calloc(GUEST_SIZE / 4, 2); act_lr = calloc(GUEST_SIZE / 4, 2); act_lv = calloc(GUEST_SIZE / 4, 4); }
  who = act_who(); w = (a & (GUEST_SIZE - 1)) >> 2; e = ((a & (GUEST_SIZE - 1)) + n - 1) >> 2;
  for (; w <= e && w < GUEST_SIZE / 4; w++) { act_lw[w] = (uint16_t)who; act_lv[w] = (uint32_t)k_vblanks; act_lr[w] = 0xffff; }
}
static void act_dma(int ch, uint32_t a, uint32_t nq)
{
  uint32_t b, i;
  if (!act_on() || !act_lw) return;
  b = (a & 0x80000000u) ? (a & 0x3ff0) : (a & (GUEST_SIZE - 1) & ~15u);
  for (i = 0; i < nq * 4 && (b >> 2) + i < GUEST_SIZE / 4; i++) act_count((b >> 2) + i, 3 + ch);
}
static void act_copy(uint32_t dst, uint32_t src, uint32_t n)
{
  uint32_t i;
  if (!act_on() || !act_lw) return;
  for (i = 0; i < n / 4; i++) {
    uint32_t s = ((src & (GUEST_SIZE - 1)) >> 2) + i, d = ((dst & (GUEST_SIZE - 1)) >> 2) + i;
    act_lw[d] = act_lw[s]; act_lv[d] = (uint32_t)k_vblanks; act_lr[d] = 0xffff;
  }
}
static void act_report(void)
{
  FILE *f; int i;
  if (act_a < 0 || !(f = fopen(act_out, "w"))) return;
  fprintf(f, "# MGS2_ACT_TRACE vblanks %ld..%ld: writer reader class(0 same vblank, 1 previous, 2 older) reads words; ids: 0 before the trace,"
          " 1 main, 2 thread, 3+ch DMA, else the actor's act address\n", act_a, act_b - 1);
  for (i = 0; i < 1 << 16; i++) if (act_t[i].n) {
    uint32_t k = act_t[i].k; int wr = k >> 14, r = k >> 2 & 0xfff, c = k & 3;
    fprintf(f, "%06x %06x %d %llu %llu\n", wr >= 16 ? act_addr[wr] : (uint32_t)wr, r >= 16 ? act_addr[r] : (uint32_t)r, c,
            (unsigned long long)act_t[i].n, (unsigned long long)act_t[i].w);
  }
  if (act_dt) for (i = 0; i < 1 << 20; i++) if (act_dt[i].n) {
    int wr = act_dt[i].wr, r = act_dt[i].r;
    fprintf(f, "d %06x %08x %06x %d %llu\n", act_addr[wr], act_dt[i].w * 4, r >= 16 ? act_addr[r] : (uint32_t)r, act_dt[i].c, (unsigned long long)act_dt[i].n);
  }
  fclose(f); fprintf(stderr, "act trace: %s (%d pairs, %ld detail rows)\n", act_out, act_tn, act_dn);
}
#else
static void act_rd(uint32_t a) { (void)a; } static void act_wr(uint32_t a, int n) { (void)a; (void)n; }
static void act_dma(int ch, uint32_t a, uint32_t nq) { (void)ch; (void)a; (void)nq; } static void act_copy(uint32_t d, uint32_t s, uint32_t n) { (void)d; (void)s; (void)n; }
static void act_report(void) { }
#endif
static uint8_t dgt_w[GUEST_SIZE / 4];
static uint8_t dgt_ein[GUEST_SIZE / 4], dgt_gr[GUEST_SIZE / 4];   /* from the first vblank: written inside the pipeline; read, after a write inside, by
                                                                game code (the CPU outside it, VIF0 / SIF / other DMA, scratchpad copies) */
static int dgt_depth, dgt_th, dgt_skip, dgt_prog_in; static uint32_t dgt_skip_a; static long dgt_from = -2; static char dgt_out[256];
static uint64_t dgt_wr_in, dgt_wr_out, dgt_hits, dgt_mpg_in, dgt_mpg_out;
struct dgt_e { uint32_t w; uintptr_t ra; uint64_t n; }; static struct dgt_e *dgt_t; static long dgt_n;
struct dgt_v { uint32_t h; int pc, prog_in; uint64_t n; }; static struct dgt_v dgt_vt[256]; static int dgt_vn;
static uintptr_t *dgt_lw;   /* per word: the host return address of its last write inside the pipeline (scratchpad copies: 0x200 + channel) */
struct dgt_x { uint32_t w; uintptr_t wr; uint64_t n; }; static struct dgt_x dgt_xt[4096]; static int dgt_xn;   /* words read by game code: their writers */
static struct dgt_x dgt_dt[4096]; static int dgt_dn;   /* words read by DMA: their writers, per channel (w = 0x100 + channel) */
/* the pipeline's input: reads inside it of words last written outside it, per 256-byte block, by reader and by the last writer outside
   (0: never written by guest code since the start, e.g. loaded from disc) */
static uintptr_t *dgt_lwo; struct dgt_i { uint32_t b; uintptr_t ra; uint64_t n; }; static struct dgt_i *dgt_it[4]; static long dgt_in[4];
static uint64_t dgt_dma_out[4];   /* DMA reads of words not written inside the pipeline (model data by reference); per block and last writer: "dv" (VIF1) / "do" (other) */
/* the pipeline's state across calls: reads inside it (CPU, DMA, scratchpad copies) of words it last wrote in an earlier call, i.e. what a
   step that skipped it would leave stale for the next one; per word, writer and reader ("cx" lines) */
static uint32_t dgt_call, *dgt_wcall; static uint8_t dgt_xcw[GUEST_SIZE / 4]; static uint64_t dgt_xc_hits;
static uint32_t *dgt_pda; static uint64_t dgt_pd_hits;   /* per word, the largest call age at a DMA read outside the pipeline (rt_dgt_dma, "pd" lines) */
struct dgt_c { uint32_t w; uintptr_t wr, rd; uint64_t n; }; static struct dgt_c *dgt_ct; static long dgt_cn;
#define DGT_CHASH (1 << 20)
static void dgt_cnote(uint32_t w, uintptr_t rd);
#define DGT_IHASH (1 << 20)
static void dgt_inote(int k, uint32_t b, uintptr_t ra)
{
  uint32_t h; if (!dgt_it[k]) dgt_it[k] = calloc(DGT_IHASH, sizeof *dgt_it[k]);
  h = (uint32_t)((b * 2654435761u) ^ (uint32_t)(ra * 40503u)) & (DGT_IHASH - 1);
  while (dgt_it[k][h].n && (dgt_it[k][h].b != b || dgt_it[k][h].ra != ra)) h = (h + 1) & (DGT_IHASH - 1);
  if (!dgt_it[k][h].n) { if (dgt_in[k] > DGT_IHASH * 3 / 4) return; dgt_it[k][h].b = b; dgt_it[k][h].ra = ra; dgt_in[k]++; }
  dgt_it[k][h].n++;
}
#define DGT_HASH (1 << 22)
static uint32_t dgt_ra[16]; static uint64_t dgt_ran[16], dgt_vu_in;
void host_enter(int k)
{
  int i;
  if (k != HOST_DG) return;
  if (!dgt_depth++) { dgt_th = k_cur; dgt_call++; }
  for (i = 0; i < 16 && dgt_ra[i] && dgt_ra[i] != (uint32_t)R[31]; i++) ;
  if (i < 16) { dgt_ra[i] = (uint32_t)R[31]; dgt_ran[i]++; }   /* where the guest calls the draw pipeline from */
}
void host_leave(int k) { if (k == HOST_DG) dgt_depth--; }
static inline int dgt_inside(void) { return dgt_depth && k_cur == dgt_th; }
static int ct_dg_inside(void) { return dgt_inside(); }
static int dgt_active(void)
{
  if (dgt_from == -2) { const char *e = getenv("MGS2_DG_TRACE"); dgt_from = -1; if (e && sscanf(e, "%ld:%255s", &dgt_from, dgt_out) != 2) dgt_from = -1; }
  return dgt_from >= 0 && k_vblanks >= dgt_from;
}
static void dgt_note(uint32_t w, uintptr_t ra)
{
  uint32_t h; if (!dgt_t) dgt_t = calloc(DGT_HASH, sizeof *dgt_t);
  h = (uint32_t)((w * 2654435761u) ^ (uint32_t)(ra * 40503u)) & (DGT_HASH - 1);
  while (dgt_t[h].n && (dgt_t[h].w != w || dgt_t[h].ra != ra)) h = (h + 1) & (DGT_HASH - 1);
  if (!dgt_t[h].n) { if (dgt_n > DGT_HASH * 3 / 4) return; dgt_t[h].w = w; dgt_t[h].ra = ra; dgt_n++; }
  if (ra >= 0x1000 || (ra != 0x101 && ra != 0x102)) dgt_gr[w] = 1;   /* VIF1 and GIF are the renderer's readers */
  dgt_t[h].n++; dgt_hits++;
  if (ra < 0x1000 && dgt_lw) {   /* a DMA read: which pipeline code wrote what the channel reads (the packet builders) */
    int i; uintptr_t wr = dgt_lw[w];
    for (i = 0; i < dgt_dn && (dgt_dt[i].w != (uint32_t)ra || dgt_dt[i].wr != wr); i++) ;
    if (i == dgt_dn) { if (dgt_dn == 4096) return; dgt_dt[i].w = (uint32_t)ra; dgt_dt[i].wr = wr; dgt_dn++; }
    dgt_dt[i].n++;
  }
  if (ra >= 0x1000 && dgt_lw) {
    int i; uintptr_t wr = dgt_lw[w];
    for (i = 0; i < dgt_xn && (dgt_xt[i].w != w || dgt_xt[i].wr != wr); i++) ;
    if (i == dgt_xn) { if (dgt_xn == 4096) return; dgt_xt[i].w = w; dgt_xt[i].wr = wr; dgt_xn++; }
    dgt_xt[i].n++;
  }
}
static void dgt_wwatch(uint32_t w, uint32_t e, uintptr_t ra, int v)   /* dev: MGS2_WWATCH=<from vb>:<addr>[,<addr>...] prints each write of
                                                                         those words from that vblank on, with the writer's host address */
{
  static long from = -2; static uint32_t ww[8]; static int nw; int i;
  if (from == -2) { const char *e2 = getenv("MGS2_WWATCH"); char *p; from = -1;
    if (e2) { from = strtol(e2, &p, 10); while (*p == ':' || *p == ',') { if (nw < 8) ww[nw++] = (strtoul(p + 1, &p, 16) & (GUEST_SIZE - 1)) >> 2; else break; } } }
  if (from < 0 || k_vblanks < from) return;
  for (i = 0; i < nw; i++) if (ww[i] >= w && ww[i] <= e) fprintf(stderr, "wwatch: vb %ld %07x by %p%s\n", k_vblanks, ww[i] << 2, (void *)ra, v ? " (pipeline)" : "");
}
/* sbg0 (task #165, G0H): the hide bits line_sph's native G0 form sets on a prim (+164 |= 0x3000: the draw pipeline then skips it) are
   the one word G0 changes that game code outside the renderer loads: the act's next turn loads it and ORs the bits (hidden, 0x333400)
   or ANDs them off (visible, 0x333434) before any use.  rt_g0h_mark (nat/native_w00a.c) marks the word; any store to it by game code or a
   copy unmarks it; a load of a marked word by line_sph's act (MGS2_READ_G0=<lo>:<hi>, host offsets from rt_dgt_rd as MGS2_READ_SKIP) is
   hashed with those two bits clear, as the translation leaves them, and unmarks it; any other load of a marked word is hashed as it is
   and reported ("g0h:" lines, the first 64). */
static uint8_t *g0h_mk; static unsigned long g0h_fn[2]; static int g0h_on = -1; static long g0h_rep, g0h_n, g0h_set;
void rt_g0h_mark(uint32_t a) { if (!g0h_mk) g0h_mk = calloc(GUEST_SIZE / 4, 1); g0h_mk[(a & (GUEST_SIZE - 1)) >> 2] = 1; g0h_set++; }
static void dgt_wr(uint32_t a, int n, uintptr_t ra)
{
  uint32_t w = (a & (GUEST_SIZE - 1)) >> 2, e = ((a & (GUEST_SIZE - 1)) + n - 1) >> 2; uint8_t v = dgt_inside();
  dgt_wwatch(w, e, ra, v);
  act_wr(a, n);
  if (v) dgt_wr_in++; else dgt_wr_out++;
  if (v && !dgt_lw) dgt_lw = calloc(GUEST_SIZE / 4, sizeof *dgt_lw);
  if (!v && !dgt_lwo) dgt_lwo = calloc(GUEST_SIZE / 4, sizeof *dgt_lwo);
  if (v && !dgt_wcall) dgt_wcall = calloc(GUEST_SIZE / 4, sizeof *dgt_wcall);
  for (; w <= e && w < GUEST_SIZE / 4; w++) { if (g0h_mk) g0h_mk[w] = 0; dgt_w[w] = v; if (v) { dgt_lw[w] = ra; dgt_wcall[w] = dgt_call; if (dgt_active()) dgt_ein[w] = 1; } else dgt_lwo[w] = ra; }
}
static void dgt_cnote(uint32_t w, uintptr_t rd)
{
  uint32_t h; uintptr_t wr = dgt_lw ? dgt_lw[w] : 0;
  if (!dgt_ct) dgt_ct = calloc(DGT_CHASH, sizeof *dgt_ct);
  dgt_xcw[w] = 1; dgt_xc_hits++;
  h = (uint32_t)((w * 2654435761u) ^ (uint32_t)(rd * 40503u) ^ (uint32_t)(wr * 97u)) & (DGT_CHASH - 1);
  while (dgt_ct[h].n && (dgt_ct[h].w != w || dgt_ct[h].rd != rd || dgt_ct[h].wr != wr)) h = (h + 1) & (DGT_CHASH - 1);
  if (!dgt_ct[h].n) { if (dgt_cn > DGT_CHASH * 3 / 4) return; dgt_ct[h].w = w; dgt_ct[h].rd = rd; dgt_ct[h].wr = wr; dgt_cn++; }
  dgt_ct[h].n++;
}
#define DGT_XC(w) (dgt_w[w] && dgt_wcall && dgt_wcall[w] != dgt_call)   /* inside: last written inside an earlier call */
void rt_dgt_wr(uint32_t a, int n) { dgt_wr(a, n, (uintptr_t)__builtin_return_address(0)); }
uint32_t rt_dgt_wrh(uint32_t a, int n) { dgt_wr(a, n, (uintptr_t)__builtin_return_address(0)); dgt_skip = 1; dgt_skip_a = a; return a; }
/* dev: MGS2_READ_HASH=1 prints per vblank "rh: vb <n> <hash> <reads>" over every guest CPU load outside the draw pipeline (the bytes it
   uses, at their address: gen_pre.h / mmi.h / vu0.h pass each load's width and offset in g_dgt_w / g_dgt_o; a partial store's merge read is
   not a load), so two runs that differ only in the renderer (full / reduced) can be checked to read the same values: a vblank whose hashes
   differ is where game code saw different data (tools/dev/rh_cmp.py).  MGS2_READ_SKIP=<lo>:<hi>[,...] (hex host offsets from rt_dgt_rd, up
   to 16 ranges) leaves out loads by that host code: the renderer's frame code that runs outside the pipeline call (packet memory pointer,
   chain list, DMA kick, buffer swap; tools/dev/rl_skip.py).
   MGS2_READ_LOG=<from vb>:<to vb>[,<from>:<to>...] also prints each of those loads in those ranges (up to 32), "rl: vb <n> <index> <address>
   <bytes> r<reader> w<writer>", the bytes as one little-endian number, reader and the first word's last writer as host offsets from
   rt_dgt_rd (wc0: no guest store since the start; wc<k>: DMA / copy code k) and " in" when that writer was the draw pipeline: diff two
   runs' rl lines (tools/dev/rl_cmp.py), map the offsets with nm. */
int g_dgt_w, g_dgt_o;
static uint64_t dgt_rh = 0xcbf29ce484222325ull, dgt_rh_n; static long dgt_rh_vb = -1, dgt_rl_r[32][2]; static int dgt_rh_on = -1, dgt_rl_n, dgt_rl_on;
static unsigned long dgt_rs[16][2]; static int dgt_rs_n;
void rt_dgt_rd(uint32_t a);
static void dgt_rh_add(uint32_t a, uintptr_t ra, int n, int o)
{
  uint8_t v[16]; int k, i; uintptr_t b = (uintptr_t)rt_dgt_rd;
  if (dgt_rh_on < 0) { const char *e = getenv("MGS2_READ_LOG"); dgt_rh_on = getenv("MGS2_READ_HASH") != 0;
                       while (e && dgt_rl_n < 32 && sscanf(e, "%ld:%ld%n", &dgt_rl_r[dgt_rl_n][0], &dgt_rl_r[dgt_rl_n][1], &k) == 2) { dgt_rl_n++; e += k; if (*e++ != ',') break; }
                       if (dgt_rl_n) dgt_rh_on = 1;
                       e = getenv("MGS2_READ_SKIP");
                       while (e && dgt_rs_n < 16 && sscanf(e, "%lx:%lx%n", &dgt_rs[dgt_rs_n][0], &dgt_rs[dgt_rs_n][1], &k) == 2) { dgt_rs_n++; e += k; if (*e++ != ',') break; } }
  if (!dgt_rh_on) return;
  if (k_vblanks != dgt_rh_vb) {
    if (dgt_rh_vb >= 0) fprintf(stderr, "rh: vb %ld %016llx %llu\n", dgt_rh_vb, (unsigned long long)dgt_rh, (unsigned long long)dgt_rh_n);
    dgt_rh_vb = k_vblanks; dgt_rh = 0xcbf29ce484222325ull; dgt_rh_n = 0;
    for (dgt_rl_on = 0, k = 0; k < dgt_rl_n; k++) if (k_vblanks >= dgt_rl_r[k][0] && k_vblanks <= dgt_rl_r[k][1]) dgt_rl_on = 1; }
  if (n < 0) return;                    /* a partial store's merge read */
  for (i = 0; i < dgt_rs_n; i++) if ((unsigned long)(ra - b) >= dgt_rs[i][0] && (unsigned long)(ra - b) < dgt_rs[i][1]) return;
  if (!n) n = 4;
  if (n > 16) n = 16;
  a += o;
  for (i = 0; i < n; i++) v[i] = g_mem[(a + i) & (GUEST_SIZE - 1)];
  if (g0h_mk) {
    uint32_t m = a & (GUEST_SIZE - 1), w;
    for (w = m >> 2; w <= ((m + n - 1) & (GUEST_SIZE - 1)) >> 2; w++) if (g0h_mk[w]) {
      if (g0h_on < 0) { const char *e2 = getenv("MGS2_READ_G0"); g0h_on = e2 && sscanf(e2, "%lx:%lx", &g0h_fn[0], &g0h_fn[1]) == 2; }
      if (g0h_on && (unsigned long)(ra - b) >= g0h_fn[0] && (unsigned long)(ra - b) < g0h_fn[1]) {
        int j = (int)((w << 2) + 1 - m);   /* bits 12-13 of the word are 0x30 in its byte 1 */
        if (j >= 0 && j < n) v[j] &= (uint8_t)~0x30u;
        g0h_mk[w] = 0; g0h_n++;
      } else if (g0h_rep++ < 64) fprintf(stderr, "g0h: vb %ld word %07x loaded by r%lx while hidden\n", k_vblanks, w << 2, (unsigned long)(ra - b));
    }
  }
  if (dgt_rl_on) {
    uint32_t w = (a & (GUEST_SIZE - 1)) >> 2; uintptr_t wr = dgt_w[w] ? (dgt_lw ? dgt_lw[w] : 0) : (dgt_lwo ? dgt_lwo[w] : 0); char hx[40], *p = hx;
    for (i = n - 1; i >= 0; i--) p += sprintf(p, "%02x", v[i]);
    if (wr >= 0x1000) fprintf(stderr, "rl: vb %ld %llu %08x %s r%lx w%lx%s\n", k_vblanks, (unsigned long long)dgt_rh_n, a, hx, (unsigned long)(ra - b), (unsigned long)(wr - b), dgt_w[w] ? " in" : "");
    else fprintf(stderr, "rl: vb %ld %llu %08x %s r%lx wc%lx%s\n", k_vblanks, (unsigned long long)dgt_rh_n, a, hx, (unsigned long)(ra - b), (unsigned long)wr, dgt_w[w] ? " in" : ""); }
  dgt_rh = (dgt_rh ^ a) * 0x100000001b3ull; dgt_rh = (dgt_rh ^ (uint64_t)n) * 0x100000001b3ull;
  for (i = 0; i < n; i++) dgt_rh = (dgt_rh ^ v[i]) * 0x100000001b3ull;
  dgt_rh_n++;
}
void rt_dgt_rd(uint32_t a)
{
  uint32_t w; int n = g_dgt_w, o = g_dgt_o;
  g_dgt_w = g_dgt_o = 0;
  if (dgt_skip) { dgt_skip = 0; if (a == dgt_skip_a) return; }
  act_rd(a);
  w = (a & (GUEST_SIZE - 1)) >> 2;
  if (!dgt_inside()) dgt_rh_add(a, (uintptr_t)__builtin_return_address(0), n, o);
  if (dgt_inside()) {
    if (!dgt_w[w] && dgt_active()) { dgt_inote(0, w >> 6, (uintptr_t)__builtin_return_address(0)); dgt_inote(1, w >> 6, dgt_lwo ? dgt_lwo[w] : 0); }
    else if (DGT_XC(w) && dgt_active()) dgt_cnote(w, (uintptr_t)__builtin_return_address(0));
    return;
  }
  if (dgt_w[w] && dgt_active()) dgt_note(w, (uintptr_t)__builtin_return_address(0));
}
void rt_dgt_dma(int ch, uint32_t a, uint32_t nq)
{
  uint32_t b, i;
  act_dma(ch, a, nq);
  if (!dgt_active()) return;
  b = (a & 0x80000000u) ? (a & 0x3ff0) : (a & (GUEST_SIZE - 1) & ~15u);
  if (dgt_inside()) { for (i = 0; i < nq * 4 && (b >> 2) + i < GUEST_SIZE / 4; i++) if (DGT_XC((b >> 2) + i)) dgt_cnote((b >> 2) + i, 0x100 + ch); return; }
  for (i = 0; i < nq * 4 && (b >> 2) + i < GUEST_SIZE / 4; i++) {
    uint32_t w = (b >> 2) + i;
    if (dgt_w[w] && dgt_wcall && dgt_wcall[w] != dgt_call && ch < 4) {   /* persistent packet data: sent after a later call than the one that wrote it */
      if (!dgt_pda) dgt_pda = calloc(GUEST_SIZE / 4, sizeof *dgt_pda);
      if (dgt_call - dgt_wcall[w] > dgt_pda[w]) dgt_pda[w] = dgt_call - dgt_wcall[w];
      dgt_pd_hits++;
    }
    if (dgt_w[w]) dgt_note(w, 0x100 + ch);
    else if (ch < 4) { dgt_dma_out[ch]++; if (dgt_lwo) dgt_inote(2 + (ch == 1 ? 0 : 1), ((b >> 2) + i) >> 6, dgt_lwo[(b >> 2) + i]); }
  }
}
void rt_dgt_copy(uint32_t dst, uint32_t src, uint32_t n)
{
  uint32_t i; uint8_t v = dgt_inside();
  act_copy(dst, src, n);
  for (i = 0; i < n / 4; i++) {
    uint32_t s = ((src & (GUEST_SIZE - 1)) >> 2) + i, d = ((dst & (GUEST_SIZE - 1)) >> 2) + i;
    if (!v && dgt_w[s] && dgt_active()) dgt_note(s, 0x200 + (dst < 0x4000 ? 9 : 8));
    if (v && DGT_XC(s) && dgt_active()) dgt_cnote(s, 0x200 + (dst < 0x4000 ? 9 : 8));
    if (v) { if (!dgt_lw) dgt_lw = calloc(GUEST_SIZE / 4, sizeof *dgt_lw);   /* a copy inside keeps the writer of the source word (packets built in the scratchpad) */
             dgt_lw[d] = dgt_w[s] && dgt_lw[s] ? dgt_lw[s] : 0x200 + (dst < 0x4000 ? 9 : 8); if (dgt_active()) dgt_ein[d] = 1;
             if (!dgt_wcall) dgt_wcall = calloc(GUEST_SIZE / 4, sizeof *dgt_wcall); dgt_wcall[d] = dgt_call; }
    else { if (!dgt_lwo) dgt_lwo = calloc(GUEST_SIZE / 4, sizeof *dgt_lwo); dgt_lwo[d] = 0x200 + (dst < 0x4000 ? 9 : 8); }
    dgt_w[d] = v; if (g0h_mk) g0h_mk[d] = 0;
  }
}
void rt_dgt_mpg(void) { dgt_prog_in = dgt_inside(); if (dgt_prog_in) dgt_mpg_in++; else dgt_mpg_out++; }
void rt_dgt_vu(uint32_t h, int pc)
{
  int i;
  if (!dgt_active()) return;
  if (dgt_inside()) { dgt_vu_in++; return; }
  for (i = 0; i < dgt_vn; i++) if (dgt_vt[i].h == h && dgt_vt[i].pc == pc && dgt_vt[i].prog_in == dgt_prog_in) break;
  if (i == dgt_vn) { if (dgt_vn == 256) return; dgt_vt[i].h = h; dgt_vt[i].pc = pc; dgt_vt[i].prog_in = dgt_prog_in; dgt_vn++; }
  dgt_vt[i].n++;
}
void rt_dgt_report(void)
{
  FILE *f; long i, marked = 0;
  act_report();
  if (dgt_from < 0 || !(f = fopen(dgt_out, "w"))) return;
  for (i = 0; i < GUEST_SIZE / 4; i++) marked += dgt_w[i];
  fprintf(f, "base rt_dgt_rd %lx\nfrom %ld to %ld\nwrites inside %llu outside %llu; words last written inside now %ld; reads of them outside %llu (%ld word/reader pairs)\n"
          "micro code uploads inside %llu outside %llu\n", (unsigned long)(uintptr_t)rt_dgt_rd, dgt_from, k_vblanks, (unsigned long long)dgt_wr_in,
          (unsigned long long)dgt_wr_out, marked, (unsigned long long)dgt_hits, dgt_n, (unsigned long long)dgt_mpg_in, (unsigned long long)dgt_mpg_out);
  fprintf(f, "micro calls inside %llu\n", (unsigned long long)dgt_vu_in);
  for (i = 0; i < 16 && dgt_ra[i]; i++) fprintf(f, "draw pipeline called from ra %06x: %llu\n", dgt_ra[i], (unsigned long long)dgt_ran[i]);
  for (i = 0; i < dgt_vn; i++) fprintf(f, "vu %08x %d %d %llu\n", dgt_vt[i].h, dgt_vt[i].pc, dgt_vt[i].prog_in, (unsigned long long)dgt_vt[i].n);
  fprintf(f, "dma reads of words not written inside: VIF0 %llu VIF1 %llu GIF %llu\n", (unsigned long long)dgt_dma_out[0], (unsigned long long)dgt_dma_out[1], (unsigned long long)dgt_dma_out[2]);
  for (int k = 0; k < 4; k++) for (i = 0; dgt_it[k] && i < DGT_IHASH; i++) if (dgt_it[k][i].n)
    fprintf(f, "%s %06x %lx %llu\n", (const char *[]){ "ir", "iw", "dv", "do" }[k], dgt_it[k][i].b * 256, (unsigned long)dgt_it[k][i].ra, (unsigned long long)dgt_it[k][i].n);
  for (i = 0; i < dgt_dn; i++) fprintf(f, "dw %x %lx %llu\n", dgt_dt[i].w, (unsigned long)dgt_dt[i].wr, (unsigned long long)dgt_dt[i].n);
  for (i = 0; i < dgt_xn; i++) fprintf(f, "wr %06x %lx %llu\n", dgt_xt[i].w * 4, (unsigned long)dgt_xt[i].wr, (unsigned long long)dgt_xt[i].n);
  for (i = 0; dgt_t && i < DGT_HASH; i++) if (dgt_t[i].n) fprintf(f, "rd %06x %lx %llu\n", dgt_t[i].w * 4, (unsigned long)dgt_t[i].ra, (unsigned long long)dgt_t[i].n);
  for (i = 0; i < GUEST_SIZE / 4; i++) if (dgt_w[i]) { long j = i; while (j < GUEST_SIZE / 4 && dgt_w[j]) j++; fprintf(f, "in %06lx %06lx\n", i * 4, j * 4); i = j; }
  /* renderer-only memory: written inside the pipeline from the first vblank on and never read after such a write by game code ("mask"),
     and what game code does read of it ("keep") */
  { long nm = 0, nk = 0;
    for (i = 0; i < GUEST_SIZE / 4; i++) if (dgt_ein[i]) { int g = dgt_gr[i]; long j = i; while (j < GUEST_SIZE / 4 && dgt_ein[j] && dgt_gr[j] == g) j++;
      fprintf(f, "%s %06lx %06lx\n", g ? "keep" : "mask", i * 4, j * 4); if (g) nk += j - i; else nm += j - i; i = j - 1; }
    fprintf(stderr, "dg trace: written inside the pipeline %ld words: renderer-only %ld, read by game code %ld\n", nm + nk, nm, nk); }
  { long nx = 0;   /* the pipeline's state across calls */
    for (i = 0; i < GUEST_SIZE / 4; i++) if (dgt_xcw[i]) { long j = i; while (j < GUEST_SIZE / 4 && dgt_xcw[j]) j++; fprintf(f, "xc %06lx %06lx\n", i * 4, j * 4); nx += j - i; i = j - 1; }
    for (i = 0; dgt_ct && i < DGT_CHASH; i++) if (dgt_ct[i].n)
      fprintf(f, "cx %06x %lx %lx %llu\n", dgt_ct[i].w * 4, (unsigned long)dgt_ct[i].wr, (unsigned long)dgt_ct[i].rd, (unsigned long long)dgt_ct[i].n);
    fprintf(stderr, "dg trace: %ld calls; %ld words read inside the pipeline that it wrote in an earlier call (%llu reads, %ld word/writer/reader triples)\n",
            (long)dgt_call, nx, (unsigned long long)dgt_xc_hits, dgt_cn); }
  { long np = 0;   /* persistent packet data: words DMA read outside the pipeline after a later call than the one that wrote them, by writer */
    for (i = 0; dgt_pda && i < GUEST_SIZE / 4; i++) if (dgt_pda[i]) {
      long j = i; uint32_t age = 0; uintptr_t wr = dgt_lw ? dgt_lw[i] : 0;
      while (j < GUEST_SIZE / 4 && dgt_pda[j] && (dgt_lw ? dgt_lw[j] : 0) == wr) { if (dgt_pda[j] > age) age = dgt_pda[j]; j++; }
      fprintf(f, "pd %06lx %06lx %lx %u\n", i * 4, j * 4, (unsigned long)wr, age); np += j - i; i = j - 1; }
    fprintf(stderr, "dg trace: %ld words DMA sent after a later call than the one that wrote them (%llu reads)\n", np, (unsigned long long)dgt_pd_hits); }
  fclose(f);
  fprintf(stderr, "dg trace: %llu reads outside the draw pipeline of words it wrote, %ld word/reader pairs (%s)\n", (unsigned long long)dgt_hits, dgt_n, dgt_out);
}
#endif
#if defined(DG_EXP) && !defined(__psp__)
/* dev (x86 only; boot_rt.o and gen_dispatch.o compiled with -DDG_EXP): the draw pipeline's call (0x11da90 from the actor loop, dispatch.h).
   MGS2_DG_EXP=t[:<from vb>:<to vb>]: what happens inside it: the virtual time its work takes (its polls, before rt_dg_call sets the time
   back), and the interrupt handlers (INTC, DMAC), alarms, vblanks, thread switches and time reads (timer COUNT, COP0 Count) that run
   inside it (on any thread while it runs); a summary at the stop, and a line per call from <from vb> to <to vb>.
   (Until 2026-10-21 MGS2_DG_ZT=<vb> turned the zero-time clock on here from a vblank and MGS2_DG_ZT_LIVE=1 left its time reads live;
   the zero-time clock is now rt_dg_call in every build, g_opt OPT_ZT.)
   MGS2_DG_SKIP=<vb>[:<mask>]: (with OPT_ZT) from that vblank on the call is skipped in the vblanks whose number has a bit of <mask> set
   (default 1: the odd ones): what the game would see if the renderer did not run there.
   MGS2_DG_LITE=<vb>:<mask>:<hex>[,<hex>...]: (with OPT_ZT; gen.o compiled with -DDG_LITE) in those calls the listed translated functions
   return at once (gen_pre.h FN_ENTER): a reduced pipeline.  Without these variables the call runs as in every other build. */
int g_dgx_in; long g_dgx_cnt[8]; extern uint64_t g_zt_t1; int rt_dg_call(uint32_t a);
int g_dgx_lite; static uint32_t dgx_lite_fn[64]; static long dgx_lite_skip[64], dgx_lite_calls, dgx_lfrom = -1, dgx_lmask = 1; static int dgx_lite_n;
int rt_dg_lite(uint32_t a) { for (int i = 0; i < dgx_lite_n; i++) if (dgx_lite_fn[i] == a) { dgx_lite_skip[i]++; return 1; } return 0; }
static const char *const dgx_kind[8] = { "intc", "dmac", "alarm", "vblank", "switch", "timer", "count", "-" };
static long dgx_calls, dgx_tot[8], dgx_with[8], dgx_from = -1, dgx_to = -1, dgx_sfrom = -1, dgx_smask = 1, dgx_skipped; static uint64_t dgx_dt, dgx_dtmax, dgx_dtmin = ~0ull, dgx_slack = ~0ull;
static int dgx_mode = -1;
static void dgx_report(void)
{
  int k;
  if (dgx_skipped) fprintf(stderr, "dgx: %ld calls skipped\n", dgx_skipped);
  if (dgx_lite_calls) { fprintf(stderr, "dgx: %ld reduced calls;", dgx_lite_calls); for (int i = 0; i < dgx_lite_n; i++) fprintf(stderr, " %06x %ld", dgx_lite_fn[i], dgx_lite_skip[i]); fputc('\n', stderr); }
  if (!dgx_calls) return;
  fprintf(stderr, "dgx: %ld calls of the draw pipeline; virtual time inside: mean %.0f polls, min %llu, max %llu; least time left to the next vblank at "
          "the exit %llu polls\n", dgx_calls, (double)dgx_dt / dgx_calls / K_POLL_CYCLES, (unsigned long long)(dgx_dtmin / K_POLL_CYCLES),
          (unsigned long long)(dgx_dtmax / K_POLL_CYCLES), (unsigned long long)(dgx_slack / K_POLL_CYCLES));
  for (k = 0; k < 7; k++) fprintf(stderr, "dgx: inside: %-6s %ld in %ld calls\n", dgx_kind[k], dgx_tot[k], dgx_with[k]);
}
int dg_exp_call(uint32_t a)
{
  uint64_t t0, t1; long c0[8]; int k, ok, zt;
  if (dgx_mode < 0) {
    const char *e = getenv("MGS2_DG_EXP"); dgx_mode = e ? e[0] : 0;
    if (e && e[1] == ':' && sscanf(e + 2, "%ld:%ld", &dgx_from, &dgx_to) != 2) dgx_from = dgx_to = -1;
    if (getenv("MGS2_DG_SKIP")) { if (!dgx_mode) dgx_mode = 's'; if (sscanf(getenv("MGS2_DG_SKIP"), "%ld:%li", &dgx_sfrom, &dgx_smask) < 1) dgx_sfrom = -1; }
    if (getenv("MGS2_DG_LITE")) { const char *e = getenv("MGS2_DG_LITE"); int k = 0; if (!dgx_mode) dgx_mode = 'l';
      if (sscanf(e, "%ld:%li%n", &dgx_lfrom, &dgx_lmask, &k) < 2) dgx_lfrom = -1;
      else for (const char *p = e + k; (*p == ':' || *p == ',') && dgx_lite_n < 64; ) dgx_lite_fn[dgx_lite_n++] = (uint32_t)strtoul(p + 1, (char **)&p, 16); }
    if (dgx_mode) atexit(dgx_report);
  }
  if (!dgx_mode || g_dgx_in) return rt_dg_call(a);
  zt = (g_opt & OPT_ZT) != 0;
  if (zt && dgx_sfrom >= 0 && k_vblanks >= dgx_sfrom && (k_vblanks & dgx_smask)) { dgx_skipped++; return 1; }
  t0 = K_NOW(); memcpy(c0, g_dgx_cnt, sizeof c0);
  g_dgx_in = 1;
  if (zt && dgx_lfrom >= 0 && k_vblanks >= dgx_lfrom && (k_vblanks & dgx_lmask)) { g_dgx_lite = 1; dgx_lite_calls++; }
  ok = rt_dg_call(a);
  g_dgx_lite = 0;
  t1 = zt ? g_zt_t1 : K_NOW();
  g_dgx_in = 0;
  dgx_calls++; dgx_dt += t1 - t0; if (t1 - t0 > dgx_dtmax) dgx_dtmax = t1 - t0; if (t1 - t0 < dgx_dtmin) dgx_dtmin = t1 - t0;
  if (k_next_vblank > t1 && k_next_vblank - t1 < dgx_slack) dgx_slack = k_next_vblank - t1;
  for (k = 0; k < 7; k++) { long d = g_dgx_cnt[k] - c0[k]; dgx_tot[k] += d; if (d) dgx_with[k]++; }
  if (k_vblanks >= dgx_from && k_vblanks <= dgx_to) {
    fprintf(stderr, "dgx: vb %ld t0 %llu polls %llu to vblank %lld", k_vblanks, (unsigned long long)t0, (unsigned long long)((t1 - t0) / K_POLL_CYCLES),
            (long long)((int64_t)(k_next_vblank - t1) / K_POLL_CYCLES));
    for (k = 0; k < 7; k++) if (g_dgx_cnt[k] - c0[k]) fprintf(stderr, " %s %ld", dgx_kind[k], g_dgx_cnt[k] - c0[k]);
    fputc('\n', stderr);
  }
  return ok;
}
#endif
void rt_missing(uint32_t a) { fprintf(stderr, "missing function %06x (left out of the reduced build) at vb %ld\n", a, k_vblanks); exit(7); }
void rt_st8(uint32_t a, uint32_t v) { uint8_t x = v; rt_store(a, &x, 1); }
void rt_st16(uint32_t a, uint32_t v) { uint16_t x = v; rt_store(a, &x, 2); }
void rt_st32(uint32_t a, uint32_t v) { rt_store(a, &v, 4); }
void rt_st64(uint32_t a, uint64_t v) { rt_store(a, &v, 8); }
void rt_hww32(uint32_t a, uint32_t v) { HWW32(a, v); if (g_io_arm) { g_io_pend = g_io_arm; g_io_arm = 0; } if (g_io_pend) io_flush(); }   /* the write takes effect at once */
void rt_syscall(uint32_t pc) { SYSCALL(pc); }
void rt_break(uint32_t pc) { BREAK(pc); }
uint32_t rt_cop0r(int r, int s) { return COP0R(r, s); }
void rt_cop0w(int r, int s, uint32_t v) { COP0W(r, s, v); }
void rt_cop0op(int f, uint32_t pc) { COP0OP(f, pc); }
#else
#include GEN
#include "dispatch.h"
#endif
int main(int argc, char **argv)
{
#if !defined(__psp__)
  if (getenv("MGS2_FPU_RZ")) _mm_setcsr(_mm_getcsr() | 0x6000u | 0x8000u);   /* the PSP's FCR31 0x01000001 (lessons A9): round toward zero, flush denormal results, no DAZ */
  else if (getenv("MGS2_FPU_RN")) _mm_setcsr(_mm_getcsr() | 0x8000u);   /* dev measurement only: round to nearest (the VFPU's rounding), same flush: how far the game drifts from the reference when every float op rounds the other way */
#endif
  FILE *f = fopen(argc > 1 ? argv[1] : "SLES_503.83", "rb");
  if (argc > 2) max_vblanks = atol(argv[2]);
  if (!f) { perror("elf"); return 1; }
  fseek(f, 0x80, SEEK_SET); if (fread(g_mem + 0x100000, 1, 0xb08b8, f) != 0xb08b8) return 1; fclose(f);
  io_init(); atexit(io_dump);
  g_vu_old_fcget = getenv("MGS2_OLD_FCGET") != 0;   /* dev: FCGET as before 2026-10-19 (status bits instead of the clip flags; the compiled programs, vupipe.py, only have the fixed form) */
  if (!getenv("MGS2_NO_VF00")) vu_reset();   /* VF00 = (0, 0, 0, 1.0), hardwired on the VU; without it every vf00w read (macro MOVEs, the micro interpreter) saw 0.  dev MGS2_NO_VF00: the old state */
  if (getenv("MGS2_SIF_LOG")) sif_log = atoi(getenv("MGS2_SIF_LOG"));   /* sif.h: 2 every packet (default), 1 commands only, 0 none (the PSP timing runs) */
  iop_host_init(getenv("MGS2_DISC") ? getenv("MGS2_DISC") : "disc");
  if (getenv("BOOT_VERBOSE")) verbose_from = atol(getenv("BOOT_VERBOSE"));
#if !defined(__psp__)
  if (getenv("BOOT_ALARM")) alarm(atoi(getenv("BOOT_ALARM")));   /* dev: stop under gdb after N seconds to see where it runs */
#endif
  if (getenv("MGS2_VUM_TEST")) {        /* dev: which macro-op words (hex, comma separated) the VU0 core counts as unsupported */
    const char *e = getenv("MGS2_VUM_TEST"); static VuSnap sv; vu_snap(&sv);
    while (*e) { char *n; uint32_t w = strtoul(e, &n, 16); int u = g_unsup_vu; VUM(w); fprintf(stderr, "vum %08x: %s\n", w, g_unsup_vu != u ? "UNSUPPORTED" : "ok"); vu_restore(&sv); e = *n ? n + 1 : n; }
    return 0; }
  fn_trace_init(); gen_start(); fprintf(stderr, "returned from the entry\n"); return 0;
}
#ifdef MGS2_GEN_VU_TAIL   /* recomp/vupipe.py VP_KEEP: micro programs compiled after the PGO profile, defined after every other function so those keep their profile */
#include MGS2_GEN_VU_TAIL
#endif
#if defined(__psp__) && !defined(QCOPY_NO_VFPU)
/* v18: a quadword copy between guest buffers that share no 64-byte line (rt_idiom_qcopy, ee_io.h scratchpad DMA).  The same bytes land in
   the same places; only the cache traffic changes: each destination line the copy fills completely is first claimed with `cache 0x18`
   (create dirty exclusive: the line is allocated without reading memory, since all 64 bytes are about to be written), so the copy reads only
   its source.  Only once main_psp.c's cde_selftest has passed on this machine (g_cde_ok); otherwise 64 bytes at a time as before.  Defined
   here, after every other function of the unit, so those keep their PGO records. */
uint32_t g_cde_ok;
void rt_qcopy_nov(uint8_t *d, const uint8_t *s, uint32_t n)   /* n a multiple of 16; d, s 16-byte aligned */
{
  uint8_t *e = d + n;
  if (g_cde_ok && (uintptr_t)d >= 0x08000000u) {   /* never claim a line outside main RAM (the scratchpad is not cached) */
    for (; d < e && ((uintptr_t)d & 63); d += 16, s += 16) __asm__ volatile ("lv.q C000, 0(%0)\n\tsv.q C000, 0(%1)" :: "r"(s), "r"(d) : "memory");
    for (; e - d >= 64; d += 64, s += 64)
      __asm__ volatile ("cache 0x18, 0(%1)\n\tlv.q C000, 0(%0)\n\tlv.q C010, 16(%0)\n\tlv.q C020, 32(%0)\n\tlv.q C030, 48(%0)\n\t"
                        "sv.q C000, 0(%1)\n\tsv.q C010, 16(%1)\n\tsv.q C020, 32(%1)\n\tsv.q C030, 48(%1)" :: "r"(s), "r"(d) : "memory");
  }
  for (; e - d >= 64; d += 64, s += 64)
    __asm__ volatile ("lv.q C000, 0(%0)\n\tlv.q C010, 16(%0)\n\tlv.q C020, 32(%0)\n\tlv.q C030, 48(%0)\n\t"
                      "sv.q C000, 0(%1)\n\tsv.q C010, 16(%1)\n\tsv.q C020, 32(%1)\n\tsv.q C030, 48(%1)" :: "r"(s), "r"(d) : "memory");
  for (; d < e; d += 16, s += 16) __asm__ volatile ("lv.q C000, 0(%0)\n\tsv.q C000, 0(%1)" :: "r"(s), "r"(d) : "memory");
}
#endif
#if defined(__psp__) && defined(GUEST_SPRSTATE)
/* v18: psp/spr_state.ld places the machine state in the PSP scratchpad by hand.  The number of faults (main_psp.c prints it): an object
   outside 0x00010000-0x00013fff, one not aligned to its size (16 at most), or two that overlap. */
int rt_spr_check(void)
{
#define SPRO(x) { (uintptr_t)&(x), sizeof (x) }
  const struct { uintptr_t a; uint32_t n; } o[] = {
    SPRO(VF), SPRO(VN), SPRO(R), SPRO(RH), SPRO(F), SPRO(HI), SPRO(LO), SPRO(HI1), SPRO(LO1), SPRO(ACC), SPRO(CC), SPRO(SAR), SPRO(k_left),
    SPRO(k_tbase), SPRO(k_poll_at), SPRO(VIr), SPRO(VACC), SPRO(VQr), SPRO(VPr), SPRO(VIreg), SPRO(VRr), SPRO(VMAC), SPRO(VSTAT), SPRO(VCLIP),
    SPRO(VCMSAR0), SPRO(g_vmac), SPRO(g_io_arm), SPRO(g_io_pend), SPRO(g_vu_ext), SPRO(g_vgrp_fb), SPRO(g_fn_hook), SPRO(vu_prog_clean),
    SPRO(vu_prog_hash), SPRO(vu_steps), SPRO(vu_calls), SPRO(vu_trace), SPRO(vu_tpc), SPRO(g_unsup_vu), SPRO(g_vu_old_fcget), SPRO(rt_idiom_n),
    SPRO(g_hash_ee), SPRO(g_hash_iop), SPRO(VUMEM), SPRO(VUPROG) };
#undef SPRO
  int bad = 0; unsigned i, j, n = sizeof o / sizeof o[0];
  for (i = 0; i < n; i++) {
    if (o[i].a < 0x00010000u || o[i].a + o[i].n > 0x00014000u || (o[i].a & ((o[i].n >= 16 ? 16 : o[i].n >= 8 ? 8 : 4) - 1))) bad++;
    for (j = i + 1; j < n; j++) if (o[i].a < o[j].a + o[j].n && o[j].a < o[i].a + o[i].n) bad++;
  }
  return bad;
}
#endif
/* dev, defined after every other function so the PGO profile keeps its function numbers (state_hash's own profile no longer matches:
   the PSP build passes -Wno-error=coverage-mismatch for this unit).  MGS2_HASH_DENSE=<from>:<to>: a hash at every vblank of that range as
   well.  MGS2_HASH_REF=<file>: lines "<vb> <ee hash> <vfb>" (the x86 reference's hash lines); every hash taken is compared with the
   reference's for its vblank, and the first difference in ee or vfb prints "split: ..." with the VU0 registers and writes EE RAM to
   <MGS2_HASH_REF_DUMP>.<vb> (default "split").  In the dense range each hash line is followed by "vuh: vb <n> <hash>", FNV-1a 32 of VF,
   ACC, VI, Q, I, CLIP and micro memory (VU0 state the RAM hash does not see), and a reference line's fourth field is that hash. */
int hash_dense(void)
{
  static long a = -2, b;
  if (a == -2) { const char *e = getenv("MGS2_HASH_DENSE"); a = -1; if (e && sscanf(e, "%ld:%ld", &a, &b) != 2) a = -1; }
  return a >= 0 && (long)k_vblanks >= a && (long)k_vblanks <= b;
}
static uint32_t fnv32(const void *p, size_t n, uint32_t h) { const uint8_t *q = p; size_t i; for (i = 0; i < n; i++) { h ^= q[i]; h *= 16777619u; } return h; }
void hash_ref(uint64_t ee)
{
  static int st = -1, nr, vuh_split; static long *rv; static uint64_t *re; static uint32_t *rf, *ru; static char pre[200] = "split";
  extern uint32_t g_vgrp_fb; uint32_t vh = 0; int i;
  if (hash_dense()) {
    uint32_t vi[16]; for (i = 0; i < 16; i++) vi[i] = VIr[i] & 0xffff;
    vh = fnv32(VF, sizeof VF, 2166136261u); vh = fnv32(VACC, sizeof VACC, vh); vh = fnv32(vi, sizeof vi, vh);
    vh = fnv32(&VQr, 4, vh); vh = fnv32(&VIreg, 4, vh); vh = fnv32(&VCLIP, 4, vh); vh = fnv32(VUMEM, sizeof VUMEM, vh);
    fprintf(stderr, "vuh: vb %ld %08x\n", (long)k_vblanks, vh);
  }
  if (st < 0) {
    const char *e = getenv("MGS2_HASH_REF"), *d = getenv("MGS2_HASH_REF_DUMP"); FILE *f; char ln[256]; st = 0;
    if (!e || !(f = fopen(e, "r"))) { if (e) fprintf(stderr, "split: cannot read %s\n", e); return; }
    if (d) snprintf(pre, sizeof pre, "%s", d);
    rv = malloc(8192 * sizeof *rv); re = malloc(8192 * sizeof *re); rf = malloc(8192 * 4); ru = malloc(8192 * 4);
    while (nr < 8192 && fgets(ln, sizeof ln, f)) { unsigned long long h; unsigned long fb = 0, u = 0; long v; int k = sscanf(ln, "%ld %llx %lu %lx", &v, &h, &fb, &u);
      if (k >= 2) { rv[nr] = v; re[nr] = h; rf[nr] = k >= 3 ? (uint32_t)fb : 0xffffffffu; ru[nr] = k >= 4 ? (uint32_t)u : 0; nr++; } }
    fclose(f); st = 1; fprintf(stderr, "split: %d reference hashes from %s\n", nr, e);
  }
  if (st != 1) return;
  for (i = 0; i < nr && rv[i] != (long)k_vblanks; i++) {}
  if (i == nr) return;
  if (ru[i] && hash_dense() && vh != ru[i] && !vuh_split) { vuh_split = 1; fprintf(stderr, "split: VU0 state first differs at vb %ld (%08x, reference %08x)\n", (long)k_vblanks, vh, ru[i]); }
  if (re[i] == ee && (rf[i] == 0xffffffffu || rf[i] == g_vgrp_fb)) return;
  { char fn[260]; FILE *f; int r;
    fprintf(stderr, "split: first difference at vb %ld: ee %016llx (reference %016llx), vfb %u (reference %u)\n", (long)k_vblanks, (unsigned long long)ee,
            (unsigned long long)re[i], g_vgrp_fb, rf[i]);
    for (r = 0; r < 32; r++) fprintf(stderr, "split: VF%d %08x %08x %08x %08x\n", r, VF[r].u[0], VF[r].u[1], VF[r].u[2], VF[r].u[3]);
    fprintf(stderr, "split: ACC %08x %08x %08x %08x Q %08x I %08x CLIP %06x MAC %04x VI", VACC[0], VACC[1], VACC[2], VACC[3], VQr, VIreg, VCLIP, VMAC);
    for (r = 1; r < 16; r++) fprintf(stderr, " %04x", VIr[r] & 0xffff);
    fputc('\n', stderr);
    snprintf(fn, sizeof fn, "%s.%ld", pre, (long)k_vblanks);
    if ((f = fopen(fn, "wb"))) { fwrite(g_mem, 1, GUEST_SIZE, f); fclose(f); fprintf(stderr, "split: EE RAM written to %s\n", fn); }
    else fprintf(stderr, "split: cannot write %s\n", fn);
    st = 2; }
}

/* dev: MGS2_GS_MASK=<file>: a game-state hash ("gs") of EE RAM (the scratchpad at 0..0x3fff with GUEST_SPR0) without the ranges of the
   file's "mask <from> <to>" lines (hex byte offsets, ascending, from the GEN_DG_TRACE report: memory only the renderer reads).  Defined
   after every other function so the PGO profile keeps its function numbers. */
static uint32_t (*gs_rng)[2];
static void gs_load(void)
{
  const char *fn = getenv("MGS2_GS_MASK"); FILE *f; char ln[128]; uint32_t a, b; int cap = 0;
  gs_n = 0; if (!fn || !(f = fopen(fn, "r"))) return;
  while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "mask %x %x", &a, &b) == 2 && a < b && b <= GUEST_SIZE && (!gs_n || a >= gs_rng[gs_n - 1][1])) {
    if (gs_n == cap) { cap = cap ? 2 * cap : 1024; gs_rng = realloc(gs_rng, cap * sizeof *gs_rng); }
    gs_rng[gs_n][0] = a; gs_rng[gs_n][1] = b; gs_n++; }
  fclose(f); fprintf(stderr, "gs mask: %d ranges from %s\n", gs_n, fn);
}
/* sbg0 (task #165, G0): gs_mask_dyn(a, n) adds [a, a + n) to the mask for the rest of the run (x86 dev; boot/native_w00a.c calls it for
   every line_sph prim it sees, G0 on or off, so two runs of the same game mask the same ranges at the same times: the prim's vertex
   buffers A and B and its word +164) */
int gs_dn; static uint32_t (*gs_dyn)[2]; static int gs_dcap; static uint32_t *gs_dset; static int gs_dsetn;
void gs_mask_dyn(uint32_t a, uint32_t n)
{
  uint32_t m = a & (GUEST_SIZE - 1), e = m + n > GUEST_SIZE ? GUEST_SIZE : m + n, k, i;
  if (!n) return;
  if (gs_dn * 2 >= gs_dsetn) {                    /* the set of ranges by start and end: open addressing, rebuilt at half full */
    int nn = gs_dsetn ? 2 * gs_dsetn : 4096; uint32_t *ns = calloc(2 * (size_t)nn, 4);
    for (int j = 0; j < gs_dn; j++) { k = (gs_dyn[j][0] * 0x9e3779b1u ^ gs_dyn[j][1]) & (nn - 1); while (ns[2 * k + 1]) k = (k + 1) & (nn - 1);
      ns[2 * k] = gs_dyn[j][0]; ns[2 * k + 1] = gs_dyn[j][1]; }
    free(gs_dset); gs_dset = ns; gs_dsetn = nn; }
  for (k = (m * 0x9e3779b1u ^ e) & (gs_dsetn - 1); gs_dset[2 * k + 1]; k = (k + 1) & (gs_dsetn - 1)) if (gs_dset[2 * k] == m && gs_dset[2 * k + 1] == e) return;
  gs_dset[2 * k] = m; gs_dset[2 * k + 1] = e;
  if (gs_dn == gs_dcap) { gs_dcap = gs_dcap ? 2 * gs_dcap : 1024; gs_dyn = realloc(gs_dyn, gs_dcap * sizeof *gs_dyn); }
  i = gs_dn++; gs_dyn[i][0] = m; gs_dyn[i][1] = e;
}
static int gs_cmp(const void *a, const void *b) { const uint32_t *x = a, *y = b; return x[0] < y[0] ? -1 : x[0] > y[0]; }
static uint64_t gs_hash(void)
{
  uint64_t h = 0xcbf29ce484222325ull; uint32_t p = 0; int i, n = gs_n;
  uint32_t (*r)[2] = gs_rng;
  if (gs_dn) {                                     /* the file's ranges and the added ones, sorted and joined */
    r = malloc((gs_n + gs_dn) * sizeof *r); memcpy(r, gs_rng, gs_n * sizeof *r); memcpy(r + gs_n, gs_dyn, gs_dn * sizeof *r);
    qsort(r, gs_n + gs_dn, sizeof *r, gs_cmp); n = 0;
    for (i = 0; i < gs_n + gs_dn; i++) { if (n && r[i][0] <= r[n - 1][1]) { if (r[i][1] > r[n - 1][1]) r[n - 1][1] = r[i][1]; } else { r[n][0] = r[i][0]; r[n][1] = r[i][1]; n++; } }
  }
  for (i = 0; i < n; i++) { h = fnv64(g_mem + p, r[i][0] - p, h); p = r[i][1]; }
  h = fnv64(g_mem + p, GUEST_SIZE - p, h);
  { static const char *dn = (const char *)1; static long dv = -1; char fn[300]; FILE *f;   /* dev: MGS2_GS_DYN_OUT=<prefix>:<vb> writes the added ranges */
    if (dn == (const char *)1) { static char pre[256]; dn = 0; const char *e = getenv("MGS2_GS_DYN_OUT"); if (e && sscanf(e, "%255[^:]:%ld", pre, &dv) == 2) dn = pre; }
    if (dn && k_vblanks == dv) { snprintf(fn, sizeof fn, "%s.%ld", dn, k_vblanks);
      if ((f = fopen(fn, "w"))) { for (i = 0; i < gs_dn; i++) fprintf(f, "dyn %06x %06x\n", gs_dyn[i][0], gs_dyn[i][1]); fclose(f); } } }
  if (r != gs_rng) free(r);
  return h;
}

/* ZT (Ffm 2026-10-21, option A; speed_notes §31): the call of the PS2 draw pipeline (0x11da90, dispatch.h) takes no game time.  Inside
   it no timed event is due (kernel.h k_poll_arm / k_poll_rest), a guest time read sees its entry time (ee_io.h K_READ), and its exit sets
   the time back, so the game state does not depend on how much work the renderer does.  Interrupts it raises itself (DMA, GS) are still
   taken at its polls.  g_opt without OPT_ZT: the clock before v20.  Defined after every other function (PGO function numbers). */
#if defined(DG_EXP) && !defined(__psp__)
uint64_t g_zt_t1;                               /* dev (dg_exp_call): the time at its exit before it is set back */
#define ZT_T1() ((void)(g_zt_t1 = K_NOW()))
#define DGC_DEV g_dgx_lite                      /* dev: a call reduced by MGS2_DG_LITE */
#else
#define ZT_T1() ((void)0)
#define DGC_DEV 0
#endif
/* The reduced pipeline (speed_notes §33): at most one shown step per two vblanks (25 fps), so with OPT_LITE a call in the vblank after a
   shown one runs with g_dg_lite set, and the packet builders of boot/dg_lite.h return at once (gen_pre.h GEN_LITE).  The GS draws the
   last call before a vblank, so every call in a shown vblank is full (16 vblanks of the deck route have two calls; v20 reduced the
   second one, and the shown frame with it). */
int g_dg_lite; static long dg_shown_vb = -1000000;
uint32_t g_dg_calls[2];                         /* calls, full / reduced (the PSP harness counts them per step and clears them) */
int rt_dg_call(uint32_t a)
{
  uint64_t t0; int ok;
  if (g_zt || !(g_opt & OPT_ZT)) { if (!g_zt) g_dg_calls[0]++; return guest_call(a); }
  t0 = K_NOW(); g_zt_t0 = t0; g_zt = 1; k_poll_arm();
  if ((g_opt & OPT_LITE) && k_vblanks != dg_shown_vb && k_vblanks - dg_shown_vb < 2) g_dg_lite = 1; else dg_shown_vb = k_vblanks;
  g_dg_calls[g_dg_lite]++;
#ifndef __psp__
  if (g_pkt_on) fprintf(stderr, "dgc: vb %ld %d\n", k_vblanks, g_dg_lite | DGC_DEV);   /* dev (MGS2_PKT_HASH window): each call, 1 = reduced (tools/dev/lite_cmp.py) */
#endif
  ok = guest_call(a);
  g_dg_lite = 0;
  ZT_T1(); g_zt = 0; K_SET(t0); k_poll_arm();
  return ok;
}
#if defined(DG_LITE_DYN) && !defined(__psp__)
/* dev (x86, every object compiled with -DGEN_LITE -DDG_LITE_DYN; boot/dg_lite.h): on a reduced call (rt_dg_call's rule) the translated
   functions of MGS2_LITE_FN=<hex>[,<hex>...] return at entry (unset: 0x1214a0 alone, the default build's set); the number of returns per
   function is printed at the exit */
static uint32_t lite_fn[64]; static long lite_fn_n = -1, lite_fn_skip[64];
static void lite_fn_report(void)
{ fprintf(stderr, "lite: reduced calls %lu, returns at entry:", (unsigned long)g_dg_calls[1]); for (long i = 0; i < lite_fn_n; i++) fprintf(stderr, " %06x %ld", lite_fn[i], lite_fn_skip[i]); fputc('\n', stderr); }
int rt_lite_fn(uint32_t a)
{
  long i;
  if (lite_fn_n < 0) {
    const char *e = getenv("MGS2_LITE_FN"), *p = e; lite_fn_n = 0;
    if (!e) lite_fn[lite_fn_n++] = 0x1214a0u;
    else while (*p && lite_fn_n < 64) { char *q; uint32_t v = (uint32_t)strtoul(p, &q, 16); if (q == p) break; lite_fn[lite_fn_n++] = v; p = *q == ',' ? q + 1 : q; }
    atexit(lite_fn_report);
  }
  for (i = 0; i < lite_fn_n; i++) if (lite_fn[i] == a) { lite_fn_skip[i]++; return 1; }
  return 0;
}
#endif
/* State hash 2 (2026-10-26; psp_harness.md "State hash 2"): the "hash:" lines' ee and iop from v31 on.  The FNV-1a 64 before it (fnv64,
   a byte at a time through a 64-bit multiply) took about 1.9 s per hash on the PSP, about 14 minutes of a 40-minute run.  This one reads
   the same memory in the same order a 32-bit word at a time, in four lanes: lane j takes words j, j + 4, j + 8, ... as
   h = rotl(h ^ w, 13) * P[j], a bijection of the lane for each word, so a lane that differs stays different unless a later word cancels
   the difference exactly; two fmix32 chains over the four lanes give the 64 bits.  EE: with GUEST_SPR0 RAM 0..0x3fff (the scratchpad's
   place there) as zeros, then the rest of RAM, then the scratchpad, the same words as a build with a separate scratchpad; IOP: its RAM.
   The first hash of a run also prints "state hash 2: ..." with the time of both hashes and, once, of the FNV-1a 64 pair; MGS2_HASH_FNV=1
   (dev) prints that pair at every hash as "hash1: vb N ee X iop Y", to check a run against the references made before v31. */
#ifdef __psp__
long long sceKernelGetSystemTimeWide(void);
static uint64_t sh2_us(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }
#else
#include <time.h>
static uint64_t sh2_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u; }
#endif
typedef uint32_t __attribute__((may_alias)) sh2_u32;
static const uint8_t sh2_zero[0x4000] __attribute__((aligned(16)));
static uint32_t sh2_rotl(uint32_t x, int r) { return x << r | x >> (32 - r); }
static void __attribute__((noinline)) sh2_run(uint32_t s[4], const void *p, size_t n)   /* n bytes, a multiple of 16, p 4-aligned */
{
  const sh2_u32 *q = p, *e = q + n / 4; uint32_t a = s[0], b = s[1], c = s[2], d = s[3];
  for (; q < e; q += 4) {
    a = sh2_rotl(a ^ q[0], 13) * 0x9e3779b1u; b = sh2_rotl(b ^ q[1], 13) * 0x85ebca77u;
    c = sh2_rotl(c ^ q[2], 13) * 0xc2b2ae3du; d = sh2_rotl(d ^ q[3], 13) * 0x27d4eb2fu;
  }
  s[0] = a; s[1] = b; s[2] = c; s[3] = d;
}
static uint32_t sh2_fmix(uint32_t h) { h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; return h ^ h >> 16; }
static uint64_t sh2_end(const uint32_t s[4])
{
  uint32_t lo = sh2_fmix(s[0] ^ sh2_fmix(s[1] ^ sh2_fmix(s[2] ^ sh2_fmix(s[3])))),
           hi = sh2_fmix(s[3] ^ sh2_fmix(s[2] ^ sh2_fmix(s[1] ^ sh2_fmix(s[0] ^ 0x5bd1e995u))));
  return (uint64_t)hi << 32 | lo;
}
static void sh2_fnv(uint64_t *ee, uint64_t *iop)   /* the FNV-1a 64 pair of the references before v31 (state_hash's code until then) */
{
#ifdef GUEST_SPR0                        /* the same hash as with a separate scratchpad (RAM 0..0x3fff is zero there) */
  *ee = fnv64(g_spr, 0x4000, fnv64(g_mem + 0x4000, GUEST_SIZE - 0x4000, fnv64(sh2_zero, 0x4000, 0xcbf29ce484222325ull)));
#else
  *ee = fnv64(g_spr, 0x4000, fnv64(g_mem, GUEST_SIZE, 0xcbf29ce484222325ull));
#endif
  *iop = fnv64(iop_rt_mem(g_iop), iop_rt_mem_size(g_iop), 0xcbf29ce484222325ull);
}
void state_hash2(uint64_t *ee, uint64_t *iop)
{
  static int first = 1, fnv = -1; uint32_t s[4]; uint64_t t0 = 0, t1 = 0, t2 = 0; size_t n = iop_rt_mem_size(g_iop);
  if (fnv < 0) fnv = getenv("MGS2_HASH_FNV") != 0;
  if (first) t0 = sh2_us();
  s[0] = 0x243f6a88u; s[1] = 0x85a308d3u; s[2] = 0x13198a2eu; s[3] = 0x03707344u;
#ifdef GUEST_SPR0
  sh2_run(s, sh2_zero, 0x4000); sh2_run(s, g_mem + 0x4000, GUEST_SIZE - 0x4000);
#else
  sh2_run(s, g_mem, GUEST_SIZE);
#endif
  sh2_run(s, g_spr, 0x4000); *ee = sh2_end(s);
  if (first) t1 = sh2_us();
  s[0] = 0xa4093822u; s[1] = 0x299f31d0u; s[2] = 0x082efa98u; s[3] = 0xec4e6c89u;
  sh2_run(s, iop_rt_mem(g_iop), n & ~(size_t)15);
  if (n & 15) { uint32_t t[4] = { 0 }; memcpy(t, iop_rt_mem(g_iop) + (n & ~(size_t)15), n & 15); sh2_run(s, t, 16); }
  *iop = sh2_end(s);
  if (first) t2 = sh2_us();
  if (first || fnv) {
    uint64_t fe, fi, t3 = sh2_us(), t4; sh2_fnv(&fe, &fi); t4 = sh2_us();
    if (fnv) fprintf(stderr, "hash1: vb %ld ee %016llx iop %016llx\n", (long)k_vblanks, (unsigned long long)fe, (unsigned long long)fi);
    if (first) fprintf(stderr, "state hash 2: EE RAM and scratchpad (%u KB) in %.1f ms, IOP RAM (%u KB) in %.1f ms; the FNV-1a 64 pair of before in %.1f ms\n",
                       (unsigned)((GUEST_SIZE + 0x4000) >> 10), (t1 - t0) / 1000.0, (unsigned)(n >> 10), (t2 - t1) / 1000.0, (t4 - t3) / 1000.0);
  }
  first = 0;
}
