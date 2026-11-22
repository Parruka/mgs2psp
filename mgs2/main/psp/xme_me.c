/* xme_me.c (task #180 step 4d-4, me_step4_notes.md 4d): the Media Engine's side of the picture loop (ge/xme.c): its entry, its waits and
   its cache steps.  Built with the ME-safe picture objects (-fno-jump-tables -DMGS2_ME_SAFE; its .data and .bss, none, renamed) and run
   only on the ME, at the kseg0 alias of its addresses.  It holds the instructions the picture code may not (tools/dev/me14/mecheck.py
   rule 4), as hle_me.h's ME-side helpers do: Count reads, cache operations, sync, HALT, the FPU rule (ctc1) and $gp's top bit; its
   own check is in the build script (mkpsp18f.sh).  Here the ME writes nothing cached outside its stack: its entry and waits touch the
   control block and the loop's shared words through the uncached alias only (a wait comes between entries, or inside one while the
   ME holds its turn), so none of its writes lands in memory the main CPU owns at the time.  gl28 (MGS2_ME_REST): the rest's CP0 moves,
   eret and HALT with interrupts live in psp/xme_rest.S (its own object, checked by tools/xme_rest_chk.py); this object only calls it
   (psp.sh: ge_xme_me.o names xme_rest, xme_rest_halt, xme_rest_init and xme_run) and reads the ME's boot words 0xBFC00434-0x43C and
   Sony's request words 0xBFC00600-0x714 (reads only). */
#include <stdint.h>
#include "xme_me.h"

#define XME_ME_5US 1657u    /* 5 us of the ME's Count (331.310 MHz, probe 2 [C]) */

static inline void me_spin(uint32_t ticks)   /* gl19: at most `ticks` rounds as well.  A round takes several cycles, so with Count running
                                                the Count limit always ends it first; a stopped Count can no longer hold the ME for good
                                                (gl18me on Ffm's PSP: Count never started, the first wait never ended, heartbeat dead) */
{
  uint32_t t0 = hle_me_count();
  for (uint32_t k = 0; k < ticks && hle_me_count() - t0 < ticks; k++) ;
}

static void __attribute__((noreturn)) me_leave(volatile XmeMeCtl *c)   /* out of the loop: the whole data cache written back and dropped,
                                                                           state 2 (hle_me_stop waits for it), halted for good */
{
  hle_me_dwbinv_all();
  c->h.state = 2; hle_me_sync();
  hle_me_halt();
}

/* gl28 (MGS2_ME_REST=1; power_design.md section 5 "R1", power_critique.md section B): in the main CPU's turns the queue stays empty
   and the ME rests in HALT (psp/xme_rest.S) until the main CPU's interrupt (hle_me_kick, 0xBC100044) at the switch to the ME's turn.
   v30me5's handshake (psp/me_mix_me.c me_rest_once, 76,548 rests and 0 lost wakes on Ffm's PSP): the ME counts the rest (sleeps),
   writes asleep, syncs, then looks at the head, the rest bit and the exit request once more and halts only if nothing changed (else the
   rest is given up); the main CPU clears the rest bit or raises the head, syncs, then reads asleep and interrupts the ME once per rest
   (by sleeps).  Each side syncs between its write and its read, so at least one sees the other's: the ME never halts through an ME
   turn.  After a wake the words xme_rest.S's handler left in the ME's boot memory ([13] wakes, [14] flags seen, [15] the wake's EPC)
   and Sony's ME request words (reads only) go to the control block.  Count around the rest gives its ticks (0 when Count stops in
   HALT); every wait is uncached words only, as before.  The last flag word ([12]) has no room in the control block: a flag that never
   clears (the handler gives up after 4,096 reads) shows as "before HALT" about equal to the rests in a turn. */
#define XMW(i) (((volatile uint32_t *)0xBFC00400u)[i])     /* xme_rest.S's words ([12..15]) */
#define XRPC(o) (*(volatile uint32_t *)(0xBFC00000u + (o)))   /* Sony's ME request words (uOFW me_wrapper) */
static void xme_rest_once(volatile XmeMeCtl *c, volatile XmeSh *s, uint32_t t)
{
  uint32_t w0 = XMW(13);
  c->sleeps = c->sleeps + 1;
  hle_me_sync();
  c->asleep = 1;
  hle_me_sync();
  if (s->head == t && (c->rest & XME_REST_OK) && !c->h.exit_req) {
    uint32_t c0 = hle_me_count(), w;
    xme_rest();
    c->t_rest = c->t_rest + (hle_me_count() - c0);
    if ((w = XMW(13)) != w0) {        /* the handler ran */
      uint32_t e = XMW(15), hl = (uint32_t)(uintptr_t)xme_rest_halt & 0x7FFFFFFFu;
      if (e == hl || e == hl + 4u) c->halted = c->halted + 1; else c->pending = c->pending + 1;
      c->wakes = w; c->flags_or = XMW(14);
      c->rpc[0] = XRPC(0x600); c->rpc[1] = XRPC(0x608); c->rpc[2] = XRPC(0x700); c->rpc[3] = XRPC(0x710); c->rpc[4] = XRPC(0x714);
    } else c->nowake = c->nowake + 1;   /* the HALT returned with no interrupt taken */
  } else c->late = c->late + 1;
  c->asleep = 0;
  hle_me_sync();
}

