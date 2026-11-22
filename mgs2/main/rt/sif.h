/* SIF: the EE <-> IOP link for the x86 development host.  The EE side is the game's own libkernel code (recompiled) talking to the SIF
   through the kernel calls sceSifSetDma / sceSifDmaStat / sceSifSetDChain / sceSifSetReg / sceSifGetReg; the IOP side runs the game's
   recompiled IOP modules (PS2RC runtime, third_party/ps2rc) with sifcmd / sifman implemented here on the documented wire protocol
   (packet layouts: ps2sdk common/include/sifcmd-common.h, sifrpc-common.h; the same protocol as Sony's libraries).
   - EE -> IOP (SIF1): a transfer is copied into IOP memory at once; a packet sent to the IOP receive buffer with SIF_DMA_INT_O is
     dispatched at once (system commands here, user commands to the handlers the modules registered, in IOP interrupt context).
   - IOP -> EE (SIF0): transfers and command packets go through one FIFO, in order.  A command packet lands in the EE receive buffer
     and raises the EE DMAC channel 5 interrupt; the next one waits until the EE handler re-arms the channel (isceSifSetDChain), as
     the hardware chain does.  sceSifDmaStat reports every transfer as finished (it sits in the FIFO already: an IOP thread that polls
     it must not spin, the EE does not run in parallel here).
   - RPC: binds and calls to servers the IOP modules registered run on their server threads; servers with no IOP module (loadfile,
     cdvdfsv, ...) are host functions (sif_host_server).
   Needs ee_io.h, kernel.h (EE side) and the PS2RC runtime internals (iop_rt_int.h) first. */
#ifndef SIF_H
#define SIF_H
#include "host_time.h"
#include "iop_rt_int.h"

static IopRt *g_iop;
static int sif_log = 2;
static uint32_t sif_reg[5];                       /* 1 MSCOM, 2 SMCOM, 3 MSFLG, 4 SMFLG */
static uint32_t sif_sysreg[4];                    /* 0x80000000 SUBADDR, ..01 MAINADDR, ..02 RPCINIT: kept by the EE kernel */
static uint32_t sif_ee_pktbuf;                    /* EE address of the EE receive buffer (INIT_CMD / CHANGE_SADDR) */
static uint32_t sif_iop_rbuf, sif_iop_copy, sif_iop_spkt;   /* IOP receive buffer, handler copy, packets the host sends */
static int sif_reboot_pending, sif_ee_armed, sif_rpc_inited;
static uint32_t sif_iop_sreg[32];
static uint32_t sif_dma_id;
#define SIF_STAT_SIFINIT 0x10000u
#define SIF_STAT_CMDINIT 0x20000u
#define SIF_STAT_BOOTEND 0x40000u
#define SIF_DMA_INT_O 0x4u

static uint32_t ee_rd32(uint32_t a) { return RD32(a); }
static void ee_copy_to(uint32_t ee, const uint8_t *src, uint32_t n) { uint32_t i; DGT_PRE(ee, n); for (i = 0; i < n; i++) WR8(ee + i, src[i]); }   /* me13: DGT_PRE, the ME hand-off's guard (ee_io.h ME_GUARD_RT) */
static void ee_copy_from(uint8_t *dst, uint32_t ee, uint32_t n) { uint32_t i; for (i = 0; i < n; i++) dst[i] = RD8(ee + i); }
static uint8_t *iop_p(uint32_t a) { return g_iop->mem + (a & IOP_MEM_MASK); }

/* ---- IOP -> EE FIFO ---- */
typedef struct { int cmd; uint32_t dest, n; uint8_t *data; } SifItem;
#define SIF_FIFO 256
static SifItem sif_fifo[SIF_FIFO]; static int sif_fh, sif_ft;
static int sif_in_run;                            /* inside sif_iop_run_body: the EE does not run until its sif_ee_deliver */
static uint32_t sif_npush;                        /* items pushed, delivered at once or queued (sif_iop_run_body's settle test) */
/* Data pushed during an IOP run lands in EE memory at once, after the data queued ahead of it, unless a command waits ahead: the
   same writes in the same order as sif_ee_deliver at the run's end, and the EE runs only after that.  Until 2026-10-27 every
   item was copied into the heap first: a 1,110 KB codec load (34 x 32 KB in one run, vb 15,681 and 26,108 indoors) held it all
   at once, and on the PSP malloc returned 0 at vb 26,108 (gl2d's crash, a write to address 0 in memcpy) */
