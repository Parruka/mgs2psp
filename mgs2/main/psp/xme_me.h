/* xme_me.h (task #180 step 4d-4, me_step4_notes.md 4d): the picture loop (ge/xme.c) on the PSP's Media Engine: the control block that
   hle_me_start hands the ME, and the ME's side of the loop (psp/xme_me.c: its entry, its waits, its cache steps), which the main CPU
   installs (psp/xl_psp.c mq_me_start).  The emulator has no ME: there hle_me_start refuses and the loop runs as 4c's thread. */
#ifndef MGS2_XME_ME_H
#define MGS2_XME_ME_H
#include <stdint.h>
#include "hle_me.h"
#include "../ge/xme.h"

typedef struct XmeMeCtl {   /* 128 bytes, 64-aligned; both cores reach it only through the uncached alias */
  HleMeHdr h;               /* hle_me.h's head: magic, heartbeat (the ME's waits move it), state (1 running, 2 left the loop: its cache
                               written back, halting), exit_req, count (the ME's Count at its last wait), boots */
  XmeCtx *x;                /* the loop's ctx (cached; the ME reaches its shared words through XS, xme.h) */
  /* gl28 (MGS2_ME_REST, power_design.md section 5 "R1"; gl27: pad[15], zeros): the ME rests in HALT through the main CPU's turns
     (psp/xme_rest.S, v30me5's psp/me_rest.S renamed; the handshake is v30me5's psp/me_mix_me.c me_rest_once).  All zero with
     MGS2_ME_REST=0: the ME never sets up the rest and never rests (gl27) */
  uint32_t rest;            /* main: XME_REST_CFG (before the start: the ME sets up xme_rest's vector and interrupt), XME_REST_OK (the
                               ME may rest now: set when a main-CPU turn begins, cleared before the ME's turn begins) */
  uint32_t asleep;          /* ME: 1 from just before its last look at the queue's head until it is awake again */
  uint32_t sleeps;          /* ME: rests begun (written before asleep) */
  uint32_t halted, pending, nowake;   /* ME: rests ended by the interrupt in the HALT / by one taken before the HALT / with no interrupt */
  uint32_t late;            /* ME: rests given up: an entry, the end of the rest or the exit request came after asleep was written */
  uint32_t wakes;           /* ME: the handler's wakes (boot word [13]) after its last rest */
  uint32_t flags_or;        /* ME: every flag bit of 0xBC300000 its handler has found (boot word [14]) */
  uint32_t t_rest;          /* ME: its Count ticks inside xme_rest, summed (wraps at 2^32, about 13 s: the main CPU reads differences
                               a turn apart); 0 for a rest means Count stops in HALT */
  uint32_t rpc[5];          /* ME: Sony's ME request words at its last wake (reads only; uOFW me_wrapper): 0xBFC00600, 608, 700, 710,
                               714 */
} XmeMeCtl;
#define XME_REST_OK 1u      /* gl28: XmeMeCtl.rest's bits */
#define XME_REST_CFG 2u
_Static_assert(sizeof(XmeMeCtl) == 128, "XmeMeCtl stays 128 bytes (gl28: the rest's words in gl27's pad)");

void xme_me_main(volatile HleMeHdr *h);   /* the ME's entry (hle_me_start's work, at its kseg0 alias): $gp's top bit (ME_CALLP), the main
                                             thread's FPU rule, then the loop until XE_STOP; its cache written back, state 2, halt */
void xme_me_wait(XmeCtx *x, int why);     /* the loop's waits on the ME (xme.h XmeWait): 0 the queue empty (until the main CPU posts, its
                                             head looked at every 5 us, at most 0.5 ms), 1 the GE (20 us), 2 the hang test (halted for
                                             good), 3 the heap's answer (5 us); an exit request leaves the loop (hle_me_stop) */
uint32_t xme_me_clk(void);                /* gl20: the ME's Count (331.31 MHz [C, probe 2]), the loop's clock (xme.h XmeCtx.clk) on the ME */
void xme_rest_init(void);                 /* gl28: psp/xme_rest.S (the ME only): its vector in CP0 $25, the main CPU's interrupt enabled */
void xme_rest(void);                      /* gl28: psp/xme_rest.S (the ME only): IE + IM2, HALT until an interrupt, IE off again */
extern char xme_rest_halt[];              /* gl28: the HALT's address in xme_rest (a wake's EPC there: woken in the HALT) */
void xme_me_cache(XmeCtx *x, int op, const void *p, uint32_t n);   /* the ME's cache steps (xme.h XmeCache): XME_C_ALL its whole data
                                                                       cache written back and invalidated, XME_C_INV a range invalidated */
#endif
