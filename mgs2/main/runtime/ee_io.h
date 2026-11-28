/* EE memory map and hardware registers for the x86 development host (boot probe): RAM (32 MB, with its kseg / uncached mirrors), the
   scratchpad, and the register space 0x10000000..0x13ffffff (EE timers, IPU, GIF, VIF, DMAC, INTC; VU memory; GS privileged registers)
   as plain memory with side effects.  A register write is seen after it lands: the write hook arms it, the next guest memory access
   (or kernel call) runs its effect.  DMA starts complete at once; VIF0 feeds the VU0 core (vif0_run here, vu0.h), GIF / VIF1 are parsed for the GS interrupts, IPU is not run.
   Include before guest.h:  #define GUEST_MM_EXTERN / GUEST_HW_EXTERN, then this file; it pulls in guest.h itself. */
#ifndef EE_IO_H
#define EE_IO_H
#ifndef DGX_NOTE
#define DGX_NOTE(k) ((void)0)                  /* boot.c DG_EXP (x86 dev): what happens inside the draw pipeline's call */
#endif
#define GUEST_MM_EXTERN
#define GUEST_HW_EXTERN
#include "host_time.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/* ZT (Ffm 2026-10-21, speed_notes §31): the call of the PS2 draw pipeline (0x11da90, boot.c rt_dg_call) takes no game time.  Inside it
   no timed event (vblank, alarm) is due, a guest time read sees the time of its entry, and its exit sets the time back, so how much work
   the renderer does (the PS2 one, a step that skips it, the GE one) cannot move the game.  g_zt: inside it with g_opt OPT_ZT */
static int g_zt; static uint64_t g_zt_t0;
#define DGX_FROZEN() g_zt
static inline uint8_t *mm_map(uint32_t a);
static inline uint32_t io_wr_note(uint32_t a);
#define MM(a) mm_map((uint32_t)(a))
#define GUEST_WR_HOOK(a, n) io_wr_note((uint32_t)(a))
#include "guest.h"

#ifdef GUEST_SPR0                         /* the scratchpad lives in guest RAM 0x0000..0x3fff (the EE kernel's area: the HLE kernel leaves it
                                             unused), so the generated code reaches 0x70000000 + x as RAM x (gen_pre.h GEN_MM_FAST) */
#define g_spr g_mem
#else
static uint8_t g_spr[0x4000] __attribute__((aligned(16)));
#endif
/* the register space 0x10000000 .. 0x13ffffff as three blocks (the PSP has no room for a 64 MB shadow): the EE registers 0x10000000 ..
   0x1000ffff, the GS privileged registers 0x12000000 .. 0x12001fff, and one spare 64 KB that every other address shares (reported once
   per 256-byte page: none in the boot-to-deck run; VU0 memory is mapped apart, mm_map) */
static uint8_t g_io_ee[0x10000] __attribute__((aligned(16))), g_io_gs[0x2000] __attribute__((aligned(16))), g_io_else[0x10000] __attribute__((aligned(16)));
static uint8_t *io_else(uint32_t p)
{ static uint32_t said[16]; static int n; int i; for (i = 0; i < n && said[i] != p >> 8; i++) {}
  if (i == n && n < 16) { said[n++] = p >> 8; fprintf(stderr, "io: register page %08x has no block, shares the spare one\n", 0x10000000u + (p & ~0xffu)); }
  return g_io_else + (p & 0xffffu); }
static inline uint8_t *io_at(uint32_t p) { return p < 0x10000u ? g_io_ee + p : p - 0x2000000u < 0x2000u ? g_io_gs + (p - 0x2000000u) : io_else(p); }
static uint8_t g_io_seen[0x4000000 >> 8];
GSVS uint32_t g_io_arm, g_io_pend;                       /* a register write: armed by the hook, pending once it lands */
static uint32_t io_dstat, io_istat, io_csr;             /* shadows of the write-1-to-clear registers */
static long io_dma_count[10], io_chain_qwc[10];   /* io_chain_qwc: chain-mode starts with QWC > 0 (dma_feed) */
static int io_log = 1;
#define IO32(off) (*(uint32_t *)io_at(off))

static void io_flush(void);
#if defined(__psp__) && !defined(QCOPY_NO_VFPU)
void rt_qcopy_nov(uint8_t *d, const uint8_t *s, uint32_t n);   /* boot.c (v18): quadwords between buffers that share no 64-byte line */
#define SPR_COPY(d, s, n) rt_qcopy_nov((d), (s), (n))
#else
#define SPR_COPY(d, s, n) memcpy((d), (s), (n))
#endif
/* EE timers 0..3 (0x10000000 + 0x800 * i: COUNT, MODE +0x10, COMP +0x20, HOLD +0x30): COUNT is computed from the virtual time when
   it is read.  MODE bits 0-1 pick the clock (BUSCLK = EE clock / 2, /16, /256, or hblank: PAL 15625 Hz), bit 7 enables counting.
   Compare / overflow interrupts are not raised yet (the game enables none of INTC 9..12 so far). */
/* the virtual time in EE cycles (294.912 MHz) and kernel.h's poll threshold.  K_COUNTDOWN: the time is k_tbase - k_left, k_left being the
   distance to k_poll_at (clamped to INT32_MAX; the poll's slow path then finds nothing due and re-arms), so a loop poll of the generated
   code is one 32-bit decrement and test (gen_pre.h IRQ_POLLF); every change of the time or of k_poll_at goes through K_ADV / K_SET / K_SYNC.
   Without it, k_time itself.  Same polls, same times either way. */
SPRS uint64_t k_poll_at;                                     /* kernel.h k_poll_arm: below this time a poll only advances time */
#ifdef K_COUNTDOWN
SPRS uint64_t k_tbase; SPRS int32_t k_left;
#define K_NOW() (k_tbase - (uint64_t)(int64_t)k_left)
#define K_SET(t) ({ uint64_t t_ = (t), d_ = k_poll_at > t_ ? k_poll_at - t_ : 0; k_left = d_ > 0x7fffffffu ? 0x7fffffff : (int32_t)d_; k_tbase = t_ + (uint64_t)(int64_t)k_left; (void)0; })   /* a macro, not a function: a new function would renumber the later ones and void their PGO profile */
#define K_ADV(d) ((void)(k_left -= (int32_t)(d)))
#define K_SYNC() K_SET(K_NOW())
#else
uint64_t k_time;
#define K_NOW() k_time
#define K_ADV(d) ((void)(k_time += (d)))
#define K_SET(t) ((void)(k_time = (t)))
#define K_SYNC() ((void)0)
#endif
#define K_READ() (g_zt ? g_zt_t0 : K_NOW())    /* the time a guest read sees (ZT: the draw pipeline's entry time inside it) */
static uint64_t io_tbase[4]; static uint32_t io_tcount0[4], io_tmode[4];
static uint32_t io_timer_count(int i)
{
  DGX_NOTE(5);
  uint32_t mode = io_tmode[i]; uint64_t t = K_READ() - io_tbase[i];
  static const uint32_t div[4] = { 2, 32, 512, 18874 };
  if (!(mode & 0x80)) return io_tcount0[i];
  return (uint32_t)(io_tcount0[i] + t / div[mode & 3]) & 0xffff;
}
static inline uint32_t io_phys(uint32_t a, int *spr)
{
  uint32_t top = a >> 28; *spr = top == 7;
  return (top == 2 || top == 3) ? (a & 0x0fffffffu) : (a & 0x1fffffffu);
}
static inline uint8_t *mm_map(uint32_t a)
{
  int spr; uint32_t p;
  if (g_io_pend) io_flush();
  if (g_io_arm) { g_io_pend = g_io_arm; g_io_arm = 0; }
  p = io_phys(a, &spr);
  if (spr) return g_spr + (a & 0x3fff);
  if (p >= 0x10000000u && p < 0x14000000u) {
    p -= 0x10000000u; g_io_seen[p >> 8] = 1;
    if (p < 0x2000u && (p & 0x7f0u) == 0) *(uint32_t *)io_at(p & ~0x7ffu) = io_timer_count(p >> 11);   /* timer COUNT */
    if ((p & ~0x7fffu) == 0x1000000u) {                  /* VU0 micro memory 0x11000000 (4 KB, mirrored to 0x11003fff), VU0 data 0x11004000 (mirrored to 0x11007fff) */
      if (p & 0x4000) return VUMEM + (p & 0xfff);
      vu_prog_clean = 0; return (uint8_t *)VUPROG + (p & 0xfff);
    }
    return io_at(p);
  }
  return g_mem + (p & (GUEST_SIZE - 1));
}
static inline uint32_t io_wr_note(uint32_t a)
{
  int spr; uint32_t p = io_phys(a, &spr);
  if (!spr && p >= 0x10000000u && p < 0x14000000u) g_io_arm = p;
  return a;
}
static uint32_t HWR32(uint32_t a) { return *(uint32_t *)mm_map(a); }
static void HWW32(uint32_t a, uint32_t v) { *(uint32_t *)mm_map(io_wr_note(a)) = v; }

static void k_dmac_raise(int ch);                       /* kernel.h: the DMAC interrupt of a channel (delivered when interrupts are taken) */
static void k_intc_raise(int cause);                    /* kernel.h: an INTC interrupt (0 = GS) */

/* GS packets: the GIF tags the paths carry are followed so that the GS privileged side effects happen: a FINISH (0x61) or SIGNAL (0x60)
   register write sets its CSR bit and raises the GS interrupt (INTC 0) when the GS IMR (GsPutIMR) leaves it unmasked.  Drawing itself is
   not executed yet.  Path 3 = GIF DMA, path 2 = VIF1 DIRECT / DIRECTHL.  Path 1 (VU1 XGKICK) is not seen: VU1 does not run yet. */
