/* texconv.c: see texconv.h. */
#include "texconv.h"
#include "gslayout.h"
#include <string.h>
#include "me_rt.h"   /* task #149: -DMGS2_ME_SAFE (ME-safe calls) */

/* GS memory layout (block order in a page, pixel order in a block), as PCSX2 GS/GSTables.cpp (same tables as tools/gsref.c) */
static const uint8_t bt32[4][8] = { {0,1,4,5,16,17,20,21}, {2,3,6,7,18,19,22,23}, {8,9,12,13,24,25,28,29}, {10,11,14,15,26,27,30,31} };
static const uint8_t bt8[4][8] = { {0,1,4,5,16,17,20,21}, {2,3,6,7,18,19,22,23}, {8,9,12,13,24,25,28,29}, {10,11,14,15,26,27,30,31} };
static const uint8_t bt4[8][4] = { {0,2,8,10}, {1,3,9,11}, {4,6,12,14}, {5,7,13,15}, {16,18,24,26}, {17,19,25,27}, {20,22,28,30}, {21,23,29,31} };
static const uint8_t ct32[8][8] = { {0,1,4,5,8,9,12,13}, {2,3,6,7,10,11,14,15}, {16,17,20,21,24,25,28,29}, {18,19,22,23,26,27,30,31}, {32,33,36,37,40,41,44,45}, {34,35,38,39,42,43,46,47}, {48,49,52,53,56,57,60,61}, {50,51,54,55,58,59,62,63} };
static const uint8_t ct8[16][16] = { {0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54}, {8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62}, {33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23}, {41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31}, {96,100,112,116,64,68,80,84,98,102,114,118,66,70,82,86}, {104,108,120,124,72,76,88,92,106,110,122,126,74,78,90,94}, {65,69,81,85,97,101,113,117,67,71,83,87,99,103,115,119}, {73,77,89,93,105,109,121,125,75,79,91,95,107,111,123,127}, {128,132,144,148,160,164,176,180,130,134,146,150,162,166,178,182}, {136,140,152,156,168,172,184,188,138,142,154,158,170,174,186,190}, {161,165,177,181,129,133,145,149,163,167,179,183,131,135,147,151}, {169,173,185,189,137,141,153,157,171,175,187,191,139,143,155,159}, {224,228,240,244,192,196,208,212,226,230,242,246,194,198,210,214}, {232,236,248,252,200,204,216,220,234,238,250,254,202,206,218,222}, {193,197,209,213,225,229,241,245,195,199,211,215,227,231,243,247}, {201,205,217,221,233,237,249,253,203,207,219,223,235,239,251,255} };
static const uint16_t ct4[16][32] = { {0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110}, {16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126}, {65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47}, {81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63}, {192,200,224,232,128,136,160,168,194,202,226,234,130,138,162,170,196,204,228,236,132,140,164,172,198,206,230,238,134,142,166,174}, {208,216,240,248,144,152,176,184,210,218,242,250,146,154,178,186,212,220,244,252,148,156,180,188,214,222,246,254,150,158,182,190}, {129,137,161,169,193,201,225,233,131,139,163,171,195,203,227,235,133,141,165,173,197,205,229,237,135,143,167,175,199,207,231,239}, {145,153,177,185,209,217,241,249,147,155,179,187,211,219,243,251,149,157,181,189,213,221,245,253,151,159,183,191,215,223,247,255}, {256,264,288,296,320,328,352,360,258,266,290,298,322,330,354,362,260,268,292,300,324,332,356,364,262,270,294,302,326,334,358,366}, {272,280,304,312,336,344,368,376,274,282,306,314,338,346,370,378,276,284,308,316,340,348,372,380,278,286,310,318,342,350,374,382}, {321,329,353,361,257,265,289,297,323,331,355,363,259,267,291,299,325,333,357,365,261,269,293,301,327,335,359,367,263,271,295,303}, {337,345,369,377,273,281,305,313,339,347,371,379,275,283,307,315,341,349,373,381,277,285,309,317,343,351,375,383,279,287,311,319}, {448,456,480,488,384,392,416,424,450,458,482,490,386,394,418,426,452,460,484,492,388,396,420,428,454,462,486,494,390,398,422,430}, {464,472,496,504,400,408,432,440,466,474,498,506,402,410,434,442,468,476,500,508,404,412,436,444,470,478,502,510,406,414,438,446}, {385,393,417,425,449,457,481,489,387,395,419,427,451,459,483,491,389,397,421,429,453,461,485,493,391,399,423,431,455,463,487,495}, {401,409,433,441,465,473,497,505,403,411,435,443,467,475,499,507,405,413,437,445,469,477,501,509,407,415,439,447,471,479,503,511} };

