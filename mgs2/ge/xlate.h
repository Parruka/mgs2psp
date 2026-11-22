/* xlate.h: the Stage A packet translator (renderer_notes 11.2).  It reads what the EE sends to VIF1 and to the GIF, keeps its own
   model of the VU1 data memory and of the GS registers, and hands each VU1 call it has a converter for to its caller, without running
   any VU1 micro code.  Portable C: tools/gsref.c runs it beside its VU1 interpreter (GSREF_XLATE=1) and compares; the PSP runs it on
   the walk of a shown call.  Game data passes through it: what it writes is personal use only. */
#ifndef MGS2_XLATE_H
#define MGS2_XLATE_H
#include <stdint.h>
#include "gsfront.h"

#define XL_DF0 0x1991d8u        /* VU1 entries by the guest address their first micro instruction was uploaded from (gsref's names) */
#define XL_FX 0x19a9f0u
#define XL_FX2 0x19ac78u

/* what the translator knows of an entry: where the program's first A+D packet is (P04 / P06 / P07 kick T+0xe8 first, re/vu1_programs.md
   2.6; P09 abs 0x78; P01 abs 0x314) and the GIF tag template whose PRIM its geometry starts with (T+0x15; P07 T+3, which it copies to
   T+5; P09 abs 0x11; P01 abs 0x319; the stage program abs 9, which its setup entry copies to T+0xd0); cont: the entry an MSCNT after
   this entry's call continues at; cp: a qword the program copies at the start of the call (abs cp_src to abs cp_dst[0] and [1]; later
   calls kick it); passes: 1 P06's extra texture passes (flags in header T+0x14 y), 2 P07's (flags T+0.w) */
typedef struct { uint32_t entry; int16_t ad, tag; uint8_t ad_abs, tag_abs; uint32_t cont; int16_t cp_src, cp_dst[2]; uint8_t passes; } XlRule;
const XlRule *xl_rule(uint32_t entry);             /* 0: an entry the translator has no rule for */

/* one VU1 call as it starts: the VU1 data memory as the EE's uploads left it and the GS registers after the call's A+D packet and the
   PRIM of its tag template */
typedef struct {
  uint32_t entry;               /* 0 when the micro code at the start address was never uploaded, or for an MSCNT after an entry
                                   without a known continuation */
  uint32_t pc, top, itop;       /* start address (doublewords), TOP and ITOP (quadwords) */
  int cont;                     /* MSCNT: continues where the previous program ended (entry: the rule's cont, else 0) */
  int ad;                       /* 1: the entry's A+D packet was applied */
  uint32_t ad_qa, tag_qa;       /* the absolute qword addresses of the A+D packet and the tag template (rule), else ~0 */
  const uint8_t *vm;            /* 16 KB, valid until the next VIF1 word (after xl_vif1_span: the quadwords decoded so far; xl_vm_sync
                                   decodes the rest) */
  const uint64_t *gr;           /* GS registers (gsref's numbering: the A+D address), valid until the next word */
} XlCall;
typedef void (*XlCallFn)(void *ctx, const XlCall *c);

void xl_reset(void);
void xl_on_call(XlCallFn fn, void *ctx);
void xl_vif1(uint32_t w, uint32_t src);            /* one VIF1 word in stream order; src: its guest address (0 if unknown) */
void xl_gif3(const uint32_t *q, uint32_t src);      /* one GIF PATH3 quadword */
/* the span front (renderer_notes 11.12 step 1), for the PSP: xl_vif1_span(p, n, src) does what n calls of xl_vif1 do (word k: p[k] from
   guest address src + 4 k), except that an UNPACK whose data the span holds whole is not decoded when it comes: the translator keeps
   where its data is and decodes a VU quadword when something reads it (a rule's A+D packet, tag template and pass header, the copies of
   P01 and the stage program, xl_vm_q), when an UNPACK that cannot wait writes over it, or at xl_flush.  The data is read where it lies, so
   the caller calls xl_flush at the end of each transfer, before the game can change that RAM.  DIRECT's IMAGE data goes to the texture
   sources a run at a time.  The census (XlStats) counts as the per-word front does */
