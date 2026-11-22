/* gsup.h: where the GS memory that a texture or a CLUT reads came from, without a GS memory image (renderer_notes 11.10, step 5).
   Each host -> local transfer (BITBLTBUF / TRXPOS / TRXREG, TRXDIR, then its IMAGE data, which the game sends from EE RAM: on the deck
   the texture pages of re/dg_objects.md 4, which stay in EE RAM) is kept as a record: its rectangle and the EE address of its first
   byte.  Each GS block (256 bytes) lists the transfers that wrote it, newest first, with the bits of each word they wrote (a T4HH
   transfer writes bits 28-31 only).  A word of GS memory is rebuilt from the bytes the game sent.  The CLUT buffer keeps, per row of
   16 entries, the load that filled it, with the block lists as they were at that load: the GS keeps the colours, so a later upload to
   CBP does not change them.  Modelled: transfers in the 32-bit layout (PSMCT32, CT24, T8H, T4HL, T4HH: one word per pixel) and in the
   T4, T8, CT16 and CT16S layouts (a nibble, byte or half per pixel; T4 and T8 with an even width); texture and CLUT reads in CT32,
   CT24, CT16, CT16S, T8, T4, T8H, T4HL, T4HH.  A word that no modelled transfer wrote (written before the map started, by a transfer
   of a Z format, by one whose data came in pieces, by an unresolved local copy) reads as missing, never guessed.
   A local -> local transfer is resolved when it runs: the map keeps the pixels it copied as the map knew them then (the font: T4 at
   0x2000 copied into T4HL at 0x3000 at boot); a copy of a pixel the map does not know is not known.
   Draws: gu_drawn marks the area a draw may write (its frame and depth buffer within the scissor) as drawn; its words read as missing
   until a transfer writes them (on the PSP the GE drew them).  Portable C; the host gives the EE RAM.  Game data passes through it:
   what it gives is personal use only. */
#ifndef MGS2_GSUP_H
#define MGS2_GSUP_H
#include <stdint.h>

typedef struct GuMap GuMap;
/* a host pointer to n bytes of EE RAM at EE address ee, or 0 */
typedef const uint8_t *(*GuRamFn)(void *ctx, uint32_t ee, uint32_t n);
GuMap *gu_new(GuRamFn ram, void *ctx);
void gu_free(GuMap *m);

/* the GS register writes and IMAGE data, as the GIF hands them over (ge/gsfront.c calls these when the front end has a map);
   gr: the GS registers by A+D address after the write */
void gu_trxdir(GuMap *m, const uint64_t *gr);            /* TRXDIR written: a transfer starts (BITBLTBUF, TRXPOS, TRXREG of gr) */
void gu_image(GuMap *m, uint32_t ee, uint32_t n);        /* n bytes of IMAGE data, sent from EE address ee */
/* n bytes (a multiple of 16) sent from ee on, with the effect of n / 16 calls gu_image(m, ee + 16 k, 16): pieces end where a transfer
   gets all its bytes, and what no transfer takes counts a quadword at a time (the span front, renderer_notes 11.12) */
void gu_image_run(GuMap *m, uint32_t ee, uint32_t n);
void gu_clut_load(GuMap *m, const uint64_t *gr, int ctxt);   /* TEX0 or TEX2 of context ctxt written: the CLUT buffer load (CLD) */

/* GS memory word wa (byte address / 4); *known: the bits of it that are known */
uint32_t gu_word(const GuMap *m, uint32_t wa, uint32_t *known);
/* the 64 words of block bn (wa = bn << 6 | i) and their known bits, as 64 gu_word calls give them, in the bits of need (the other bits:
   not known; e.g. 0x0f000000 for a T4HL texture, which reads only those) */
void gu_block(const GuMap *m, uint32_t bn, uint32_t need, uint32_t v[64], uint32_t known[64]);
/* the index at texel (x, y) of a texture (psm T8, T4, T8H, T4HL or T4HH at block tbp, width tbw); -1: not known */
int gu_index(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y);
/* the texel at (x, y) in any format gu_index reads, or CT32 (32 bits), CT24 (24 bits), CT16 / CT16S (16 bits); -1: not known */
int64_t gu_texel(const GuMap *m, int psm, uint32_t tbp, uint32_t tbw, int x, int y);
/* a draw may write the rectangle x0..x1, y0..y1 (inclusive, buffer pixels) of the buffer at block bp, width bw (64-pixel units), in
   frame or depth format psm (CT32, CT24, CT16, CT16S, Z32, Z24, Z16, Z16S; the Z formats as the GS lays them out), bits: the bits of
   a pixel it writes (~FBMSK; ~0 for depth).  An area marked before is marked again only when a transfer wrote into it since. */
