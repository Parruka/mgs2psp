/* texconv.h: the game's texture pages -> GE textures and palettes.

   A texture page (re/dg_objects.md §4) is uploaded to GS memory every frame by its packet: up to three blocks of
   BITBLTBUF / TRXPOS / TRXREG / TRXDIR + IMAGE REF, each writing rows of 64 CT32 pixels (DBW 1) at a DBP. The first
   block(s) hold the texels, the last one the CLUTs. The materials of the page (0xA0 each) then read sub-rectangles
   (CLAMP_1 REGION_CLAMP) of that memory as T8 or T4 (TEX0_1) with a CT32 CLUT (CSM1) at CBP.

   This module works from the page's own packet and the EE RAM it points to, without a GS memory image:
   - tc_page():    parse the packet into its uploads.
   - tc_texture(): one material's region as a GE CLUT8 / CLUT4 texture, swizzled. The GE texture size is the
                   region rounded up to powers of two, but only the region plus one repeated edge column and row is
                   stored: on the deck, bilinear taps never go more than one texel past a region edge (gsref), so the
                   GE reads exactly what the GS's REGION_CLAMP gives. A tap further out would read past the stored
                   rows (check gsref's "texuse" extents on new scenes). REGION_REPEAT with a power-of-two mask and an
                   aligned offset becomes a GE repeat texture of that size.
   - tc_palette(): the 256-entry palette at CBP as GE 8888 (PS2 alpha 0x80 = 1.0 becomes 0xff).
     T4 materials use 16 entries of it, from entry CSA * 16 (measured on every T4 draw of the deck: the game
     loads 256 entries through TEX2 and selects with CSA; gsref "clut check").
   Plain C, no PSP dependencies: the PSP renderer and the container tools (gsref) share it. */
#ifndef MGS2_TEXCONV_H
#define MGS2_TEXCONV_H
#include <stddef.h>
#include <stdint.h>

typedef struct { const uint8_t *src; uint32_t dbp; int dsay, rows; } TcUpload;   /* rows of 64 CT32 pixels from src, at rows dsay.. of the buffer at dbp */
typedef struct { TcUpload up[4]; int n; } TcPage;

/* Parse a page upload packet (page entry + buf * 0x190; DMA CNT / REF / RET chain). ram(a) returns a host pointer to
   EE address a. Returns the number of uploads, or -1 if the packet has a shape this parser does not know. */
int tc_page(TcPage *pg, const uint8_t *pkt, const uint8_t *(*ram)(uint32_t));

/* The source byte that holds GS byte address a, or NULL when no upload of the page wrote it. */
const uint8_t *tc_byte(const TcPage *pg, uint32_t a);

typedef struct {
  int psm;          /* 8 (CLUT8), 4 (CLUT4) or 16 (direct colour: the PS2's PSMCT16 halves, which are the GE's 5551 order) */
  int w, h;         /* region size in texels */
  int tw, th;       /* GE texture size: powers of two >= w, h */
  int bw, rows;     /* stored width in texels (GE buffer width; multiple of 16 bytes) and rows (multiple of 8), swizzled */
  int u0, v0;       /* region origin in the PS2 texture (texels) */
  int repeat;       /* bit 0: u repeats, bit 1: v repeats (GU_REPEAT on that axis); else clamp */
  uint8_t used[32]; /* bit i: index i appears in the region */
  int missing;      /* texels whose GS address no upload wrote (should be 0) */
} TcTex;

/* Size in bytes of the buffer tc_texture() / tc_texture_cb() need for this TEX0 / CLAMP, 0 if unsupported (not an index or
   16-bit format; a REGION_REPEAT that is not a power of two at an aligned offset; or a side over 512). */
size_t tc_texture_size(uint64_t tex0, uint64_t clamp);
/* The region of the PS2 texture that tc_texture_cb() converts for this TEX0 / CLAMP: texels u0 .. u0 + w - 1, v0 .. v0 + h - 1 (the
   converted texture repeats its last column and row past it), and its bits a texel in psm. Returns 0, or -1 if unsupported. */
int tc_region(uint64_t tex0, uint64_t clamp, int *psm, int *u0, int *v0, int *w, int *h);
/* Convert one material's region. out must hold tc_texture_size() bytes. Returns 0, or -1 if unsupported. */
int tc_texture(const TcPage *pg, uint64_t tex0, uint64_t clamp, TcTex *t, uint8_t *out);

/* The same layout from any source: fetch(ctx, x, y) returns the index at texel (x, y) of the PS2 texture. Also takes T8H,
   T4HL, T4HH, the 16-bit direct-colour formats (CT16 / CT16S: fetch returns the texel's 16 bits) and the plain REPEAT / CLAMP modes
   (over the TEX0 size). Used for textures that are not in a page (the font) and by the tools. */
int tc_texture_cb(uint64_t tex0, uint64_t clamp, int (*fetch)(void *ctx, int x, int y), void *ctx, TcTex *t, uint8_t *out);
/* The same bytes and TcTex for the index formats, read a GS block at a time: blk(ctx, bn, v, kn) gives the 64 words of block bn and
   their known bits (ge/gsup.h gu_block), each block the region touches once.  Returns 0; 1 when the texture must go through
   tc_texture_cb instead (a texel not known, which tc_texture_cb counts; direct colour; a region past 2048); -1 if unsupported. */
int tc_texture_blk(uint64_t tex0, uint64_t clamp, void (*blk)(void *ctx, uint32_t bn, uint32_t v[64], uint32_t kn[64]), void *ctx, TcTex *t,
                   uint8_t *out);

/* The 256-entry palette at block cbp (CT32, CSM1 order), GE 8888 (R in bits 0-7, alpha doubled and clamped to 0xff).
   ps2_alpha (optional) gets the original alpha bytes, for the alpha-test decision; present (optional) gets bit e set
   when an upload wrote entry e (a T4 material's block holds only its own 16 entries). Returns the entries no upload wrote. */
int tc_palette(const TcPage *pg, uint32_t cbp, uint32_t out[256], uint8_t ps2_alpha[256], uint8_t present[32]);

/* Lowest PS2 alpha among the palette entries the texture uses (T4: entries csa * 16 + i). */
int tc_min_alpha(const TcTex *t, const uint8_t ps2_alpha[256], int csa);

/* The texel the GE will sample at (x, y) of a converted texture (for tests). */
int tc_texel(const TcTex *t, const uint8_t *data, int x, int y);
#endif