static uint32_t gs_imr = 0x7f00;                        /* GsGetIMR / GsPutIMR: bits 8.. SIGNAL, FINISH, HSYNC, VSYNC, EDWRITE (1 = masked) */
static long gs_n_finish, gs_n_signal, gs_n_tags[4];
typedef struct { uint32_t nloop, nreg, flg, ri, left_regs; uint64_t regs; int pend_half; } GifPath;
static GifPath gif_path[4];
static void gs_reg_write(int path, uint32_t reg, uint64_t v)
{
  (void)path; (void)v;
  if (reg == 0x61) { gs_n_finish++; io_csr |= 2; IO32(0x2001000) = io_csr; if (!(gs_imr & 0x200)) k_intc_raise(0); }
  else if (reg == 0x60) { gs_n_signal++; io_csr |= 1; IO32(0x2001000) = io_csr; if (!(gs_imr & 0x100)) k_intc_raise(0); }
}
static void gif_qw(int path, const uint8_t *q)          /* one quadword of a GIF path */
{
  GifPath *g = &gif_path[path]; uint64_t lo, hi; memcpy(&lo, q, 8); memcpy(&hi, q + 8, 8);
  if (!g->nloop) {                                      /* a GIF tag */
    g->nloop = lo & 0x7fff; g->flg = (lo >> 58) & 3; g->nreg = (lo >> 60) & 15; if (!g->nreg) g->nreg = 16; g->regs = hi; g->ri = 0;
    gs_n_tags[path]++;
    if (g->flg == 0 && ((lo >> 46) & 1)) gs_reg_write(path, 0, (lo >> 47) & 0x7ff);    /* PRE: PRIM */
    g->left_regs = g->flg == 2 || g->flg == 3 ? g->nloop : g->nloop * g->nreg;
    if (!g->left_regs) g->nloop = 0;
    return;
  }
  if (g->flg == 0) {                                    /* PACKED: one register per quadword; A+D (0xe) names its register */
    uint32_t r = (g->regs >> (4 * g->ri)) & 15;
    if (r == 0xe) gs_reg_write(path, hi & 0xff, lo);
    if (++g->ri == g->nreg) g->ri = 0;
    if (!--g->left_regs) g->nloop = 0;
  } else if (g->flg == 1) {                             /* REGLIST: two 64-bit registers per quadword */
    int h;
    for (h = 0; h < 2 && g->left_regs; h++) { if (++g->ri == g->nreg) g->ri = 0; g->left_regs--; }
    if (!g->left_regs) g->nloop = 0;
  } else if (!--g->left_regs) g->nloop = 0;              /* IMAGE */
}
/* VIF1: the command stream, fed one 32-bit word at a time (commands and their data cross DMA tags) */
typedef struct { uint32_t cmd, left, cl, wl; int qi; uint8_t qbuf[16]; } VifState;
static VifState vif1;
static long vif1_n_cmd[128], vif1_n_irq;
static void vif1_word(uint32_t w)
{
  VifState *v = &vif1;
  if (v->left) {                                        /* data of the current command */
    if (v->cmd == 0x50 || v->cmd == 0x51) { memcpy(v->qbuf + 4 * v->qi, &w, 4); if (++v->qi == 4) { v->qi = 0; gif_qw(2, v->qbuf); } }
    v->left--; return;
  }
  v->cmd = (w >> 24) & 0x7f; vif1_n_cmd[v->cmd]++; if (w >> 31) vif1_n_irq++;
  { uint32_t imm = w & 0xffff, num = (w >> 16) & 0xff;
    switch (v->cmd) {
    case 0x01: v->cl = imm & 0xff; v->wl = (imm >> 8) & 0xff; break;          /* STCYCL */
    case 0x20: v->left = 1; break;                                            /* STMASK */
    case 0x30: case 0x31: v->left = 4; break;                                 /* STROW, STCOL */
    case 0x4a: v->left = (num ? num : 256) * 2; break;                        /* MPG */
    case 0x50: case 0x51: v->left = (imm ? imm : 65536) * 4; v->qi = 0; break;   /* DIRECT, DIRECTHL */
    default:
      if ((v->cmd & 0x60) == 0x60) {                                          /* UNPACK: vl bits 0-1, vn bits 2-3 */
        uint32_t vl = v->cmd & 3, vn = (v->cmd >> 2) & 3, n = num ? num : 256, bits;
        if (v->wl > v->cl && v->wl) n = (n / v->wl) * v->cl + (n % v->wl < v->cl ? n % v->wl : v->cl);
        bits = n * (32u >> vl) * (vn + 1); v->left = (bits + 31) / 32;
      }
    }
  }
}
#ifndef __psp__
/* dev: MGS2_GSCAP=<file>:<first vblank>:<vblanks> records what the VIF1 (ch 1) and GIF (ch 2) DMA carry in that range, for
   tools/gscap.py: 24-byte records { u8 kind (0 data quadword, 1 DMA tag, 2 vblank start, 3 transfer start), u8 channel, u8 info, u8 0,
   u32 guest address (kind 2: the vblank), 16 bytes (kind 3: CHCR MADR QWC TADR; kind 4, after each kind 2: a GS privileged register at that
   address, 8 bytes and 8 following) }; info: kind 2, the calls of the PS2 draw pipeline in the vblank that ended (bit 0 a full one, bit
   1 a reduced one: boot.c rt_dg_call); kind 3, 1 when the transfer starts inside such a call (g_zt) (tools/gsref.c GSREF_SKIP) */
static FILE *gscap_f; static long gscap_from = -1, gscap_to; static uint8_t gscap_vinfo;
static void gscap(int kind, int ch, uint32_t a, const void *q)
{ uint8_t r[24] = { (uint8_t)kind, (uint8_t)ch, (uint8_t)(kind == 2 ? gscap_vinfo : kind == 3 ? g_zt != 0 : 0) }; memcpy(r + 4, &a, 4); if (q) memcpy(r + 8, q, 16); fwrite(r, 24, 1, gscap_f); }
extern uint32_t g_dg_calls[2];
static void gscap_vblank(long vb)
{
  static char fn[256]; static uint32_t dgc0, dgc1;
  gscap_vinfo = (uint8_t)((g_dg_calls[0] != dgc0) | (g_dg_calls[1] != dgc1) << 1); dgc0 = g_dg_calls[0]; dgc1 = g_dg_calls[1];
  if (gscap_from < 0) { const char *e = getenv("MGS2_GSCAP"); long n = 0; gscap_from = 0x7fffffff;
    if (e && sscanf(e, "%255[^:]:%ld:%ld", fn, &gscap_from, &n) == 3) gscap_to = gscap_from + n; else gscap_from = 0x7fffffff; }
  if (vb == gscap_from) gscap_f = fopen(fn, "wb");
  if (gscap_f && vb >= gscap_to) { fclose(gscap_f); gscap_f = 0; fprintf(stderr, "gscap: %s written (vblanks %ld..%ld)\n", fn, gscap_from, gscap_to - 1); }
  if (gscap_f) { gscap(2, 0, (uint32_t)vb, 0);
    static const uint16_t pr[6] = { 0x00, 0x20, 0x70, 0x80, 0x90, 0xa0 };   /* PMODE SMODE2 DISPFB1 DISPLAY1 DISPFB2 DISPLAY2 */
    for (int i = 0; i < 6; i++) gscap(4, 0, 0x12000000u + pr[i], g_io_gs + pr[i]); }
}
#define GSCAP(kind, ch, a, q) do { if (gscap_f && ((ch) == 1 || (ch) == 2)) gscap(kind, ch, a, q); } while (0)
#define GSCAP_ON (gscap_f != 0)
#else
#define GSCAP_ON 0
#define GSCAP(kind, ch, a, q) ((void)0)
#define gscap_vblank(vb) ((void)0)
#endif
#ifdef MGS2_XLIVE   /* dev, x86 check build: the packet translator's second walk of each VIF1 / GIF transfer (boot/xlive_chk.c, ge/xlive.c) */
/* one: 1 when vif1_chain_fast walks this transfer (VIF1, VIF1_FAST_OK), so the translator may take it from that walk (the one walk) */
void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one);
/* the one walk (renderer_notes 11.12, task #47): when xlive_chk_dma sets g_xl_one for a VIF1 transfer instead of walking it a second
   time, vif1_chain_fast hands the translator (ge/xlate.c xl_vif1_span) the runs of words it walks, with their guest addresses, as
   ge/xlive.c's walk would, and calls xlive_one_end when the transfer ends.  The translator only reads them: the exact walk is the same */
int g_xl_one;
void xl_vif1_span(const uint32_t *p, uint32_t n, uint32_t src);
void xlive_one_end(void);
#define XL_ONE(p, n, src) do { if (g_xl_one) xl_vif1_span((p), (n), (src)); } while (0)
#ifndef __psp__
void xl_tag_probe(const uint32_t *t, uint32_t tadr);   /* sb1 probe (x86: boot/xlive_chk.c MGS2_SB_PROBE) */
#define XL_TAG(t, a) do { if (g_xl_one) xl_tag_probe((t), (a)); } while (0)
#else
#define XL_TAG(t, a) ((void)0)
#endif
/* sb1, Stage B lane 1 (renderer_notes 11.20): g_xl_lane bit 0 (boot/xlive_chk.c MGS2_SB_LANE) lets the one walk hand a P1214a0 group to
   the translator whole (lane_p04 below, ge/xlate.c xl_lane_p04); sb2, lane 2: bit 1 a P12e8b8 group (lane_p06, xl_lane_p06); sb5, lane 4
   (renderer_notes 11.32): bit 3 a run of P07 slots (lane_p07, xl_lane_p07); sb6, lane 3 (renderer_notes 11.33): bit 2 a run of the sort
   pass's effect prims (lane_fx, xl_lane_fx) */
int g_xl_lane;
int xl_lane_p04(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp);
int xl_lane_p06(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp);
int xl_lane_p07(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp);
int xl_lane_fx(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp);
#ifndef __psp__
long lane7_miss[8];   /* sb5, x86 census: tags starting with STCYCL 7 / 1 where lane_p07 took no slot, by the first test that failed (lane_p07) */
long lane3_miss[8];   /* sb6, x86 census: tags starting with STCYCL 3, 6 or 12 / 1 and V4-32 to T + 0x100 where lane_fx took no prim, by the first
                         test that failed (lane_fx) */
long lane3_gen[8];    /* sb6, x86 census: such tags the generic walk took (vif1_chain_fast), by why: 4 not the one walk (the translator does not
                         take that transfer from this walk), else 0 lane 3 off, 1 lane_fx took none (lane3_miss says why), 2 command data left,
                         3 no TTE */