void gu_drawn(GuMap *m, int psm, uint32_t bp, uint32_t bw, uint32_t bits, int x0, int y0, int x1, int y1);
/* the CLUT buffer as the GS reads it for CPSM cpsm: CT32 / CT24 entry k (0..255) as 32 bits; CT16 / CT16S half k (0..511) as 16 bits;
   -1: not known */
int64_t gu_clut(const GuMap *m, int cpsm, int k);

/* words no entry holds read as zero (known) rather than missing: the boot's GS clear (16 transfers of 256 KB, all zero bytes) wrote
   every word, so this holds when the map runs from vblank 0 or from any vblank after it with no transfer missed; lanes the map lost (an
   entry dropped from a full list) still read as missing */
void gu_zero_empty(GuMap *m, int on);
/* content identities, for the PSP's texture cache (ge/texcache.h, renderer_notes 11.10 step 7).  With keys on, each upload gets one from
   its format, buffer, rectangle, EE address, size, the bytes it received and their key: 64 words spread over them, or all of them once
   its source (EE address and size) has shown a second key (step 6: no stale key on either route); a resolved local copy one from its
   pixels.  A drawn area, and a local copy of one, are the GE's own render targets. */
void gu_keys(GuMap *m, int on);
#define GU_ID_DRAWN 1   /* a drawn area (or a copy of one) holds some of the lanes */
#define GU_ID_LOST 2    /* the map lost some of them: an entry it does not model, data in pieces, a dropped entry, a CLUT row never loaded */
/* a hash of the entries of block bn's list that hold some of lanes (1: bits 0-23, 2: bits 24-27, 4: bits 28-31), newest first */
uint64_t gu_lanes_id(GuMap *m, uint32_t bn, uint32_t lanes, int *flags);
/* the same over the blocks of the rectangle x0..x1, y0..y1 (inclusive) of a buffer in format psm (its lanes: T8H 24-31, T4HL 24-27,
   T4HH 28-31, CT24 and Z24 0-23, the rest whole words of their own layout) */
uint64_t gu_area_id(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, int *flags);
/* the identity of CLUT entries k0 .. k0 + n - 1 as the GS reads them for CPSM cpsm (gu_clut's numbering): the loads that filled their rows */
uint64_t gu_clut_id(const GuMap *m, int cpsm, int k0, int n, int *flags);
/* the flags gu_area_id gives for that area, without its identity (a flags-only question: each state word once, no area identity kept) */
int gu_area_flags(GuMap *m, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1);
/* exact speed-ups of the two identities, on by default (runtime/host_time.h OPT_GU_MEMO, OPT_GU_CLUT; codec_speed.md step 1): gu_area_id
   walks a block's states only where they differ from the block's before it; gu_clut_id keeps the identity of entries 0..255 that one load
   put in place in order with that load.  The same identities either way (gsup.c has nothing to switch) */
void gu_area_memo(GuMap *m, int on);
void gu_clut_memo(GuMap *m, int on);
/* a number that changes whenever block bn's list changes */
uint32_t gu_block_ver(const GuMap *m, uint32_t bn);
/* sb9, Stage C step 0 (stagec_notes.md 0.3, 0.4; gsup2.c only).  0a: gu_settle puts a transfer still taking data into the blocks, as the
   identities do first; gu_area_pages gives the pages of the cells gu_area_id walks for an area (at most max; -1: more or none), the
   blocks it reads in each (bit j: block 32 p + j) and its lane groups; gu_pages_sig (after settling) gives 0 and *sig when each page
   and lane group read holds one plain final record or nothing, so that gu_area_id of an area over them is a function of the area and
   *sig, with flags 0; else why not (1 keys off, 2 mixed states, 3 a list, 4 drawn, 5 lost, 6 not final).  0b: gu_ld_ident: 1 (the
   default) a CLUT load whose blocks hold records with the identities of a load kept by identity (64 kept) is that load again, its
   states refreshed; 2 a new load as before, compared with the kept one (GuStats ld_ident_chk, ld_ident_bad); 0 off.  The same
   identities and CLUT entries either way */