void xl_vif1_span(const uint32_t *p, uint32_t n, uint32_t src);
void xl_gif3_span(const uint32_t *p, uint32_t nq, uint32_t src);   /* nq GIF PATH3 quadwords (4 words each) from src on */
void xl_flush(void);                    /* a transfer ended: decode every pending quadword */
void xl_vm_sync(void);                  /* decode every pending quadword now (xl_vm does) */
/* how: 1 (the default, 2026-10-27) each pending UNPACK's vectors in order, those it still owns; 0 every VU quadword scanned (as before).
   Both decode the same quadwords from the same UNPACKs, so the VU memory is the same; the PSP harness times one against the other */
void xl_vm_mode(int by_ud);
/* V4-8 unsigned UNPACKs in MODE 0 without a mask: 1 (the default, 2026-10-30) their own decode, 0 the bit-by-bit one (ud_any), as before.
   The same vectors either way (codec_speed.md step 2); the PSP harness times one against the other */
void xl_ud_v48(int on);
const uint32_t *xl_vm_q(uint32_t qa);   /* VU memory quadword qa (0..1023), decoded first if pending */
void xl_vm_logical(uint8_t *out);       /* dev: the VU memory as it reads now into out (16 KB); pending quadwords are decoded there only */
const uint64_t *xl_gs(void);                       /* the GS register model */
/* the GS front end (ge/gsfront.h) fed with every GIF path the translator sees: PATH2 / PATH3 as they come, PATH1 only the A+D packets
   it kicks itself; a VU1 call marks PRIM, the vertex queue and the vertex fields unknown (gf_unknown) after its tag template's PRIM.
   fn gets each primitive with the path in g->path (set it before xl_reset or after: both keep it) */
void xl_on_prim(GfPrimFn fn, void *ctx);
const GfGs *xl_front(void);
/* the texture sources (ge/gsup.h) the front end tells of transfers, IMAGE data (with its EE address) and CLUT loads; kept by xl_reset */
void xl_uploads(struct GuMap *m);
/* P06's and P07's extra texture passes: after a call's callback the translator gives the front end pass 2's and pass 3's A+D packets,
   in the program's order, and calls fn after each with the pass number (2, 3); the front end then holds that pass's GS state */
typedef void (*XlPassFn)(void *ctx, const XlCall *c, int pass);
void xl_on_pass(XlPassFn fn, void *ctx);
const uint8_t *xl_vm(void);             /* the VU memory model, every quadword decoded */
/* counters for a report: words, VIF commands, calls by entry (first 64 entries), GIF tags per path, A+D kicks, unknown commands;
   fills: UNPACKs with WL > CL (filling writes, which unpack() does not model: audit L3); cont2: MSCNTs right after an MSCNT, whose
   entry is taken from the rule as for the first (audit L4), with cont2_after the MSCAL entry before the first of them */
typedef struct { long words, cmds[128], calls, calls_cont, calls_noentry, gif_tags[4], gif_image_qw, ad_kicks, ad_bad, unk, fills, cont2;
                 uint32_t cont2_after, ent[64]; long ent_n[64]; int nent;
                 /* the volume census, counted only in a build with -DXL_CENSUS (renderer_notes 11.12): data words by VIF command, vectors
                    an UNPACK wrote by its format (command bits 0-4: VL, VN, the mask bit), GIF quadwords and IMAGE quadwords by path */
                 long wd[128], uq[32], gq[4], img_qw[4];
                 /* UNPACK kinds (first 64): key = command bits 0-4 | MODE << 5 | USN << 7 | FLG << 8 | CL << 16 | WL << 24 | MASK << 32
                    (the mask only for masked formats); commands and vectors of each */
                 uint64_t uk[64]; long uk_cmds[64], uk_vec[64]; int nuk;
                 long ent_uq[64], uq_open;   /* vectors unpacked since the call before, by the entry of the call they precede */
                 /* the span front, counted with -DXL_CENSUS: UNPACKs left to decode later and their vectors; quadwords decoded later when
                    read, before an UNPACK that cannot wait wrote over them, at a flush (the end of a transfer, xl_vm_sync, a full table);
                    flushes that found work, of those for a full table of UNPACKs or of ROW / COL copies */
                 long lz_cmds, lz_vec, dec_read, dec_write, dec_flush, flushes, full_ud, full_rc; } XlStats;
const XlStats *xl_stats(void);
#endif