#endif
#else
#define XL_ONE(p, n, src) ((void)0)
#define XL_TAG(t, a) ((void)0)
#endif
/* a source chain (CHCR mode 1) or normal transfer, each quadword handed to fn; TTE passes the tag's upper half (VIF1) */
/* a contiguous run of n quadwords at guest address a, as one block when blk takes blocks (and no capture runs), else one by one */
#ifdef GEN_DG_TRACE   /* dev (boot.c MGS2_DG_TRACE): DMA reads of RAM and the scratchpad copies */
void rt_dgt_dma(int ch, uint32_t a, uint32_t nq); void rt_dgt_copy(uint32_t dst, uint32_t src, uint32_t n);
#define DGT_DMA(ch, a, nq) rt_dgt_dma(ch, a, nq)
#define DGT_COPY(dst, src, n) rt_dgt_copy(dst, src, n)
#else
#define DGT_DMA(ch, a, nq) ((void)0)
#define DGT_COPY(dst, src, n) ((void)0)
#endif
#ifndef __psp__   /* dev: boot.c MGS2_PKT_HASH=<from vb>:<to vb>: FNV-1a 64 of what the VIF1 and GIF DMA carry (the data, and the tag fields that act), per vblank */
int g_pkt_on; uint64_t g_pkt_h = 0xcbf29ce484222325ull; FILE *g_pkt_df;   /* g_pkt_df: boot.c MGS2_PKT_DUMP, the hashed bytes as records */
static void pkt_fnv(int ch, uint32_t a, const uint8_t *p, uint32_t n)
{
  uint64_t h = g_pkt_h; for (uint32_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; } g_pkt_h = h;
  if (g_pkt_df) { uint32_t r[4] = { 0x544b50u, (uint32_t)ch, a, n }; fwrite(r, 4, 4, g_pkt_df); fwrite(p, 1, n, g_pkt_df); }   /* "PKT", channel, guest address (tags: | 1), bytes */
}
#define PKT_HASH(ch, a, p, n) do { if (g_pkt_on && ((ch) == 1 || (ch) == 2)) pkt_fnv((ch), (a), (p), (n)); } while (0)
static void pkt_tag(int ch, uint32_t a, const uint8_t *t, uint32_t chcr)   /* a chain tag: only the fields that act (QWC, ID, IRQ; ADDR except
                                                                               for cnt / ret / end; the upper half, VIF codes, for VIF1 with TTE) */
{
  uint32_t w[4], id; memcpy(w, t, 16); id = (w[0] >> 28) & 7;
  w[0] &= 0xf000ffffu; if (id == 1 || id == 6 || id == 7) w[1] = 0; if (ch != 1 || !(chcr & 0x40)) w[2] = w[3] = 0;
  pkt_fnv(ch, a | 1, (const uint8_t *)w, 16);
}
#define PKT_TAG(ch, a, t, chcr) do { if (g_pkt_on && ((ch) == 1 || (ch) == 2)) pkt_tag((ch), (a), (t), (chcr)); } while (0)
#else
#define PKT_HASH(ch, a, p, n) ((void)0)
#define PKT_TAG(ch, a, t, chcr) ((void)0)
#endif
static void dma_run(int ch, uint32_t a, uint32_t n, void (*fn)(const uint8_t *q, int tag), void (*blk)(const uint8_t *p, uint32_t nq))
{
  uint32_t i, b = a & (GUEST_SIZE - 1) & ~15u;
  DGT_DMA(ch, a, n);
  if (blk && (g_opt & (ch ? OPT_GS_BLOCK : OPT_VIF0_FAST)) && !GSCAP_ON && !(a & 0x80000000u) && b + 16 * n <= GUEST_SIZE) { if (n) { PKT_HASH(ch, b, g_mem + b, 16 * n); blk(g_mem + b, n); } return; }
  for (i = 0; i < n; i++) {
    uint32_t q = a + 16 * i; const uint8_t *p = (q & 0x80000000u) ? g_spr + (q & 0x3ff0) : g_mem + (q & (GUEST_SIZE - 1) & ~15u);
    GSCAP(0, ch, q, p); PKT_HASH(ch, q, p, 16); fn(p, 0);
  }
}
static void dma_feed(int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, void (*fn)(const uint8_t *q, int tag),
                     void (*blk)(const uint8_t *p, uint32_t nq))
{
  uint32_t asp[2], nasp = 0, i, guard = 0;
  { uint32_t st[4] = { chcr, madr, qwc, tadr }; GSCAP(3, ch, 0, st); }
  if (((chcr >> 2) & 3) == 0) { dma_run(ch, madr & ~0x80000000u, qwc, fn, blk); return; }   /* (normal mode: RAM only, as before) */
  if (qwc) {      /* chain mode started with QWC > 0: the QWC quadwords at MADR first, then the chain at TADR unless the tag CHCR holds (the
                     last one read: ID in bits 28..30, IRQ in 31) ends it: REFE, END, or IRQ with TIE (as PCSX2 dmaVIF0 / dmaVIF1 / GIF "QWC on chain") */
    uint32_t id = (chcr >> 28) & 7;
    io_chain_qwc[ch]++;
    dma_run(ch, madr, qwc, fn, blk);
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) return;
  }
  for (;;) {
    const uint8_t *t = (tadr & 0x80000000u) ? g_spr + (tadr & 0x3ff0) : g_mem + (tadr & (GUEST_SIZE - 1) & ~15u);
    uint64_t tag; uint32_t n, id, addr, data, next; int end = 0;
    memcpy(&tag, t, 8); n = tag & 0xffff; id = (tag >> 28) & 7; addr = (uint32_t)(tag >> 32) & 0x7ffffff0u; addr |= (uint32_t)(tag >> 32) & 0x80000000u;
    GSCAP(1, ch, tadr, t); DGT_DMA(ch, tadr, 1); PKT_TAG(ch, tadr, t, chcr);
    if (chcr & 0x40) fn(t, 1);
    switch (id) {
    case 0: data = addr; next = 0; end = 1; break;                    /* refe */
    case 1: data = tadr + 16; next = data + 16 * n; break;              /* cnt */
    case 2: data = tadr + 16; next = addr; break;                       /* next */
    case 3: case 4: data = addr; next = tadr + 16; break;               /* ref, refs */
    case 5: data = tadr + 16; if (nasp < 2) asp[nasp++] = data + 16 * n; next = addr; break;   /* call */
    case 6: data = tadr + 16; if (nasp) next = asp[--nasp]; else { next = 0; end = 1; } break;   /* ret */
    default: data = tadr + 16; next = 0; end = 1; break;                /* end */
    }
    dma_run(ch, data, n, fn, blk);
    if (end || ((chcr & 0x80) && (tag >> 31) & 1)) break;               /* end tag, or IRQ with TIE */
    tadr = next;
    if (++guard > 1000000) { fprintf(stderr, "dma %d: chain does not end\n", ch); break; }
  }
}
static void dma_gif_qw(const uint8_t *q, int tag) { if (!tag) gif_qw(3, q); }
/* the walks in blocks: the payload of an IMAGE transfer and of a VIF1 command other than DIRECT is skipped at once (only the GIF tags,
   A+D registers and VIF codes have effects here); the result is the same as one quadword / word at a time */
static void gif_block(int path, const uint8_t *p, uint32_t nq)
{
  GifPath *g = &gif_path[path]; uint32_t i = 0;
  while (i < nq) {
    if (g->nloop && g->flg >= 2 && g->left_regs > 1) { uint32_t k = g->left_regs - 1 < nq - i ? g->left_regs - 1 : nq - i; g->left_regs -= k; i += k; continue; }
    gif_qw(path, p + 16 * i); i++;
  }
}
static void dma_gif_block(const uint8_t *p, uint32_t nq) { gif_block(3, p, nq); }
static void dma_vif1_block(const uint8_t *p, uint32_t nq)
{
  VifState *v = &vif1; GifPath *g = &gif_path[2]; uint32_t i = 0, nw = 4 * nq;
  while (i < nw) {
    if (v->left) {
      if (v->cmd != 0x50 && v->cmd != 0x51) { uint32_t k = v->left < nw - i ? v->left : nw - i; v->left -= k; i += k; continue; }
      if (!v->qi && g->nloop && g->flg >= 2 && g->left_regs > 1 && v->left >= 8 && nw - i >= 8) {   /* DIRECT carrying IMAGE data */
        uint32_t k = g->left_regs - 1; if (k > v->left / 4 - 1) k = v->left / 4 - 1; if (k > (nw - i) / 4) k = (nw - i) / 4;
        g->left_regs -= k; v->left -= 4 * k; i += 4 * k; continue;
      }
    }
    { uint32_t w; memcpy(&w, p + 4 * i, 4); vif1_word(w); i++; }
  }
}
static void dma_vif1_qw(const uint8_t *q, int tag)
{
  uint32_t w[4]; int i; memcpy(w, q, 16);
  if (tag) { vif1_word(w[2]); vif1_word(w[3]); return; }
  for (i = 0; i < 4; i++) vif1_word(w[i]);
}
/* OPT_GS_FAST: the VIF1 chain walk of dma_feed + dma_run + dma_vif1_block + vif1_word in one function, the VIF state in locals for the
   whole chain (the tags, the TTE words, the commands and the data skips inline; RAM and scratchpad data alike).  It makes the same gif_qw
   calls in the same order, so the GS side effects (FINISH / SIGNAL) are the same; used only with OPT_GS_BLOCK and no capture, packet hash
   or trace (VIF1_FAST_OK). */
static const uint8_t vif1_ebits[16] = { 32, 16, 8, 4, 64, 32, 16, 8, 96, 48, 24, 12, 128, 64, 32, 16 };   /* UNPACK bits per vector: (32 >> vl) * (vn + 1) */
typedef struct { uint32_t left, cmd, cl, wl; } VifLoc;   /* vif1's left / cmd / cl / wl while a chain is walked (qi and qbuf stay in vif1) */
/* the words [wp, we) as vif1_word one at a time, with the data of commands other than DIRECT, and the IMAGE data DIRECT carries, skipped in
   bulk as dma_vif1_block skips them */