/* block, pixel -> (x, y) inverses of bt32 / ct32, built once */
static uint8_t ibt32[32], ict32[64], inv_ok;
static void inv_init(void)
{
  for (int y = 0; y < 4; y++) for (int x = 0; x < 8; x++) ibt32[bt32[y][x]] = (uint8_t)(y << 3 | x);
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) ict32[ct32[y][x]] = (uint8_t)(y << 3 | x);
  inv_ok = 1;
}
static inline uint32_t pa32(uint32_t bp, uint32_t bw, int x, int y)
{ uint32_t bn = (bp + (((uint32_t)(y >> 5) * bw + (x >> 6)) << 5) + bt32[(y >> 3) & 3][(x >> 3) & 7]) & 0x3fff; return bn << 8 | ct32[y & 7][x & 7] << 2; }
static inline uint32_t pa8(uint32_t bp, uint32_t bw, int x, int y)
{ uint32_t bn = (bp + (((uint32_t)(y >> 6) * (bw >> 1) + (x >> 7)) << 5) + bt8[(y >> 4) & 3][(x >> 4) & 7]) & 0x3fff; return bn << 8 | ct8[y & 15][x & 15]; }
static inline uint32_t pa4(uint32_t bp, uint32_t bw, int x, int y)   /* nibble address */
{ uint32_t bn = (bp + (((uint32_t)(y >> 7) * (bw >> 1) + (x >> 7)) << 5) + bt4[(y >> 4) & 7][(x >> 5) & 3]) & 0x3fff; return bn << 9 | ct4[y & 15][x & 31]; }

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t rd64(const uint8_t *p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }

int tc_page(TcPage *pg, const uint8_t *pkt, const uint8_t *(*ram)(uint32_t))
{
  /* each block: CNT 6 + DIRECT 6 { GIFtag A+D NLOOP 4; BITBLTBUF; TRXPOS; TRXREG; TRXDIR; GIFtag IMAGE }, REF n + DIRECT n
     (the image); the chain ends with RET. */
  pg->n = 0;
  for (int b = 0; b < 4; b++, pkt += 0x80) {
    uint32_t tag = rd32(pkt), id = tag >> 28 & 7;
    if (id == 6) return pg->n;                                        /* RET */
    if (id != 1 || (tag & 0xffff) != 6) return -1;                    /* CNT 6 */
    uint64_t r[4]; for (int k = 0; k < 4; k++) { r[k] = rd64(pkt + 0x20 + 16 * k); if (rd64(pkt + 0x28 + 16 * k) != 0x50u + k) return -1; }
    uint32_t ref = rd32(pkt + 0x70), qwc = ref & 0xffff;
    if ((ref >> 28 & 7) != 3) return -1;                              /* REF */
    int dbp = r[0] >> 32 & 0x3fff, dbw = r[0] >> 48 & 63, dpsm = r[0] >> 56 & 63, dsax = r[1] >> 32 & 0x7ff, dsay = r[1] >> 48 & 0x7ff;
    int rrw = r[2] & 0xfff, rrh = r[2] >> 32 & 0xfff;
    if (!qwc || !rrh) continue;                                       /* empty block */
    if (dpsm != 0 || dbw != 1 || dsax != 0 || rrw != 64 || (uint32_t)rrh * 16 != qwc || (r[3] & 3) != 0 || pg->n == 4) return -1;
    pg->up[pg->n++] = (TcUpload){ ME_CALLP(ram)(rd32(pkt + 0x74)), (uint32_t)dbp, dsay, rrh };
  }
  return -1;
}

