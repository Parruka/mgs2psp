/* DMA chain executor + VIF decoder feeding the VU0 core (vu0.h). Exact PS2 semantics for the subset the game collision code uses:
   DMA tags refe/cnt/next/ref/refs/call/ret/end; VIF NOP, STCYCL, FLUSH variants, MSCAL, MSCNT, STMASK, STROW, STCOL, MPG, UNPACK V4-32.
   Anything else is counted in g_unsup_vif. */
#ifndef VIF_H
#define VIF_H
static int g_unsup_vif; static uint32_t vif_cl = 1, vif_wl = 1;
static void vif_stream(const uint32_t *w, int n)
{
  for (int i = 0; i < n;) {
    uint32_t c = w[i++]; int cmd = (c >> 24) & 0x7f, num = (c >> 16) & 0xff, imm = c & 0xffff;
    if (cmd == 0x00 || (cmd >= 0x02 && cmd <= 0x07) || cmd == 0x10 || cmd == 0x11 || cmd == 0x13) continue;
    if (cmd == 0x01) { vif_cl = imm & 0xff; vif_wl = (imm >> 8) & 0xff; continue; }
    if (cmd == 0x14 || cmd == 0x15) { vu_run(imm); continue; }          /* MSCAL / MSCALF */
    if (cmd == 0x17) { vu_run(vu_tpc); continue; }                      /* MSCNT */
    if (cmd == 0x20) { i += 1; continue; }                              /* STMASK */
    if (cmd == 0x30 || cmd == 0x31) { i += 4; continue; }               /* STROW / STCOL */
    if (cmd == 0x4a) {                                                  /* MPG */
      int cnt = num ? num : 256;
      for (int k = 0; k < cnt * 2; k++) VUPROG[((imm * 2) + k) & 1023] = w[i + k];
      vu_prog_clean = 0;
#ifdef GEN_DG_TRACE
      { void rt_dgt_mpg(void); rt_dgt_mpg(); }   /* dev (boot.c MGS2_DG_TRACE): who uploads micro code */
#endif
#ifdef VU_STATS
      g_mpg_n++; g_mpg_words += cnt * 2;
#endif
#ifdef VU_DUMP
      vu_dump_dirty = 1;
#endif
      i += cnt * 2; continue;
    }
    if (cmd >= 0x60 && (cmd & 0xf) == 0xc) {                            /* UNPACK V4-32 */
      int cnt = num ? num : 256, a = imm & 0x3ff, cl = vif_cl, wl = vif_wl ? vif_wl : 1;
      if (c & 0x10000000) g_unsup_vif++;                                /* mask bit: not used */
      int src = 0, dst = 0;
      if (cl >= wl) { for (int k = 0; k < cnt; k++) { int q = (k / wl) * cl + (k % wl); memcpy(VUMEM + (((a + q) & 0xff) << 4), w + i + 4 * k, 16); src++; } }
      else g_unsup_vif++;
      (void)dst; i += src * 4; continue;
    }
    g_unsup_vif++; return;
  }
}
/* process a DMA chain starting at guest address `a` (stops at end/ret-with-empty-stack) */
static void dma_chain(uint32_t a)
{
  uint32_t stk[8]; int sp = 0;
  for (int guard = 0; guard < 4096; guard++) {
    uint32_t t0 = *(uint32_t *)MM(a), t1 = *(uint32_t *)MM(a + 4); int id = (t0 >> 28) & 7, q = t0 & 0xffff;
    uint32_t data = a + 16, next = a + 16 + q * 16;
    switch (id) {
      case 0: vif_stream((uint32_t *)MM(t1), q * 4); return;                                    /* refe */
      case 1: vif_stream((uint32_t *)MM(data), q * 4); a = next; break;                         /* cnt */
      case 2: vif_stream((uint32_t *)MM(data), q * 4); a = t1; break;                           /* next */
      case 3: case 4: vif_stream((uint32_t *)MM(t1), q * 4); a = next; break;                   /* ref */
      case 5: vif_stream((uint32_t *)MM(data), q * 4); if (sp < 8) stk[sp++] = next; a = t1; break; /* call */
      case 6: vif_stream((uint32_t *)MM(data), q * 4); if (!sp) return; a = stk[--sp]; break;   /* ret */
      default: vif_stream((uint32_t *)MM(data), q * 4); return;                                 /* end */
    }
  }
}
#endif