static void sif_push(int cmd, uint32_t dest, const uint8_t *src, uint32_t n)
{
  SifItem *it;
  sif_npush++;
  if (!cmd && sif_in_run) {
    while (sif_fh != sif_ft && !sif_fifo[sif_fh % SIF_FIFO].cmd) {
      it = &sif_fifo[sif_fh % SIF_FIFO]; ee_copy_to(it->dest, it->data, it->n); free(it->data); sif_fh++; }
    if (sif_fh == sif_ft) { ee_copy_to(dest, src, n); return; }
  }
  it = &sif_fifo[sif_ft % SIF_FIFO];
  if (sif_ft - sif_fh >= SIF_FIFO) { fprintf(stderr, "sif: IOP->EE FIFO full\n"); exit(7); }
  it->cmd = cmd; it->dest = dest; it->n = n; it->data = malloc(n ? n : 1);
  if (!it->data) { fprintf(stderr, "sif: no memory for %u bytes of IOP->EE data (%d items queued)\n", n, sif_ft - sif_fh); exit(7); }
  memcpy(it->data, src, n); sif_ft++;
}
static void sif_ee_deliver(void)
{
  while (sif_fh != sif_ft) {
    SifItem *it = &sif_fifo[sif_fh % SIF_FIFO];
    if (it->cmd && (!sif_ee_armed || !sif_ee_pktbuf)) return;
    ee_copy_to(it->cmd ? sif_ee_pktbuf : it->dest, it->data, it->n);
    free(it->data); sif_fh++;
    if (it->cmd) { if (sif_log > 1) fprintf(stderr, "sif: delivered to EE %08x\n", sif_ee_pktbuf); sif_ee_armed = 0; k_dmac_raise(5); return; }
  }
}
/* an IOP -> EE command: packet (psize bytes, header filled here) and optional extra data copied to dest first */
static void sif_send_to_ee(uint32_t cid, uint8_t *pkt, uint32_t psize, const uint8_t *extra, uint32_t dest, uint32_t esize)
{
  uint32_t h0 = (psize & 0xff) | (esize << 8);
  memcpy(pkt, &h0, 4); memcpy(pkt + 4, &dest, 4); memcpy(pkt + 8, &cid, 4);
  if (sif_log) fprintf(stderr, "sif: IOP->EE cmd %08x psize %u extra %u -> %08x\n", cid, psize, esize, dest);
  if (esize && extra) sif_push(0, dest, extra, esize);
  sif_push(1, 0, pkt, psize);
}

/* ---- RPC servers ---- */
typedef int (*SifHostFn)(uint32_t fno, uint32_t buf, uint32_t ssize, uint32_t *rep);   /* rep: IOP address of the reply (0: none) */
typedef struct { uint32_t sid, sd, buf; SifHostFn fn; const char *name; } SifHostSrv;
static SifHostSrv sif_hsrv[32]; static int sif_nhsrv;
static void sif_host_server(uint32_t sid, SifHostFn fn, uint32_t bufsize, const char *name)
{
  SifHostSrv *s = &sif_hsrv[sif_nhsrv++];
  s->sid = sid; s->fn = fn; s->name = name; s->sd = iop_rt_alloc(g_iop, 0x80, 0, 0); s->buf = iop_rt_alloc(g_iop, bufsize, 0, 0);
  memset(iop_p(s->sd), 0, 0x80); memcpy(iop_p(s->sd), &sid, 4);
}
static void (*sif_after_end)(void);
static void (*sif_iop_tick)(void);
static void (*sif_step_host)(uint64_t ns);         /* host hardware on the IOP clock (SPU2 voices): runs each timer step */
static void (*sif_vblank_host)(void);             /* host-side IOP modules' per-frame work (padman's pad data DMA) */                /* host-side IOP events (disc callbacks), run between IOP thread slices */
typedef struct { int used, active; uint32_t pkt[16]; uint8_t *reply; IopQueue *q; IopServer *s; } SifCall;
static SifCall sif_calls[32];