void gu_settle(GuMap *m);
int gu_area_pages(int psm, uint32_t bp, uint32_t bw, int x0, int y0, int x1, int y1, uint16_t *pages, uint32_t *masks, int max, uint32_t *lanes);
int gu_pages_sig(GuMap *m, uint32_t lanes, int n, const uint16_t *pages, const uint32_t *masks, uint64_t *sig);
void gu_ld_ident(GuMap *m, int mode);

/* optional: told of each piece of IMAGE data the map takes for a host -> local transfer: the transfer's number (1, 2, ... in the order
   they started, as GuStats.xfers counts them), the EE address and size of the piece, and the bytes that transfer has now and needs */
typedef void (*GuDataFn)(void *ctx, uint32_t xfer, uint32_t ee, uint32_t n, uint32_t got, uint32_t need);
void gu_on_data(GuMap *m, GuDataFn fn, void *ctx);

#ifdef GU_SENT
/* dev, x86 check builds (boot/xlive_chk.c): with keep on, each host -> local transfer that starts keeps a copy of its bytes as they
   were when its IMAGE data came; with read_sent on, gu_word / gu_index / gu_clut read those copies (what the GS holds) instead of EE
   RAM; gu_sent_scan compares the copy of every transfer the map can still read with EE RAM now: fn for each that differs (its number
   as gu_on_data gives it, its EE address and bytes, the first byte that differs and how many differ); returns how many it compared */
void gu_keep_sent(GuMap *m, int on);
void gu_read_sent(GuMap *m, int on);
long gu_sent_scan(const GuMap *m, void (*fn)(void *ctx, uint32_t xfer, uint32_t ee, uint32_t got, uint32_t first, uint32_t ndiff), void *ctx);
/* why GS memory word wa reads as it does: each entry of its block's list (transfer number, format, rectangle, lanes, flags), as text */
int gu_why(const GuMap *m, uint32_t wa, char *buf, int n);
/* told of each host -> local and local -> local transfer (TRXDIR 0 and 2) with the GS registers (BITBLTBUF, TRXPOS, TRXREG, TRXDIR at
   gr[0x50..0x53]) */
void gu_on_trx(GuMap *m, void (*fn)(void *ctx, const uint64_t *gr), void *ctx);
/* print each entry pushed on block bn's list, and the list after it (-1: none) */
void gu_watch(GuMap *m, int bn);
#endif

typedef struct {
  long xfers, xfer_other, xfer_unmod, xfer_split, xfer_wrap, xfer_local, image_orphan;   /* transfers: all, other layout (of those
                                         not modelled), data in pieces, past 2048 or width 0, local copies; IMAGE data with no transfer */
  long drawn, drawn_kept;           /* areas marked drawn; draws whose area was still marked */
  long loads, load_wide, dropped;   /* CLUT loads, loads over too many blocks (CSM2 256), block entries dropped (list full) */
  long local_unknown, local_drawn, own_bytes;   /* local copies the map could not resolve (and of those, copies of a drawn area); bytes
                                                   of the resolved ones' pixels it holds now */
  long restacked, restack_deep, dt_new, dt_dirty;   /* gsup2.c only: blocks where a record went back on top (restack), records too deep
                                       for it; drawn areas marked again: not in the table / dirty */
  long gcs, ld_memo;                /* gsup2.c only: sweeps of unused records, list nodes and loads; CLUT loads found by their parameters */
  long elided, early, ld_made, areas_made, areas_kept;   /* gsup2.c only: uploads dropped as equal to the one in place; transfers that
                                       went into the blocks before their data was all in; CLUT loads kept; area identities made / kept */
  long blocks, blocks_one;          /* gsup2.c only: gu_block calls; of those, blocks whose lanes each came whole from one upload (8
                                       rows at a time) */
  long ld_ident, ld_ident_put, ld_ident_nolist, ld_ident_nosig, ld_ident_chk, ld_ident_bad;   /* sb9 gsup2.c gu_ld_ident: loads used
                                       again by identity; loads that joined the table; look-ups without a block list (the table of loads by
                                       parameters held another's) or whose records do not qualify; the check mode's comparisons, differing */
  long dt_fail;                     /* sb10: drawn areas dropped for want of a record (their blocks' marks cleared without a draw) */
  int live_xfers, live_loads;
} GuStats;
const GuStats *gu_stats(const GuMap *m);
#endif
