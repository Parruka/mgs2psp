/* xlive.c: see xlive.h.  The chain rules are runtime/ee_io.h dma_feed's (and dma_run's addressing), so this walk takes the same
   quadwords in the same order as the exact walk; what it hands the translator is what dma_vif1_qw / dma_gif_qw hand VIF1 and the GIF. */
#include <string.h>
#include "xlive.h"
#include "xlate.h"

static const uint8_t *qp(const XlvMem *m, uint32_t a)
{ return (a & 0x80000000u) ? m->spr + (a & 0x3ff0) : m->mem + (a & (m->mem_size - 1) & ~15u); }

/* n quadwords from guest address a: VIF1 takes each word with its address, the GIF each quadword; with fast, the runs that lie in one
   piece of host memory (RAM and the scratchpad wrap) as spans */
static void run(XlvMem *m, int ch, uint32_t a, uint32_t n, uint32_t chcr)
{
  if (m->fast) {
    while (n) {
      uint32_t spr = a & 0x80000000u, o = spr ? (a & 0x3ff0) : (a & (m->mem_size - 1) & ~15u), k = ((spr ? 0x4000 : m->mem_size) - o) / 16;
      const uint8_t *p = qp(m, a);
      if (k > n) k = n;
      if (m->seen) for (uint32_t i = 0; i < k; i++) m->seen(m->ctx, ch, a + 16 * i, p + 16 * i, 0, chcr);
      m->qw += k;
      if (ch == 1) xl_vif1_span((const uint32_t *)p, 4 * k, a); else xl_gif3_span((const uint32_t *)p, k, a);
      a += 16 * k; n -= k;
    }
    return;
  }
  for (uint32_t i = 0; i < n; i++) {
    uint32_t q = a + 16 * i, w[4]; const uint8_t *p = qp(m, q);
    memcpy(w, p, 16); m->qw++;
    if (m->seen) m->seen(m->ctx, ch, q, p, 0, chcr);
    if (ch == 1) { xl_vif1(w[0], q); xl_vif1(w[1], q + 4); xl_vif1(w[2], q + 8); xl_vif1(w[3], q + 12); }
    else xl_gif3(w, q);
  }
}

static void walk(XlvMem *m, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr)
{
  uint32_t asp[2], nasp = 0, guard = 0;
  m->transfers++;
  if (((chcr >> 2) & 3) == 0) { run(m, ch, madr & ~0x80000000u, qwc, chcr); return; }   /* normal mode: RAM only, as dma_feed */
  if (qwc) {      /* chain mode started with QWC > 0: those quadwords first, then the chain unless CHCR's tag fields end it (dma_feed) */
    uint32_t id = (chcr >> 28) & 7;
    run(m, ch, madr, qwc, chcr);
    if (id == 0 || id == 7 || ((chcr & 0x80) && (chcr >> 31))) return;
  }
  for (;;) {
    const uint8_t *t = qp(m, tadr); uint64_t tag; uint32_t n, id, addr, data, next; int end = 0;
    memcpy(&tag, t, 8); n = tag & 0xffff; id = (tag >> 28) & 7; addr = (uint32_t)(tag >> 32) & 0x7ffffff0u; addr |= (uint32_t)(tag >> 32) & 0x80000000u;
    m->tags++;
    if (m->seen) m->seen(m->ctx, ch, tadr, t, 1, chcr);
    if ((chcr & 0x40) && ch == 1) {   /* TTE */
      if (m->fast) xl_vif1_span((const uint32_t *)t + 2, 2, tadr + 8);
      else { uint32_t w[4]; memcpy(w, t, 16); xl_vif1(w[2], tadr + 8); xl_vif1(w[3], tadr + 12); } }
    switch (id) {
    case 0: data = addr; next = 0; end = 1; break;                    /* refe */
    case 1: data = tadr + 16; next = data + 16 * n; break;              /* cnt */
    case 2: data = tadr + 16; next = addr; break;                       /* next */
    case 3: case 4: data = addr; next = tadr + 16; break;               /* ref, refs */
    case 5: data = tadr + 16; if (nasp < 2) asp[nasp++] = data + 16 * n; next = addr; break;   /* call */
    case 6: data = tadr + 16; if (nasp) next = asp[--nasp]; else { next = 0; end = 1; } break;   /* ret */
    default: data = tadr + 16; next = 0; end = 1; break;                /* end */
    }
    run(m, ch, data, n, chcr);
    if (end || ((chcr & 0x80) && (tag >> 31) & 1)) break;               /* end tag, or IRQ with TIE */
    tadr = next;
    if (++guard > 1000000) { m->guard_stops++; break; }
  }
}
void xlv_dma(XlvMem *m, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr)
{
  walk(m, ch, chcr, madr, qwc, tadr);
  if (m->fast) xl_flush();   /* the game may change this RAM before the next transfer */
}