static void sif_rpc_end(const uint32_t *call, uint32_t cid, uint32_t rep_iop, uint32_t rsize, const uint8_t *rep_host)
{
  /* call: the request packet (words); reply: SifRpcRendPkt_t { hdr, rec_id, pkt_addr, rpc_id, cd, cid, sd, buf, cbuf } */
  uint32_t rp[16] = { 0 }; uint32_t rmode = call[12], recvbuf = call[10];
  rp[4] = call[4]; rp[5] = call[5]; rp[6] = call[6]; rp[7] = call[7]; rp[8] = cid;
  if (!rmode) {                                   /* no-wait call: the reply data and the packet go by plain DMA, the EE polls */
    if (rsize) sif_push(0, recvbuf, rep_host, rsize);
    rp[4] = 0; rp[6] = 0; { uint32_t h0 = 64; memcpy(rp, &h0, 4); rp[2] = 0x80000008u; }
    sif_push(0, call[5], (uint8_t *)rp, 64);
  } else sif_send_to_ee(0x80000008u, (uint8_t *)rp, 64, rep_host, recvbuf, rsize);
  (void)rep_iop;
}
static IopServer *sif_rt_server(uint32_t by_sd, uint32_t by_sid)
{
  int i;
  for (i = 0; i < IOP_MAX_SERVERS; i++) if (g_iop->srv[i].used && (by_sd ? g_iop->srv[i].sd == by_sd : g_iop->srv[i].sid == by_sid)) return &g_iop->srv[i];
  return 0;
}
static IopQueue *sif_rt_queue(uint32_t addr)
{
  int i;
  for (i = 0; i < IOP_MAX_QUEUES; i++) if (g_iop->q[i].used && g_iop->q[i].addr == addr) return &g_iop->q[i];
  return 0;
}
/* start the next waiting call of each server queue; finish the calls their server thread has answered */
static void sif_rpc_poll(void)
{
  int i, j;
  for (i = 0; i < 32; i++) {
    SifCall *c = &sif_calls[i];
    if (!c->used || !c->active || !c->q->req.done) continue;
    c->q->req.done = 0;
    sif_rpc_end(c->pkt, 0x8000000au, 0, c->pkt[11], c->reply);
    free(c->reply); c->used = c->active = 0;
  }
  for (i = 0; i < 32; i++) {
    SifCall *c = &sif_calls[i]; int busy = 0;
    if (!c->used || c->active) continue;
    for (j = 0; j < 32; j++) if (sif_calls[j].used && sif_calls[j].active && sif_calls[j].q == c->q) busy = 1;
    if (busy || c->q->req.pending) continue;
    c->active = 1; c->reply = calloc(1, c->pkt[11] + 16);
    if (!c->reply) { fprintf(stderr, "sif: no memory for a %u-byte RPC reply\n", c->pkt[11]); exit(7); }
    c->q->req.pending = 1; c->q->req.done = 0; c->q->req.sid = c->s->sid; c->q->req.fno = c->pkt[8];
    c->q->req.send = 0; c->q->req.ssize = c->pkt[9]; c->q->req.recv = c->reply; c->q->req.rsize = c->pkt[11];
    if (c->q->thread >= 0) { IopThread *T = &g_iop->th[c->q->thread]; if (T->state == TS_WAIT && T->wait == W_RPC) iop_rt_ready(g_iop, c->q->thread, KE_OK); }
  }
}
static void sif_iop_run_body(void)
{
  int k, was = sif_in_run;
  sif_in_run = 1;
  for (k = 0; k < 64; k++) {                      /* until the answered calls and the started ones settle */
    uint32_t before = sif_npush;
    iop_rt_run(g_iop); if (sif_iop_tick) sif_iop_tick(); sif_rpc_poll();
    if (sif_npush == before) { int act = 0, i; for (i = 0; i < 32; i++) if (sif_calls[i].used && !sif_calls[i].active) act = 1; if (!act) break; }
  }
  sif_in_run = was;
  sif_ee_deliver();
}
static void sif_iop_run(void) { HOST_ENTER(HOST_IOP); sif_iop_run_body(); HOST_LEAVE(HOST_IOP); }