static inline __attribute__((always_inline)) void vif1_fast_words(const uint32_t *wp, const uint32_t *we, VifLoc *s)
{
  VifState *v = &vif1; uint32_t left = s->left, cmd = s->cmd;
  while (wp < we) {
    if (left) {
      if (cmd != 0x50 && cmd != 0x51) { uint32_t k = (uint32_t)(we - wp); if (left < k) k = left; left -= k; wp += k; continue; }
      if (!v->qi) { GifPath *g = &gif_path[2]; uint32_t nw = (uint32_t)(we - wp);
        if (g->nloop && g->flg >= 2 && g->left_regs > 1 && left >= 8 && nw >= 8) {   /* DIRECT carrying IMAGE data */
          uint32_t k = g->left_regs - 1; if (k > left / 4 - 1) k = left / 4 - 1; if (k > nw / 4) k = nw / 4;
          g->left_regs -= k; left -= 4 * k; wp += 4 * k; continue;
        }
        if (left >= 4 && nw >= 4 && (g_opt & OPT_DIRECT_QW)) { gif_qw(2, (const uint8_t *)wp); wp += 4; left -= 4; continue; }   /* sb3: the
                                                         call the next four words make below, from the data itself (qbuf is not read
                                                         while qi is 0) */
      }
      memcpy(v->qbuf + 4 * v->qi, wp, 4); wp++; left--;
      if (++v->qi == 4) { v->qi = 0; gif_qw(2, v->qbuf); }
      continue;
    }
    { uint32_t w = *wp++;
      cmd = (w >> 24) & 0x7f;
#ifndef __psp__
      vif1_n_cmd[cmd]++; if (w >> 31) vif1_n_irq++;
#endif
      if (cmd >= 0x60) {                                                      /* UNPACK */
        uint32_t num = (w >> 16) & 0xff, n = num ? num : 256, cl = s->cl, wl = s->wl;
        if (wl > cl && wl) n = (n / wl) * cl + (n % wl < cl ? n % wl : cl);
        left = (n * vif1_ebits[cmd & 15] + 31) / 32;
      } else switch (cmd) {
      case 0x01: s->cl = w & 0xff; s->wl = (w >> 8) & 0xff; break;            /* STCYCL */
      case 0x20: left = 1; break;                                             /* STMASK */
      case 0x30: case 0x31: left = 4; break;                                  /* STROW, STCOL */
      case 0x4a: { uint32_t num = (w >> 16) & 0xff; left = (num ? num : 256) * 2; } break;   /* MPG */
      case 0x50: case 0x51: { uint32_t imm = w & 0xffff; left = (imm ? imm : 65536) * 4; v->qi = 0; } break;   /* DIRECT, DIRECTHL */
      }
    }
  }
  s->left = left; s->cmd = cmd;
}
static __attribute__((noinline)) void vif1_run_any(uint32_t a, uint32_t n, VifLoc *s)   /* dma_run for VIF1, any n quadwords at a: RAM in one
                                                         piece or across the 32 MB mask, the scratchpad in pieces up to its end */
{
  if (!(a & 0x80000000u)) {
    uint32_t i, b = a & (GUEST_SIZE - 1) & ~15u;
    if (b + 16 * n <= GUEST_SIZE) { XL_ONE((const uint32_t *)(g_mem + b), 4 * n, a); vif1_fast_words((const uint32_t *)(g_mem + b), (const uint32_t *)(g_mem + b) + 4 * n, s); return; }
    for (i = 0; i < n; i++) { uint32_t q = (a + 16 * i) & (GUEST_SIZE - 1) & ~15u; XL_ONE((const uint32_t *)(g_mem + q), 4, a + 16 * i); vif1_fast_words((const uint32_t *)(g_mem + q), (const uint32_t *)(g_mem + q) + 4, s); }
    return;
  }
  { uint32_t o = a & 0x3ff0;
    while (n) { uint32_t k = (0x4000u - o) >> 4; if (k > n) k = n; XL_ONE((const uint32_t *)(g_spr + o), 4 * k, a); vif1_fast_words((const uint32_t *)(g_spr + o), (const uint32_t *)(g_spr + o) + 4 * k, s); n -= k; a += 16 * k; o = 0; } }
}
#ifdef MGS2_XLIVE
/* the n quadwords of data at guest address a in one piece, as vif1_chain_fast finds them; 0 when they are not */
static inline const uint32_t *lane_ptr(uint32_t a, uint32_t n)
{
  if (!(a & 0x80000000u)) { uint32_t b = a & (GUEST_SIZE - 1) & ~15u; return b + 16 * n <= GUEST_SIZE ? (const uint32_t *)(g_mem + b) : 0; }
  { uint32_t o = a & 0x3ff0; return o + 16 * n <= 0x4000u ? (const uint32_t *)(g_spr + o) : 0; }
}
/* sb1, Stage B lane 1 (renderer_notes 11.20): at tag t (guest address tadr, TTE on, no command data left), a P1214a0 group: REF tags whose
   TTE words are STCYCL and an UNPACK that takes exactly the REF's data (the constant block, when its tag comes first; the material;
   streams A, B, C at T + 0x100, 0x101, 0x102 every third quadword), then a CNT of 2 quadwords: the header's UNPACK data, then MSCAL and
   three NOPs.  No IRQ bit in a tag or a VIF code, every tag and its data in one piece.  When the translator takes the group (xl_lane_p04)
   the walk's state is what the generic walk leaves after it (STCYCL 1 / 1, no data left, the last command a NOP; no DIRECT, so no GIF
   word) and *next the tag after it; 0: not such a group, or the translator declines (the generic walk then takes it tag by tag) */
static inline int lane_p04(const uint32_t *t, uint32_t tadr, VifLoc *s, uint32_t *next)
{
  int q0 = t[2] == 0x01000101u && t[3] == 0x6c188000u, nr = 4 + q0;
  uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : GUEST_SIZE - (tadr & (GUEST_SIZE - 1) & ~15u);
  const uint32_t *dp[5], *c = t + 4 * nr;
  if (room < 16u * (uint32_t)(nr + 3)) return 0;   /* the tags and the CNT's data */
  for (int j = 0; j < nr; j++) {
    const uint32_t *g = t + 4 * j; uint32_t n = g[0] & 0xffff, w = g[3], num = (w >> 16) & 0xff; int k = j - q0;   /* k: -1 the constant block,
                                                                                                0 the material, 1 2 3 the streams */
    if (g[0] >> 28 != 3) return 0;   /* REF, no IRQ */
    if (!num) num = 256;
    if (k < 0) { if (n != 24) return 0; }   /* 6c188000: V4-32, 24 vectors at T + 0 */
    else if (!k) { if (g[2] != 0x01000101u || w != 0x6c0880e8u || n != 8) return 0; }   /* V4-32, 8 vectors at T + 0xe8 */
    else if (g[2] != 0x01000103u || (w & 0xff00ffffu) != (k == 3 ? 0x75008102u : 0x6d008100u + (uint32_t)(k - 1)) || num != (k == 3 ? 4u : 2u) * n)
      return 0;   /* STCYCL 3 / 1; V4-16 (8 bytes a vector) at T + 0x100, 0x101, V2-16 masked (4 bytes) at T + 0x102 */
    if (!(dp[j] = lane_ptr(g[1] & 0xfffffff0u, n))) return 0;
  }
  if (c[0] >> 28 != 1 || (c[0] & 0xffff) != 2 || c[2] != 0x01000101u || c[3] != 0x6c018014u || c[8] >> 24 != 0x14 || c[9] || c[10] || c[11]) return 0;
  if (!xl_lane_p04(t, tadr, q0, dp)) return 0;
  s->left = 0; s->cmd = 0; s->cl = 1; s->wl = 1;
#ifndef __psp__
  vif1_n_cmd[0x01] += nr + 1; vif1_n_cmd[0x6c] += 2 + q0; vif1_n_cmd[0x6d] += 2; vif1_n_cmd[0x75]++; vif1_n_cmd[0x14]++; vif1_n_cmd[0x00] += 3;
#endif
  *next = tadr + 16 * (nr + 3); return nr + 1;
}
#ifdef NAT_HOOK
static uint32_t nat_stop = 0xffffffffu;   /* native step 1 (boot/nat_x86.c): the next native group's key (RAM offset) of the buffer being
                                             walked: a run lane stops before it, so the walk reaches the tag where the group is merged */
static long nat_stop_hit;
#define NAT_RUN_STOP(a) (!((a) & 0x80000000u) && ((a) & (GUEST_SIZE - 1) & ~15u) >= nat_stop && (nat_stop_hit++, 1))
#else
#define NAT_RUN_STOP(a) 0
#endif
/* sb2, Stage B lane 2 (renderer_notes 11.20): at tag t (as lane_p04), a P12e8b8 group (P06: Snake, the crew, the guards; re/mesh_packets.md
   §6): the constant block's REF when it comes first (24 quadwords to T + 0, STCYCL 1 / 1), three material slots (8 quadwords to T + 0xe8,
   0xf0, 0xf8, STCYCL 1 / 1) and six stream slots (A, B V4-16 at T + 0x100, 0x101; C, D, E V2-16 masked at 0x102, 0x103, 0x104; F V4-16
   at 0x105; STCYCL 6 / 1), each slot a REF whose UNPACK takes exactly the REF's data or, when the builder does not send it, a CNT of 0
   whose TTE words are the slot's STCYCL and a NOP; then the CNT of 2 quadwords as P1214a0's.  No IRQ bit, every piece in one run of RAM
   or scratchpad.  As lane_p04: afterwards the walk's state is the generic walk's and *next the tag after the group; 0: not such a group,
   or the translator declines */
static inline int lane_p06(const uint32_t *t, uint32_t tadr, VifLoc *s, uint32_t *next)
{
  int q0 = t[0] >> 28 == 3 && (t[0] & 0xffff) == 24 && t[3] == 0x6c188000u, nr = 9 + q0, n6c = 1 + q0, n6d = 0, n75 = 0, nop = 3;
  uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : GUEST_SIZE - (tadr & (GUEST_SIZE - 1) & ~15u);
  const uint32_t *dp[10], *c = t + 4 * nr;
  if (room < 16u * (uint32_t)(nr + 3)) return 0;   /* the tags and the CNT's data */
  for (int j = 0; j < nr; j++) {
    const uint32_t *g = t + 4 * j; uint32_t n = g[0] & 0xffff, w = g[3], num = (w >> 16) & 0xff; int k = j - q0;   /* k: -1 the constant
                                                                                    block, 0 1 2 the materials, 3 .. 8 the streams A .. F */
    if (g[2] != (k < 3 ? 0x01000101u : 0x01000106u)) return 0;   /* the slot's STCYCL */
    dp[j] = 0;
    if (k >= 0 && g[0] >> 28 == 1 && !n && !w) { nop++; continue; }   /* not sent: CNT 0, no IRQ, a NOP */
    if (g[0] >> 28 != 3) return 0;   /* REF, no IRQ */
    if (!num) num = 256;
    if (k < 0) { if (n != 24) return 0; }   /* 6c188000: V4-32, 24 vectors at T + 0 */
    else if (k < 3) { if (w != 0x6c0880e8u + 8u * (uint32_t)k || n != 8) return 0; n6c++; }   /* V4-32, 8 vectors at T + 0xe8 + 8 k */
    else { int v2 = k >= 5 && k <= 7;   /* C, D, E: V2-16 masked (4 bytes a vector); A, B, F: V4-16 (8 bytes) */
      if ((w & 0xff00ffffu) != (v2 ? 0x75008100u : 0x6d008100u) + (uint32_t)(k - 3) || num != (v2 ? 4u : 2u) * n) return 0;
      if (v2) n75++; else n6d++; }
    if (!(dp[j] = lane_ptr(g[1] & 0xfffffff0u, n))) return 0;
  }
  if (c[0] >> 28 != 1 || (c[0] & 0xffff) != 2 || c[2] != 0x01000101u || c[3] != 0x6c018014u || c[8] >> 24 != 0x14 || c[9] || c[10] || c[11]) return 0;
  if (!xl_lane_p06(t, tadr, q0, dp)) return 0;
  s->left = 0; s->cmd = 0; s->cl = 1; s->wl = 1;
#ifndef __psp__
  vif1_n_cmd[0x01] += nr + 1; vif1_n_cmd[0x6c] += n6c; vif1_n_cmd[0x6d] += n6d; vif1_n_cmd[0x75] += n75; vif1_n_cmd[0x14]++; vif1_n_cmd[0x00] += nop;
#endif
  *next = tadr + 16 * (nr + 3); return nr + 1;
}
/* sb5, Stage B lane 4 (renderer_notes 11.32): at tag t (as lane_p04), a run of P07 slots (the codec faces; re/p07_builder.md 3.4), each 24
   quadwords and 20 tags: the stream slots A .. E (V4-16 at T + 0x100 .. 0x104, STCYCL 7 / 1) and F (V4-8 unsigned at T + 0x105, STCYCL
   7 / 2); a CNT of 4 (STCYCL 1 / 1, V4-32 4 to T + 0, its data); the material slots (V4-32 8 to T + 0xe8, 0xf0, 0xf8, STCYCL 1 / 1); a CNT
   of 0 with a NOP and FLUSHE or NOP; the bone slots (V4-32 4 to abs 0x20 + 4 i, STCYCL 1 / 1); a CNT of 0 with a NOP and MSCAL.  Each
   stream, material and bone slot a REF whose UNPACK takes exactly the REF's data or, when the builder does not send it, a CNT of 0 with
   the slot's STCYCL and a NOP.  No IRQ bit, every piece in one run of RAM or scratchpad.  The slots one after another while the next tag
   starts one, each handed to the translator (xl_lane_p07: one call each); afterwards the walk's state is what the generic walk leaves
   (STCYCL 1 / 1, no data left, the last command the MSCAL) and *next the tag after the last slot taken; 0: no slot here, or the
   translator declines the first */