const uint8_t *tc_byte(const TcPage *pg, uint32_t a)
{
  if (!inv_ok) inv_init();
  uint32_t bn = a >> 8 & 0x3fff;
  for (int i = pg->n - 1; i >= 0; i--) {                              /* the last upload wins, as on the GS */
    const TcUpload *u = &pg->up[i];
    uint32_t rel = (bn - u->dbp) & 0x3fff;
    int b = ibt32[rel & 31], p = ict32[a >> 2 & 63];
    int x = (b & 7) * 8 + (p & 7), y = (int)(rel >> 5) * 32 + (b >> 3) * 8 + (p >> 3);
    if (y >= u->dsay && y < u->dsay + u->rows) return u->src + ((y - u->dsay) * 64 + x) * 4 + (a & 3);
  }
  return 0;
}

static int p2(int n) { int v = 1; while (v < n) v <<= 1; return v; }
/* one axis of CLAMP_1: REPEAT / CLAMP over the texture size, REGION_CLAMP [min, max], or REGION_REPEAT (c & mask) | fix
   with a power-of-two mask + 1 and fix & mask = 0 */
static int axis(int wm, int mn, int mx, int size, int *o, int *n, int *rep)
{
  if (wm == 0) { *o = 0; *n = size; *rep = 1; return 0; }
  if (wm == 1) { *o = 0; *n = size; *rep = 0; return 0; }
  if (wm == 2) { *o = mn; *n = mx - mn + 1; *rep = 0; return *n > 0 ? 0 : -1; }
  if (wm == 3 && !((mn + 1) & mn) && !(mx & mn)) { *o = mx; *n = mn + 1; *rep = 1; return 0; }
  return -1;
}
static int geomx(uint64_t tex0, uint64_t clamp, int wide, int *psm, int *u0, int *v0, int *w, int *h, int *tw, int *th, int *bw, int *rows, int *repeat)
{
  int ps = tex0 >> 20 & 63, ru, rv, sw = 1 << (tex0 >> 26 & 15), sh = 1 << (tex0 >> 30 & 15);
  if (ps == 0x13 || ps == 0x1b) *psm = 8;                           /* T8, T8H */
  else if (ps == 0x14 || ps == 0x24 || ps == 0x2c) *psm = 4;        /* T4, T4HL, T4HH */
  else if (wide && (ps == 0x02 || ps == 0x0a)) *psm = 16;           /* CT16, CT16S: direct colour, 16 bits a texel */
  else return -1;
  if (axis(clamp & 3, clamp >> 4 & 0x3ff, clamp >> 14 & 0x3ff, sw, u0, w, &ru) || axis(clamp >> 2 & 3, clamp >> 24 & 0x3ff, clamp >> 34 & 0x3ff, sh, v0, h, &rv)) return -1;
  *repeat = ru | rv << 1;
  /* a REGION_CLAMP one texel over the GE's 512: the codec subtitle reads its 512-texel image in the T4HH text page through
     MINU 0 .. MAXU 512 (gsref, codec capture vb 17,000-17,003: its taps reach texel 496, the scissor cuts the rest). The GE
     texture holds texels u0 .. u0 + 511 and clamps there, so a tap on texel u0 + 512 would read u0 + 511 [checked on gsref's
     texuse extents]. The side is stored without the edge column: the GE's own clamp gives it. */
  int cu = (clamp & 3) == 2 && *w == 513, cv = (clamp >> 2 & 3) == 2 && *h == 513;
  if (cu) *w = 512;
  if (cv) *h = 512;
  *tw = p2(*w); *th = p2(*h); if (*tw > 512 || *th > 512) return -1;
  /* stored: the region plus one edge column and row (bilinear taps reach at most one texel past a REGION_CLAMP edge:
     gsref, every deck draw), rounded to the GE swizzle block of 16 bytes x 8 rows. A repeat axis stores its period. */
  int minw = 128 / *psm;   /* the GE swizzle block is 16 bytes x 8 rows */
  *bw = ru || cu ? *tw : (*w + 1 + minw - 1) / minw * minw; if (*bw < minw) *bw = minw;
  *rows = rv || cv ? *th : (*h + 1 + 7) / 8 * 8; if (*rows < 8) *rows = 8;
  return 0;
}
size_t tc_texture_size(uint64_t tex0, uint64_t clamp)
{
  int psm, u0, v0, w, h, tw, th, bw, rows, rp;
  if (geomx(tex0, clamp, 1, &psm, &u0, &v0, &w, &h, &tw, &th, &bw, &rows, &rp)) return 0;
  return (size_t)bw * rows * psm / 8;
}
int tc_region(uint64_t tex0, uint64_t clamp, int *psm, int *u0, int *v0, int *w, int *h)
{
  int tw, th, bw, rows, rp;
  return geomx(tex0, clamp, 1, psm, u0, v0, w, h, &tw, &th, &bw, &rows, &rp);
}