/* ---- IOP command dispatch (an EE -> IOP packet arrived in the IOP receive buffer) ---- */
typedef struct { uint32_t fn, harg, gp; } SifIopH;
static SifIopH sif_sys_h[32];                     /* system commands the modules handle themselves */
static uint32_t sif_usr_tab, sif_usr_n, sif_usr_gp[256];
static void sif_iop_dispatch(void)
{
  uint8_t *p = iop_p(sif_iop_rbuf); uint32_t w[32], psize = p[0], cid, id;
  if (!psize || psize > 128) { fprintf(stderr, "sif: bad packet size %u\n", psize); return; }
  memcpy(w, p, psize); memcpy(iop_p(sif_iop_copy), p, psize); p[0] = 0;
  cid = w[2]; id = cid & 0x7fffffffu;
  if (sif_log) fprintf(stderr, "sif: EE->IOP cmd %08x psize %u dsize %u dest %08x opt %08x w4 %08x w5 %08x w6 %08x w7 %08x w8 %08x\n", cid, psize, w[0] >> 8, w[1], w[3], w[4], w[5], w[6], w[7], w[8]);
  if (getenv("MGS2_SIF_BT") && w[8] == strtoul(getenv("MGS2_SIF_BT"), 0, 16)) {   /* dev: likely return addresses on the guest stack */
    uint32_t sp = (uint32_t)R[29], a, v, i; fprintf(stderr, "sif-bt: sp %08x ra %06x:", sp, (uint32_t)R[31]);
    for (i = 0, a = sp; i < 1024; i++, a += 4) { v = RD32(a);
      if (v >= 0x100008 && v < 0x1000000 && !(v & 3)) { uint32_t c = RD32(v - 8); if (c >> 26 == 3 || (c >> 26 == 0 && (c & 63) == 9)) fprintf(stderr, " %06x@+%x", v, a - sp); } }
    fprintf(stderr, "\n"); }
  if (cid & 0x80000000u) {
    switch (id) {
    case 0: sif_ee_pktbuf = w[4]; return;                                            /* CHANGE_SADDR */
    case 1: if (w[4] < 32) sif_iop_sreg[w[4]] = w[5]; return;                        /* SET_SREG */
    case 2:                                                                          /* INIT_CMD */
      if (!w[3]) { sif_ee_pktbuf = w[4]; sif_reg[3] |= SIF_STAT_CMDINIT; }
      else if (!sif_rpc_inited) {                    /* RPC init: the IOP's sceSifInitRpc answers with SET_SREG(SIF_SREG_RPCINIT, 1) */
        uint32_t sp[8] = { 0 }; sp[4] = 0; sp[5] = 1; sif_rpc_inited = 1; sif_send_to_ee(0x80000001u, (uint8_t *)sp, 24, 0, 0, 0); }
      return;
    case 3:                                                                          /* RESET_CMD: reboot (rom0:UDNL <image>) */
      fprintf(stderr, "sif: IOP reboot: %.*s\n", (int)(w[4] < 80 ? w[4] : 80), (const char *)(iop_p(sif_iop_copy) + 24));
      sif_reboot_pending = 1; sif_ee_pktbuf = 0; sif_rpc_inited = 0; return;
    case 9: {                                                                        /* RPC_BIND */
      uint32_t rp[16] = { 0 }; IopServer *s = sif_rt_server(0, w[8]); int i;
      rp[4] = w[4]; rp[5] = w[5]; rp[6] = w[6]; rp[7] = w[7]; rp[8] = cid;
      if (s) { rp[9] = s->sd; rp[10] = s->buf; rp[11] = s->cbuf; }
      else for (i = 0; i < sif_nhsrv; i++) if (sif_hsrv[i].sid == w[8]) { rp[9] = sif_hsrv[i].sd; rp[10] = sif_hsrv[i].buf; }
      if (!rp[9]) fprintf(stderr, "sif: bind to server %08x: not registered (yet)\n", w[8]);
      sif_send_to_ee(0x80000008u, (uint8_t *)rp, 64, 0, 0, 0); return; }
    case 10: {                                                                       /* RPC_CALL: the send data is in the server buffer already */
      int i;
      if (sif_log) fprintf(stderr, "sif: call fno %u send %u recvbuf %08x recv %u rmode %u\n", w[8], w[9], w[10], w[11], w[12]); IopServer *s = sif_rt_server(w[13], 0);
      for (i = 0; i < sif_nhsrv; i++) if (sif_hsrv[i].sd == w[13]) {
        uint32_t rep = 0; sif_hsrv[i].fn(w[8], sif_hsrv[i].buf, w[9], &rep);
        sif_rpc_end(w, cid, rep, rep ? w[11] : 0, rep ? iop_p(rep) : 0);
        if (sif_after_end) { void (*f)(void) = sif_after_end; sif_after_end = 0; f(); }   /* a server's own command after its RPC reply */
        return; }
      if (!s) { fprintf(stderr, "sif: call to unknown server data %08x\n", w[13]); return; }
      for (i = 0; i < 32; i++) if (!sif_calls[i].used) { SifCall *c = &sif_calls[i]; memset(c, 0, sizeof *c); c->used = 1; memcpy(c->pkt, w, 64); c->s = s; c->q = sif_rt_queue(s->queue); break; }
      return; }
    case 12: {                                                                       /* RPC_RDATA: EE reads IOP memory */
      uint32_t rp[16] = { 0 };
      rp[4] = w[4]; rp[5] = w[5]; rp[6] = w[6]; rp[7] = w[7]; rp[8] = cid; rp[9] = w[8]; rp[10] = w[9]; rp[11] = w[10];
      sif_send_to_ee(0x80000008u, (uint8_t *)rp, 64, iop_p(w[8]), w[9], w[10]); return; }
    }
    if (id < 32 && sif_sys_h[id].fn) { iop_rt_call_handler(g_iop, sif_sys_h[id].fn, sif_sys_h[id].gp, sif_iop_copy, sif_sys_h[id].harg, 0); return; }
    fprintf(stderr, "sif: system command %08x not handled\n", cid); return;
  }
  if (id < sif_usr_n) {
    uint32_t fn = rd32(g_iop, sif_usr_tab + 8 * id), harg = rd32(g_iop, sif_usr_tab + 8 * id + 4);
    if (fn) { iop_rt_call_handler(g_iop, fn, sif_usr_gp[id & 255], sif_iop_copy, harg, 0); return; }
  }
  fprintf(stderr, "sif: user command %u has no IOP handler\n", id);
}