static inline int lane_p07(const uint32_t *t, uint32_t tadr, VifLoc *s, uint32_t *next)
{
  int ns = 0;
#ifndef __psp__
  long nop = 0, n6c = 0, n6d = 0, n6e = 0, n10 = 0;
#endif
  for (;;) {
    uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : GUEST_SIZE - (tadr & (GUEST_SIZE - 1) & ~15u);
    const uint32_t *dp[20]; int k, why = 0; uint32_t snop = 0, s6c = 0, s6d = 0, s6e = 0, s10 = 0;
    if (room < 384u) {   /* the slot's 24 quadwords in one piece */
#ifndef __psp__
      if (!ns) lane7_miss[0]++;
#endif
      break; }
    for (k = 0; k < 20; k++) {   /* tag k: quadword k, after the CNT's four data quadwords k + 4 */
      const uint32_t *g = t + 4 * (k < 7 ? k : k + 4); uint32_t n = g[0] & 0xffff, w = g[3], num = (w >> 16) & 0xff;
      dp[k] = 0; why = 1 + (k > 0) + (k > 5);
      if (k == 6) { if (g[0] >> 28 != 1 || n != 4 || g[2] != 0x01000101u || w != 0x6c048000u) break; s6c++; continue; }   /* CNT 4: V4-32 4 at T + 0 */
      if (k == 10) { if (g[0] >> 28 != 1 || n || g[2] || (w && w != 0x10000000u)) break; snop += 1 + !w; s10 += w != 0; continue; }   /* NOP, FLUSHE / NOP */
      if (k == 19) { if (g[0] >> 28 != 1 || n || g[2] || w >> 24 != 0x14) break; snop++; continue; }   /* NOP, MSCAL */
      if (g[2] != (k < 5 ? 0x01000107u : k == 5 ? 0x01000207u : 0x01000101u)) break;   /* the slot's STCYCL */
      if (g[0] >> 28 == 1 && !n && !w) { snop++; continue; }   /* not sent: CNT 0, no IRQ, a NOP */
      if (g[0] >> 28 != 3) break;   /* REF, no IRQ */
      if (!num) num = 256;
      if (k < 5) { if ((w & 0xff00ffffu) != 0x6d008100u + (uint32_t)k || num != 2 * n) break; s6d++; }   /* V4-16 at T + 0x100 + k: 8 bytes a vector */
      else if (k == 5) { if ((w & 0xff00ffffu) != 0x6e00c105u || num != 4 * n) break; s6e++; }   /* V4-8 unsigned at T + 0x105: 4 bytes */
      else if (k < 10) { if (w != 0x6c0880e8u + 8u * (uint32_t)(k - 7) || n != 8) break; s6c++; }   /* V4-32 8 at T + 0xe8, 0xf0, 0xf8 */
      else { if (w != 0x6c040020u + 4u * (uint32_t)(k - 11) || n != 4) break; s6c++; }   /* V4-32 4 at abs 0x20 + 4 i */
      if (!(dp[k] = lane_ptr(g[1] & 0xfffffff0u, n))) { why = 4; break; }
    }
    if (k == 20 && !xl_lane_p07(t, tadr, dp)) { k = 0; why = 5; }
#ifndef __psp__
    if (k < 20 && !ns) lane7_miss[why]++;
#endif
    if (k < 20) break;
    ns++;
#ifndef __psp__
    nop += snop; n6c += s6c; n6d += s6d; n6e += s6e; n10 += s10;
#endif
    tadr += 384; t = (const uint32_t *)((tadr & 0x80000000u) ? g_spr + (tadr & 0x3ff0) : g_mem + (tadr & (GUEST_SIZE - 1) & ~15u));
    if (NAT_RUN_STOP(tadr)) break;
    if (t[2] != 0x01000107u) break;   /* the next tag does not start a slot */
  }
  if (!ns) return 0;
  s->left = 0; s->cmd = 0x14; s->cl = 1; s->wl = 1;
#ifndef __psp__
  vif1_n_cmd[0x01] += 18 * ns; vif1_n_cmd[0x6c] += n6c; vif1_n_cmd[0x6d] += n6d; vif1_n_cmd[0x6e] += n6e; vif1_n_cmd[0x10] += n10;
  vif1_n_cmd[0x14] += ns; vif1_n_cmd[0x00] += nop;
#endif
  *next = tadr; return 20 * ns;
}
/* sb6, Stage B lane 3 (renderer_notes 11.33): at tag t (as lane_p04), a run of the sort pass's effect prims (P12d910; re/mesh_packets.md
   7), each 10 quadwords and 6 tags as the builder writes them: REF the positions (STCYCL cl 3, 6 or 12 / wl 1, V4-32 at T + 0x100), REF the
   attributes (STCYCL 3 / 2, or 12 / 4 after cl 12; V4-16 signed at T + 0x101), REF 8 the owner's block (V4-32 at T + 0xe8), CNT 4 the
   matrix (V4-32 at T + 0x10, its data), REF 3 (V4-32 at T + 0x14: the count, the GIF tag and a third quadword), the last four with STCYCL
   1 / 1; a CNT of 0 with a NOP and MSCAL.  Each REF's UNPACK takes exactly the REF's data; no IRQ bit, every piece in one run of RAM or
   scratchpad.  The prims one after another while the next tag starts one, each handed to the translator (xl_lane_fx: one call each);
   afterwards the walk's state is what the generic walk leaves (STCYCL 1 / 1, no data left, the last command the MSCAL) and *next the tag
   after the last prim taken; 0: no prim here, or the translator declines the first */
static inline int lane_fx(const uint32_t *t, uint32_t tadr, VifLoc *s, uint32_t *next)
{
  int np = 0;
  for (;;) {
    uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : GUEST_SIZE - (tadr & (GUEST_SIZE - 1) & ~15u), cl = t[2] & 0xff;
    const uint32_t *dp[6]; int k, why = 0;
    if (room < 160u) {   /* the prim's 10 quadwords in one piece */
#ifndef __psp__
      if (!np) lane3_miss[0]++;
#endif
      break; }
    for (k = 0; k < 6; k++) {   /* tag k: quadword k; the REF 3 and the last CNT after the matrix's four data quadwords (k + 4) */
      const uint32_t *g = t + 4 * (k < 4 ? k : k + 4); uint32_t n = g[0] & 0xffff, w = g[3], num = (w >> 16) & 0xff;
      dp[k] = 0; why = k < 2 ? 1 + k : 3;
      if (!num) num = 256;
      if (k == 5) { if (g[0] >> 28 != 1 || n || g[2] || w >> 24 != 0x14) break; continue; }   /* CNT 0: NOP, MSCAL */
      if (k == 3) { if (g[0] >> 28 != 1 || n != 4 || g[2] != 0x01000101u || w != 0x6c048010u) break; continue; }   /* CNT 4: V4-32 4 at
                                                                                                         T + 0x10, its data */
      if (g[0] >> 28 != 3) break;   /* REF, no IRQ */
      if (k == 0) { if ((w & 0xff00ffffu) != 0x6c008100u || num != n) break; }   /* (its STCYCL tested before) V4-32 n at T + 0x100 */
      else if (k == 1) { if (g[2] != (cl == 12 ? 0x0100040cu : 0x01000203u) || (w & 0xff00ffffu) != 0x6d008101u || num != 2 * n) break; }
                                                                                    /* V4-16 signed at T + 0x101: 8 bytes a vector */
      else if (g[2] != 0x01000101u || w != (k == 2 ? 0x6c0880e8u : 0x6c038014u) || n != (k == 2 ? 8u : 3u)) break;   /* REF 8: V4-32 8 at
                                                                                    T + 0xe8; REF 3: V4-32 3 at T + 0x14 */
      if (!(dp[k] = lane_ptr(g[1] & 0xfffffff0u, n))) { why = 4; break; }
    }
    if (k == 6 && !xl_lane_fx(t, tadr, dp)) { k = 0; why = 5; }
#ifndef __psp__
    if (k < 6 && !np) lane3_miss[why]++;
#endif
    if (k < 6) break;
    np++;
    tadr += 160; t = (const uint32_t *)((tadr & 0x80000000u) ? g_spr + (tadr & 0x3ff0) : g_mem + (tadr & (GUEST_SIZE - 1) & ~15u));
    if (NAT_RUN_STOP(tadr)) break;
    if ((t[2] != 0x01000103u && t[2] != 0x01000106u && t[2] != 0x0100010cu) || (t[3] & 0xff00ffffu) != 0x6c008100u) break;   /* the next
                                                                                                tag does not start a prim */
  }
  if (!np) return 0;
  s->left = 0; s->cmd = 0x14; s->cl = 1; s->wl = 1;
#ifndef __psp__
  vif1_n_cmd[0x01] += 5 * np; vif1_n_cmd[0x6c] += 4 * np; vif1_n_cmd[0x6d] += np; vif1_n_cmd[0x00] += np; vif1_n_cmd[0x14] += np;
#endif
  *next = tadr; return 6 * np;
}
#endif
#ifdef NAT_HOOK
extern int g_nat_cfg;   /* native step 1: boot/nat_stub.h; the walk's hooks in boot/nat_x86.c (included by boot_x86.c) */
static int nat_walk(const uint32_t *t, uint32_t tadr, VifLoc *s, uint32_t *next, uint32_t chcr, uint32_t depth);
static void nat_xfer(uint32_t tadr);
#endif
static void vif1_chain_fast(uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr)   /* dma_feed for VIF1 */
{
  VifLoc s = { vif1.left, vif1.cmd, vif1.cl, vif1.wl };
  uint32_t asp[2], nasp = 0, guard = 0;
  if (((chcr >> 2) & 3) == 0) { VifLoc u = s; vif1_run_any(madr & ~0x80000000u, qwc, &u); s = u; goto out; }   /* normal mode (RAM only) */
  if (qwc) {                                            /* chain mode started with QWC > 0 (dma_feed) */
    uint32_t id = (chcr >> 28) & 7;
    VifLoc u = s;
    io_chain_qwc[1]++;
    vif1_run_any(madr, qwc, &u); s = u;
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) goto out;
  }
#ifdef NAT_HOOK
  if (g_nat_cfg) nat_xfer(tadr);
