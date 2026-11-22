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
/* sb1, Stage B lane 1 (renderer_notes 11.20): a call whose input came as a P1214a0 group (xl_lane_p04): its vertex streams in the game's
   data, vector i of stream s being VU quadword T + 0x100 + s + 3 i: p the data, num the vectors it holds, kind as the translator decodes
   them (2 V4-16 signed, 3 the same plus ROW, 4 V2-16 signed as x, y, ROW z, y), row ROW.  Given only when the header's count is at most
   every stream's num; the translator has decoded the quadwords of the VU memory model that P04+df0 reads besides (T + 0 to 3, T + 0x14,
   abs 2 to 5).
   sb2, Stage B lane 2: a P06 call whose input came as a P12e8b8 group (xl_lane_p06): lane 2, streams A to F in slots 0 to 5, vector i
   of slot s being VU quadword T + 0x100 + s + 6 i; p[s] 0 when the group did not send slot s (the VU memory keeps an older group's).
   Given with every such group: the converter checks the streams it reads (ge/vu1conv.h vc_p06_rec).  The translator has decoded the
   quadwords P06 reads besides (T + 0 to 0x14, T + 0xfe, 0xff, abs 2 to 5).
   sb5, Stage B lane 4: a P07 call whose input came as a P07 slot (xl_lane_p07): lane 4, streams A to F in slots 0 to 5, vector i of slot
   s being VU quadword T + 0x100 + s + 7 i for A to E (kind 2: V4-16 signed), and for F (kind 5: V4-8 unsigned, two vectors a vertex)
   T + 0x105 + 7 i (vector 2 i) and T + 0x106 + 7 i (vector 2 i + 1); p[s] 0 when the slot did not send stream s.  The translator has
   decoded the quadwords P07 reads besides (T + 0 to 3, T + 0xfe, 0xff, abs 2 to 11, 16 to 19, 24 to 63).
   sb6, Stage B lane 3: a call whose input came as an effect prim of the sort pass (xl_lane_fx; renderer_notes 11.33): lane 3, p[0] the
   positions (kind 1: V4-32 as they are; vector i is VU quadword T + 0x100 + cl i), p[1] the attributes (kind 2 or 3: V4-16 signed, plus
   ROW for 3; with cl 3 vectors 2 i and 2 i + 1 are T + 0x101 + 3 i and T + 0x102 + 3 i), kind[2] 1 when the prim's layout is cl 3 (STCYCL
   3 / 1 for the positions, 3 / 2 for the attributes: P04+25f0's and +2870's), else 0.  Given with every prim; the converter checks what it
   reads (ge/vu1conv.h vc_fpos_rec).  For P04+25f0 and +2870 the translator has decoded the quadwords they read besides (T + 0x10 to 0x15,
   abs 2 to 5) */
typedef struct { const void *p[6]; uint16_t num[6]; uint8_t kind[6], lane; uint32_t row[4]; } XlRec;
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
  const XlRec *rec;             /* sb1, sb2, sb5: the streams of a lane's P04+df0 call (lane 1), P06 call (lane 2) or P07 call (lane 4), else 0 */
  uint32_t src;                 /* task #114: the guest address of the data of the last UNPACK to T + 0x100 (the first vertex quadword)
                                   since the previous call; 0: none (and a call of lanes 1, 2 and 4; lane 3 sets it as the words would) */
  int lane;                     /* sb6: the lane that handed the call's group to the translator (1 .. 4; XlRec.lane), else 0 */
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
void xl_flush(void);                    /* a transfer ended: decode every pending quadword (then the flush callback) */
/* sb4, step c (renderer_notes 11.29): fn(ctx) at each xl_flush, after the decode: the host converts the calls it held back (ge/gelive.c
   gl_flush) while their streams still lie in the game's data */
void xl_on_flush(void (*fn)(void *ctx), void *ctx);
void xl_vm_sync(void);                  /* decode every pending quadword now (xl_vm does) */
/* how: 1 (the default, 2026-10-27) each pending UNPACK's vectors in order, those it still owns; 0 every VU quadword scanned (as before).
   Both decode the same quadwords from the same UNPACKs, so the VU memory is the same; the PSP harness times one against the other */
void xl_vm_mode(int by_ud);
/* V4-8 unsigned UNPACKs in MODE 0 without a mask: 1 (the default, 2026-10-30) their own decode, 0 the bit-by-bit one (ud_any), as before.
   The same vectors either way (codec_speed.md step 2); the PSP harness times one against the other */