/* ---- EE kernel calls ---- */
static int sif_syscall(int32_t n)
{
  uint32_t a0 = R[4], a1 = R[5];
  switch (n) {
  case 0x77: case -0x77: {                        /* (i)sceSifSetDma(SifDmaTransfer_t *, count): { src (EE), dest (IOP), size, attr } */
    uint32_t i, dispatch = 0;
    for (i = 0; i < a1; i++) {
      uint32_t t = a0 + 16 * i, src = ee_rd32(t), dst = ee_rd32(t + 4), sz = ee_rd32(t + 8), attr = ee_rd32(t + 12);
      ee_copy_from(iop_p(dst), src, (sz + 3) & ~3u);
      if ((attr & SIF_DMA_INT_O) && (dst & IOP_MEM_MASK) == (sif_iop_rbuf & IOP_MEM_MASK)) dispatch = 1;
    }
    R[2] = ++sif_dma_id;
    if (dispatch) { sif_iop_dispatch(); if (n > 0) sif_iop_run(); }
    return 1; }
  case 0x76: case -0x76: R[2] = -1; return 1;    /* sceSifDmaStat: finished */
  case 0x78: case -0x78: sif_ee_armed = 1; if (n > 0) sif_iop_run(); else sif_ee_deliver(); R[2] = 0; return 1;   /* (i)sceSifSetDChain */
  case 0x6b: R[2] = 0; return 1;                 /* sceSifStopDma */
  case 0x79:                                     /* sceSifSetReg */
    if (a0 & 0x80000000u) { if ((a0 & 3) < 4) sif_sysreg[a0 & 3] = a1; }
    else if (a0 == 3) sif_reg[3] |= a1;          /* MSFLG: the EE sets bits */
    else if (a0 == 4) sif_reg[4] &= ~a1;         /* SMFLG: the EE clears bits */
    else if (a0 < 5) sif_reg[a0] = a1;
    R[2] = 0; return 1;
  case 0x7a:                                     /* sceSifGetReg: also lets the IOP run (the EE polls it while waiting) */
    if (sif_reboot_pending && a0 == 4) {          /* the rebooted IOP is up again */
      sif_reboot_pending = 0; sif_reg[4] |= SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND; sif_reg[2] = sif_iop_rbuf; }
    sif_iop_run();
    R[2] = (int32_t)((a0 & 0x80000000u) ? sif_sysreg[a0 & 3] : a0 < 5 ? sif_reg[a0] : 0); return 1;
  }
  return 0;
}