#endif
  for (;;) {
    const uint32_t *t = (const uint32_t *)((tadr & 0x80000000u) ? g_spr + (tadr & 0x3ff0) : g_mem + (tadr & (GUEST_SIZE - 1) & ~15u)), *p = 0;
    uint32_t t0 = t[0], t1 = t[1], n = t0 & 0xffff, id = (t0 >> 28) & 7, addr = t1 & 0xfffffff0u, data, next; int end = 0;
    XL_TAG(t, tadr);
#ifdef NAT_HOOK
    if (g_nat_cfg) { uint32_t nx_; int nt_ = nat_walk(t, tadr, &s, &nx_, chcr, nasp); if (nt_) { tadr = nx_; guard += (uint32_t)nt_; continue; } }
#endif
#ifdef MGS2_XLIVE
    if (g_xl_lane && g_xl_one && !s.left && (chcr & 0x40) && (t[2] & 0xffffff00u) == 0x01000100u) {   /* every lane's first tag starts with
                                                    STCYCL cl / 1 (sb6: one test of the tag's first TTE word, then its CL picks the lane) */
      uint32_t nx, cl = t[2] & 0xff; int nt = 0;
      if (cl == 1) {   /* lanes 1 and 2: both groups start with STCYCL 1 / 1 */
        int l = g_xl_lane;
        if (g_opt & OPT_LANE_MATCH) {   /* sb3: the TTE word of the tag after the group's first material (after the constant block's tag
                                           when t[3] is its UNPACK) is STCYCL 3 / 1 in every group lane_p04 takes and 1 / 1 in every one
                                           lane_p06 takes, and both need seven quadwords of room at least: so only that lane can take it */
          uint32_t room = (tadr & 0x80000000u) ? 0x4000u - (tadr & 0x3ff0) : GUEST_SIZE - (tadr & (GUEST_SIZE - 1) & ~15u), u;
          if (room < 112u) l = 0;
          else { u = t[t[3] == 0x6c188000u ? 10 : 6]; l &= u == 0x01000103u ? 1 : u == 0x01000101u ? 2 : 0; } }
        if (l & 1) nt = lane_p04(t, tadr, &s, &nx);
        if (!nt && (l & 2)) nt = lane_p06(t, tadr, &s, &nx); }
      else if (cl == 7) { if (g_xl_lane & 8) nt = lane_p07(t, tadr, &s, &nx); }   /* sb5: lane 4's slots start with STCYCL 7 / 1 */
      else if ((cl == 3 || cl == 6 || cl == 12) && (g_xl_lane & 4) && (t[3] & 0xff00ffffu) == 0x6c008100u) nt = lane_fx(t, tadr, &s, &nx);
                                                    /* sb6: lane 3's prims start with STCYCL 3, 6 or 12 / 1 and V4-32 to T + 0x100 (lane 1's
                                                       streams after STCYCL 3 / 1 are V4-16) */
      if (nt) { tadr = nx; guard += (uint32_t)nt; continue; } }
#ifndef __psp__
    if ((t[2] == 0x01000103u || t[2] == 0x01000106u || t[2] == 0x0100010cu) && (t[3] & 0xff00ffffu) == 0x6c008100u)   /* sb6 census: an effect
                                                    prim's first tag left to the generic walk */
      lane3_gen[!g_xl_one ? 4 : !(g_xl_lane & 4) ? 0 : s.left ? 2 : !(chcr & 0x40) ? 3 : 1]++;
#endif
#endif
    if (chcr & 0x40) { XL_ONE(t + 2, 2, tadr + 8); vif1_fast_words(t + 2, t + 4, &s); }   /* TTE: the tag's upper half */
    switch (id) {
    case 0: data = addr; next = 0; end = 1; break;                    /* refe */
    case 1: data = tadr + 16; next = data + 16 * n; break;              /* cnt */
    case 2: data = tadr + 16; next = addr; break;                       /* next */
    case 3: case 4: data = addr; next = tadr + 16; break;               /* ref, refs */
    case 5: data = tadr + 16; if (nasp < 2) asp[nasp++] = data + 16 * n; next = addr; break;   /* call */
    case 6: data = tadr + 16; if (nasp) next = asp[--nasp]; else { next = 0; end = 1; } break;   /* ret */
    default: data = tadr + 16; next = 0; end = 1; break;                /* end */
    }
    if (!(data & 0x80000000u)) { uint32_t b = data & (GUEST_SIZE - 1) & ~15u; if (b + 16 * n <= GUEST_SIZE) p = (const uint32_t *)(g_mem + b); }
    else { uint32_t o = data & 0x3ff0; if (o + 16 * n <= 0x4000u) p = (const uint32_t *)(g_spr + o); }
    if (p) { XL_ONE(p, 4 * n, data); vif1_fast_words(p, p + 4 * n, &s); }
    else { VifLoc u = s; vif1_run_any(data, n, &u); s = u; }
    if (end || ((chcr & 0x80) && (t0 >> 31))) break;
    tadr = next;
    if (++guard > 1000000) { fprintf(stderr, "dma %d: chain does not end\n", 1); break; }
  }
out:
  vif1.left = s.left; vif1.cmd = s.cmd; vif1.cl = s.cl; vif1.wl = s.wl;
#ifdef MGS2_XLIVE
  if (g_xl_one) xlive_one_end();
#endif
}
#if defined(GEN_DG_TRACE)
#define VIF1_FAST_OK 0
#elif defined(__psp__)
#define VIF1_FAST_OK ((g_opt & (OPT_GS_FAST | OPT_GS_BLOCK)) == (OPT_GS_FAST | OPT_GS_BLOCK))
#else
#define VIF1_FAST_OK ((g_opt & (OPT_GS_FAST | OPT_GS_BLOCK)) == (OPT_GS_FAST | OPT_GS_BLOCK) && !GSCAP_ON && !g_pkt_on)
#endif
/* VIF0: the full UNPACK set (S / V2 / V3 / V4 x 32 / 16 / 8, V4-5; sign or zero extension), the write cycle (CL >= WL skipping;
   WL > CL filling), STMOD offset / difference with ROW, STMASK with ROW / COL / write protect, MPG, MSCAL / MSCALF / MSCNT
   (the micro program runs at once, vu0.h).  Semantics: PS2 EE User's Manual, VIF chapter.  Unverified, noted: the components
   V2 / V3 leave undefined (V2 z w, V3 w) keep the VU memory value; a filling write's extra vectors take no input data. */
static struct { uint32_t row[4], col[4], mask, mode, cl, wl, unsup, unsup_code; } vif0 = { .cl = 1, .wl = 1 };
static void vif0_unpack(uint32_t c, const uint32_t *w, uint32_t avail, uint32_t *used)
{
  uint32_t cmd = (c >> 24) & 0x7f, num = (c >> 16) & 0xff, imm = c & 0xffff, vn = (cmd >> 2) & 3, vl = cmd & 3, msk = cmd & 0x10;
  uint32_t n = num ? num : 256, addr = imm & 0x3ff, usn = (imm >> 14) & 1, cl = vif0.cl, wl = vif0.wl ? vif0.wl : 1, k, nin = 0, bit = 0;
  uint32_t ecount = vn + 1, ebits = 32u >> vl; const uint8_t *src = (const uint8_t *)w;
  if (vl == 3 && vn != 3) { vif0.unsup++; vif0.unsup_code = c; }
  if ((g_opt & OPT_VIF0_FAST) && vn == 3 && vl <= 1 && !msk && vif0.mode <= 1 && cl == wl) {   /* V4-32 / V4-16, every vector written
       in full from its input (no mask, no skipping; STMOD none or offset): the same words as the loop below */
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0;
    if (vif0.mode == 1) { r0 = vif0.row[0]; r1 = vif0.row[1]; r2 = vif0.row[2]; r3 = vif0.row[3]; }
    for (k = 0; k < n; k++) {
      uint32_t o[4];
      if (vl == 0) memcpy(o, src + 16 * k, 16);
      else { uint16_t h[4]; memcpy(h, src + 8 * k, 8);
        if (usn) { o[0] = h[0]; o[1] = h[1]; o[2] = h[2]; o[3] = h[3]; }
        else { o[0] = (uint32_t)(int32_t)(int16_t)h[0]; o[1] = (uint32_t)(int32_t)(int16_t)h[1]; o[2] = (uint32_t)(int32_t)(int16_t)h[2]; o[3] = (uint32_t)(int32_t)(int16_t)h[3]; } }
      o[0] += r0; o[1] += r1; o[2] += r2; o[3] += r3;
      memcpy(VUMEM + (((addr + k) & 0xff) << 4), o, 16);
    }
    *used = vl == 0 ? 4 * n : 2 * n; return;
  }
  for (k = 0; k < n; k++) {
    uint32_t pos = cl >= wl ? k % wl : k % wl, off = cl >= wl ? (k / wl) * cl + (k % wl) : k, cyc = pos > 3 ? 3 : pos;
    int fill = wl > cl && pos >= cl;
    uint32_t v[4] = { 0, 0, 0, 0 }, have = 0, e;
    uint8_t *d = VUMEM + (((addr + off) & 0xff) << 4); uint32_t cur[4]; memcpy(cur, d, 16);
    if (!fill) {
      if (vl == 3 && vn == 3) {                           /* V4-5: one 16-bit RGBA5551 */
        uint16_t h; memcpy(&h, src + bit / 8, 2); bit += 16;
        v[0] = (h & 0x1f) << 3; v[1] = ((h >> 5) & 0x1f) << 3; v[2] = ((h >> 10) & 0x1f) << 3; v[3] = ((h >> 15) & 1) << 7; have = 4;
      } else {
        for (e = 0; e < ecount; e++) {
          uint32_t x = 0;
          if (ebits == 32) memcpy(&x, src + bit / 8, 4);
          else if (ebits == 16) { uint16_t h; memcpy(&h, src + bit / 8, 2); x = usn ? h : (uint32_t)(int32_t)(int16_t)h; }
          else { uint8_t b = src[bit / 8]; x = usn ? b : (uint32_t)(int32_t)(int8_t)b; }
          v[e] = x; bit += ebits;
        }
        if (vn == 0) { v[1] = v[2] = v[3] = v[0]; have = 4; } else have = ecount;
      }
      nin++;
    }
    for (e = 0; e < 4; e++) {
      uint32_t m = msk ? (vif0.mask >> ((cyc * 4 + e) * 2)) & 3 : 0, out;
      if (fill && m == 0) m = 3;                          /* no input data for a filling write's extra vector */
      if (m == 3) continue;                               /* write protect */
      if (m == 1) out = vif0.row[e];
      else if (m == 2) out = vif0.col[cyc];
      else {
        if (e >= have) { out = cur[e]; }                  /* undefined component */
        else if (vif0.mode == 1) out = v[e] + vif0.row[e];
        else if (vif0.mode == 2) { vif0.row[e] += v[e]; out = vif0.row[e]; }
        else out = v[e];
      }
      memcpy(d + 4 * e, &out, 4);
    }
  }
  (void)nin; (void)avail;
  *used = (bit + 31) / 32;
}
static void vu_unsup_scan(int pc, int n)   /* dev MGS2_VU_UNSUP=1: on the first unsupported op of each program image, list them */
{
  static uint32_t seen[64]; static int nseen; int k; uint32_t h = vu_hash();
  for (k = 0; k < nseen; k++) if (seen[k] == h) return; if (nseen < 64) seen[nseen++] = h;
  { static VuSnap sv; int p; vu_snap(&sv);
    fprintf(stderr, "vu0 image %08x entry %d: %d unsupported ops\n", h, pc, n);
    for (p = 0; p < 512; p++) { uint32_t lo = VUPROG[p * 2], hi = VUPROG[p * 2 + 1]; int v = g_unsup_vu;
      vu_upper(hi & 0x07ffffff); if (g_unsup_vu != v) fprintf(stderr, "  pc %d upper %08x\n", p, hi);
      v = g_unsup_vu; if (!(hi & 0x80000000u)) vu_lower(lo, p); if (g_unsup_vu != v) fprintf(stderr, "  pc %d lower %08x\n", p, lo);
      vu_restore(&sv); } }
}
static int vu_dbg, vu_dbg_n;              /* dev (boot.c MGS2_VU_DBG): dump the VU0 state around each MSCAL while set */
static void vu_dbg_dump(const char *tag, int pc)
{
  char fn[64]; FILE *f; snprintf(fn, sizeof fn, "vudbg.%03d.%s", vu_dbg_n, tag);
  if ((f = fopen(fn, "wb"))) { fwrite(VUPROG, 4, 1024, f); fwrite(VUMEM, 1, 4096, f); fwrite(VF, 16, 32, f); fwrite(VIr, 4, 16, f); fwrite(VACC, 4, 4, f);
    fwrite(&VQr, 4, 1, f); fwrite(&VIreg, 4, 1, f); fwrite(&VSTAT, 4, 1, f); fwrite(&VMAC, 4, 1, f); fwrite(&VCLIP, 4, 1, f); fwrite(&pc, 4, 1, f); fclose(f); }
}
static void vif0_mscal(int pc)          /* MSCAL / MSCNT */
{
  static int on = -1; int u = g_unsup_vu;
  if (on < 0) on = getenv("MGS2_VU_UNSUP") != 0;
  if (vu_dbg) { vu_dbg_dump("in", pc); fflush(stdout); printf("vudbg %d pc %d:", vu_dbg_n, pc); vu_trace = 1; }
  vu_run(pc);
  if (vu_dbg) { vu_trace = 0; printf("\n"); fflush(stdout); vu_dbg_dump("out", vu_tpc); vu_dbg_n++; }
  if (on && g_unsup_vu != u) vu_unsup_scan(pc, g_unsup_vu - u);
}
#ifdef VIF0_STAT   /* dev: MGS2_VIF0_STAT=<first vb>:<last vb>: VIF0 transfers, MPG uploads (words that change the program), UNPACK formats, MSCALs
                      that rehash the program (printed in io_dump) */
