/* texcache.h: the PSP's texture path (renderer_notes 11.10 step 7).  For a draw's TEX0 / CLAMP / TEXA it gives a GE texture and palette,
   converted from what the texture map (ge/gsup.h, with keys and zero_empty on) knows and kept under the identity of what they were made
   from, so a texture page the game sends again every frame is converted once.  A texture whose texels a GS draw wrote is the GE's own
   render target, and one the map lost is not converted; both are counted.  Portable C; the x86 check build (boot/xlive_chk.c,
   MGS2_XLIVE_TK) compares every texel and palette entry it gives with the map.  Game data passes through it: personal use only. */
#ifndef MGS2_TEXCACHE_H
#define MGS2_TEXCACHE_H
#include <stdint.h>
#include "gsup.h"
#include "texconv.h"

typedef struct TkCache TkCache;
/* budget: bytes of converted textures and palettes kept (0: no limit).  Textures used in this frame or the one before (the GE may still
   be drawing it) are never evicted, so the cache can go over budget for a frame. */
TkCache *tk_new(GuMap *map, uint32_t budget);
void tk_free(TkCache *c);
/* the host's own memory for the converted textures and palettes, before the first tk_get (the PSP harness: the cache can then never take
   the game's heap, and every block starts at a 64-byte boundary, as the GE (16) and the data cache want).  First fit in an
   address-ordered list of free runs of 64-byte units, neighbours joined when a block is given back; when nothing fits, entries no frame
   in flight uses go (least recently used first) until it does, else the texture is not converted (TkStats.nomem: drawn untextured).
   Every choice depends on sizes and order only, so x86 and the PSP make the same.  Without a pool: malloc and free.  0, or -1: too small */
int tk_pool(TkCache *c, void *mem, uint32_t size);

enum { TK_TEX = 1, TK_RT, TK_LOST, TK_UNSUP };
typedef struct {
  int kind;                    /* TK_TEX: t, data and pal below; TK_RT: it reads texels a GS draw wrote (the GE's render target);
                                  TK_LOST: the map lost some of its texels or palette entries; TK_UNSUP: a format or size not
                                  converted yet (32-bit direct colour, a side over 512, a 16-bit texture whose TEXA the GE's
                                  5551 alpha bit cannot say) */
  TcTex t; const uint8_t *data; /* texconv's layout (CLUT8 / CLUT4 / 5551, swizzled); t.u0, t.v0, t.w, t.h: the region of the PS2 texture */
  const uint32_t *pal; int npal; /* the GE palette, npal 8888 entries (PS2 alpha 0x80 = 0xff): entry i for the texture's index i;
                                    0 for a 16-bit texture (t.psm 16), which carries its own colours */
  uint64_t id, pal_id;         /* the identities they are kept under */
  int conv, pal_conv;          /* 1: converted for this use */
  int minalpha;                /* the lowest PS2 alpha (0..255, before the GE's doubling) among the palette entries the texture uses
                                  (TcTex used); 0 for a 16-bit texture */
  int ent, pal_ent;            /* sb9: the cache entries of the texture and the palette (-1: none) */
  const TcTex *tp;             /* sb9 tk_get_um: the description (t above, or the cache entry's own copy: valid until the next look-up) */
  uint32_t size;               /* sb9 tk_get_um: tc_texture_size of the TEX0 / CLAMP */
} TkTex;
/* gr: the GS registers by A+D address (ge/xlate.h xl_gs, the front end's gr); ctxt: the draw's context (TEX0 / CLAMP of that context,
   TEXA).  Returns out->kind.  The data stays valid to the end of the next frame. */
int tk_get(TkCache *c, const uint64_t *gr, int ctxt, TkTex *out);
/* the same with the draw's CLAMP given instead of the context's (ge/gelive.c: the region a DIRECT primitive reads, task #83) */
int tk_get_cl(TkCache *c, const uint64_t *gr, int ctxt, uint64_t clamp, TkTex *out);
void tk_frame(TkCache *c);     /* a frame has ended */
/* gl23: the frame count now (tk_frame's), and a guard: entries used at or after frame g are kept too (the frame the GE may still be
   drawing while the next one is made: psp/xl_psp.c MGS2_ME_PIC_GE2); TK_NO_GUARD (the default) none, as before */