/* ---- IOP side: sifcmd / sifman calls of the modules ---- */
static void sifc_InitCmd(IopCpu *c) { (void)c; }
static void sifc_GetSreg(IopCpu *c) { iop_ret(c, sif_iop_sreg[iop_arg(c, 0) & 31]); }
static void sifc_SetSreg(IopCpu *c) { sif_iop_sreg[iop_arg(c, 0) & 31] = iop_arg(c, 1); }
static void sifc_SetCmdBuffer(IopCpu *c) { sif_usr_tab = iop_arg(c, 0); sif_usr_n = iop_arg(c, 1); }
static void sifc_AddCmdHandler(IopCpu *c)
{
  uint32_t cid = iop_arg(c, 0), fn = iop_arg(c, 1), harg = iop_arg(c, 2), id = cid & 0x7fffffffu;
  if (sif_log) fprintf(stderr, "sif: IOP handler for cmd %08x: %08x (arg %08x)\n", cid, fn, harg);
  if (cid & 0x80000000u) { if (id < 32) { sif_sys_h[id].fn = fn; sif_sys_h[id].harg = harg; sif_sys_h[id].gp = c->r[28]; } return; }
  if (id < sif_usr_n) { wr32(g_iop, sif_usr_tab + 8 * id, fn); wr32(g_iop, sif_usr_tab + 8 * id + 4, harg); sif_usr_gp[id & 255] = c->r[28]; }
}
static void sifc_RemoveCmdHandler(IopCpu *c)
{
  uint32_t cid = iop_arg(c, 0), id = cid & 0x7fffffffu;
  if (cid & 0x80000000u) { if (id < 32) sif_sys_h[id].fn = 0; } else if (id < sif_usr_n) wr32(g_iop, sif_usr_tab + 8 * id, 0);
}
static void sifc_SendCmd(IopCpu *c)              /* (cid, packet, psize, src_extra, dest_extra, size_extra) */
{
  uint32_t cid = iop_arg(c, 0), pkt = iop_arg(c, 1), psize = iop_arg(c, 2) & 0xff, src = iop_arg(c, 3), dest = iop_arg(c, 4), size = iop_arg(c, 5);
  if (psize > 112) { iop_ret(c, 0); return; }
  { uint32_t h0 = psize | (size << 8); wr32(g_iop, pkt, h0); wr32(g_iop, pkt + 4, size ? dest : 0); wr32(g_iop, pkt + 8, cid); }
  sif_send_to_ee(cid, iop_p(pkt), psize, size ? iop_p(src) : 0, size ? dest : 0, size);
  iop_ret(c, ++sif_dma_id);
}
static void sifm_SetDma(IopCpu *c)               /* (SifDmaTransfer_t *, count): { src (IOP), dest (EE), size, attr } */
{
  uint32_t t = iop_arg(c, 0), n = iop_arg(c, 1), i;
  for (i = 0; i < n; i++) {
    uint32_t a = t + 16 * i, src = rd32(g_iop, a), dst = rd32(g_iop, a + 4), sz = rd32(g_iop, a + 8);
    sif_push(0, dst, iop_p(src), (sz + 3) & ~3u);
  }
  iop_ret(c, ++sif_dma_id);
}
static void sif_ok(IopCpu *c) { iop_ret(c, 0); }
static void sifm_DmaStat(IopCpu *c) { iop_ret(c, (uint32_t)-1); }
static void sifm_GetSMFlag(IopCpu *c) { iop_ret(c, sif_reg[4]); }
static void sifm_SetSMFlag(IopCpu *c) { sif_reg[4] |= iop_arg(c, 0); }
static void sifm_GetMSFlag(IopCpu *c) { iop_ret(c, sif_reg[3]); }
static void sifm_SetMSFlag(IopCpu *c) { sif_reg[3] &= ~iop_arg(c, 0); }
/* loadcore RegisterLibraryEntries: the module's exports become the imports of the modules loaded after it
   (export table: magic 0x41c00000, next, version, mode, name[8], function addresses up to 0) */
