/* xlive.h: the second walk of a VIF1 / GIF DMA transfer that feeds the packet translator (ge/xlate.h; renderer_notes 11.2, "Where it
   runs").  The exact walk (runtime/ee_io.h dma_feed / vif1_chain_fast) keeps the game's DMA, VIF and GS state; this walk goes over the
   same chain again with dma_feed's rules and hands the translator what that walk hands VIF1 and the GIF: VIF1's data words and, with
   TTE, the upper half of each tag; the GIF's data quadwords.  It reads guest RAM and the scratchpad and writes nothing of the game's,
   so every hash stays as it is.  Each word goes with its guest address as the exact walk sees it (bit 31: the scratchpad), as the GS
   capture records it (MGS2_GSCAP) and tools/gsref.c feeds it.  Portable C (x86 check builds and the PSP). */
#ifndef MGS2_XLIVE_H
#define MGS2_XLIVE_H
#include <stdint.h>

typedef struct {
  const uint8_t *mem; uint32_t mem_size;   /* guest RAM and its size (a power of two) */
  const uint8_t *spr;                      /* the scratchpad (16 KB) */
  /* optional, for checks: each quadword the walk takes, as the exact walk's packet hash (ee_io.h PKT_HASH / PKT_TAG) sees it:
     tag = 1 for a chain tag (chcr: the transfer's CHCR, whose TTE decides what the tag carries) */
  void (*seen)(void *ctx, int ch, uint32_t a, const uint8_t *q, int tag, uint32_t chcr);
  void *ctx;
  long qw, tags, transfers, guard_stops;   /* counters */
  int fast;   /* 1: hand the translator each run of quadwords as one span (xl_vif1_span, xl_gif3_span) and call xl_flush at the end of
                 each transfer (renderer_notes 11.12 step 1); 0: a word or quadword at a time (xl_vif1, xl_gif3), the reference */
} XlvMem;

/* one transfer as io_dma_start starts it: ch 1 (VIF1) or 2 (GIF), the channel's CHCR, MADR, QWC and TADR */
void xlv_dma(XlvMem *m, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr);
#endif