/* GE swizzle: 16-byte x 8-row blocks, row-major; bpr = bytes per row */
static inline size_t swz(int xb, int y, int bpr) { return ((size_t)(y >> 3) * (bpr >> 4) + (xb >> 4)) * 128 + (y & 7) * 16 + (xb & 15); }

typedef struct { const TcPage *pg; uint32_t tbp, tbw; int psm, missing; } PageFetch;
static int page_fetch(void *ctx, int sx, int sy)
{
  PageFetch *f = ctx;
  if (f->psm == 8) { const uint8_t *s = tc_byte(f->pg, pa8(f->tbp, f->tbw, sx, sy)); if (!s) f->missing++; return s ? *s : 0; }
  uint32_t na = pa4(f->tbp, f->tbw, sx, sy); const uint8_t *s = tc_byte(f->pg, na >> 1); if (!s) f->missing++;
  return s ? (*s >> ((na & 1) * 4)) & 15 : 0;
}
int tc_texture(const TcPage *pg, uint64_t tex0, uint64_t clamp, TcTex *t, uint8_t *out)
{
  int ps = tex0 >> 20 & 63;
  if (ps != 0x13 && ps != 0x14) return -1;                          /* pages hold T8 / T4 only */
  PageFetch f = { pg, (uint32_t)(tex0 & 0x3fff), (uint32_t)(tex0 >> 14 & 63), ps == 0x13 ? 8 : 4, 0 };
  if (tc_texture_cb(tex0, clamp, page_fetch, &f, t, out)) return -1;
  t->missing = f.missing;
  return 0;
}
int tc_texture_cb(uint64_t tex0, uint64_t clamp, int (*fetch)(void *ctx, int x, int y), void *ctx, TcTex *t, uint8_t *out)
{
  int psm, u0, v0, w, h, tw, th, bw, rows, rp;
  if (geomx(tex0, clamp, 1, &psm, &u0, &v0, &w, &h, &tw, &th, &bw, &rows, &rp)) return -1;
  memset(t, 0, sizeof *t);
  t->psm = psm; t->w = w; t->h = h; t->tw = tw; t->th = th; t->bw = bw; t->rows = rows; t->u0 = u0; t->v0 = v0; t->repeat = rp;
  int bpr = bw * psm / 8;
  memset(out, 0, (size_t)bpr * rows);
  for (int y = 0; y < rows; y++) {
    int sy = v0 + (y < h ? y : h - 1);
    for (int x = 0; x < bw; x++) {
      int sx = u0 + (x < w ? x : w - 1), v = ME_CALLP(fetch)(ctx, sx, sy);
      if (psm == 16) { out[swz(2 * x, y, bpr)] = (uint8_t)v; out[swz(2 * x + 1, y, bpr)] = (uint8_t)(v >> 8); continue; }   /* 5551, the PS2's own order */
      if (x < w && y < h) t->used[v >> 3] |= (uint8_t)(1 << (v & 7));
      if (psm == 8) out[swz(x, y, bpr)] = (uint8_t)v;
      else out[swz(x >> 1, y, bpr)] |= (uint8_t)(v << ((x & 1) * 4));   /* CLUT4: even texel in the low nibble (I: to check on the PSP) */
    }
  }
  return 0;
}
int tc_texture_blk(uint64_t tex0, uint64_t clamp, void (*blk)(void *ctx, uint32_t bn, uint32_t v[64], uint32_t kn[64]), void *ctx, TcTex *t,
                   uint8_t *out)
{
  int psm, u0, v0, w, h, tw, th, bw, rows, rp, gp = (int)(tex0 >> 20 & 63);
  uint32_t tbp = (uint32_t)(tex0 & 0x3fff), tbw = (uint32_t)(tex0 >> 14 & 63), v[64], kn[64];
  const uint8_t *vb = (const uint8_t *)v;   /* the block's bytes in GS order (little-endian hosts: x86 and the PSP) */
  uint8_t mark[256];
  if (geomx(tex0, clamp, 1, &psm, &u0, &v0, &w, &h, &tw, &th, &bw, &rows, &rp)) return -1;
  if (u0 + w > 2048 || v0 + h > 2048) return 1;   /* a region the GS's 2048 wrap would cut */
  memset(t, 0, sizeof *t);
  t->psm = psm; t->w = w; t->h = h; t->tw = tw; t->th = th; t->bw = bw; t->rows = rows; t->u0 = u0; t->v0 = v0; t->repeat = rp;
  int bpr = bw * psm / 8;
  memset(out, 0, (size_t)bpr * rows); memset(mark, 0, sizeof mark);
  /* the region's texels block by block: a block is 16 x 16 texels in T8 (texel = byte ct8 of the block), 32 x 16 in T4 (nibble ct4),
     16 x 8 in CT16 / CT16S (half gs_ct16: half 1 of a word is its bits 16-31), 8 x 8 in T8H, T4HL and T4HH (the CT32 layout: the top
     bits of word ct32), as gs_pa8 / gs_pa4 / gs_pa16 / gs_pa32 place them */
  int bwx = gp == 0x13 || psm == 16 ? 16 : gp == 0x14 ? 32 : 8, bhy = gp == 0x13 || gp == 0x14 ? 16 : 8;
  uint32_t m0 = psm == 8 ? 0xffu : 0xfu, sh0 = gp == 0x2c ? 28 : 24;
  for (int by = v0 & -bhy; by < v0 + h; by += bhy)
    for (int bx = u0 & -bwx; bx < u0 + w; bx += bwx) {
      ME_CALLP(blk)(ctx, gp == 0x13 ? pa8(tbp, tbw, bx, by) >> 8 : gp == 0x14 ? pa4(tbp, tbw, bx, by) >> 9 : psm == 16 ? gs_pa16(tbp, tbw, bx, by, gp == 0x0a, 0) >> 8
          : pa32(tbp, tbw, bx, by) >> 8, v, kn);
      uint32_t all = ~0u; for (int i = 0; i < 64; i++) all &= kn[i];
      int ya = by > v0 ? by : v0, yb = by + bhy < v0 + h ? by + bhy : v0 + h, xa = bx > u0 ? bx : u0, xb = bx + bwx < u0 + w ? bx + bwx : u0 + w;
      for (int sy = ya; sy < yb; sy++) {
        int y = sy - v0; uint8_t *row = out + (size_t)(y >> 3) * (size_t)(bpr >> 4) * 128 + (y & 7) * 16;   /* swz's row */
        /* a texel the map cannot give: texel by texel, which counts them */
        if (psm == 16) {
          const uint8_t *c = gs_ct16[sy & 7];
          for (int sx = xa; sx < xb; sx++) {
            uint32_t o = c[sx & 15], sh = (o & 1) * 16, val = v[o >> 1] >> sh & 0xffff; int x = sx - u0;
            if (all != ~0u && (kn[o >> 1] >> sh & 0xffff) != 0xffff) return 1;
            uint8_t *p = row + (x >> 3) * 128 + (x & 7) * 2; p[0] = (uint8_t)val; p[1] = (uint8_t)(val >> 8);   /* swz(2 x) and swz(2 x + 1) */
          }
        } else if (gp == 0x13) {
          const uint8_t *c = ct8[sy & 15];
          for (int sx = xa; sx < xb; sx++) {
            uint32_t o = c[sx & 15]; int x = sx - u0, val = vb[o];
            if (all != ~0u && (kn[o >> 2] >> (o & 3) * 8 & 0xff) != 0xff) return 1;
            mark[val] = 1; row[(x >> 4) * 128 + (x & 15)] = (uint8_t)val;
          }
        } else if (gp == 0x14) {
          const uint16_t *c = ct4[sy & 15];
          for (int sx = xa; sx < xb; sx++) {
            uint32_t o = c[sx & 31]; int x = sx - u0, val = vb[o >> 1] >> (o & 1) * 4 & 15;
            if (all != ~0u && (kn[o >> 3] >> (o & 7) * 4 & 15) != 15) return 1;
            mark[val] = 1; row[(x >> 5) * 128 + (x >> 1 & 15)] |= (uint8_t)(val << ((x & 1) * 4));
          }
        } else {
          const uint8_t *c = ct32[sy & 7];
          for (int sx = xa; sx < xb; sx++) {
            uint32_t wi = c[sx & 7]; int x = sx - u0, val = (int)(v[wi] >> sh0 & m0);
            if ((kn[wi] >> sh0 & m0) != m0) return 1;
            mark[val] = 1;
            if (psm == 8) row[(x >> 4) * 128 + (x & 15)] = (uint8_t)val; else row[(x >> 5) * 128 + (x >> 1 & 15)] |= (uint8_t)(val << ((x & 1) * 4));
          }
        }
      }
    }
  for (int i = 0; i < 256; i++) if (mark[i]) t->used[i >> 3] |= (uint8_t)(1 << (i & 7));
  /* the stored edge: past the region, its last column and row again, read back from what was just written */
  for (int y = 0; y < rows; y++) {
    int ys = y < h ? y : h - 1;
    for (int x = y < h ? w : 0; x < bw; x++) {
      if (psm == 16) { size_t a = swz(2 * (x < w ? x : w - 1), ys, bpr), b = swz(2 * x, y, bpr); out[b] = out[a]; out[b + 1] = out[a + 1]; continue; }
      int xs = x < w ? x : w - 1, val = psm == 8 ? out[swz(xs, ys, bpr)] : out[swz(xs >> 1, ys, bpr)] >> ((xs & 1) * 4) & 15;
      if (psm == 8) out[swz(x, y, bpr)] = (uint8_t)val; else out[swz(x >> 1, y, bpr)] |= (uint8_t)(val << ((x & 1) * 4));
    }
  }
  return 0;
}

