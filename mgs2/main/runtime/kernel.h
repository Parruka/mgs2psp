/* EE kernel HLE for the recompiled MGS2 (x86 development host first).
   Guest threads are host coroutines (ucontext): each has its own host stack and its own copy of the guest register file, which is
   swapped in and out of the globals (R, RH, HI, LO, F, ...) at a switch.  Scheduling follows the EE kernel: strict priority (lower
   number first), FIFO within a priority, no time slicing; a switch happens only at a kernel call or after an interrupt.
   Interrupts (INTC causes, DMAC channels) and alarms are delivered on virtual time: when no thread is ready, time jumps to the next
   event.  Syscall numbers: ps2sdk ee/kernel/include/syscallnr.h; thread / semaphore structures: ps2sdk ee/kernel/include/kernel.h.
   Needs guest.h and mmi.h (SAR) first.  The including program provides k_log (or defines K_LOG_OFF). */
#ifndef KERNEL_H
#define KERNEL_H
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

#define K_THREADS 256
#define K_SEMAS 256
#define K_HANDLERS 128
#define K_ALARMS 64
#ifndef K_HSTACK
#define K_HSTACK (1 << 20)                       /* host stack per guest thread */
#endif
#ifdef HOST_STACK_POOL                           /* PSP: the guest and IOP threads' host stacks are carved from the main thread's own stack
                                                    (psp/main_psp.c; a stack outside the thread's block powered the console off, sandstone_review.md 2.8) */
void *host_stack_alloc(size_t n);
#define HOST_STACK_ALLOC(n) host_stack_alloc(n)
#else
#define HOST_STACK_ALLOC(n) malloc(n)
#endif
/* dev (MGS2_STACK_MARK): host stacks are filled with 0xa5 when made, and stack_report() gives the deepest use of each (the PSP sizes) */
#define STK_MARK_KEEP 512                        /* the top bytes left unfilled (the host's makecontext may write there) */
static void stk_mark(void *p, size_t n) { if (getenv("MGS2_STACK_MARK")) memset(p, 0xa5, n - STK_MARK_KEEP); }
static size_t stk_used(const void *p, size_t n) { const uint8_t *b = (const uint8_t *)p; size_t i = 0; while (i < n - STK_MARK_KEEP && b[i] == 0xa5) i++; return n - i; }
#define K_ISTACK 0x00080000u                     /* guest stack for interrupt handlers: top of the kernel area (unused by the game) */
#define K_EXIT_RA 0x00000100u                    /* return address given to a thread entry: returning from it is ExitThread */
#define EE_HZ 294912000u
#define K_VBLANK_CYCLES (EE_HZ / 50)             /* PAL (SYSTEM.CNF VMODE=PAL) */

typedef struct { int64_t R[32], RH[32], HI, LO, HI1, LO1; float F[32], ACC; int CC; uint32_t SAR; } KCtx;
enum { THS_RUN = 1, THS_READY = 2, THS_WAIT = 4, THS_SUSPEND = 8, THS_WAITSUSPEND = 12, THS_DORMANT = 16 };
enum { TSW_NONE, TSW_SLEEP, TSW_SEMA };
typedef struct {
  int used, status, prio, init_prio, wait_type, wait_id, wakeups, suspends;
  uint32_t entry, stack, ssize, gp, attr, option, arg;
  uint64_t qseq;                                  /* order in the ready queue of its priority */
  int fresh;                                      /* the host context must be (re)built at the next switch to it */
  KCtx c; ucontext_t uc; void *hstack;
} KThread;
typedef struct { int used, count, max, init, attr, waiters; uint32_t option; } KSema;
typedef struct { int used, cause, enabled, dmac; uint32_t fn, arg; } KHandler;
typedef struct { int used; uint64_t when; uint32_t fn, arg; } KAlarm;

static KThread k_th[K_THREADS]; static KSema k_sem[K_SEMAS]; static KHandler k_hd[K_HANDLERS]; static KAlarm k_al[K_ALARMS];
static int k_cur = 1, k_in_intr, k_need_resched, k_dispatch_off;
static uint64_t k_qseq, k_next_vblank = K_VBLANK_CYCLES;
/* the virtual time (K_NOW) and k_poll_at, below which a poll only advances time (k_poll_arm; 0 = the next poll runs k_poll_rest): ee_io.h */
#define K_POLL_KICK() (k_poll_at = 0, K_SYNC())            /* after any change that can give a poll something to do (pending interrupt, ready thread, ...) */
static uint64_t k_due = K_VBLANK_CYCLES;         /* no alarm or vblank is due before this time (at most the earliest one): k_poll skips the scan below it */
static uint32_t k_intc_mask, k_dmac_mask, k_heap_end;
static long k_vblanks;
#ifdef K_HANDLER_LOG   /* x86 dev builds only (recomp/lean_plan.md 1b): every interrupt handler / alarm function the game registers */
#define K_HLOG(kind, fn) fprintf(stderr, "khandler: %s %08x vb %ld\n", kind, (unsigned)(fn), k_vblanks)
#else
#define K_HLOG(kind, fn) ((void)0)
#endif
static ucontext_t k_sched_uc;

static void k_save(KCtx *c) { memcpy(c->R, R, sizeof R); memcpy(c->RH, RH, sizeof RH); c->HI = HI; c->LO = LO; c->HI1 = HI1; c->LO1 = LO1; memcpy(c->F, F, sizeof F); c->ACC = ACC; c->CC = CC; c->SAR = SAR; }
static void k_load(const KCtx *c) { memcpy(R, c->R, sizeof R); memcpy(RH, c->RH, sizeof RH); HI = c->HI; LO = c->LO; HI1 = c->HI1; LO1 = c->LO1; memcpy(F, c->F, sizeof F); ACC = c->ACC; CC = c->CC; SAR = c->SAR; }

static void k_fatal(const char *what, int v) { fprintf(stderr, "kernel: %s %d (thread %d, ra %08x)\n", what, v, k_cur, (uint32_t)R[31]); exit(5); }
static void k_ready(int t) { k_th[t].status = THS_READY; k_th[t].qseq = ++k_qseq; k_need_resched = 1; K_POLL_KICK(); }
static int k_th_hi = 2;                         /* threads 1 .. k_th_hi - 1 have been created (k_pick scans no further: same result, the
                                                   unused rest of the 256 entries are never touched; each was a D-cache miss on the PSP) */
static int k_pick(void)
{
  int b = 0, t;
  for (t = 1; t < k_th_hi; t++)
    if (k_th[t].used && (k_th[t].status == THS_READY || k_th[t].status == THS_RUN) &&
        (!b || k_th[t].prio < k_th[b].prio || (k_th[t].prio == k_th[b].prio && k_th[t].qseq < k_th[b].qseq))) b = t;
  return b;
}

static int guest_call(uint32_t a);              /* the host: a translated function at guest address a (ELF or the loaded stage), 1 if called */
static void k_exit_thread(void);
static void k_thread_main(void)
{
  KThread *th = &k_th[k_cur];
  memset(&th->c, 0, sizeof th->c); k_load(&th->c);
  R[4] = (int32_t)th->arg; R[28] = (int32_t)th->gp; R[29] = (int32_t)(th->stack + th->ssize - 0x10); R[31] = K_EXIT_RA;
  if (!guest_call(th->entry)) k_fatal("thread entry not translated", (int)th->entry);
  k_exit_thread();
}

static void k_deliver_pending(void);
/* switch to the thread that should run (called at kernel calls and after interrupts, never inside an interrupt handler) */
static void k_resched(void)
{
  for (;;) {
    int n;
    k_need_resched = 0;
    if (k_dispatch_off && k_th[k_cur].status == THS_RUN) return;
    n = k_pick();
    if (!n) { k_deliver_pending(); continue; }    /* idle: advance virtual time to the next event */
    if (n == k_cur) { k_th[n].status = THS_RUN; return; }
    {
      KThread *from = &k_th[k_cur], *to = &k_th[n];
      DGX_NOTE(4);
      if (from->status == THS_RUN) { from->status = THS_READY; from->qseq = 0; }   /* preempted: stays first among its priority (assumed, to check) */
      k_save(&from->c);
      k_cur = n; to->status = THS_RUN;
      if (to->fresh) {
        to->fresh = 0; getcontext(&to->uc);
        if (!to->hstack) { to->hstack = HOST_STACK_ALLOC(K_HSTACK); stk_mark(to->hstack, K_HSTACK); }
        to->uc.uc_stack.ss_sp = to->hstack; to->uc.uc_stack.ss_size = K_HSTACK; to->uc.uc_link = &k_sched_uc;
        makecontext(&to->uc, k_thread_main, 0);
      } else k_load(&to->c);
      swapcontext(&from->uc, &to->uc);
      /* back on this thread */
      k_load(&k_th[k_cur].c);
      return;
    }
  }
}
/* the current thread leaves the RUN state (wait / sleep / exit) and the next one runs */
static void k_block(int st) { k_th[k_cur].status = st; k_resched(); }

static void k_exit_thread(void)
{
  KThread *th = &k_th[k_cur];
  th->status = THS_DORMANT; th->fresh = 1; th->prio = th->init_prio; th->wakeups = 0;
  { int n = k_pick(); KThread *to; if (!n) { k_deliver_pending(); n = k_pick(); } if (!n) k_fatal("no thread after exit", k_cur);
    to = &k_th[n]; k_cur = n; to->status = THS_RUN;
    if (to->fresh) { to->fresh = 0; getcontext(&to->uc); if (!to->hstack) { to->hstack = HOST_STACK_ALLOC(K_HSTACK); stk_mark(to->hstack, K_HSTACK); }
      to->uc.uc_stack.ss_sp = to->hstack; to->uc.uc_stack.ss_size = K_HSTACK; to->uc.uc_link = &k_sched_uc; makecontext(&to->uc, k_thread_main, 0); }
    else k_load(&to->c);
    setcontext(&to->uc); }
}

/* ---- interrupts ---- */
/* A handler or an alarm starts in a cleared register file, as a thread does (k_thread_main), then gets a0-a2, sp and ra: no value of
   the interrupted thread reaches it, so a poll stores no register for it (recomp/lean_plan.md 1b, Ffm's choice 2026-10-24; regcache.py
   RC_POLL_R= RC_POLL_F=).  The handlers' prologues save zeros into the interrupt stack where builds up to v26 saved the thread's values
   (hash references from v27 on: prof/hashes_x86_*_v27.txt).  K_HKEEP: the thread's registers, as up to v26.  K_HPOISON (dev check): a
   pattern in every register but zero, to show that no handler uses the thread's values (only those saves change). */
static void k_handler_regs(void)
{
#if defined(K_HPOISON)
  static KCtx z; static int i0; int i; union { uint32_t u; float f; } c;
  if (!i0) { i0 = 1; for (i = 0; i < 32; i++) { if (i) { z.R[i] = (int32_t)(0xdead0000u | (uint32_t)i << 8); z.RH[i] = (int32_t)(0xbeef0000u | (uint32_t)i << 8); }
      c.u = 0x7e000000u | (uint32_t)i << 8; z.F[i] = c.f; }
    z.HI = (int32_t)0xdead4100u; z.LO = (int32_t)0xdead4200u; z.HI1 = (int32_t)0xdead4300u; z.LO1 = (int32_t)0xdead4400u;
    c.u = 0x7e004500u; z.ACC = c.f; z.CC = 1; z.SAR = 0x2a; }
  k_load(&z);
#elif !defined(K_HKEEP)
  static const KCtx z; k_load(&z);
#endif
}
static void k_call_handler(KHandler *h, int cause)
{
  KCtx save; DGX_NOTE(h->dmac ? 1 : 0); k_save(&save); k_in_intr++; k_handler_regs();
  R[4] = cause; R[5] = (int32_t)h->arg; R[6] = 0; R[29] = (int32_t)(K_ISTACK - 0x100 * k_in_intr); R[31] = K_EXIT_RA;
  if (!guest_call(h->fn)) k_fatal("handler not translated", (int)h->fn);
  k_in_intr--; K_POLL_KICK(); k_load(&save);
}
static void k_intc(int cause)
{
  int i;
  if (!(k_intc_mask & (1u << cause))) return;
  for (i = 0; i < K_HANDLERS; i++) if (k_hd[i].used && !k_hd[i].dmac && k_hd[i].cause == cause && k_hd[i].enabled) k_call_handler(&k_hd[i], cause);
}
static void k_dmac(int ch)
{
  int i;
  if (!(k_dmac_mask & (1u << ch))) return;
  for (i = 0; i < K_HANDLERS; i++) if (k_hd[i].used && k_hd[i].dmac && k_hd[i].cause == ch && k_hd[i].enabled) k_call_handler(&k_hd[i], ch);
}
static uint32_t k_status = 0x70030c13;           /* COP0 Status of a user thread: EIE (bit 16) follows ei / di (initial value to check) */
static uint32_t k_dmac_pend, k_intc_pend;
static void k_dmac_raise(int ch) { k_dmac_pend |= 1u << ch; K_POLL_KICK(); }
static void k_intc_raise(int cause) { k_intc_pend |= 1u << cause; K_POLL_KICK(); }   /* an INTC interrupt raised by the hardware model (GS FINISH ...) */
/* pending DMAC interrupts, taken at a kernel call or at idle when the thread has interrupts enabled */
static void k_take_interrupts(void)
{
  int ch;
  if (k_in_intr || !(k_status & 0x10000u) || !(k_dmac_pend | k_intc_pend)) return;
  for (ch = 0; ch < 16; ch++) if (k_intc_pend & (1u << ch)) { k_intc_pend &= ~(1u << ch); k_intc(ch); }
  for (ch = 0; ch < 10; ch++) if (k_dmac_pend & (1u << ch)) { k_dmac_pend &= ~(1u << ch); k_dmac(ch); }
}
static void k_vblank_hook(int start);            /* the including program: GS CSR bits, frame output */
/* idle: jump to the next timed event (vblank start / end, alarm) and deliver it */
static void k_alarm(int al)
{
  KCtx save; uint32_t fn = k_al[al].fn, arg = k_al[al].arg;
  k_al[al].used = 0; DGX_NOTE(2); k_save(&save); k_in_intr++; k_handler_regs();
  R[4] = al; R[5] = (int32_t)(K_NOW() & 0xffff); R[6] = (int32_t)arg; R[29] = (int32_t)(K_ISTACK - 0x100 * k_in_intr); R[31] = K_EXIT_RA;
  if (!guest_call(fn)) k_fatal("alarm handler not translated", (int)fn);
  k_in_intr--; K_POLL_KICK(); k_load(&save);
}
/* the timed events due at k_time: alarms, then the vblank */
static void k_events_due(void)
{
  int i;
  for (i = 0; i < K_ALARMS; i++) if (k_al[i].used && k_al[i].when <= K_NOW()) k_alarm(i);
  if (K_NOW() >= k_next_vblank) {
    k_vblanks++; k_next_vblank += K_VBLANK_CYCLES; DGX_NOTE(3);
    k_vblank_hook(1); k_intc(2);                 /* INTC 2: VBLANK start */
    k_vblank_hook(0); k_intc(3);                 /* INTC 3: VBLANK end (delivered at once: no guest work runs inside the blank yet) */
  }
  k_due = k_next_vblank;                         /* the earliest pending event (SetAlarm lowers it) */
  for (i = 0; i < K_ALARMS; i++) if (k_al[i].used && k_al[i].when < k_due) k_due = k_al[i].when;
  if (k_due < k_poll_at) { k_poll_at = k_due; K_SYNC(); }
}
/* idle (no thread ready): the kernel's idle loop runs with interrupts on; time jumps to the next event */
static void k_deliver_pending(void)
{
  uint64_t t = k_next_vblank; int i, al = -1;
  if (k_dmac_pend | k_intc_pend) { uint32_t st = k_status; k_status |= 0x10000u; k_take_interrupts(); k_status = st; K_POLL_KICK(); return; }
  for (i = 0; i < K_ALARMS; i++) if (k_al[i].used && k_al[i].when < t) { t = k_al[i].when; al = i; }
  if (al < 0 && !(k_intc_mask & 0xc)) k_fatal("deadlock: no thread ready, no alarm, vblank interrupts off", 0);
  if (t > K_NOW()) K_SET(t);
  k_events_due();
}
/* a loop back-edge of the game code (IRQ_POLL): time passes; with interrupts enabled, pending and due interrupts are taken there and a
   thread they made ready preempts.  K_POLL_CYCLES: the virtual time of one loop iteration */
#ifndef K_POLL_CYCLES
#define K_POLL_CYCLES 64
#endif
/* k_poll_at: the time from which k_poll_rest can have work.  Interrupts off or inside a handler: none until they are enabled / it ends
   (those kick); an interrupt pending, a thread switch wanted or the events checked at every poll (OPT_POLL_DUE off): the next poll; else
   the next timed event (k_due).  Every change that can bring work earlier kicks (K_POLL_KICK) or lowers it (k_events_due, SetAlarm). */
static void k_poll_arm(void)
{
  if (k_in_intr || !(k_status & 0x10000u)) k_poll_at = ~(uint64_t)0;
  else if ((k_dmac_pend | k_intc_pend) || (k_need_resched && !k_dispatch_off) || !(g_opt & OPT_POLL_DUE)) k_poll_at = 0;
  else if (DGX_FROZEN()) k_poll_at = ~(uint64_t)0;   /* ZT (ee_io.h): no timed event inside the draw pipeline's call */
  else k_poll_at = k_due;
  K_SYNC();
}
static void k_poll_rest(void)                    /* the poll after its time step (the generated units call it as rt_poll_slow from k_poll_at) */
{
  if (!k_in_intr && (k_status & 0x10000u)) {
    if (k_dmac_pend | k_intc_pend) k_take_interrupts();
    if (!DGX_FROZEN() && (K_NOW() >= k_due || !(g_opt & OPT_POLL_DUE))) k_events_due();        /* below k_due no alarm and no vblank is due: k_events_due would do nothing */
    if (k_need_resched && !k_dispatch_off) k_resched();
  }
  k_poll_arm();
}
static void k_poll(void) { K_ADV(K_POLL_CYCLES); k_poll_rest(); }

/* ---- syscalls ---- */
static int k_new(void *arr, int n, size_t sz) { int i; for (i = 1; i < n; i++) if (!*(int *)((char *)arr + i * sz)) return i; return -1; }
#define KW(a) RD32(a)
static int k_syscall(int32_t n)
{
  uint32_t a0 = R[4], a1 = R[5], a2 = R[6], a3 = R[7]; int i;
  int nn = n < 0 ? -n : n;
  switch (n) {
  case 0x3c:                                     /* SetupThread(gp, stack, size, args, root): the main thread, returns its stack top */
    { KThread *th = &k_th[1]; uint32_t top = (a1 == 0xffffffffu ? 0x2000000u : a1 + a2);
      memset(th, 0, sizeof *th); th->used = 1; th->status = THS_RUN; th->prio = th->init_prio = 0; th->gp = a0; th->stack = top - a2; th->ssize = a2;
      k_cur = 1; R[2] = (int32_t)top; return 1; }
  case 0x3d: k_heap_end = a1 == 0xffffffffu ? 0x2000000u : a0 + a1; R[2] = (int32_t)a0; return 1;   /* SetupHeap(start, size) */
  case 0x3e: R[2] = (int32_t)k_heap_end; return 1;                                                  /* EndOfHeap */
  case 0x64: case -0x68: case 0x61: case 0x62: return 1;                                             /* FlushCache, iFlushCache, EnableCache, DisableCache */
  case 0x7f: R[2] = 0x2000000; return 1;                                                             /* GetMemorySize */
  /* libkernel's start-up kernel patch (0x174490: SetSyscall(0x5a, copy), Copy() of 0x740 bytes to 0x80076000, then SetSyscall of the alarm
     calls 0xfc..0xff to that code): the HLE implements those calls itself, so the patch is copied into the unused kernel area and not used */
  case 0x74: R[2] = 0; return 1;                                                                     /* SetSyscall */
  case 0x5a: for (i = 0; i < (int)(a2 >> 2); i++) WR32(a0 + 4 * i, RD32(a1 + 4 * i)); R[2] = 0; return 1;   /* Copy (as set by the game: word copy) */
  case 0x5b: R[2] = 0; return 1;                                                                     /* GetEntryAddress */
  /* threads */
  case 0x20:                                     /* CreateThread(ee_thread_t *) */
    if ((i = k_new(k_th, K_THREADS, sizeof *k_th)) < 0) { R[2] = -1; return 1; }
    if (i >= k_th_hi) k_th_hi = i + 1;
    { KThread *th = &k_th[i]; void *hs = th->hstack; memset(th, 0, sizeof *th); th->hstack = hs; th->used = 1; th->status = THS_DORMANT; th->fresh = 1;
      th->entry = KW(a0 + 4); th->stack = KW(a0 + 8); th->ssize = KW(a0 + 12); th->gp = KW(a0 + 16); th->prio = th->init_prio = KW(a0 + 20);
      th->attr = KW(a0 + 28); th->option = KW(a0 + 32); }
    R[2] = i; return 1;
  case 0x21: if (a0 < K_THREADS && k_th[a0].used && k_th[a0].status == THS_DORMANT) { k_th[a0].used = 0; R[2] = a0; } else R[2] = -1; return 1;   /* DeleteThread */
  case 0x22:                                     /* StartThread(id, arg) */
    if (a0 >= K_THREADS || !k_th[a0].used || k_th[a0].status != THS_DORMANT) { R[2] = -1; return 1; }
    k_th[a0].arg = a1; k_th[a0].fresh = 1; k_ready(a0); R[2] = a0;
    if (!k_in_intr) k_resched();
    return 1;
  case 0x23: k_exit_thread(); return 1;                                                              /* ExitThread */
  case 0x24: k_th[k_cur].used = 0; k_exit_thread(); return 1;                                        /* ExitDeleteThread */
  case 0x25: case -0x26:                                                                             /* (i)TerminateThread */
    if (a0 >= K_THREADS || !k_th[a0].used || a0 == (uint32_t)k_cur) { R[2] = -1; return 1; }
    k_th[a0].status = THS_DORMANT; k_th[a0].fresh = 1; k_th[a0].prio = k_th[a0].init_prio; R[2] = a0; return 1;
  case 0x27: k_dispatch_off = 1; R[2] = k_cur; return 1;                                             /* DisableDispatchThread */
  case 0x28: k_dispatch_off = 0; K_POLL_KICK(); R[2] = k_cur; if (!k_in_intr) k_resched(); return 1;                /* EnableDispatchThread */
  case 0x29: case -0x2a:                                                                             /* (i)ChangeThreadPriority(id, prio) */
    { uint32_t t = a0 ? a0 : (uint32_t)k_cur; if (t >= K_THREADS || !k_th[t].used) { R[2] = -1; return 1; }
      R[2] = k_th[t].prio; k_th[t].prio = a1; if (k_th[t].status == THS_READY) k_th[t].qseq = ++k_qseq; k_need_resched = 1; K_POLL_KICK();
      if (!k_in_intr) k_resched(); return 1; }
  case 0x2b: case -0x2c:                                                                             /* (i)RotateThreadReadyQueue(prio) */
    for (i = 1; i < K_THREADS; i++) if (k_th[i].used && (k_th[i].status == THS_READY || k_th[i].status == THS_RUN) && k_th[i].prio == (int)a0) k_th[i].qseq = ++k_qseq;
    k_need_resched = 1; K_POLL_KICK(); R[2] = a0; if (!k_in_intr) k_resched(); return 1;
  case 0x2d: case -0x2e:                                                                             /* (i)ReleaseWaitThread(id) */
    if (a0 >= K_THREADS || !k_th[a0].used || !(k_th[a0].status & THS_WAIT)) { R[2] = -1; return 1; }
    if (k_th[a0].wait_type == TSW_SEMA) k_sem[k_th[a0].wait_id].waiters--;
    k_th[a0].wait_type = TSW_NONE; if (k_th[a0].status == THS_WAITSUSPEND) k_th[a0].status = THS_SUSPEND; else k_ready(a0);
    R[2] = a0; if (!k_in_intr) k_resched(); return 1;
  case 0x2f: case -0x2f: R[2] = k_cur; return 1;                                                     /* GetThreadId */
  case 0x30: case -0x31:                                                                             /* (i)ReferThreadStatus(id, ee_thread_status_t *) */
    { uint32_t t = a0 ? a0 : (uint32_t)k_cur; KThread *th; if (t >= K_THREADS) { R[2] = -1; return 1; }
      /* a free slot (never created, or after ExitDeleteThread / DeleteThread) answers status 0: d00t's loop at 0x204140 waits for 0
         after its loader thread's ExitDeleteThread.  Inferred from that loop, not checked against the retail kernel. */
      if (!k_th[t].used) { if (a1) { int k; for (k = 0; k < 48; k += 4) WR32(a1 + k, 0); } R[2] = 0; return 1; }
      th = &k_th[t];
      if (a1) { WR32(a1, th->status); WR32(a1 + 4, th->entry); WR32(a1 + 8, th->stack); WR32(a1 + 12, th->ssize); WR32(a1 + 16, th->gp);
        WR32(a1 + 20, th->init_prio); WR32(a1 + 24, th->prio); WR32(a1 + 28, th->attr); WR32(a1 + 32, th->option); WR32(a1 + 36, th->wait_type);
        WR32(a1 + 40, th->wait_id); WR32(a1 + 44, th->wakeups); }
      R[2] = th->status; return 1; }
  case 0x32:                                                                                         /* SleepThread */
    if (k_th[k_cur].wakeups) { k_th[k_cur].wakeups--; R[2] = k_cur; return 1; }
    k_th[k_cur].wait_type = TSW_SLEEP; k_block(THS_WAIT); R[2] = k_cur; return 1;
  case 0x33: case -0x34:                                                                             /* (i)WakeupThread(id) */
    if (a0 >= K_THREADS || !k_th[a0].used || (a0 == (uint32_t)k_cur && !k_in_intr)) { R[2] = -1; return 1; }
    if ((k_th[a0].status & THS_WAIT) && k_th[a0].wait_type == TSW_SLEEP) {
      k_th[a0].wait_type = TSW_NONE; if (k_th[a0].status == THS_WAITSUSPEND) k_th[a0].status = THS_SUSPEND; else k_ready(a0); }
    else k_th[a0].wakeups++;
    R[2] = a0; if (!k_in_intr) k_resched(); return 1;
  case 0x35: case -0x36: { uint32_t t = a0 ? a0 : (uint32_t)k_cur; R[2] = k_th[t].wakeups; k_th[t].wakeups = 0; return 1; }   /* (i)CancelWakeupThread */
  case 0x37: case -0x38:                                                                             /* (i)SuspendThread */
    if (a0 >= K_THREADS || !k_th[a0].used) { R[2] = -1; return 1; }
    if (k_th[a0].status == THS_WAIT) k_th[a0].status = THS_WAITSUSPEND; else if (k_th[a0].status == THS_READY || k_th[a0].status == THS_RUN) k_th[a0].status = THS_SUSPEND;
    k_th[a0].suspends++; R[2] = a0; if (a0 == (uint32_t)k_cur && !k_in_intr) k_resched(); return 1;
  case 0x39: case -0x3a:                                                                             /* (i)ResumeThread */
    if (a0 >= K_THREADS || !k_th[a0].used) { R[2] = -1; return 1; }
    if (k_th[a0].status == THS_SUSPEND) k_ready(a0); else if (k_th[a0].status == THS_WAITSUSPEND) k_th[a0].status = THS_WAIT;
    R[2] = a0; if (!k_in_intr) k_resched(); return 1;
  /* semaphores: ee_sema_t { count, max_count, init_count, wait_threads, attr, option } */
  case 0x40:                                                                                         /* CreateSema */
    if ((i = k_new(k_sem, K_SEMAS, sizeof *k_sem)) < 0) { R[2] = -1; return 1; }
    memset(&k_sem[i], 0, sizeof k_sem[i]); k_sem[i].used = 1; k_sem[i].max = KW(a0 + 4); k_sem[i].count = k_sem[i].init = KW(a0 + 8);
    k_sem[i].attr = KW(a0 + 16); k_sem[i].option = KW(a0 + 20); R[2] = i; return 1;
  case 0x41: case -0x49: if (a0 < K_SEMAS && k_sem[a0].used) { k_sem[a0].used = 0; R[2] = a0; } else R[2] = -1; return 1;   /* (i)DeleteSema */
  case 0x42: case -0x43:                                                                             /* (i)SignalSema: wakes the first waiter (priority order) */
    if (a0 >= K_SEMAS || !k_sem[a0].used) { R[2] = -1; return 1; }
    if (k_sem[a0].waiters) {
      int b = 0; for (i = 1; i < K_THREADS; i++) if (k_th[i].used && (k_th[i].status & THS_WAIT) && k_th[i].wait_type == TSW_SEMA && k_th[i].wait_id == (int)a0 &&
                                                       (!b || k_th[i].qseq < k_th[b].qseq)) b = i;
      k_sem[a0].waiters--; k_th[b].wait_type = TSW_NONE;
      if (k_th[b].status == THS_WAITSUSPEND) k_th[b].status = THS_SUSPEND; else k_ready(b);
    } else k_sem[a0].count++;
    R[2] = a0; if (!k_in_intr) k_resched(); return 1;
  case 0x44:                                                                                         /* WaitSema */
    if (a0 >= K_SEMAS || !k_sem[a0].used) { R[2] = -1; return 1; }
    if (k_sem[a0].count > 0) { k_sem[a0].count--; R[2] = a0; return 1; }
    k_sem[a0].waiters++; k_th[k_cur].wait_type = TSW_SEMA; k_th[k_cur].wait_id = a0; k_th[k_cur].qseq = ++k_qseq;   /* qseq: FIFO of the waiters */
    k_block(THS_WAIT); R[2] = a0; return 1;
  case 0x45: case -0x46:                                                                             /* (i)PollSema */
    if (a0 >= K_SEMAS || !k_sem[a0].used || k_sem[a0].count <= 0) { R[2] = -1; return 1; }
    k_sem[a0].count--; R[2] = a0; return 1;
  case 0x47: case -0x48:                                                                             /* (i)ReferSemaStatus */
    if (a0 >= K_SEMAS || !k_sem[a0].used) { R[2] = -1; return 1; }
    WR32(a1, k_sem[a0].count); WR32(a1 + 4, k_sem[a0].max); WR32(a1 + 8, k_sem[a0].init); WR32(a1 + 12, k_sem[a0].waiters);
    WR32(a1 + 16, k_sem[a0].attr); WR32(a1 + 20, k_sem[a0].option); R[2] = a0; return 1;
  /* interrupts */
  case 0x10: case 0x12:                                                                              /* AddIntcHandler / AddDmacHandler(cause, fn, next, arg) */
    if ((i = k_new(k_hd, K_HANDLERS, sizeof *k_hd)) < 0) { R[2] = -1; return 1; }
    k_hd[i].used = 1; k_hd[i].dmac = n == 0x12; k_hd[i].cause = a0; k_hd[i].fn = a1; k_hd[i].arg = a3; k_hd[i].enabled = 1; R[2] = i; K_HLOG(n == 0x12 ? "dmac" : "intc", a1); return 1;   /* added enabled: the game calls no Enable...Handler after adding (log) */
  case 0x11: case 0x13: if (a1 < K_HANDLERS) k_hd[a1].used = 0; R[2] = 0; return 1;                 /* Remove...Handler(cause, id) */
  case 0x5c: case -0x5c: case 0x5e: case -0x5e: if (a0 < K_HANDLERS) k_hd[a0].enabled = 1; R[2] = 0; return 1;   /* (i)Enable{Intc,Dmac}Handler */
  case 0x5d: case -0x5d: case 0x5f: case -0x5f: if (a0 < K_HANDLERS) k_hd[a0].enabled = 0; R[2] = 0; return 1;   /* (i)Disable{Intc,Dmac}Handler */
  case 0x14: case -0x1a: R[2] = !(k_intc_mask & (1u << a0)); k_intc_mask |= 1u << a0; return 1;    /* (i)EnableIntc */
  case 0x15: case -0x1b: R[2] = !!(k_intc_mask & (1u << a0)); k_intc_mask &= ~(1u << a0); return 1; /* (i)DisableIntc */
  case 0x16: case -0x1c: R[2] = !(k_dmac_mask & (1u << a0)); k_dmac_mask |= 1u << a0; return 1;    /* (i)EnableDmac */
  case 0x17: case -0x1d: R[2] = !!(k_dmac_mask & (1u << a0)); k_dmac_mask &= ~(1u << a0); return 1; /* (i)DisableDmac */
  /* alarms: SetAlarm(hsync ticks, fn, arg); PAL: 15625 Hz line rate */
  /* alarms as the game's libkernel numbers them (wrappers at 0x173560..0x1735d8): SetAlarm 0xfc, ReleaseAlarm 0xfd (called with the id
     after the timed wait at 0x15d9d0), iSetAlarm -0xfe, iReleaseAlarm -0xff */
  case 0xfc: case -0xfe: case 0x18: case -0x1e:
    if ((i = k_new(k_al, K_ALARMS, sizeof *k_al)) < 0) { R[2] = -1; return 1; }
    k_al[i].used = 1; k_al[i].when = K_READ() + (uint64_t)(a0 & 0xffff) * (EE_HZ / 15625); k_al[i].fn = a1; k_al[i].arg = a2; R[2] = i; K_HLOG("alarm", a1);
    if (k_al[i].when < k_due) k_due = k_al[i].when; if (k_due < k_poll_at) { k_poll_at = k_due; K_SYNC(); } return 1;
  case 0xfd: case -0xff: case 0x19: case -0x1f: if (a0 < K_ALARMS && k_al[a0].used) { k_al[a0].used = 0; R[2] = a0; } else R[2] = -1; return 1;
  }
  (void)a3; (void)nn;
  return 0;
}
#endif