void xme_me_wait(XmeCtx *x, int why)
{
  volatile XmeSh *s = XS(x);
  volatile XmeMeCtl *c = (volatile XmeMeCtl *)(uintptr_t)s->me_ctl;
  if (why == 2) hle_me_halt();   /* the hang test: halted for good, its cache written back first (ge/xme.c); the main CPU's take-back
                                    holds it in reset */
  if (c->h.exit_req) me_leave(c);
  if (why == 0) {                /* the queue empty: until the main CPU posts (its head every 5 us), at most 0.5 ms */
    uint32_t t = s->tail;
    if (!(c->rest & XME_REST_CFG)) for (int k = 0; k < 100 && s->head == t; k++) me_spin(XME_ME_5US);   /* gl27's wait */
    else {                       /* gl28: the same, ended early once the main CPU's turn allows the rest; then the rest */
      for (int k = 0; k < 100 && s->head == t && !(c->rest & XME_REST_OK); k++) me_spin(XME_ME_5US);
      if ((c->rest & XME_REST_OK) && s->head == t) {
        xme_rest_once(c, s, t);
        /* v30me5's spacing between rests (me_mix_me.c ME_REST_AFTER: 4 empty polls, about 300 us): after any rest, gl27's wait while
           the rest is still allowed (its head every 5 us, at most 0.5 ms), so a wake inside the main CPU's turn (a stray, a rekick, a
           rest ended before the HALT or with no interrupt, a rest given up) never makes rest after rest at full speed.  The kick at the
           switch comes after the rest bit is cleared: this wait then ends at once */
        for (int k = 0; k < 100 && s->head == t && (c->rest & XME_REST_OK); k++) me_spin(XME_ME_5US);
      }
    }
  } else me_spin(why == 1 ? 4u * XME_ME_5US : XME_ME_5US);   /* the GE (a frame's start): 20 us; the heap's answer: 5 us */
  c->h.count = hle_me_count(); c->h.heartbeat++;
}

uint32_t xme_me_clk(void) { return hle_me_count(); }   /* gl20 */

void xme_me_cache(XmeCtx *x, int op, const void *p, uint32_t n)
{
  (void)x;
  if (op == XME_C_ALL) hle_me_dwbinv_all();
  else if (n) { hle_me_dinval(p, n); hle_me_sync(); }
}

void xme_me_main(volatile HleMeHdr *h)
{
  volatile XmeMeCtl *c = (volatile XmeMeCtl *)h;
  XmeCtx *x;
  /* ME_CALLP (ge/me_rt.h): a call through a pointer goes to the kseg0 alias when $gp's top bit is set; the stub loads $gp = _gp */
  __asm__ volatile(".set push\n\t.set noat\n\tlui $1, 0x8000\n\tor $28, $28, $1\n\t.set pop" ::: "memory");
  { uint32_t f = 0x01000001u; __asm__ volatile("ctc1 %0, $31\n\tnop\n\tnop" : : "r"(f)); }   /* the main thread's FPU rule (flush to
                                                                                               zero, toward zero: main_psp.c, A9) */
  __asm__ volatile("mtc0 $zero, $9\n\tsync" ::: "memory");   /* gl19: Count starts.  It reads a constant until the ME writes it
                                                                 (me_probe_notes.md 3; probes 1-2 write it so, gl18me did not) */
  x = c->x;
  if (c->rest & XME_REST_CFG) xme_rest_init();   /* gl28 (MGS2_ME_REST=1): xme_rest's vector in CP0 $25 and the main CPU's interrupt
                                                    enabled at the ME's controller, once; IE stays off but inside xme_rest */
  c->h.boots++; c->h.state = 1; hle_me_sync();
  xme_run(x, xme_me_wait);       /* until XE_STOP (its cache written back and dropped there) */
  me_leave(c);
}