int tc_palette(const TcPage *pg, uint32_t cbp, uint32_t out[256], uint8_t ps2_alpha[256], uint8_t present[32])
{
  int miss = 0;
  if (present) memset(present, 0, 32);
  for (int e = 0; e < 256; e++) {
    int p = (e & 0xe7) | (e & 8) << 1 | (e & 0x10) >> 1;              /* CSM1: entries 8-15 and 16-23 of each 32 swapped */
    const uint8_t *s = tc_byte(pg, pa32(cbp, 1, p & 15, p >> 4));
    uint32_t c = s ? rd32(s) : 0; if (!s) miss++; else if (present) present[e >> 3] |= (uint8_t)(1 << (e & 7));
    uint32_t a = c >> 24; if (ps2_alpha) ps2_alpha[e] = (uint8_t)a;
    a = a >= 0x80 ? 0xff : a * 2;
    out[e] = (c & 0xffffff) | a << 24;
  }
  return miss;
}

int tc_min_alpha(const TcTex *t, const uint8_t ps2_alpha[256], int csa)
{
  int m = 255, n = t->psm == 8 ? 256 : 16, base = t->psm == 8 ? 0 : csa * 16;
  for (int i = 0; i < n; i++) if (t->used[i >> 3] >> (i & 7) & 1) { int a = ps2_alpha[(base + i) & 255]; if (a < m) m = a; }
  return m;
}

int tc_texel(const TcTex *t, const uint8_t *data, int x, int y)
{
  int bpr = t->bw * t->psm / 8;
  x = (t->repeat & 1) ? x & (t->tw - 1) : x < 0 ? 0 : x >= t->tw ? t->tw - 1 : x;
  y = (t->repeat & 2) ? y & (t->th - 1) : y < 0 ? 0 : y >= t->th ? t->th - 1 : y;
  if (t->psm == 16) return data[swz(2 * x, y, bpr)] | data[swz(2 * x + 1, y, bpr)] << 8;
  if (t->psm == 8) return data[swz(x, y, bpr)];
  return data[swz(x >> 1, y, bpr)] >> ((x & 1) * 4) & 15;
}