static void sif_RegisterLibraryEntries(IopCpu *c)
{
  uint32_t t = iop_arg(c, 0), k; char name[9] = { 0 };
  memcpy(name, iop_p(t + 12), 8);
  for (k = 0; k < 64; k++) {
    uint32_t f = rd32(g_iop, t + 20 + 4 * k); IopFn fn;
    if (!f) break;
    fn = iop_rt_fn(g_iop, f);
    if (fn && k >= 4) iop_rt_bind(g_iop, name, k, fn);
  }
  fprintf(stderr, "sif: IOP library %s: %u exports\n", name, k);
  iop_ret(c, 0);
}

static void sif_init(void)
{
  iop_rt_bind(g_iop, "sifcmd", 4, sifc_InitCmd); iop_rt_bind(g_iop, "sifcmd", 6, sifc_GetSreg); iop_rt_bind(g_iop, "sifcmd", 7, sifc_SetSreg);
  iop_rt_bind(g_iop, "sifcmd", 8, sifc_SetCmdBuffer); iop_rt_bind(g_iop, "sifcmd", 10, sifc_AddCmdHandler);
  iop_rt_bind(g_iop, "sifcmd", 11, sifc_RemoveCmdHandler); iop_rt_bind(g_iop, "sifcmd", 12, sifc_SendCmd); iop_rt_bind(g_iop, "sifcmd", 13, sifc_SendCmd);
  iop_rt_bind(g_iop, "sifman", 7, sifm_SetDma); iop_rt_bind(g_iop, "sifman", 8, sifm_DmaStat);
  iop_rt_bind(g_iop, "sifman", 21, sifm_GetMSFlag); iop_rt_bind(g_iop, "sifman", 22, sifm_SetMSFlag);
  iop_rt_bind(g_iop, "sifman", 23, sifm_GetSMFlag); iop_rt_bind(g_iop, "sifman", 24, sifm_SetSMFlag);
  iop_rt_bind(g_iop, "loadcore", 6, sif_RegisterLibraryEntries); iop_rt_bind(g_iop, "loadcore", 5, sif_ok);   /* FlushDcache */
  /* intrman 9 is CpuEnableIntr (ps2sdk iop/system/intrman exports.tab); the PS2RC table maps 9 to CpuDisableIntr and 10 to CpuEnableIntr */
  iop_rt_bind(g_iop, "intrman", 9, iop_rt_builtin("intrman", 10, 0)); iop_rt_bind(g_iop, "intrman", 8, iop_rt_builtin("intrman", 9, 0));
  sif_iop_rbuf = iop_rt_alloc(g_iop, 0x80, 0, 0); sif_iop_copy = iop_rt_alloc(g_iop, 0x80, 0, 0); sif_iop_spkt = iop_rt_alloc(g_iop, 0x100, 0, 0);
  sif_reg[2] = sif_iop_rbuf; sif_reg[4] = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;   /* the IOP has booted before the EE */
}
/* an EE vblank: the IOP's vblank and 20 ms of IOP time (PAL).  The IOP hardware timers the modules started (system clock source) fire
   at their compare period inside those 20 ms, the IOP running after each one (iop_main's sound tick: compare 92160 = 2.5 ms) */
static uint64_t sif_tim_ns[8];
static void sif_vblank_body(void)
{
  uint64_t done = 0, total = 20000000u; int k, n = iop_rt_timer_count(g_iop); if (n > 8) n = 8;
  iop_rt_vblank(g_iop); if (sif_vblank_host) sif_vblank_host();
  while (done < total) {
    uint64_t step = total - done;
    for (k = 0; k < n; k++) { uint64_t p = iop_rt_timer_period_ns(g_iop, k);
      if (iop_rt_timer_active(g_iop, k) && p && p - sif_tim_ns[k] < step) step = p - sif_tim_ns[k]; }
    iop_rt_advance_us(g_iop, (done + step) / 1000 - done / 1000); done += step;
    if (sif_step_host) sif_step_host(step);
    for (k = 0; k < n; k++) { uint64_t p = iop_rt_timer_period_ns(g_iop, k);
      if (!iop_rt_timer_active(g_iop, k) || !p) { sif_tim_ns[k] = 0; continue; }
      sif_tim_ns[k] += step; if (sif_tim_ns[k] >= p) { sif_tim_ns[k] = 0; iop_rt_timer_expire(g_iop, k); } }
    sif_iop_run();
  }
}
static void sif_vblank(void) { HOST_ENTER(HOST_IOP); sif_vblank_body(); HOST_LEAVE(HOST_IOP); }
#endif