static long io_vb;
static long vs_on = -2, vs_a, vs_b, vs_xfer, vs_words, vs_mpg, vs_mpgw, vs_mpgchg, vs_mpgchgw, vs_mscal, vs_rehash, vs_unk[4096], vs_unv[4096];
static int vs_in(void) { if (vs_on == -2) { const char *e = getenv("MGS2_VIF0_STAT"); vs_on = e && sscanf(e, "%ld:%ld", &vs_a, &vs_b) == 2; } return vs_on && io_vb >= vs_a && io_vb <= vs_b; }
#endif
static void vif0_run(const uint32_t *w, uint32_t n)
{
  uint32_t i = 0;
#ifdef VIF0_STAT
  int vs = vs_in(); uint32_t vs_cl = vif0.cl, vs_wl = vif0.wl, vs_mode = vif0.mode; if (vs) { vs_xfer++; vs_words += n; }
  while (vs && i < n) {     /* the same walk, counting only (before the run below changes anything) */
    uint32_t c = w[i++], cmd = (c >> 24) & 0x7f, num = (c >> 16) & 0xff, imm = c & 0xffff, k;
    if (cmd == 0x20) i += 1; else if (cmd == 0x30 || cmd == 0x31) i += 4;
    else if (cmd == 0x4a) { uint32_t cnt = num ? num : 256, ch = 0; for (k = 0; k < cnt * 2 && i + k < n; k++) ch += VUPROG[(imm * 2 + k) & 1023] != w[i + k];
      vs_mpg++; vs_mpgw += cnt * 2; if (ch) { vs_mpgchg++; vs_mpgchgw += ch; } i += cnt * 2; }
    else if (cmd >= 0x60) { uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, nn = num ? num : 256, key = (cmd & 0x1f) | ((imm >> 14) & 1) << 5 | (vs_mode & 3) << 6 | (vs_cl == vs_wl) << 8 | (vs_wl > vs_cl) << 9;
      uint32_t bits = (vl == 3 && vn == 3) ? 16 : (32u >> vl) * (vn + 1), nin = nn;
      if (vs_wl > vs_cl && vs_wl) nin = nn / vs_wl * vs_cl + (nn % vs_wl < vs_cl ? nn % vs_wl : vs_cl);
      vs_unk[key]++; vs_unv[key] += nn; i += (nin * bits + 31) / 32; }
    else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) vs_mscal++;
    else if (cmd == 0x01) { vs_cl = imm & 0xff; vs_wl = (imm >> 8) & 0xff; } else if (cmd == 0x05) vs_mode = imm & 3;
    else if (cmd != 0x00 && cmd != 0x04 && cmd != 0x07 && cmd != 0x10 && cmd != 0x11 && cmd != 0x13) break;
  }
  i = 0;
#endif
  while (i < n) {
    uint32_t c = w[i++], cmd = (c >> 24) & 0x7f, num = (c >> 16) & 0xff, imm = c & 0xffff, k;
    switch (cmd) {
    case 0x00: case 0x04: case 0x07: case 0x10: case 0x11: case 0x13: break;            /* NOP, ITOP, MARK, FLUSHE, FLUSH, FLUSHA */
    case 0x01: vif0.cl = imm & 0xff; vif0.wl = (imm >> 8) & 0xff; break;                 /* STCYCL */
    case 0x05: vif0.mode = imm & 3; break;                                               /* STMOD */
#ifdef VIF0_STAT
    case 0x14: case 0x15: if (vs && !vu_prog_clean) vs_rehash++; vif0_mscal(imm); break;
#else
    case 0x14: case 0x15: vif0_mscal(imm); break;                                        /* MSCAL, MSCALF */
#endif
    case 0x17: vif0_mscal(vu_tpc); break;                                                /* MSCNT */
    case 0x20: if (i < n) vif0.mask = w[i]; i += 1; break;                               /* STMASK */
    case 0x30: for (k = 0; k < 4 && i + k < n; k++) vif0.row[k] = w[i + k]; i += 4; break;   /* STROW */
    case 0x31: for (k = 0; k < 4 && i + k < n; k++) vif0.col[k] = w[i + k]; i += 4; break;   /* STCOL */
    case 0x4a: {                                                                         /* MPG: num 64-bit words at imm */
      uint32_t cnt = num ? num : 256, b = (imm * 2) & 1023;
      if ((g_opt & OPT_VIF0_FAST) && i + cnt * 2 <= n && b + cnt * 2 <= 1024) {       /* the game uploads the same program again most of the
                                                                                         time: then VUPROG and its hash stay as they are */
        const uint32_t *s = w + i; uint32_t *d = VUPROG + b, m = cnt * 2;
        k = 0;
        if (g_opt & OPT_MPG_CMP)   /* sb3: eight words at a time up to the eight that differ, then the pairs as below: the same k */
          for (; k + 8 <= m; k += 8) if ((s[k] ^ d[k]) | (s[k + 1] ^ d[k + 1]) | (s[k + 2] ^ d[k + 2]) | (s[k + 3] ^ d[k + 3]) |
                                         (s[k + 4] ^ d[k + 4]) | (s[k + 5] ^ d[k + 5]) | (s[k + 6] ^ d[k + 6]) | (s[k + 7] ^ d[k + 7])) break;
        for (; k < m; k += 2) if ((s[k] ^ d[k]) | (s[k + 1] ^ d[k + 1])) break;
        if (k < m) { memcpy(d + k, s + k, 4 * (m - k)); vu_prog_clean = 0; }
        i += m; break;
      }
      for (k = 0; k < cnt * 2 && i + k < n; k++) VUPROG[(imm * 2 + k) & 1023] = w[i + k];
      vu_prog_clean = 0; i += cnt * 2; break; }
    default:
      if (cmd >= 0x60) { uint32_t used; vif0_unpack(c, w + i, n - i, &used); i += used; break; }
      vif0.unsup++; vif0.unsup_code = c; return;                                         /* unknown: the rest of the transfer is not decodable */
    }
  }
}
#ifndef VIF0_BUF_WORDS
#define VIF0_BUF_WORDS 0x40000                  /* the PSP build: 0x4000 (the boot-to-deck run's largest transfer is 2618 words) */
#endif
static uint32_t vif0_buf[VIF0_BUF_WORDS], vif0_n;     /* a VIF0 transfer, collected and then run through vif0_run */
static uint32_t vif0_max;                              /* the largest transfer (words), in io_dump: the PSP sizes vif0_buf from it */
static void dma_vif0_qw(const uint8_t *q, int tag)
{
  if (vif0_n + 4 > sizeof vif0_buf / 4) { static int warned; if (!warned++) fprintf(stderr, "dma VIF0: transfer larger than the buffer (%u words): raise VIF0_BUF_WORDS\n", (unsigned)(sizeof vif0_buf / 4)); return; }
  if (tag) { memcpy(vif0_buf + vif0_n, q + 8, 8); vif0_n += 2; return; }
  memcpy(vif0_buf + vif0_n, q, 16); vif0_n += 4;
}
static void dma_vif0_block(const uint8_t *p, uint32_t nq)   /* OPT_VIF0_FAST: dma_vif0_qw for nq quadwords at once */
{
  uint32_t room = (uint32_t)(sizeof vif0_buf / 4 - vif0_n) / 4, k = nq < room ? nq : room;
  memcpy(vif0_buf + vif0_n, p, 16 * k); vif0_n += 4 * k;
  if (k < nq) dma_vif0_qw(p + 16 * k, 0);                   /* (the buffer is full: the same warning, the rest is dropped) */
}
static const uint32_t io_dma_base[10] = { 0x8000, 0x9000, 0xa000, 0xb000, 0xb400, 0xc000, 0xc400, 0xc800, 0xd000, 0xd400 };
static const int8_t io_dma_at[22] = { 0, -1, -1, -1, 1, -1, -1, -1, 2, -1, -1, -1, 3, 4, -1, -1, 5, 6, 7, -1, 8, 9 };   /* the channel whose
                                                         io_dma_base is 0x8000 + 0x400 * i, else -1 */