#define TK_NO_GUARD 0x7fffffffL
long tk_now(const TkCache *c);
void tk_guard(TkCache *c, long g);
/* sb9, Stage C step 0a (stagec_notes.md 0.3): tk_get_cl through a memo keyed by what was uploaded: a texture whose region reads pages
   that each hold one plain final upload record (ge/gsup.h gu_pages_sig) is found again by those records' identities, without the area
   identity and the cache's hash look-up; then exactly tk_get_cl's hit (its counters, the texture's and the palette's use in the same
   order, the lowest alpha).  Fills out->tp and out->size on TK_TEX.  Mode (tk_umemo): 1 (the default) on; 2 every answer the memo
   would give is made by tk_get_cl and compared (TkStats um_chk, um_bad); 0 tk_get_cl alone, as before */
int tk_get_um(TkCache *c, const uint64_t *gr, int ctxt, uint64_t clamp, TkTex *out);
void tk_umemo(TkCache *c, int mode);
/* sb10, Stage C step 1 T1 (stagec_notes.md 1.3): tk_get_um's front, 32 entries keyed by TEX0 & 0x3ffffffff, CLAMP and TEXA, valid while no
   block of the map has changed: a look-up the memo answered before goes straight to its entries, skipping G's hash look-up, the pages'
   signature and T's search; everything else as the memo's own path, its counters and use stamps included.  Mode (tk_ufront): 1 (the
   default) on; 2 on, and each answer checked against the memo's own path (TkStats uf_chk, uf_bad); 0 off.  Only with the memo on */
void tk_ufront(TkCache *c, int mode);

typedef struct {
  long uses, hits, misses, pal_uses, pal_hits, pal_misses, rt, lost, unsup, evicted, unknown;   /* unknown: texels or entries the map
                                                                                                    could not give in a conversion */
  uint64_t conv_bytes, live_bytes, peak_bytes, frame_bytes, peak_frame_bytes;   /* frame_bytes: of the textures and palettes used in the
                                                                                    frame so far */
  long live, frame_n;          /* textures and palettes kept; used in the frame so far */
  long nomem;                  /* tk_pool: conversions refused for want of room in the pool (counted in unsup as well) */
  long blk, blk_back, blk_diff; /* tk_blk: textures converted a block at a time; handed back to the texel path (a texel not known, direct
                                  colour); mode 2: conversions whose bytes or TcTex differed from the texel path's */
  long um_asks, um_hits, um_geo, um_texa, um_sig[7], um_slot, um_gone, um_fill, um_chk, um_bad, um_gnew;   /* sb9 tk_get_um: look-ups; memo
                                  answers; not served: its TEX0 / CLAMP (not converted, or over 16 pages), a 16-bit texture's TEXA, the
                                  pages (gu_pages_sig's reasons 1-6), no slot for it, its entry gone; slots filled; mode 2 answers
                                  compared, differing; G slots made */
  long uf_hits, uf_fill, uf_chk, uf_bad;   /* sb10 T1 tk_ufront: look-ups the front answered; entries put; mode 2 answers compared, differing */
  long tab_grows, tab_cap, tab_trims;   /* task #159: the entry table's growths, its entries now; on the PSP, growths after which newlib's
                                  free top went back (malloc_trim) */
} TkStats;
const TkStats *tk_stats(const TkCache *c);
/* how textures are converted: 0 texel by texel through gu_texel (as before 2026-10-27); 1 (the default) a GS block at a time
   (texconv.c tc_texture_blk through gu_block), the same bytes; 2 (dev) both, compared (TkStats blk_diff) */
void tk_blk(TkCache *c, int mode);
/* how a texture not converted is told from a render target: 1 (the default) the map's flags alone (gu_area_flags, each state word once);
   0 the whole area identity (gu_area_id), as before 2026-10-30.  The same answer either way (codec_speed.md step 1) */
void tk_flags(TkCache *c, int on);
/* 16-bit textures and DIRECT primitives' regions (task #83): 1 (the default) a 16-bit texture converts whenever each alpha TEXA gives
   its texels is 0 or at least 0x80, with the GE's alpha bit set from it, and ge/gelive.c converts a DIRECT primitive's 16-bit texture, or
   one over the GE's 512, as the region it reads; 0 only TEXA TA0 = 0, TA1 >= 0x80 and the whole texture, as before 2026-10-30 */
void tk_t16(TkCache *c, int on);
int tk_t16_on(const TkCache *c);
#endif