void xl_ud_v48(int on);
/* sb8 (task #121): xl_ud_eager(1) (the default): the lanes' constant blocks, headers and materials (all V4-32, MODE 0, no mask, CL = WL)
   are copied into VU memory as they arrive, not decoded when read (the same VU memory; any other form waits as before); xl_ud_mark(1) (the
   default): ud_new marks WL 1 strides and WL 2 strides with CL > 2 that do not wrap without the wrap test, four or two entries a turn (the
   same entries).  Both exact, for timing turns (OPT_XL_EAGER, OPT_XL_MARK) */
void xl_ud_eager(int on);
void xl_ud_mark(int on);
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
                 long lz_cmds, lz_vec, dec_read, dec_write, dec_flush, flushes, full_ud, full_rc;
                 long eg_cmds, eg_vec;   /* sb8: lane constants decoded at once (ud_eager) */
                 long need_calls, need_runs;   /* sb1 xl_vm_need: converter calls, descriptors it decoded */ } XlStats;
const XlStats *xl_stats(void);
/* sb1, Stage B lane 1: a P1214a0 group the one walk recognised (runtime/ee_io.h lane_p04): t its first tag (guest address tadr), q0 1
   when the constant block's tag comes first, dp the REF tags' data in order.  Does what xl_vif1_span does with the same words (the
   descriptors, own[], STCYCL, TOPS, the counters, the call) and gives a P04+df0 call its streams in XlCall.rec.  0: it cannot take the
   group this way (an UNPACK that cannot wait, a running ROW, a command with data left); the walk then hands it over word by word.
   xl_lane(1): flushes with many vectors pending scan own[] (the same quadwords decoded) */
int xl_lane_p04(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp);
/* sb2, Stage B lane 2: a P12e8b8 group (runtime/ee_io.h lane_p06): q0 as above, dp the data of its nine slots after the constant block
   (0: a slot not sent, its tag a CNT of 0 with the slot's STCYCL and a NOP).  As xl_lane_p04, for every P06 entry (XlCall.rec lane 2) */
int xl_lane_p06(const uint32_t *t, uint32_t tadr, int q0, const uint32_t *const *dp);
/* sb5, Stage B lane 4: one P07 slot (runtime/ee_io.h lane_p07; renderer_notes 11.32): t its first tag (guest address tadr), dp[k] the data
   of tag k (0: not sent, and for the tags without an UNPACK; the CNT's data is the slot's own).  As xl_lane_p04, the call's streams in
   XlCall.rec lane 4 when its entry is P07 */
int xl_lane_p07(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp);
/* sb6, Stage B lane 3: one effect prim of the sort pass (runtime/ee_io.h lane_fx; renderer_notes 11.33): t its first tag (guest address
   tadr), dp[k] the data of REF k (0 the positions, 1 the attributes, 2 the owner's block, 4 the REF 3; dp[3] and dp[5] 0: the CNT of 4's
   data is the prim's own, the last CNT has none).  As xl_lane_p04, for every prim (one call, XlRec lane 3) */
int xl_lane_fx(const uint32_t *t, uint32_t tadr, const uint32_t *const *dp);
extern long xl_lane_groups[5];   /* sb2: groups the lanes took (index: the lane; sb5: lane 4, its P07 slots; sb6: lane 3, its prims) */
void xl_lane(int on);
/* sb1, Stage B step "direct reads" (renderer_notes 11.22): xl_vm_need decodes, before a converter call, only the quadwords of the VU
   memory model that its converter reads (for an entry with no converter: all, as xl_vm_sync); the rest wait for a later read, a write
   over them or the end of the transfer.  xl_need(1): flushes with many vectors pending scan own[], as with xl_lane(1) */
void xl_vm_need(const XlCall *c);
void xl_need(int on);
/* sb10, Stage C step 1 T2 (stagec_notes.md 1.5): VU addresses 0 .. 63 (the constants every program reads and P07's bones) in 16 slots of
   four quadwords.  xl_vgen[s] changes with every write to slot s (an UNPACK registered or done, a copy) and at each transfer's end;
   when the last write was one straight copy of the whole slot from the game's data (V4-32 in MODE 0 without a mask, CL = WL, the slot's
   four quadwords in one UNPACK), xl_vsrc[s] is that data and xl_vsrc_gen[s] the generation it set: while xl_vsrc_gen[s] == xl_vgen[s]
   the slot holds the 64 bytes at xl_vsrc[s], which stay as they are until the transfer ends */
extern uint32_t xl_vgen[16], xl_vsrc_gen[16];
extern const void *xl_vsrc[16];
#endif