static const char *const io_dma_name[10] = { "VIF0", "VIF1", "GIF", "fromIPU", "toIPU", "SIF0", "SIF1", "SIF2", "fromSPR", "toSPR" };
static int io_spr_chain_warned;
static __attribute__((noinline)) void io_dma_start(int ch)   /* sb3: not inlined, so io_flush's other register writes do not save and
                                                         restore the registers the walk needs (renderer_notes 11.28) */
{
  uint32_t b = io_dma_base[ch], chcr = IO32(b), madr = IO32(b + 0x10), qwc = IO32(b + 0x20) & 0xffff, tadr = IO32(b + 0x30);
  io_dma_count[ch]++;
  if (io_log && io_dma_count[ch] <= 4) fprintf(stderr, "dma %s chcr %08x madr %08x qwc %x tadr %08x\n", io_dma_name[ch], chcr, madr, qwc, tadr);
#ifdef MGS2_XLIVE
  if ((ch == 1 || ch == 2) && (chcr & 1)) xlive_chk_dma(g_mem, g_spr, ch, chcr, madr, qwc, tadr, ch == 1 && VIF1_FAST_OK);   /* before the exact
                                                         walk, reading only */
#endif
  if (ch == 2 && (chcr & 1)) { HOST_ENTER(HOST_GS); dma_feed(ch, chcr, madr, qwc, tadr, dma_gif_qw, dma_gif_block); HOST_LEAVE(HOST_GS); }
  if (ch == 1 && (chcr & 1)) { HOST_ENTER(HOST_GS); if (VIF1_FAST_OK) vif1_chain_fast(chcr, madr, qwc, tadr);
                              else dma_feed(ch, chcr, madr, qwc, tadr, dma_vif1_qw, dma_vif1_block); HOST_LEAVE(HOST_GS); }
  if (ch == 0 && (chcr & 1)) {
    uint32_t u = vif0.unsup; vif0_n = 0; dma_feed(ch, chcr, madr, qwc, tadr, dma_vif0_qw, dma_vif0_block); if (vif0_n > vif0_max) vif0_max = vif0_n; vif0_run(vif0_buf, vif0_n);
    if (vif0.unsup != u && io_log) {
      static int shown;
      fprintf(stderr, "dma VIF0: unsupported VIF code %08x in the transfer at %08x chcr %08x tadr %08x (%u so far)\n", vif0.unsup_code, madr, chcr, tadr, vif0.unsup);
      if (!shown++ && getenv("MGS2_VIF0_DUMP")) { FILE *df = fopen(getenv("MGS2_VIF0_DUMP"), "wb"); if (df) { fwrite(vif0_buf, 4, vif0_n, df); fclose(df); } }
    }
  }
  if (ch == 8 || ch == 9) {                             /* scratchpad: QWC quadwords between the scratchpad (SADR) and MADR */
    uint32_t sadr = IO32(b + 0x80) & 0x3ff0, mode = (chcr >> 2) & 3, sq = IO32(0xe030), tq = (sq >> 16) & 0xff, sk = sq & 0xff, m = madr, q, k = 0;
    { static long seen[2][16]; if (!seen[ch - 8][(chcr >> 2) & 15]++ && getenv("MGS2_SPR_LOG"))      /* dev: first transfer of each mode */
        fprintf(stderr, "dma %s mode %u chcr %08x madr %08x qwc %x sadr %08x tadr %08x sqwc %08x\n", io_dma_name[ch], mode, chcr, madr, qwc, sadr, tadr, sq); }
    if (mode == 1 && !io_spr_chain_warned++) fprintf(stderr, "dma %s: chain mode not modelled (chcr %08x)\n", io_dma_name[ch], chcr);
    if ((g_opt & OPT_SPR_BULK) && qwc && sadr + 16 * qwc <= 0x4000) {   /* runs of quadwords at once (all of them, or TQWC at a time when
         interleaved): the loop below's copies when the RAM side neither wraps nor reaches the scratchpad's bytes (RAM 0..16 KB under GUEST_SPR0) */
      uint32_t run = mode == 2 && tq ? tq : qwc, nrun = (qwc + run - 1) / run, m0 = m & (GUEST_SIZE - 1) & ~15u;
      uint32_t span = 16 * qwc + (nrun - 1) * 16 * sk;
      if (m0 >= 0x4000 && m0 + span <= GUEST_SIZE) {
        for (q = 0; q < qwc; q += run) {
          uint32_t nq = qwc - q < run ? qwc - q : run;
          uint8_t *mm = g_mem + (m & (GUEST_SIZE - 1) & ~15u), *ss = g_spr + sadr + 16 * q;
          if (ch == 8) SPR_COPY(mm, ss, 16 * nq); else SPR_COPY(ss, mm, 16 * nq);   /* the two sides share no line: RAM side from 0x4000 */
#ifdef GUEST_SPR0
          if (ch == 8) DGT_COPY((uint32_t)(mm - g_mem), (uint32_t)(ss - g_mem), 16 * nq); else DGT_COPY((uint32_t)(ss - g_mem), (uint32_t)(mm - g_mem), 16 * nq);
#endif
          m += 16 * nq; if (q + nq < qwc) m += 16 * sk;
        }
        q = qwc;
      } else q = 0;
    } else q = 0;
    for (; q < qwc; q++) {                         /* interleave (mode 2, D_SQWC): TQWC quadwords, then MADR skips SQWC; SADR is contiguous */
      uint8_t *mm = g_mem + (m & (GUEST_SIZE - 1) & ~15u), *ss = g_spr + ((sadr + 16 * q) & 0x3ff0);
      if (ch == 8) memcpy(mm, ss, 16); else memcpy(ss, mm, 16);
#ifdef GUEST_SPR0
      if (ch == 8) DGT_COPY((uint32_t)(mm - g_mem), (uint32_t)(ss - g_mem), 16); else DGT_COPY((uint32_t)(ss - g_mem), (uint32_t)(mm - g_mem), 16);
#endif
      m += 16;
      if (mode == 2 && tq && ++k == tq && q + 1 < qwc) { k = 0; m += 16 * sk; }
    }
    IO32(b + 0x10) = m; IO32(b + 0x80) = (sadr + 16 * qwc) & 0x3fff;   /* the registers end past the data, as on hardware */
  }
  IO32(b) = chcr & ~0x100u; IO32(b + 0x20) = 0;          /* done */
  io_dstat |= 1u << ch; IO32(0xe010) = io_dstat;
  k_dmac_raise(ch);
}
static void io_flush(void)
{
  uint32_t p = g_io_pend, v; int ch;
  g_io_pend = 0;
  p = (p - 0x10000000u) & ~3u; v = IO32(p);
  if (p - 0x8000u <= 0x5400u && !(p & 0x3ffu) && (ch = io_dma_at[(p - 0x8000u) >> 10]) >= 0) { if (v & 0x100) io_dma_start(ch); return; }   /* p == io_dma_base[ch] */
  if (p < 0x2000u && (p & 0x7ffu) == 0) { io_tcount0[p >> 11] = v & 0xffff; io_tbase[p >> 11] = K_READ(); return; }   /* timer COUNT write */
  if (p < 0x2000u && (p & 0x7ffu) == 0x10) {                                                                       /* timer MODE write */
    int i = p >> 11;
    io_tcount0[i] = io_timer_count(i); io_tbase[i] = K_READ();    /* latch under the old mode, then count on under the new one */
    IO32(p) = (v & ~0xc00u) | (io_tmode[i] & 0xc00u & ~v); io_tmode[i] = IO32(p); return;
  }
  switch (p) {
  case 0xe010:                                          /* D_STAT: bits 0..9 write 1 to clear, 16..25 write 1 to toggle the mask */
    io_dstat = (io_dstat & ~(v & 0x3ffu)) ^ (v & 0x3ff0000u); IO32(p) = io_dstat; return;
  case 0xf000: io_istat &= ~v; IO32(p) = io_istat; return;   /* I_STAT: write 1 to clear */
  case 0xf010: IO32(p) = 0; return;                     /* I_MASK: the kernel layer keeps the enable mask (EnableIntc) */
  case 0x2001000:                                       /* GS CSR: bits 0..4 write 1 to clear, bit 9 reset */
    io_csr &= ~(v & 0x1fu); IO32(p) = io_csr; return;
  }
}
static long io_vb;                                       /* vblank starts seen (= k_vblanks) */
static void io_vblank(int start)
{
  { static int on = -1, last; if (on < 0) { on = getenv("MGS2_VU_UNSUP") != 0; l41_init(); }   /* dev: unsupported ops seen since the last vblank (EE VCALLMS); gl41: l41_init (runtime/vu_l41.h) */
    if (on && g_unsup_vu != last) { vu_unsup_scan(-1, g_unsup_vu - last); last = g_unsup_vu; } }
  if (start) { io_csr |= 8; io_csr ^= 1u << 13; io_istat |= 4; gscap_vblank(++io_vb); } else io_istat |= 8;
  L41_VBLANK(start, io_vb);   /* gl41: the levers' counters (MGS2_L41_LOG=1) */
  IO32(0x2001000) = io_csr; IO32(0xf000) = io_istat;
}
static void io_init(void) { }
static void io_dump(void)
{
  uint32_t i, s = 0; int in = 0, ch;
  fprintf(stderr, "EE/GS register ranges touched:\n");
  for (i = 0; i <= sizeof g_io_seen; i++) {
    int v = i < sizeof g_io_seen && g_io_seen[i];
    if (v && !in) { s = i; in = 1; } else if (!v && in) { fprintf(stderr, "  %08x..%08x\n", 0x10000000u + (s << 8), 0x10000000u + (i << 8) - 1); in = 0; }
  }
  for (ch = 0; ch < 10; ch++) if (io_dma_count[ch]) fprintf(stderr, "  dma %s: %ld transfers (%ld chain starts with QWC)\n", io_dma_name[ch], io_dma_count[ch], io_chain_qwc[ch]);
  fprintf(stderr, "  dma VIF0: largest transfer %u words (buffer %u)\n", vif0_max, (unsigned)(sizeof vif0_buf / 4));
#ifdef VIF0_STAT
  if (vs_on > 0) { int k; fprintf(stderr, "VIF0 stat vb %ld-%ld: %ld transfers, %ld words; MPG %ld (%ld words), %ld changed the program (%ld words); MSCAL %ld, %ld rehash\n",
    vs_a, vs_b, vs_xfer, vs_words, vs_mpg, vs_mpgw, vs_mpgchg, vs_mpgchgw, vs_mscal, vs_rehash);
    for (k = 0; k < 4096; k++) if (vs_unk[k]) fprintf(stderr, "VIF0 unpack cmd %02x usn %d mode %d cl==wl %d fill %d: %ld x, %ld vectors\n", 0x60 | (k & 0x1f), (k >> 5) & 1, (k >> 6) & 3, (k >> 8) & 1, (k >> 9) & 1, vs_unk[k], vs_unv[k]); }
#endif
  fprintf(stderr, "GS: GIF tags path2 %ld path3 %ld, FINISH %ld, SIGNAL %ld; VIF1 interrupt bits %ld; VIF1 commands:", gs_n_tags[2], gs_n_tags[3],
          gs_n_finish, gs_n_signal, vif1_n_irq);
  for (ch = 0; ch < 128; ch++) if (vif1_n_cmd[ch]) fprintf(stderr, " %02x:%ld", ch, vif1_n_cmd[ch]);
  fprintf(stderr, "\n");
}
#endif
