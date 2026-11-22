/* gsref.c: a reference renderer for an MGS2_GSCAP capture (runtime/ee_io.h): VIF1, a VU1 interpreter, the GIF and a software GS, so the
   GE renderer's output can be compared with what the PS2 path draws, image against image and primitive against primitive.
   Container only: its images are game data (personal use, never published).  What it prints is numbers (GS feature counts).
   Build: gcc -O2 -ffp-contract=off -o gsref gsref.c ../ge/texconv.c ../ge/vu1conv.c ../ge/xlate.c ../ge/xconv.c ../ge/gsfront.c ../ge/gebatch.c ../ge/gsup.c ../ge/xldig.c -lm
   Run:   gsref <capture> <out dir> [-q]
     <out dir>/vb<N>_disp.ppm    the display read circuit (DISPFB / DISPLAY of the vblank N record) after the draws before vblank N
     <out dir>/vb<N>_fb<FBP>.ppm every frame buffer drawn between vblank N-1 and N (its bounding box from 0,0)
     stdout: per vblank, counts of VU1 calls, GS primitives by kind and state (blend, tests, texture formats, filters, targets).
   Options (environment): GSREF_GEOUT=<file>:<vb> one vblank as a GE replay file (ge/replay_fmt.h: df0 and the effect meshes P04+25f0 as
     transform batches beside the PS2's primitives), GSREF_ZSCALE=<k> its depth = Z24 x k;
     GSREF_P06DUMP=<file> every P06 call's input and kicked GIF tags and vertices (tools/dev/p06_census.py; task #63);
     GSREF_DF0CHK=1 the converter checks against the VU1 interpreter (ge/vu1conv.c: df0; the effect meshes P04+25f0 / +2870 with the port
     of the PS2's clipper, renderer_notes 11.9); GSREF_NOFOG=1, GSREF_FOGONLY=<entry, hex>[,..] fog off /
     only on up to 4 VU1 entries; GSREF_RAM=<EE RAM dump> the texture converter check; GSREF_ZQ=<shift>[,<max, hex>] a depth buffer of fewer bits;
     GSREF_ZCENSUS=1 the depth tests whose result changes in 16 bits, by entry, and the written depth range per target; GSREF_ZMAP=<file> the
     GE renderer's depth mapping (16 bits per depth buffer and frame, renderer_notes 11.8); GSREF_PICK=<x>,<y>,<vb> every write at one pixel;
     GSREF_XLATE=1 the packet translator (ge/xlate.c) beside the interpreter: entries, VU memory and GS registers at each call
     (GSREF_XLRUN, GSREF_XLSHOW, GSREF_XLPRIM: trial runs and examples, see the check); GSREF_XLCONV=1 with it: the translator's
     converters (ge/xconv.c) beside the interpreter and the export, whose df0 / effect batches they replace (see that check); GSREF_XLCUT=1
     with it: the converters leave out the df0 calls wholly off the scissor, as the live renderer does (ge/xconv.h XcHost.cut: a call the
     PS2 drew a pixel from must never be left out: "gsref batch, converter none" stays 0);
     GSREF_XLDIRECT=1 with it: the GS front end (ge/gsfront.c) fed by the translator against every PATH2 / PATH3 primitive drawn, and
     the export's batches of those primitives made from the front end's data (see that check); GSREF_XLTEX=1 with it: each indexed
     export texture made again from the translator's texture sources (ge/gsup.c) and compared, the export taking theirs (see that check);
     GSREF_XLDIG=<file> with it: ge/xldig.c's digest lines (one per VU1 call and per vblank), to compare with the live x86 check
     build's (boot/xlive_chk.c, MGS2_XLIVE) on a capture of the same run;
     GSREF_GELIVE=1 with GSREF_GEOUT, GSREF_XLATE, GSREF_XLCONV and GSREF_XLDIRECT: the export as the live renderer makes the frame
     (ge/gelive.h, renderer_notes 11.13): the front end's path 2 / 3 primitives (VU1 entry 0) and the converters' batches only (also
     those of calls the PS2 drew nothing from), a transform batch ending the current batch, df0 without the joined index list; to compare
     with the x86 check build's replay file of the same vblank (MGS2_XLIVE_GER, tools/dev/ger_cmp.py);
     GSREF_RZ=1 the PSP's float mode for the whole run (round toward zero, denormal results flushed: FCR31 0x01000001, as the x86
     build's MGS2_FPU_RZ): the live renderer's converters run in the game's thread, in that mode, so the GELIVE comparison takes it too;
     GSREF_SKIPTEX, GSREF_TRACE, GSREF_VIFLOG, GSREF_SPRITES, GSREF_XFERLOG, GSREF_TEXLOG, GSREF_VDUMP, GSREF_FPDBG (debug logs).
   Sources: the VU1 core is runtime/vu0.h's (same float, flag and pipeline model) with the VU1 memory sizes, XGKICK, XTOP / XITOP, the EFU
   (latencies as PCSX2 VUops.cpp) and the branch-after-integer-op rule (a conditional branch reads the VI value from before an IADD /
   IADDI / IADDIU / IAND / IOR / ISUB / ISUBIU / MTIR / LQI / LQD / SQI / SQD in the instruction just before it, as PCSX2 VI_BACKUP).
   VIF unpack rules and the GS memory layout tables as PCSX2 (Vif_Unpack.cpp, GS/GSTables.cpp; layout facts of the hardware).
   Not modelled: dithering, AA1 coverage, the GS's exact interpolation precision and fill rounding (edge pixels can differ), field
   offsets of interlaced output, MTBA automatic mip addresses, PATH arbitration (a VU1 program runs to its end at MSCAL). */
#include <math.h>
#include "../ge/texconv.h"
#include "../ge/replay_fmt.h"
#include "../ge/vu1conv.h"
#include "../ge/xlate.h"
#include "../ge/xconv.h"
#include "../ge/gsfront.h"
#include "../ge/gebatch.h"
#include "../ge/gsup.h"
#include "../ge/xldig.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <xmmintrin.h>   /* GSREF_RZ */

static int quiet;

/* ================= counters: key -> count, printed per vblank ================= */
typedef struct { char k[96]; long n; } Cnt;
static Cnt cnt[8192]; static int cnt_used[8192], ncnt;
static void count(long add, const char *fmt, ...)
{
  char k[96]; va_list ap; va_start(ap, fmt); vsnprintf(k, sizeof k, fmt, ap); va_end(ap);
  uint32_t h = 2166136261u; for (const char *p = k; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  for (uint32_t i = h & 8191;; i = (i + 1) & 8191) {
    if (!cnt[i].k[0]) { if (ncnt >= 8000) return; strcpy(cnt[i].k, k); cnt_used[ncnt++] = i; }
    if (!strcmp(cnt[i].k, k)) { cnt[i].n += add; return; }
  }
}
static int cnt_cmp(const void *a, const void *b) { return strcmp(cnt[*(const int *)a].k, cnt[*(const int *)b].k); }
static void count_flush(long vb)
{
  qsort(cnt_used, ncnt, sizeof(int), cnt_cmp);
  if (!quiet) for (int i = 0; i < ncnt; i++) printf("vb %ld  %-80s %ld\n", vb, cnt[cnt_used[i]].k, cnt[cnt_used[i]].n);
  memset(cnt, 0, sizeof cnt); ncnt = 0;
}

/* ================= GS local memory (4 MB) ================= */
static uint8_t gsm[4 << 20];
/* GS memory layout (block order in a page, pixel order in a block), as PCSX2 GS/GSTables.cpp */
static const uint8_t bt32[4][8] = { {0,1,4,5,16,17,20,21}, {2,3,6,7,18,19,22,23}, {8,9,12,13,24,25,28,29}, {10,11,14,15,26,27,30,31} };
static const uint8_t bt16[8][4] = { {0,2,8,10}, {1,3,9,11}, {4,6,12,14}, {5,7,13,15}, {16,18,24,26}, {17,19,25,27}, {20,22,28,30}, {21,23,29,31} };
static const uint8_t bt16s[8][4] = { {0,2,16,18}, {1,3,17,19}, {8,10,24,26}, {9,11,25,27}, {4,6,20,22}, {5,7,21,23}, {12,14,28,30}, {13,15,29,31} };
static const uint8_t bt8[4][8] = { {0,1,4,5,16,17,20,21}, {2,3,6,7,18,19,22,23}, {8,9,12,13,24,25,28,29}, {10,11,14,15,26,27,30,31} };
static const uint8_t bt4[8][4] = { {0,2,8,10}, {1,3,9,11}, {4,6,12,14}, {5,7,13,15}, {16,18,24,26}, {17,19,25,27}, {20,22,28,30}, {21,23,29,31} };
static const uint8_t ct32[8][8] = { {0,1,4,5,8,9,12,13}, {2,3,6,7,10,11,14,15}, {16,17,20,21,24,25,28,29}, {18,19,22,23,26,27,30,31}, {32,33,36,37,40,41,44,45}, {34,35,38,39,42,43,46,47}, {48,49,52,53,56,57,60,61}, {50,51,54,55,58,59,62,63} };
static const uint8_t ct16[8][16] = { {0,2,8,10,16,18,24,26,1,3,9,11,17,19,25,27}, {4,6,12,14,20,22,28,30,5,7,13,15,21,23,29,31}, {32,34,40,42,48,50,56,58,33,35,41,43,49,51,57,59}, {36,38,44,46,52,54,60,62,37,39,45,47,53,55,61,63}, {64,66,72,74,80,82,88,90,65,67,73,75,81,83,89,91}, {68,70,76,78,84,86,92,94,69,71,77,79,85,87,93,95}, {96,98,104,106,112,114,120,122,97,99,105,107,113,115,121,123}, {100,102,108,110,116,118,124,126,101,103,109,111,117,119,125,127} };
static const uint8_t ct8[16][16] = { {0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54}, {8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62}, {33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23}, {41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31}, {96,100,112,116,64,68,80,84,98,102,114,118,66,70,82,86}, {104,108,120,124,72,76,88,92,106,110,122,126,74,78,90,94}, {65,69,81,85,97,101,113,117,67,71,83,87,99,103,115,119}, {73,77,89,93,105,109,121,125,75,79,91,95,107,111,123,127}, {128,132,144,148,160,164,176,180,130,134,146,150,162,166,178,182}, {136,140,152,156,168,172,184,188,138,142,154,158,170,174,186,190}, {161,165,177,181,129,133,145,149,163,167,179,183,131,135,147,151}, {169,173,185,189,137,141,153,157,171,175,187,191,139,143,155,159}, {224,228,240,244,192,196,208,212,226,230,242,246,194,198,210,214}, {232,236,248,252,200,204,216,220,234,238,250,254,202,206,218,222}, {193,197,209,213,225,229,241,245,195,199,211,215,227,231,243,247}, {201,205,217,221,233,237,249,253,203,207,219,223,235,239,251,255} };
static const uint16_t ct4[16][32] = { {0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110}, {16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126}, {65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47}, {81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63}, {192,200,224,232,128,136,160,168,194,202,226,234,130,138,162,170,196,204,228,236,132,140,164,172,198,206,230,238,134,142,166,174}, {208,216,240,248,144,152,176,184,210,218,242,250,146,154,178,186,212,220,244,252,148,156,180,188,214,222,246,254,150,158,182,190}, {129,137,161,169,193,201,225,233,131,139,163,171,195,203,227,235,133,141,165,173,197,205,229,237,135,143,167,175,199,207,231,239}, {145,153,177,185,209,217,241,249,147,155,179,187,211,219,243,251,149,157,181,189,213,221,245,253,151,159,183,191,215,223,247,255}, {256,264,288,296,320,328,352,360,258,266,290,298,322,330,354,362,260,268,292,300,324,332,356,364,262,270,294,302,326,334,358,366}, {272,280,304,312,336,344,368,376,274,282,306,314,338,346,370,378,276,284,308,316,340,348,372,380,278,286,310,318,342,350,374,382}, {321,329,353,361,257,265,289,297,323,331,355,363,259,267,291,299,325,333,357,365,261,269,293,301,327,335,359,367,263,271,295,303}, {337,345,369,377,273,281,305,313,339,347,371,379,275,283,307,315,341,349,373,381,277,285,309,317,343,351,375,383,279,287,311,319}, {448,456,480,488,384,392,416,424,450,458,482,490,386,394,418,426,452,460,484,492,388,396,420,428,454,462,486,494,390,398,422,430}, {464,472,496,504,400,408,432,440,466,474,498,506,402,410,434,442,468,476,500,508,404,412,436,444,470,478,502,510,406,414,438,446}, {385,393,417,425,449,457,481,489,387,395,419,427,451,459,483,491,389,397,421,429,453,461,485,493,391,399,423,431,455,463,487,495}, {401,409,433,441,465,473,497,505,403,411,435,443,467,475,499,507,405,413,437,445,469,477,501,509,407,415,439,447,471,479,503,511} };
enum { CT32 = 0, CT24 = 1, CT16 = 2, CT16S = 0xa, T8 = 0x13, T4 = 0x14, T8H = 0x1b, T4HL = 0x24, T4HH = 0x2c, Z32 = 0x30, Z24 = 0x31, Z16 = 0x32, Z16S = 0x3a };
/* byte (pa4: nibble) address of pixel x, y of a buffer at block bp, width bw (64-pixel units); Z formats xor the block number with 0x18 */
static inline uint32_t pa32(uint32_t bp, uint32_t bw, int x, int y, uint32_t zx)
{ uint32_t bn = ((bp + (((uint32_t)(y >> 5) * bw + (x >> 6)) << 5) + bt32[(y >> 3) & 3][(x >> 3) & 7]) ^ zx) & 0x3fff; return bn << 8 | ct32[y & 7][x & 7] << 2; }
static inline uint32_t pa16(uint32_t bp, uint32_t bw, int x, int y, int s, uint32_t zx)
{ uint32_t bn = ((bp + (((uint32_t)(y >> 6) * bw + (x >> 6)) << 5) + (s ? bt16s : bt16)[(y >> 3) & 7][(x >> 4) & 3]) ^ zx) & 0x3fff; return bn << 8 | ct16[y & 7][x & 15] << 1; }
static inline uint32_t pa8(uint32_t bp, uint32_t bw, int x, int y)
{ uint32_t bn = (bp + (((uint32_t)(y >> 6) * (bw >> 1) + (x >> 7)) << 5) + bt8[(y >> 4) & 3][(x >> 4) & 7]) & 0x3fff; return bn << 8 | ct8[y & 15][x & 15]; }
static inline uint32_t pa4(uint32_t bp, uint32_t bw, int x, int y)
{ uint32_t bn = (bp + (((uint32_t)(y >> 7) * (bw >> 1) + (x >> 7)) << 5) + bt4[(y >> 4) & 7][(x >> 5) & 3]) & 0x3fff; return bn << 9 | ct4[y & 15][x & 31]; }
/* the shown-frames check (GSREF_SKIPCHK, below): per GS block, written since its hashes were taken (blk_dirty), and the lanes a draw
   wrote last (blk_drawn; 1: bits 0-23 of the 32-bit layout's words, 2: bits 24-31, 3: all; a transfer clears the lanes it writes) */
static int skc_on, g_wdraw, g_wlanes, skc_wlo = -1, skc_whi = -1; static uint8_t blk_dirty[16384], blk_drawn[16384]; static uint64_t blk_h[16384][5];
static int psm_lanes(int psm) { return (psm == CT24 || psm == Z24) ? 1 : (psm == T8H || psm == T4HL || psm == T4HH) ? 2 : 3; }
static uint32_t gs_baddr(int psm, uint32_t bp, uint32_t bw, int x, int y)   /* the byte that holds pixel (x, y) */
{
  x &= 2047; y &= 2047;
  switch (psm) {
    case CT32: case CT24: return pa32(bp, bw, x, y, 0);
    case Z32: case Z24: return pa32(bp, bw, x, y, 0x18);
    case CT16: case Z16: return pa16(bp, bw, x, y, 0, psm == Z16 ? 0x18 : 0);
    case CT16S: case Z16S: return pa16(bp, bw, x, y, 1, psm == Z16S ? 0x18 : 0);
    case T8: return pa8(bp, bw, x, y);
    case T4: return pa4(bp, bw, x, y) >> 1;
    default: return pa32(bp, bw, x, y, 0) + 3;   /* T8H, T4HL, T4HH */
  }
}
static long g_vb; static int g_call;   /* (defined below) */
static void skc_mark(int psm, uint32_t bp, uint32_t bw, int x, int y)
{
  uint32_t bn = gs_baddr(psm, bp, bw, x, y) >> 8 & 0x3fff; int l = g_wlanes ? g_wlanes : psm_lanes(psm);
  blk_dirty[bn] = 1; if (g_wdraw) blk_drawn[bn] |= (uint8_t)l; else blk_drawn[bn] &= (uint8_t)~l;
  if (skc_wlo >= 0 && bn >= (uint32_t)skc_wlo && bn <= (uint32_t)skc_whi && (l & 2)) {   /* GSREF_SKCW=<lo>,<hi>: who writes byte 3 of those blocks */
    static long lvb = -1; static int lcall = -1, ldraw = -1, lpsm = -1; static long nw;
    if (g_vb != lvb || g_call != lcall || g_wdraw != ldraw || psm != lpsm) {
      if (nw) printf("skw   %ld writes\n", nw);
      printf("skw vb %ld call %d %s psm %02x bp %04x bw %u first block %04x at %d,%d\n", g_vb, g_call, g_wdraw ? "draw" : "transfer", psm, bp, bw, bn, x, y);
      lvb = g_vb; lcall = g_call; ldraw = g_wdraw; lpsm = psm; nw = 0; }
    nw++; }
}
static uint32_t gs_rd(int psm, uint32_t bp, uint32_t bw, int x, int y)   /* the raw value in the format's own width */
{
  x &= 2047; y &= 2047; uint32_t a;
  switch (psm) {
    case CT32: case Z32: a = pa32(bp, bw, x, y, psm == Z32 ? 0x18 : 0); return *(uint32_t *)(gsm + a);
    case CT24: case Z24: a = pa32(bp, bw, x, y, psm == Z24 ? 0x18 : 0); return *(uint32_t *)(gsm + a) & 0xffffff;
    case CT16: case Z16: a = pa16(bp, bw, x, y, 0, psm == Z16 ? 0x18 : 0); return *(uint16_t *)(gsm + a);
    case CT16S: case Z16S: a = pa16(bp, bw, x, y, 1, psm == Z16S ? 0x18 : 0); return *(uint16_t *)(gsm + a);
    case T8: return gsm[pa8(bp, bw, x, y)];
    case T4: a = pa4(bp, bw, x, y); return gsm[a >> 1] >> ((a & 1) * 4) & 15;
    case T8H: return gsm[pa32(bp, bw, x, y, 0) + 3];
    case T4HL: return gsm[pa32(bp, bw, x, y, 0) + 3] & 15;
    case T4HH: return gsm[pa32(bp, bw, x, y, 0) + 3] >> 4;
  }
  count(1, "!gs_rd psm %x", psm); return 0;
}
static void gs_wr(int psm, uint32_t bp, uint32_t bw, int x, int y, uint32_t v)
{
  x &= 2047; y &= 2047; uint32_t a;
  if (skc_on) skc_mark(psm, bp, bw, x, y);
  switch (psm) {
    case CT32: case Z32: a = pa32(bp, bw, x, y, psm == Z32 ? 0x18 : 0); *(uint32_t *)(gsm + a) = v; return;
    case CT24: case Z24: a = pa32(bp, bw, x, y, psm == Z24 ? 0x18 : 0); *(uint32_t *)(gsm + a) = (*(uint32_t *)(gsm + a) & 0xff000000u) | (v & 0xffffff); return;
    case CT16: case Z16: a = pa16(bp, bw, x, y, 0, psm == Z16 ? 0x18 : 0); *(uint16_t *)(gsm + a) = v; return;
    case CT16S: case Z16S: a = pa16(bp, bw, x, y, 1, psm == Z16S ? 0x18 : 0); *(uint16_t *)(gsm + a) = v; return;
    case T8: gsm[pa8(bp, bw, x, y)] = v; return;
    case T4: a = pa4(bp, bw, x, y); gsm[a >> 1] = (gsm[a >> 1] & (0xf0 >> ((a & 1) * 4))) | (v & 15) << ((a & 1) * 4); return;
    case T8H: gsm[pa32(bp, bw, x, y, 0) + 3] = v; return;
    case T4HL: a = pa32(bp, bw, x, y, 0) + 3; gsm[a] = (gsm[a] & 0xf0) | (v & 15); return;
    case T4HH: a = pa32(bp, bw, x, y, 0) + 3; gsm[a] = (gsm[a] & 0x0f) | (v & 15) << 4; return;
  }
  count(1, "!gs_wr psm %x", psm);
}
static int psm_bits(int psm) { switch (psm) { case CT32: case Z32: return 32; case CT24: case Z24: return 24; case CT16: case CT16S: case Z16: case Z16S: return 16;
  case T8: case T8H: return 8; default: return 4; } }

/* ================= GS registers, CLUT, transfers ================= */
static uint64_t gr[256];
static uint32_t exp16(uint32_t c)
{ uint64_t t = gr[0x3b]; uint32_t ta0 = t & 0xff, aem = t >> 15 & 1, ta1 = t >> 32 & 0xff, a = (c & 0x8000) ? ta1 : (!aem || (c & 0x7fff)) ? ta0 : 0;
  return a << 24 | (c & 0x7c00) << 9 | (c & 0x3e0) << 6 | (c & 0x1f) << 3; }
static uint32_t exp24(uint32_t c) { uint64_t t = gr[0x3b]; uint32_t a = (!(t >> 15 & 1) || (c & 0xffffff)) ? (t & 0xff) : 0; return a << 24 | (c & 0xffffff); }
static uint16_t clutb[512]; static uint32_t cbp0 = ~0u, cbp1 = ~0u;
static int is_idx(int psm) { return psm == T8 || psm == T4 || psm == T8H || psm == T4HL || psm == T4HH; }
static void clut_load(uint64_t t0)
{
  int psm = t0 >> 20 & 63, cpsm = t0 >> 51 & 15, csm = t0 >> 55 & 1, csa = t0 >> 56 & 31, cld = t0 >> 61 & 7; uint32_t cbp = t0 >> 37 & 0x3fff;
  if (!is_idx(psm)) return;
  switch (cld) { case 1: break; case 2: cbp0 = cbp; break; case 3: cbp1 = cbp; break;
    case 4: if (cbp == cbp0) return; cbp0 = cbp; break; case 5: if (cbp == cbp1) return; cbp1 = cbp; break; default: return; }
  int n = (psm == T8 || psm == T8H) ? 256 : 16;
  static uint64_t last; uint64_t key = (t0 >> 37) & 0x7ffffff;   /* CBP CPSM CSM CSA */
  count(1, "clut load %s cpsm %x csm %d cld %d%s", n == 256 ? "8" : "4", cpsm, csm, cld, key == last ? " (same as the previous load)" : "");
  last = key;
  for (int i = 0; i < n; i++) {
    int x, y; uint32_t bw = 1;
    if (csm) { uint64_t tc = gr[0x1c]; bw = tc & 63; x = (tc >> 6 & 63) * 16 + i; y = tc >> 12 & 1023; }
    else if (n == 256) { int p = (i & 0xe7) | (i & 8) << 1 | (i & 0x10) >> 1; x = p & 15; y = p >> 4; }   /* CSM1: entries 8-15 and 16-23 swapped */
    else { x = i & 7; y = i >> 3; }
    if (cpsm == CT32 || cpsm == CT24) { uint32_t c = gs_rd(CT32, cbp, bw, x, y); int k = (csa * 16 + i) & 255; clutb[k] = c; clutb[k + 256] = c >> 16; }
    else clutb[(csa * 16 + i) & 511] = gs_rd(cpsm == CT16S ? CT16S : CT16, cbp, bw, x, y);
  }
}
static uint32_t clut_rd(int cpsm, int csa, uint32_t idx)
{
  if (cpsm == CT32 || cpsm == CT24) { int k = (csa * 16 + idx) & 255; return clutb[k] | (uint32_t)clutb[k + 256] << 16; }
  return exp16(clutb[(csa * 16 + idx) & 511]);
}
/* host -> local transfers (IMAGE, HWREG) and local -> local copies */
static struct { int on, psm, x, y, sx, sy, w, h; uint32_t bp, bw; uint64_t bits; int nbits; uint32_t src0, srcn; int jumps; uint64_t hash; } xf;
static uint32_t blk_src[16384];   /* per GS block: EE source address of the transfer that last wrote it (0: none or drawn) */
static uint32_t g_src;   /* guest address of the GIF quadword being processed (XGKICK: 0xf0000000 + VU1 qword) */
static int xfer_log;     /* GSREF_XFERLOG=1: one line per host -> local transfer with its EE source (debug) */
static long g_vb;
static void xfer_start(int dir)
{
  uint64_t b = gr[0x50], p = gr[0x51], r = gr[0x52];
  int w = r & 0xfff, h = r >> 32 & 0xfff;
  if (dir == 0) { xf.on = 1; xf.psm = b >> 56 & 63; xf.bp = b >> 32 & 0x3fff; xf.bw = b >> 48 & 63; xf.sx = p >> 32 & 0x7ff; xf.sy = p >> 48 & 0x7ff; xf.w = w; xf.h = h; xf.x = xf.y = 0; xf.nbits = 0;
    count(1, "xfer host->local psm %x", xf.psm); count((long)w * h * psm_bits(xf.psm) / 8, "xfer host->local bytes"); xf.src0 = xf.srcn = 0; xf.jumps = 0; xf.hash = 0xcbf29ce484222325ull; return; }
  if (dir == 2) {
    int spsm = b >> 24 & 63, dpsm = b >> 56 & 63, ssx = p & 0x7ff, ssy = p >> 16 & 0x7ff, dsx = p >> 32 & 0x7ff, dsy = p >> 48 & 0x7ff, d = p >> 59 & 3;
    uint32_t sbp = b & 0x3fff, sbw = b >> 16 & 63, dbp = b >> 32 & 0x3fff, dbw = b >> 48 & 63;
    count(1, "xfer local->local spsm %x dpsm %x", spsm, dpsm); count((long)w * h, "xfer local->local pixels");
    if (xfer_log) printf("xfer local vb %ld spsm %02x sbp %04x sbw %u at %d,%d -> dpsm %02x dbp %04x dbw %u at %d,%d size %dx%d dir %d\n", g_vb, spsm, sbp, sbw, ssx, ssy,
                         dpsm, dbp, dbw, dsx, dsy, w, h, d);
    if (psm_bits(spsm) != psm_bits(dpsm)) count(1, "!xfer local->local size mismatch");
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
      int x = (d & 2) ? w - 1 - i : i, y = (d & 1) ? h - 1 - j : j;
      if (skc_on) g_wdraw = (blk_drawn[gs_baddr(spsm, sbp, sbw, ssx + x, ssy + y) >> 8 & 0x3fff] & psm_lanes(spsm)) != 0;
      gs_wr(dpsm, dbp, dbw, dsx + x, dsy + y, gs_rd(spsm, sbp, sbw, ssx + x, ssy + y));
      g_wdraw = 0;
    }
    return; }
  if (dir == 1) count(1, "xfer local->host (ignored)");
}
static void xfer_bytes(const uint8_t *p, int n)
{
  int bits = psm_bits(xf.psm);
  for (int i = 0; i < n && xf.on; i++) {
    xf.bits |= (uint64_t)p[i] << xf.nbits; xf.nbits += 8;
    while (xf.on && xf.nbits >= bits) {
      gs_wr(xf.psm, xf.bp, xf.bw, xf.sx + xf.x, xf.sy + xf.y, (uint32_t)(xf.bits & ((1ull << bits) - 1)));
      if (xf.psm == CT32) blk_src[pa32(xf.bp, xf.bw, (xf.sx + xf.x) & 2047, (xf.sy + xf.y) & 2047, 0) >> 8] = xf.src0;
      xf.bits >>= bits; xf.nbits -= bits;
      if (++xf.x == xf.w) { xf.x = 0; if (++xf.y == xf.h) { xf.on = 0;
        if (xfer_log) printf("xfer vb %ld src %08x..%08x jumps %d hash %016llx dbp %04x dbw %u psm %02x at %d,%d size %dx%d\n", g_vb, xf.src0, xf.srcn, xf.jumps, (unsigned long long)xf.hash, xf.bp, xf.bw, xf.psm, xf.sx, xf.sy, xf.w, xf.h); } }
    }
  }
}

/* ================= drawing ================= */
typedef GfVtx Vtx;                     /* ge/gsfront.h: x, y, u, v in 12.4 fixed point */
static Vtx cur; static float g_q = 1.0f; static Vtx vq[3]; static int vqn;
typedef GfDraw Draw;                   /* the state a primitive is drawn with: ge/gsfront.h, shared with the GE renderer's front end */
static long g_pix, g_dpx, g_rej[5], g_afail, g_prej[5];   /* g_prej: rejections in the current primitive */ static uint32_t g_entry;   /* g_entry: guest address of the running VU1 call's entry instruction */ static int g_call; static char g_st[200];   /* g_dpx, g_st: pixels and state of the current primitive */   /* pixels written, VU1 call index in this vblank */
static struct { uint32_t fbp; int psm, fbw, maxx, maxy; long prims; } tg[64]; static int ntg;
static void setup(Draw *d) { gf_setup(gr, d); }   /* ge/gsfront.c: the same rules as the GE renderer's front end */
/* texture use census (GSREF_TEXLOG=1): one entry per distinct TEX0 + CLAMP, with the EE packs it reads and the texel extent sampled */
typedef struct { uint64_t t0, cl; uint32_t tsrc, csrc; long prims, taps; int x0, x1, y0, y1; } TexUse;
static TexUse tu[8192]; static int ntu, cur_tu = -1, tex_log;
static uint8_t *ram_img; static void tc_check(Draw *d, uint64_t t0, uint64_t cl, uint32_t tsrc); static int wrapc(int c, int mode, int size, int mn, int mx);
static void tex_use(Draw *d)
{
  uint64_t t0 = gr[0x06 + d->ctxt] & ((1ull << 61) - 1), cl = gr[0x08 + d->ctxt];   /* TEX0 without CLD */
  uint32_t ts = blk_src[d->tbp[0] & 0x3fff], cs = is_idx(d->tpsm) ? blk_src[(t0 >> 37) & 0x3fff] : 0;
  int i; for (i = 0; i < ntu && !(tu[i].t0 == t0 && tu[i].cl == cl && tu[i].tsrc == ts && tu[i].csrc == cs); i++) {}
  if (i == ntu) { if (ntu == 8192) { cur_tu = -1; return; } tu[i] = (TexUse){ t0, cl, ts, cs, 0, 0, 1 << 30, -(1 << 30), 1 << 30, -(1 << 30) }; ntu++; if (ram_img) tc_check(d, t0, cl, ts); }
  tu[i].prims++; cur_tu = i;
  int cpsm = t0 >> 51 & 15, csm = t0 >> 55 & 1, csa = t0 >> 56 & 31, n = (d->tpsm == T8 || d->tpsm == T8H) ? 256 : (d->tpsm == T4 || d->tpsm == T4HL || d->tpsm == T4HH) ? 16 : 0; uint32_t cbp = t0 >> 37 & 0x3fff;
  if (n && cpsm == CT32 && !csm) {
    int bad = 0;
    for (int k = 0; k < n; k++) { int p = n == 256 ? ((k & 0xe7) | (k & 8) << 1 | (k & 0x10) >> 1) : k, x = n == 256 ? (p & 15) : (k & 7), y = n == 256 ? (p >> 4) : (k >> 3);
      if (clut_rd(cpsm, csa, n == 256 ? (uint32_t)(k - csa * 16) & 255 : (uint32_t)k) != gs_rd(CT32, cbp, 1, x, y)) bad++; }
    int bad8 = 0;   /* T4 alternative: entries csa*16.. of a 256-entry palette at CBP (CSM1 order) */
    if (n == 16) for (int k = 0; k < 16; k++) { int e = csa * 16 + k, p = (e & 0xe7) | (e & 8) << 1 | (e & 0x10) >> 1; if (clut_rd(cpsm, csa, k) != gs_rd(CT32, cbp, 1, p & 15, p >> 4)) bad8++; }
    count(1, "clut check %s: %s%s", n == 256 ? "t8" : "t4", bad ? "live CLUT differs from CBP" : "live CLUT = CBP", n == 16 && bad ? (bad8 ? ", and from CBP as 256 entries" : ", = CBP as 256 entries at CSA") : "");
  }
}
/* converter check (GSREF_RAM=<EE RAM dump of the same stage>): each new texture of the census is converted by ge/texconv.c from
   the game's page records in the dump and compared, texel by texel and one texel beyond each edge, with what this GS samples. */
static long tc_ok, tc_bad, tc_skip, tc_texels, tc_diff;
static const uint8_t *ram_ptr(uint32_t a) { return ram_img + (a & 0x1ffffff); }
static uint32_t rd_ram32(uint32_t a) { uint32_t v; memcpy(&v, ram_ptr(a), 4); return v; }
enum { PAGE_LIST = 0x1e6820, PAGE_COUNT = 0x18e8f0 };   /* SLES_503.83 texture page list (re/dg_objects.md §4) */
static void tc_check(Draw *d, uint64_t t0, uint64_t cl, uint32_t tsrc)
{
  TcPage pg; int found = 0;   /* the page one of whose uploads wrote the texture's first block */
  for (uint32_t i = 0, n = rd_ram32(PAGE_COUNT); i < n && i < 256 && !found; i++) {
    uint32_t p = rd_ram32(PAGE_LIST + 4 * i);
    if (tc_page(&pg, ram_ptr(p), ram_ptr) < 0) { count(1, "!texconv page packet not parsed"); continue; }
    for (int k = 0; k < pg.n; k++) if (pg.up[k].src == ram_ptr(tsrc)) found = 1;
  }
  if (!found || !tc_texture_size(t0, cl)) { tc_skip++; count(1, "texconv skipped (%s)", found ? "not a T8/T4 region" : "no page"); return; }
  static uint8_t buf[512 * 512]; TcTex t; uint32_t pal[256]; uint8_t pa[256];
  uint8_t have[32]; tc_texture(&pg, t0, cl, &t, buf); tc_palette(&pg, t0 >> 37 & 0x3fff, pal, pa, have);
  int csa = t0 >> 56 & 31, bad = 0, pm = 0;
  for (int i = 0; i < 256; i++) { int e = (i + (t.psm == 4 ? csa * 16 : 0)) & 255; if ((t.used[i >> 3] >> (i & 7) & 1) && !(have[e >> 3] >> (e & 7) & 1)) pm++; }
  int ex = (t.repeat & 1) ? t.w : 1, ey = (t.repeat & 2) ? t.h : 1;   /* beyond each edge: one texel (clamp) or a whole period (repeat) */
  for (int y = t.v0 - ey; y < t.v0 + t.h + ey; y++) for (int x = t.u0 - ex; x < t.u0 + t.w + ex; x++) {
    int gx = wrapc(x, d->wms, 1 << d->tw, d->minu, d->maxu), gy = wrapc(y, d->wmt, 1 << d->th, d->minv, d->maxv);
    uint32_t g = clut_rd(d->cpsm, d->csa, gs_rd(d->tpsm, d->tbp[0], d->tbw[0], gx, gy));
    int e = (tc_texel(&t, buf, x - t.u0, y - t.v0) + (t.psm == 4 ? csa * 16 : 0)) & 255;
    uint32_t c = (pal[e] & 0xffffff) | (uint32_t)pa[e] << 24;
    tc_texels++; if (c != g) { bad++; tc_diff++; }
  }
  if (bad || t.missing || pm) { tc_bad++; count(1, "!texconv mismatch: %d texels, %d missing, %d used palette entries missing", bad, t.missing, pm); }
  else tc_ok++;
}
static const char *PRIMN[8] = { "point", "line", "linestrip", "tri", "tristrip", "trifan", "sprite", "prim7" };
static void note_prim(Draw *d)
{
  static const char *AS[4] = { "Cs", "Cd", "0", "?" }, *CS[4] = { "As", "Ad", "FIX", "?" }, *TF[4] = { "modulate", "decal", "highlight", "highlight2" };
  count(1, "prim %s", PRIMN[d->prim]);
  char st[200]; int n = snprintf(st, sizeof st, "state %s%s%s ctx%d fb %x/%x z %x/%x %s", PRIMN[d->prim], d->iip ? " gouraud" : " flat", d->fge ? " fog" : "", d->ctxt, d->fpsm, d->fbp / 32, d->zpsm, d->zbp / 32,
                                 d->zte ? (const char *[]){ "never", "always", "gequal", "greater" }[d->ztst] : "off");
  n += snprintf(st + n, sizeof st - n, "%s", d->zmsk ? "/nowrite" : "/write");
  if (d->abe || d->aa1) n += snprintf(st + n, sizeof st - n, " blend (%s-%s)*%s+%s", AS[d->ba], AS[d->bb], CS[d->bc], AS[d->bd]);
  if (d->abe && d->bc == 2) n += snprintf(st + n, sizeof st - n, " fix%02x", d->fix);
  if (d->ate) n += snprintf(st + n, sizeof st - n, " atest %d/%02x fail%d", d->atst, d->aref, d->afail);
  if (d->date) n += snprintf(st + n, sizeof st - n, " datest%d", d->datm);
  if (d->aa1) n += snprintf(st + n, sizeof st - n, " aa1");
  if (d->fbmsk) n += snprintf(st + n, sizeof st - n, " fbmsk %08x", d->fbmsk);
  count(1, "%s", st); strcpy(g_st, st); g_dpx = 0; g_afail = 0;
  count(1, "entry %06x prims", g_entry); count(1, "entry %06x %s", g_entry, st);
  if (d->tme) {
    if (tex_log) tex_use(d);
    count(1, "tex psm %x cpsm %x %dx%d %s tcc%d %s wrap %d/%d mag%d min%d mxl%d lcm%d", d->tpsm, d->cpsm, 1 << d->tw, 1 << d->th, TF[d->tfx], d->tcc,
          d->fst ? "uv" : "stq", d->wms, d->wmt, d->mmag, d->mmin, d->mxl, d->lcm);
    for (int ax = 0; ax < 2; ax++) {   /* region modes: size, power of two, alignment of the origin (GE textures start on 16 bytes and repeat or clamp at a power of two) */
      int wm = ax ? d->wmt : d->wms, mn = ax ? d->minv : d->minu, mx = ax ? d->maxv : d->maxu;
      if (wm == 2) { int w = mx - mn + 1, a = mn & -mn; count(1, "region clamp %c size %d%s origin align %d", "uv"[ax], w, (w & (w - 1)) ? " (not pow2)" : "", mn ? (a > 1024 ? 1024 : a) : 1024); }
      if (wm == 3) { int a = mx & -mx; count(1, "region repeat %c mask %d%s offset align %d", "uv"[ax], mn + 1, ((mn + 1) & mn) ? " (not pow2)" : "", mx ? (a > 1024 ? 1024 : a) : 1024); }
    }
    for (int i = 0; i < ntg; i++)      /* a texture inside a buffer drawn this vblank: render to texture */
      if (d->tbp[0] >= tg[i].fbp && d->tbp[0] < tg[i].fbp + 32 * (uint32_t)((tg[i].maxy + 32) / 32 * (tg[i].fbw ? tg[i].fbw : 1)))
        count(1, "tex from target %x (tbp %x psm %x)", tg[i].fbp / 32, d->tbp[0], d->tpsm);
  }
  int i; for (i = 0; i < ntg && !(tg[i].fbp == d->fbp && tg[i].psm == d->fpsm); i++) {}
  if (i == ntg && ntg < 64) { tg[i].fbp = d->fbp; tg[i].psm = d->fpsm; tg[i].fbw = d->fbw; tg[i].maxx = tg[i].maxy = 0; ntg++; }
  if (i < 64) tg[i].prims++;
}
static void note_px(Draw *d, int x, int y)
{
  for (int i = 0; i < ntg; i++) if (tg[i].fbp == d->fbp && tg[i].psm == d->fpsm) { if (x > tg[i].maxx) tg[i].maxx = x; if (y > tg[i].maxy) tg[i].maxy = y; return; }
}
static int wrapc(int c, int mode, int size, int mn, int mx)
{
  switch (mode) { case 0: return c & (size - 1); case 1: return c < 0 ? 0 : c >= size ? size - 1 : c; case 2: return c < mn ? mn : c > mx ? mx : c; default: return (c & mn) | mx; }
}
static uint32_t texel(Draw *d, int lv, int x, int y)
{
  int w = 1 << d->tw, h = 1 << d->th; w = w >> lv ? w >> lv : 1; h = h >> lv ? h >> lv : 1;
  if (tex_log && cur_tu >= 0 && lv == 0) { TexUse *t = &tu[cur_tu]; t->taps++; if (x < t->x0) t->x0 = x; if (x > t->x1) t->x1 = x; if (y < t->y0) t->y0 = y; if (y > t->y1) t->y1 = y; }
  x = wrapc(x, d->wms, w, d->minu, d->maxu); y = wrapc(y, d->wmt, h, d->minv, d->maxv);
  uint32_t v = gs_rd(d->tpsm, d->tbp[lv], d->tbw[lv], x, y);
  switch (d->tpsm) { case CT32: return v; case CT24: return exp24(v); case CT16: case CT16S: return exp16(v);
    case T8: case T8H: case T4: case T4HL: case T4HH: return clut_rd(d->cpsm, d->csa, v); }
  return 0;
}
static uint32_t tex_level(Draw *d, int lv, int lin, float u, float v)   /* u, v: level-0 texel units */
{
  u /= (float)(1 << lv); v /= (float)(1 << lv);
  if (!lin) return texel(d, lv, (int)floorf(u), (int)floorf(v));
  u -= 0.5f; v -= 0.5f; int x = (int)floorf(u), y = (int)floorf(v); float fx = u - x, fy = v - y;
  uint32_t c00 = texel(d, lv, x, y), c10 = texel(d, lv, x + 1, y), c01 = texel(d, lv, x, y + 1), c11 = texel(d, lv, x + 1, y + 1), o = 0;
  for (int s = 0; s < 32; s += 8) {
    float a = (c00 >> s & 255) * (1 - fx) + (c10 >> s & 255) * fx, b = (c01 >> s & 255) * (1 - fx) + (c11 >> s & 255) * fx;
    o |= (uint32_t)(int)(a * (1 - fy) + b * fy + 0.5f) << s;
  }
  return o;
}
static uint32_t tex_sample(Draw *d, float u, float v, float q)
{
  float lod = d->lcm ? d->k : log2f(1.0f / fabsf(q ? q : 1e-30f)) * (float)(1 << d->l) + d->k;
  if (lod <= 0 || d->mxl == 0 && d->mmin < 2) return tex_level(d, 0, lod <= 0 ? d->mmag : d->mmin & 1, u, v);
  int lin = d->mmin == 1 || d->mmin == 4 || d->mmin == 5; if (d->mmin < 2) return tex_level(d, 0, lin, u, v);
  if (lod > d->mxl) lod = d->mxl;
  if (d->mmin == 2 || d->mmin == 4) return tex_level(d, (int)(lod + 0.5f) > d->mxl ? d->mxl : (int)(lod + 0.5f), lin, u, v);
  int l0 = (int)lod, l1 = l0 + 1 > d->mxl ? d->mxl : l0 + 1; float f = lod - l0;
  uint32_t a = tex_level(d, l0, lin, u, v), b = tex_level(d, l1, lin, u, v), o = 0;
  for (int s = 0; s < 32; s += 8) o |= (uint32_t)(int)((a >> s & 255) * (1 - f) + (b >> s & 255) * f + 0.5f) << s;
  return o;
}
static inline int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static int no_fog;   /* GSREF_NOFOG=1: skip fog (debug: compare with the GE replay, which has no fog in through mode) */
static uint32_t fog_only[4]; static int n_fog_only;   /* GSREF_FOGONLY=<entry, hex>[,<entry>..]: fog only on those VU1 entries' primitives
                                                          (the GE replay v4 fogs only df0, v5 df0 and the effects) */
static int fog_on(uint32_t e) { if (!n_fog_only) return 1; for (int k = 0; k < n_fog_only; k++) if (fog_only[k] == e) return 1; return 0; }
/* GSREF_ZCENSUS=1: depth tests whose result changes in a 16-bit buffer (Z / 256, truncated), by the testing entry and the entry that wrote the stored depth */
static long zc_zh[25], zc_np, zc_above[5], zc_cross[5], zc_crossw[5];
static long zc_abovew[5]; static int zc_nbig; static uint32_t zc_tg[64][5]; static int zc_nt; static uint32_t zc_wmax[16];
static int zc_on; static uint32_t *zc_own; static long zc_n[64][5]; static uint32_t zc_key[64][2]; static int zc_nk; static long zc_dz[2][16];
static void zc_note(const Draw *d, int x2, int y2, uint32_t z, uint32_t zd, int p24)
{
  uint32_t z16 = z >> 8, zd16 = zd >> 8; int p16 = d->ztst == 2 ? z16 >= zd16 : z16 > zd16;
  { int b = 0; while (b < 24 && (1u << b) <= z) b++; zc_zh[b]++; }
  uint32_t own = zc_own[y2 * 2048 + x2]; int k;
  for (k = 0; k < zc_nk; k++) if (zc_key[k][0] == g_entry && zc_key[k][1] == own) break;
  if (k == zc_nk) { if (zc_nk == 64) return; zc_key[k][0] = g_entry; zc_key[k][1] = own; zc_nk++; }
  zc_n[k][0]++; if (p24 != p16) { zc_n[k][p24 ? 1 : 2]++; long dz = (long)z - (long)zd; if (dz < 0) dz = -dz; int b = 0; while (b < 15 && (1L << b) <= dz) b++; zc_dz[p24 ? 0 : 1][b]++; }
  if (p24) zc_n[k][3]++;
}
static void zc_prim(const Draw *d, const Vtx **v, int n)
{
  if (!d->zte || d->ztst < 2) return;
  uint32_t lo = v[0]->z, hi = v[0]->z; for (int i = 1; i < n; i++) { if (v[i]->z < lo) lo = v[i]->z; if (v[i]->z > hi) hi = v[i]->z; }
  zc_np++; for (int t = 0; t < 5; t++) { uint32_t T = 0x100000u << t; if (lo >= T) zc_above[t]++; else if (hi >= T) { zc_cross[t]++; if (!d->zmsk) zc_crossw[t]++; } }
  if (!d->zmsk && hi > zc_wmax[g_vb & 15]) zc_wmax[g_vb & 15] = hi;
  if (!d->zmsk) { int k; for (k = 0; k < zc_nt; k++) if (zc_tg[k][0] == (uint32_t)g_vb && zc_tg[k][1] == d->fbp && zc_tg[k][2] == d->zbp) break;
    if (k == zc_nt && zc_nt < 64) { zc_tg[k][0] = (uint32_t)g_vb; zc_tg[k][1] = d->fbp; zc_tg[k][2] = d->zbp; zc_tg[k][3] = 0; zc_tg[k][4] = 0xffffffffu; zc_nt++; }
    if (k < zc_nt) { if (hi > zc_tg[k][3]) zc_tg[k][3] = hi; if (lo < zc_tg[k][4]) zc_tg[k][4] = lo; } }
  if (!d->zmsk && hi >= 0x100000u && zc_nbig < 40) { zc_nbig++; printf("zcensus near write vb %ld call %d entry %06x prim %d n %d z %u..%u | %s\n", g_vb, g_call, g_entry, d->prim, n, lo, hi, g_st); }
  if (!d->zmsk) for (int t = 0; t < 5; t++) if (hi >= 0x100000u << t) zc_abovew[t]++;
}
static void zc_report(void)
{
  printf("zcensus fragment Z24 bits:"); for (int b = 0; b < 25; b++) printf(" %ld", zc_zh[b]); printf("\n");
  printf("zcensus depth-tested prims %ld; for T = 1M 2M 4M 8M 16M: above, crossing (crossing with Z write)\n", zc_np);
  for (int t = 0; t < 5; t++) printf("zcensus T %uM above %ld crossing %ld (%ld)\n", 1u << t, zc_above[t], zc_cross[t], zc_crossw[t]);
  for (int t = 0; t < 5; t++) printf("zcensus T %uM depth-writing prims reaching it %ld\n", 1u << t, zc_abovew[t]);
  for (int k = 0; k < zc_nt; k++) printf("zcensus target vb %u fbp %x zbp %x written Z24 %u..%u\n", zc_tg[k][0], zc_tg[k][1], zc_tg[k][2], zc_tg[k][4], zc_tg[k][3]);
  printf("zcensus largest written Z24 per vblank (vb & 15):"); for (int i = 0; i < 16; i++) if (zc_wmax[i]) printf(" %d:%u", i, zc_wmax[i]); printf("\n");
  printf("zcensus: tests by entry <- stored depth's writer: tests, pass in Z24 but fail in Z16, fail in Z24 but pass in Z16, Z24 passes\n");
  for (int k = 0; k < zc_nk; k++) if (zc_n[k][1] || zc_n[k][2]) printf("zcensus %06x <- %06x tests %ld p24f16 %ld f24p16 %ld pass24 %ld\n", zc_key[k][0], zc_key[k][1], zc_n[k][0], zc_n[k][1], zc_n[k][2], zc_n[k][3]);
  for (int t = 0; t < 2; t++) { printf("zcensus |dz| bits (%s):", t ? "f24p16" : "p24f16"); for (int b = 0; b < 16; b++) printf(" %ld", zc_dz[t][b]); printf("\n"); }
}
static int zq_sh; static uint32_t zq_max = 0xffffffu;   /* GSREF_ZQ=<shift>[,<max, hex>]: a depth buffer of fewer bits (Z = min(Z, max) >> shift), the GE's 16 bits for shift 8 */
static void pix_(Draw *d, int x, int y, double zf, int r, int g, int b, int a, int f, float s, float t, float q, float uu, float vv)
{
  if (x < d->scx0 || x > d->scx1 || y < d->scy0 || y > d->scy1) { g_rej[0]++; g_prej[0]++; return; }
  if (d->tme) {
    float tu, tv; if (d->fst) { tu = uu / 16.0f; tv = vv / 16.0f; } else { float iq = q ? 1.0f / q : 0; tu = s * iq * (float)(1 << d->tw); tv = t * iq * (float)(1 << d->th); }
    uint32_t tc = tex_sample(d, tu, tv, d->fst ? 1.0f : q); int tr = tc & 255, tgc = tc >> 8 & 255, tb = tc >> 16 & 255, ta = tc >> 24;
    switch (d->tfx) {
      case 0: r = clamp255(r * tr >> 7); g = clamp255(g * tgc >> 7); b = clamp255(b * tb >> 7); if (d->tcc) a = clamp255(a * ta >> 7); break;
      case 1: r = tr; g = tgc; b = tb; if (d->tcc) a = ta; break;
      case 2: r = clamp255((r * tr >> 7) + a); g = clamp255((g * tgc >> 7) + a); b = clamp255((b * tb >> 7) + a); if (d->tcc) a = clamp255(ta + a); break;
      case 3: r = clamp255((r * tr >> 7) + a); g = clamp255((g * tgc >> 7) + a); b = clamp255((b * tb >> 7) + a); if (d->tcc) a = ta; break;
    }
  }
  if (d->fge && !no_fog && fog_on(g_entry)) { r = (f * r + (255 - f) * d->fcr) >> 8; g = (f * g + (255 - f) * d->fcg) >> 8; b = (f * b + (255 - f) * d->fcb) >> 8; }
  int fbw = 1, zbw = !d->zmsk, abw = 1;
  if (d->ate) {
    int p; switch (d->atst) { case 0: p = 0; break; case 1: p = 1; break; case 2: p = a < d->aref; break; case 3: p = a <= d->aref; break;
      case 4: p = a == d->aref; break; case 5: p = a >= d->aref; break; case 6: p = a > d->aref; break; default: p = a != d->aref; }
    if (!p) { g_rej[1]++; g_prej[1]++; if (d->afail) g_afail++; }
    if (!p) switch (d->afail) { case 0: return; case 1: zbw = 0; break; case 2: fbw = 0; break; default: zbw = 0; abw = 0; }
  }
  int x2 = x & 2047, y2 = y & 2047;
  uint32_t dst = gs_rd(d->fpsm, d->fbp, d->fbw, x2, y2); int dr, dg, db, da;
  if (d->fpsm == CT16 || d->fpsm == CT16S) { dr = (dst & 0x1f) << 3; dg = (dst >> 5 & 0x1f) << 3; db = (dst >> 10 & 0x1f) << 3; da = dst & 0x8000 ? 0x80 : 0; }
  else { dr = dst & 255; dg = dst >> 8 & 255; db = dst >> 16 & 255; da = d->fpsm == CT24 ? 0x80 : dst >> 24; }
  if (d->date && (d->fpsm == CT32 || d->fpsm == CT16 || d->fpsm == CT16S) && ((da >> 7) & 1) != d->datm) { g_rej[2]++; g_prej[2]++; return; }
  uint32_t zmax = d->zpsm == Z32 ? 0xffffffffu : d->zpsm == Z24 ? 0xffffff : 0xffff;
  uint32_t z = zf <= 0 ? 0 : zf >= (double)zmax ? zmax : (uint32_t)zf;
  if (zq_sh) { uint32_t zc = z > zq_max ? zq_max : z; z = (zc >> zq_sh) << zq_sh; }
  if (d->zte) {
    uint32_t zd = gs_rd(d->zpsm, d->zbp, d->fbw, x2, y2);
    int p = d->ztst == 1 || (d->ztst == 2 && z >= zd) || (d->ztst == 3 && z > zd);
    if (zc_on && d->ztst >= 2) zc_note(d, x2, y2, z, zd, p);
    if (!p) { g_rej[3]++; g_prej[3]++; return; }
  }
  if ((d->abe || d->aa1) && !(d->pabe && a < 0x80)) {
    int cs[3] = { r, g, b }, cd[3] = { dr, dg, db }, o[3];
    int cc = d->bc == 0 ? a : d->bc == 1 ? da : d->fix;
    for (int i = 0; i < 3; i++) {
      int A = d->ba == 0 ? cs[i] : d->ba == 1 ? cd[i] : 0, B = d->bb == 0 ? cs[i] : d->bb == 1 ? cd[i] : 0, D = d->bd == 0 ? cs[i] : d->bd == 1 ? cd[i] : 0;
      int v = ((A - B) * cc >> 7) + D; o[i] = d->colclamp ? clamp255(v) : v & 255;
    }
    r = o[0]; g = o[1]; b = o[2];
  }
  if (fbw) {
    if (d->fpsm == CT16 || d->fpsm == CT16S) {
      uint32_t v = (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10 | (uint32_t)((a >> 7 | d->fba) & 1) << 15, m = d->fbmsk;
      uint32_t m16 = (m >> 3 & 0x1f) | (m >> 6 & 0x3e0) | (m >> 9 & 0x7c00) | (m >> 16 & 0x8000); if (!abw) m16 |= 0x8000;
      g_wdraw = 1; g_wlanes = (m16 & 0xffff) != 0xffff ? 3 : 4; gs_wr(d->fpsm, d->fbp, d->fbw, x2, y2, (v & ~m16) | (dst & m16)); g_wdraw = 0; g_wlanes = 0;
    } else {
      uint32_t v = r | g << 8 | b << 16 | (uint32_t)(a | (d->fba ? 0x80 : 0)) << 24, m = d->fbmsk | (abw ? 0 : 0xff000000u);
      g_wdraw = 1; g_wlanes = ((m & 0xffffff) != 0xffffff ? 1 : 0) | (d->fpsm != CT24 && (m >> 24) != 0xff ? 2 : 0); if (!g_wlanes) g_wlanes = 4;
      gs_wr(d->fpsm, d->fbp, d->fbw, x2, y2, (v & ~m) | (dst & m)); g_wdraw = 0; g_wlanes = 0;
    }
    g_pix++; g_dpx++; note_px(d, x2, y2);
  }
  if (zbw && d->zte) { g_wdraw = 1; gs_wr(d->zpsm, d->zbp, d->fbw, x2, y2, z); g_wdraw = 0; if (zc_on) zc_own[y2 * 2048 + x2] = g_entry; }
}
static int pick_x = -1, pick_y; static long pick_vb;   /* GSREF_PICK=<x>,<y>,<vblank>: log every pixel write attempt at that frame-buffer pixel */
static void pix(Draw *d, int x, int y, double zf, int r, int g, int b, int a, int f, float s, float t, float q, float uu, float vv)
{
  if (pick_x < 0 || x != pick_x || y != pick_y || g_vb != pick_vb) { pix_(d, x, y, zf, r, g, b, a, f, s, t, q, uu, vv); return; }
  long r0[5]; memcpy(r0, g_prej, sizeof r0);
  uint32_t fb0 = gs_rd(d->fpsm, d->fbp, d->fbw, x & 2047, y & 2047), zb0 = d->zte ? gs_rd(d->zpsm, d->zbp, d->fbw, x & 2047, y & 2047) : 0;
  pix_(d, x, y, zf, r, g, b, a, f, s, t, q, uu, vv);
  uint32_t fb1 = gs_rd(d->fpsm, d->fbp, d->fbw, x & 2047, y & 2047), zb1 = d->zte ? gs_rd(d->zpsm, d->zbp, d->fbw, x & 2047, y & 2047) : 0;
  printf("pick vb %ld call %d entry %06x fbp %x z %.2f zd %06x->%06x fb %08x->%08x rej sc%ld at%ld da%ld zt%ld | %s\n", g_vb, g_call, g_entry, d->fbp, zf, zb0, zb1, fb0, fb1,
         g_prej[0] - r0[0], g_prej[1] - r0[1], g_prej[2] - r0[2], g_prej[3] - r0[3], g_st);
}
/* ================= GE replay export (GSREF_GEOUT=<file>:<vblank>): ge/replay_fmt.h ================= */
static int dc_on, dc_kind, dc_tagn, dc_have, dc_ti, dc_tw2, dc_th2, dc_ofx, dc_ofy, dc_twop; static long dc_px, dc_stdiff, dc_nx, dc_nxv, fp_nx, fp_nxv, fp_nxp; static GerBatch dc_state;   /* the running df0 call (below) */
static FILE *ge_f; static long ge_vb = -1; static int ge_on;
static double ge_zk = 1.0 / 256;   /* GSREF_ZSCALE=<k>: GE depth = the PS2's Z24 x k (1/256 maps the whole 24-bit range; the deck's depth-writing draws stay below Z24 = 292k) */
static GbOut G;   /* the export's batches, vertices, textures and data (ge/gebatch.h: the GE renderer's batch writer, whose rules were this file's) */
static int ge_live;   /* GSREF_GELIVE */
/* P06 under GSREF_XLCONV: the GS registers after pass 2's and pass 3's A+D packets as the translator gave them (xl_on_pass), and, in the
   export's frame, the state of the first primitive gsref draws in each pass (xc_p6ad: the A+D packets the running call kicked) */
static int xc_pmask, xc_p6, xc_p6ad, xc_p6have[4], xc_p6twop[4], xc_pr[4]; static uint32_t xc_p6hy; static uint64_t xc_pgr[4][256];
static GerBatch xc_p6st[4], xc_pb[4];
static int16_t ge_tex_of_tu[8192];   /* the export texture of each texture use (tu[], below), -1: not made yet */
/* GSREF_XLTEX=1 (with GSREF_XLATE=1): each indexed export texture made a second time from the translator's texture sources (ge/gsup.c:
   the transfers and CLUT loads its front end saw, texels read from the EE addresses the game sent them from, here the capture's own
   data) and compared with this file's conversion from GS memory; when the sources give every texel and palette entry, the export
   takes theirs */
static int xt_on, xt_shown; static uint8_t *xt_ram_img; static GuMap *xt_map; static long xt_n[8];
static const char *XTN[8] = { "indexed textures", "equal", "texels differ", "palette differs", "texels missing", "palette entries missing", "taken from the sources", "" };
static const uint8_t *xt_ram(void *ctx, uint32_t ee, uint32_t n) { (void)ctx; ee &= 0x1ffffff; return ee + n <= 32u << 20 ? xt_ram_img + ee : 0; }
static void xt_store(uint32_t a, const uint32_t *w, int n) { for (int k = 0; k < n; k++) memcpy(xt_ram_img + ((a + 4 * k) & 0x1ffffff), &w[k], 4); }
typedef struct { int psm; uint32_t tbp, tbw; long missing; } XtFetch;
static int xt_fetch(void *ctx, int x, int y) { XtFetch *f = ctx; int v = gu_index(xt_map, f->psm, f->tbp, f->tbw, x, y); if (v < 0) { f->missing++; return 0; } return v; }
/* P06 calls draw up to three texture passes, each after its own A+D packet: the translator gives the front end the packets in order
   (xl_on_pass) and the CLUT buffer of each pass is kept here, for the textures gsref makes while it runs that call */
static struct { uint64_t t0, cl; int64_t c[512]; } xt_ps[3]; static int xt_nps;
static void xt_rec(void)
{
  const uint64_t *g = xl_front()->gr; uint64_t t0 = g[0x06] & ((1ull << 61) - 1); int cpsm = (int)(t0 >> 51 & 15);
  if (xt_nps == 3) return;
  xt_ps[xt_nps].t0 = t0; xt_ps[xt_nps].cl = g[0x08];
  for (int k = 0; k < 512; k++) xt_ps[xt_nps].c[k] = gu_clut(xt_map, cpsm, k);
  xt_nps++;
}
static int is_p06(uint32_t e) { return e == 0x19fe08 || e == 0x1a0160 || e == 0x1a0540 || e == 0x1a08c0; }
static int is_lit(uint32_t e) { return is_p06(e) || e == 0x1a31e0; }   /* P06 or P07 (the codec faces): GERK_LIT, passes 2 and 3 */
/* buf, tt, pal, mina: this file's conversion, replaced by the sources' when they are complete */
static void xt_tex(Draw *d, uint64_t t0, uint64_t cl, uint8_t *buf, size_t n, TcTex *tt, uint32_t *pal, int np, int *mina)
{
  static uint8_t b2[512 * 512]; TcTex t2; uint32_t p2[256]; int m2 = 255; long pmiss = 0;
  XtFetch f = { (int)(t0 >> 20 & 63), (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), 0 };
  xt_n[0]++;
  if (tc_texture_cb(t0, cl, xt_fetch, &f, &t2, b2)) { count(1, "!xltex conversion failed"); return; }
  const __typeof__(xt_ps[0]) *ps = 0;   /* in a P06 or P07 call: the CLUT of the pass with this TEX0 / CLAMP */
  if (is_lit(g_entry)) for (int k = 0; k < xt_nps && !ps; k++) if (xt_ps[k].t0 == (t0 & ((1ull << 61) - 1)) && xt_ps[k].cl == cl) ps = &xt_ps[k];
  for (int i = 0; i < np; i++) {
    int64_t c = ps ? ps->c[(d->csa * 16 + i) & 511] : gu_clut(xt_map, d->cpsm, d->csa * 16 + i); uint32_t v = c < 0 ? 0 : (d->cpsm == CT32 || d->cpsm == CT24) ? (uint32_t)c : exp16((uint32_t)c), a = v >> 24;
    if (c < 0) pmiss++;
    p2[i] = (v & 0xffffff) | (a >= 0x80 ? 0xffu : a * 2) << 24;
    if ((t2.used[i >> 3] >> (i & 7) & 1) && (int)a < m2) m2 = a;
  }
  if (f.missing) xt_n[4]++;
  if (pmiss) xt_n[5]++;
  if (f.missing || pmiss) {
    if (xt_shown < 8) { xt_shown++; fprintf(stderr, "xltex vb %ld: tbp %04x tbw %u psm %02x cbp %04x cpsm %x csa %d: %ld texels and %ld palette entries without a source\n",
                                        g_vb, f.tbp, f.tbw, f.psm, (unsigned)(t0 >> 37 & 0x3fff), d->cpsm, d->csa, f.missing, pmiss); }
    return;
  }
  long tb = 0, pb = 0; for (size_t i = 0; i < n; i++) tb += buf[i] != b2[i]; for (int i = 0; i < np; i++) pb += pal[i] != p2[i];
  int tdiff = tb || memcmp(tt, &t2, sizeof t2), pdiff = pb || *mina != m2;
  xt_n[tdiff ? 2 : 0] += tdiff; xt_n[3] += pdiff; xt_n[1] += !tdiff && !pdiff;
  if ((tdiff || pdiff) && xt_shown < 8) { xt_shown++; fprintf(stderr, "xltex vb %ld: tbp %04x tbw %u psm %02x cbp %04x cpsm %x csa %d: %ld texel bytes and %ld palette entries differ, min alpha %d / %d\n",
                                                      g_vb, f.tbp, f.tbw, f.psm, (unsigned)(t0 >> 37 & 0x3fff), d->cpsm, d->csa, tb, pb, *mina, m2); }
  memcpy(buf, b2, n); *tt = t2; memcpy(pal, p2, 4 * np); *mina = m2; xt_n[6]++;
}
static void xt_report(void)
{
  fprintf(stderr, "xltex:"); for (int i = 0; i < 7; i++) fprintf(stderr, "%s %s %ld", i ? "," : "", XTN[i], xt_n[i]);
  const GuStats *g = gu_stats(xt_map);
  fprintf(stderr, "\nxltex sources: %ld transfers (%ld other layout, %ld in pieces, %ld past 2048 or width 0), %ld local copies, %ld IMAGE qw orphaned; %ld CLUT loads (%ld too wide); %ld block entries dropped; live %d transfers, %d loads\n",
          g->xfers, g->xfer_other, g->xfer_split, g->xfer_wrap, g->xfer_local, g->image_orphan, g->loads, g->load_wide, g->dropped, g->live_xfers, g->live_loads);
}
typedef struct { Draw *d; } GsFetch;
static int gs_fetch(void *ctx, int x, int y) { Draw *d = ((GsFetch *)ctx)->d; return (int)gs_rd(d->tpsm, d->tbp[0], d->tbw[0], x & 2047, y & 2047); }
/* the export texture of the current draw: converted from GS memory now, while it holds this texture (pages reuse the area) */
static int ge_tex(Draw *d)
{
  if (cur_tu < 0) return -1;
  if (ge_tex_of_tu[cur_tu] >= 0) return ge_tex_of_tu[cur_tu];
  GerTex t; memset(&t, 0, sizeof t); int u0 = 0, v0 = 0;
  if (d->tpsm == CT32 || d->tpsm == CT24) {
    if (d->tbp[0] != 0 && d->tbp[0] != 0x1000) { count(1, "!ge direct-colour texture not a frame buffer (tbp %x)", d->tbp[0]); return -1; }
    t.kind = 2; t.psm = d->tbp[0] != d->fbp; t.tw = t.th = 512; t.bw = 512; t.rows = 272;
  } else {
    uint64_t t0 = gr[0x06 + d->ctxt], cl = gr[0x08 + d->ctxt];
    size_t n = tc_texture_size(t0, cl); if (!n) { count(1, "!ge texture not convertible (psm %x wrap %d/%d)", d->tpsm, d->wms, d->wmt); return -1; }
    static uint8_t buf[512 * 512]; TcTex tt; GsFetch f = { d };
    tc_texture_cb(t0, cl, gs_fetch, &f, &tt, buf);
    uint32_t pal[256]; int np = tt.psm == 8 ? 256 : 16, mina = 255;
    for (int i = 0; i < np; i++) { uint32_t c = clut_rd(d->cpsm, d->csa, i), a = c >> 24; pal[i] = (c & 0xffffff) | (a >= 0x80 ? 0xffu : a * 2) << 24;
      if ((tt.used[i >> 3] >> (i & 7) & 1) && (int)a < mina) mina = a; }
    if (xt_on) xt_tex(d, t0, cl, buf, n, &tt, pal, np, &mina);
    t.kind = 1; t.psm = tt.psm; t.repeat = tt.repeat; t.minalpha = mina; t.tw = tt.tw; t.th = tt.th; t.bw = tt.bw; t.rows = tt.rows;
    t.size = (uint32_t)n; t.data = gb_put(&G, buf, n); t.npal = np; t.pal = gb_put(&G, pal, 4 * np);
    u0 = tt.u0; v0 = tt.v0;
  }
  int i = gb_tex(&G, &t, u0, v0); if (i < 0) return -1;
  ge_tex_of_tu[cur_tu] = (int16_t)i; return i;
}
/* GSREF_XLDIRECT (below): a primitive of the GS front end; xd_use: the one whose data the export takes for the primitive being drawn */
typedef struct { GfDraw d; GfVtx v[3]; const GfVtx *vp[3]; int n, path, unk; } XdRec;
static const XdRec *xd_use;
/* one primitive the GS draws (after setup / note_prim) into the export: its state (ge/gebatch.c gb_state; what the replay lacks is
   counted here) and its vertices (gb_prim: appended to the current batch when the state is the same and, for strips and fans, it
   continues the batch's vertex order; else a new batch) */
static void ge_prim(Draw *d, const Vtx **vv, int n)
{
  if (xd_use) { d = (Draw *)&xd_use->d; vv = (const Vtx **)xd_use->vp; }
  int ti = d->tme ? ge_tex(d) : -1;
  GerBatch b; int u = gb_state(d, ti, g_entry, &b);
  if (u & GB_U_ANEVER) count(1, "!ge alpha test NEVER with afail %d not replayed", d->afail);
  if (u & GB_U_FBMSK) count(1, "!ge fbmsk %08x not replayed", d->fbmsk);
  if (u & GB_U_DFP) count(1, "!ge date / fba / pabe not replayed");
  if (u & GB_U_AFAIL) count(1, "!ge afail %d not replayed", d->afail);
  if (xc_p6 && xc_p6ad >= 1) {   /* a P06 call (GSREF_XLCONV): the state of each pass's first primitive (pass k: the k-th A+D packet of
                                    the passes its header y draws) */
    int ps[3], np = 0; ps[np++] = 1; if (xc_p6hy & 0x10) ps[np++] = 2; if (xc_p6hy & 0x700) ps[np++] = 3;
    int p = xc_p6ad <= np ? ps[xc_p6ad - 1] : 0;
    if (p && !xc_p6have[p]) { int ta = ti >= 0 && G.t[ti].kind == 1 && d->tcc ? G.t[ti].minalpha : 0x80;
      xc_p6st[p] = b; xc_p6have[p] = 1; xc_p6twop[p] = d->ate && d->afail == 1 && d->zte && !d->zmsk && d->atst >= 2 && (vv[0]->a * ta >> 7) < d->aref; }
  }
  if (dc_on) {   /* a df0 call: its transform batch takes the state of its first primitive (the A+D packet sets it once per call) */
    if (!dc_have) { dc_state = b; dc_have = 1; dc_ti = ti; dc_tw2 = d->tw; dc_th2 = d->th; dc_ofx = d->ofx; dc_ofy = d->ofy;
      int ta = ti >= 0 && G.t[ti].kind == 1 && d->tcc ? G.t[ti].minalpha : 0x80;
      dc_twop = d->ate && d->afail == 1 && d->zte && !d->zmsk && d->atst >= 2 && (vv[0]->a * ta >> 7) < d->aref; }
    else if (memcmp(&dc_state, &b, offsetof(GerBatch, twopass))) dc_stdiff++;
  }
  if (ge_live) { if (!xd_use) return; b.entry = 0; }   /* the live frame: the front end's primitives only */
  gb_prim(&G, &b, d, vv, n);
}
static void ge_write(long vb)
{
  gb_write(&G, ge_f, (uint32_t)vb, 480, 272); fclose(ge_f); ge_f = 0;
  long tw = 0; for (int i = 0; i < G.nb; i++) tw += G.b[i].twopass;
  fprintf(stderr, "gsref: GE replay of vb %ld: %d batches (%ld two-pass), %d vertices, %d textures, %u data bytes; %ld df0 transform batches with %ld vertices (%ld primitives with another state inside a call); %ld effect-mesh (P04+25f0) batches with %ld vertices and %ld clipped-polygon vertices\n",
          vb, G.nb, tw, G.nv, G.nt, G.nd, dc_nx, dc_nxv, dc_stdiff, fp_nx, fp_nxv, fp_nxp);
}

static void draw_tri_(const Vtx *v0, const Vtx *v1, const Vtx *v2)
{
  if ((int64_t)(v1->x - v0->x) * (v2->y - v0->y) == (int64_t)(v1->y - v0->y) * (v2->x - v0->x)) { count(1, "tri zero area (strip restarts)"); g_dpx = 0; g_st[0] = 0; return; }
  Draw d; setup(&d); note_prim(&d);
  const Vtx *v[3] = { v0, v1, v2 };
  if (ge_on) ge_prim(&d, v, 3);
  if (zc_on) zc_prim(&d, v, 3);
  int64_t X[3], Y[3]; for (int i = 0; i < 3; i++) { X[i] = v[i]->x - d.ofx; Y[i] = v[i]->y - d.ofy; }
  int64_t area = (X[1] - X[0]) * (Y[2] - Y[0]) - (Y[1] - Y[0]) * (X[2] - X[0]);
  int sgn = area > 0 ? 1 : -1;
  int64_t mnx = X[0], mxx = X[0], mny = Y[0], mxy = Y[0];
  for (int i = 1; i < 3; i++) { if (X[i] < mnx) mnx = X[i]; if (X[i] > mxx) mxx = X[i]; if (Y[i] < mny) mny = Y[i]; if (Y[i] > mxy) mxy = Y[i]; }
  int x0 = (int)((mnx + 15) >> 4), x1 = (int)(mxx >> 4), y0 = (int)((mny + 15) >> 4), y1 = (int)(mxy >> 4);
  if (x0 < d.scx0) x0 = d.scx0; if (x1 > d.scx1) x1 = d.scx1; if (y0 < d.scy0) y0 = d.scy0; if (y1 > d.scy1) y1 = d.scy1;
  if (x1 - x0 > 4096 || y1 - y0 > 4096) { count(1, "!tri huge"); return; }
  /* edge i: from v[i+1] to v[i+2]; inside when sgn * E >= 0, with the top-left rule on E == 0 */
  int64_t ex[3], ey[3]; int tl[3];
  for (int i = 0; i < 3; i++) { int a = (i + 1) % 3, b = (i + 2) % 3; ex[i] = X[b] - X[a]; ey[i] = Y[b] - Y[a];
    int64_t dx = ex[i] * sgn, dy = ey[i] * sgn; tl[i] = dy < 0 || (dy == 0 && dx > 0); }
  double inv = 1.0 / (double)area;
  float qs[3]; for (int i = 0; i < 3; i++) qs[i] = v[i]->q;
  for (int py = y0; py <= y1; py++) for (int px = x0; px <= x1; px++) {
    int64_t sx = (int64_t)px * 16, sy = (int64_t)py * 16, E[3]; int in = 1;
    for (int i = 0; i < 3; i++) { int a = (i + 1) % 3; E[i] = (sx - X[a]) * ey[i] - (sy - Y[a]) * ex[i]; E[i] = -E[i];
      int64_t e = E[i] * sgn; if (e < 0 || (e == 0 && !tl[i])) { in = 0; break; } }
    if (!in) continue;
    double l0 = E[0] * inv, l1 = E[1] * inv, l2 = E[2] * inv;
    double z = l0 * v0->z + l1 * v1->z + l2 * v2->z;
    int r, g, b, a, f;
    if (d.iip) { r = (int)(l0 * v0->r + l1 * v1->r + l2 * v2->r); g = (int)(l0 * v0->g + l1 * v1->g + l2 * v2->g); b = (int)(l0 * v0->b + l1 * v1->b + l2 * v2->b); a = (int)(l0 * v0->a + l1 * v1->a + l2 * v2->a); }
    else { r = v2->r; g = v2->g; b = v2->b; a = v2->a; }
    f = (int)(l0 * v0->f + l1 * v1->f + l2 * v2->f);
    float s = (float)(l0 * v0->s + l1 * v1->s + l2 * v2->s), t = (float)(l0 * v0->t + l1 * v1->t + l2 * v2->t), q = (float)(l0 * qs[0] + l1 * qs[1] + l2 * qs[2]);
    float uu = (float)(l0 * v0->u + l1 * v1->u + l2 * v2->u), vv = (float)(l0 * v0->v + l1 * v1->v + l2 * v2->v);
    pix(&d, px, py, z, clamp255(r), clamp255(g), clamp255(b), clamp255(a), clamp255(f), s, t, q, uu, vv);
  }
}
static int sprite_log;   /* GSREF_SPRITES=1: print every sprite (debug) */
static uint32_t skip_tex[8]; static int nskip_tex;   /* GSREF_SKIPTEX=<tbp>[,<tbp>...] (hex, dev): leave out the sprites that sample one of
                                                        those buffers while drawing another (the deck's motion trail reads the other display
                                                        buffer, tbp 0 / 1000), to compare two runs without that frame-to-frame path */
static void draw_sprite_(const Vtx *a, const Vtx *b)
{
  Draw d; setup(&d);
  if (d.tme) for (int i = 0; i < nskip_tex; i++) if (d.tbp[0] == skip_tex[i] && d.tbp[0] != d.fbp) return;
  note_prim(&d);
  if (ge_on) { const Vtx *vv[2] = { a, b }; ge_prim(&d, vv, 2); }
  if (zc_on) { const Vtx *vv[2] = { a, b }; zc_prim(&d, vv, 2); }
  if (sprite_log) printf("sprite call %d fb %x x %.1f..%.1f y %.1f..%.1f %s tex %x psm %x %dx%d uv %.1f,%.1f..%.1f,%.1f st %g,%g..%g,%g rgba %d %d %d %d | %s\n", g_call, d.fbp / 32,
    (a->x - d.ofx) / 16.0, (b->x - d.ofx) / 16.0, (a->y - d.ofy) / 16.0, (b->y - d.ofy) / 16.0, d.tme ? "tme" : "-", d.tbp[0], d.tpsm, 1 << d.tw, 1 << d.th,
    a->u / 16.0, a->v / 16.0, b->u / 16.0, b->v / 16.0, a->s, a->t, b->s, b->t, b->r, b->g, b->b, b->a, g_st);
  int64_t ax = a->x - d.ofx, ay = a->y - d.ofy, bx = b->x - d.ofx, by = b->y - d.ofy;
  int64_t lx = ax < bx ? ax : bx, hx = ax < bx ? bx : ax, ly = ay < by ? ay : by, hy = ay < by ? by : ay;
  int x0 = (int)((lx + 15) >> 4), x1 = (int)((hx - 1) >> 4), y0 = (int)((ly + 15) >> 4), y1 = (int)((hy - 1) >> 4);
  if (x0 < d.scx0) x0 = d.scx0; if (x1 > d.scx1) x1 = d.scx1; if (y0 < d.scy0) y0 = d.scy0; if (y1 > d.scy1) y1 = d.scy1;
  for (int py = y0; py <= y1; py++) for (int px = x0; px <= x1; px++) {
    double fx = bx != ax ? (px * 16.0 - ax) / (double)(bx - ax) : 0, fy = by != ay ? (py * 16.0 - ay) / (double)(by - ay) : 0;
    float s = (float)(a->s + (b->s - a->s) * fx), t = (float)(a->t + (b->t - a->t) * fy);
    float uu = (float)(a->u + (b->u - a->u) * fx), vv = (float)(a->v + (b->v - a->v) * fy);
    pix(&d, px, py, b->z, b->r, b->g, b->b, b->a, b->f, s, t, b->q, uu, vv);
  }
}
static void draw_line_(const Vtx *a, const Vtx *b)
{
  Draw d; setup(&d); note_prim(&d);
  if (ge_on) { const Vtx *vv[2] = { a, b }; ge_prim(&d, vv, 2); }
  if (zc_on) { const Vtx *vv[2] = { a, b }; zc_prim(&d, vv, 2); }
  double ax = (a->x - d.ofx) / 16.0, ay = (a->y - d.ofy) / 16.0, bx = (b->x - d.ofx) / 16.0, by = (b->y - d.ofy) / 16.0;
  int n = (int)ceil(fmax(fabs(bx - ax), fabs(by - ay))); if (n <= 0) n = 1; if (n > 4096) { count(1, "!line huge"); return; }
  for (int i = 0; i < n; i++) {   /* the last pixel is left out */
    double f = (double)i / n; int px = (int)floor(ax + (bx - ax) * f + 0.5), py = (int)floor(ay + (by - ay) * f + 0.5);
    int r = d.iip ? (int)(a->r + (b->r - a->r) * f) : b->r, g = d.iip ? (int)(a->g + (b->g - a->g) * f) : b->g, bb = d.iip ? (int)(a->b + (b->b - a->b) * f) : b->b, al = d.iip ? (int)(a->a + (b->a - a->a) * f) : b->a;
    pix(&d, px, py, a->z + ((double)b->z - a->z) * f, r, g, bb, al, (int)(a->f + (b->f - a->f) * f), (float)(a->s + (b->s - a->s) * f), (float)(a->t + (b->t - a->t) * f),
        (float)(a->q + (b->q - a->q) * f), (float)(a->u + (b->u - a->u) * f), (float)(a->v + (b->v - a->v) * f));
  }
}
/* GSREF_ZMAP=<file>: the GE renderer's depth mapping, per depth buffer and frame: lines "<vb> <zbp hex> <lo> <hi>" (the Z24 range that
   depth-tested draws write, from GSREF_ZCENSUS "zcensus target" lines).  Each vertex gets GE depth trunc(clamp((Z24 - lo') x k, 0, 65535))
   with lo' = lo - m, hi' = hi + m, m = (hi - lo) / 16, k = 65535 / (hi' - lo'); pixels interpolate the vertex values and truncate (as
   PPSSPP's software GE).  The depth buffer then holds GE depth x 256.  Buffers or frames not in the file keep Z24 / 256. */
static int zm_n; static uint32_t zm_t[4096][4];
static void zm_load(const char *fn)
{
  FILE *f = fopen(fn, "r"); if (!f) { perror(fn); exit(1); }
  unsigned vb, zbp, lo, hi; while (zm_n < 4096 && fscanf(f, "%u %x %u %u", &vb, &zbp, &lo, &hi) == 4) { zm_t[zm_n][0] = vb; zm_t[zm_n][1] = zbp; zm_t[zm_n][2] = lo; zm_t[zm_n][3] = hi; zm_n++; }
  fclose(f); zq_sh = 8; zq_max = 0xffffffffu;
}
static Vtx zm_map(const Draw *d, const Vtx *v)
{
  Vtx o = *v; double lo = 0, k = 1.0 / 256;
  for (int i = 0; i < zm_n; i++) if (zm_t[i][0] == (uint32_t)g_vb && zm_t[i][1] == d->zbp) {
    double m = (zm_t[i][3] - (double)zm_t[i][2]) / 16; lo = zm_t[i][2] - m; double hi = zm_t[i][3] + m; if (lo < 0) lo = 0; k = 65535.0 / (hi - lo); break; }
  double z = (v->z > 0xffffff && d->zpsm != Z32 ? 0xffffff : v->z) - lo; z *= k; if (z < 0) z = 0; if (z > 65535) z = 65535;
  o.z = (uint32_t)z << 8; return o;
}
static void draw_tri(const Vtx *v0, const Vtx *v1, const Vtx *v2)
{
  if (!zm_n) { draw_tri_(v0, v1, v2); return; }
  Draw d; setup(&d); Vtx a = zm_map(&d, v0), b = zm_map(&d, v1), c = zm_map(&d, v2); draw_tri_(&a, &b, &c);
}
static void draw_sprite(const Vtx *a, const Vtx *b)
{
  if (!zm_n) { draw_sprite_(a, b); return; }
  Draw d; setup(&d); Vtx p = zm_map(&d, a), q = zm_map(&d, b); draw_sprite_(&p, &q);
}
static void draw_line(const Vtx *a, const Vtx *b)
{
  if (!zm_n) { draw_line_(a, b); return; }
  Draw d; setup(&d); Vtx p = zm_map(&d, a), q = zm_map(&d, b); draw_line_(&p, &q);
}
static void draw_point(const Vtx *a)
{
  Draw d; setup(&d); note_prim(&d);
  if (ge_on) { const Vtx *vv[1] = { a }; ge_prim(&d, vv, 1); }
  pix(&d, (a->x - d.ofx + 8) >> 4, (a->y - d.ofy + 8) >> 4, a->z, a->r, a->g, a->b, a->a, a->f, a->s, a->t, a->q, a->u, a->v);
}
static void prim_end(void)
{
  if (!g_st[0]) { memset(g_prej, 0, sizeof g_prej); return; }
  if (ge_on && G.cur >= 0) G.b[G.cur].px += g_dpx;
  if (dc_on) dc_px += g_dpx;
  count(g_dpx, "px %s", g_st); count(g_dpx, "entry %06x px", g_entry);
  static const char *RN[4] = { "scissor", "atest", "datest", "depth" };
  for (int i = 0; i < 4; i++) if (g_prej[i]) { count(g_prej[i], "entry %06x rejected %s", g_entry, RN[i]); g_prej[i] = 0; }
  if (g_afail) { count(1, "afail-but-written prims %s", g_st); count(g_afail, "afail-but-written px"); g_afail = 0; }
}
static int vdump;   /* GSREF_VDUMP=n: print the first n vertices of textured triangles (debug) */
static int xl_first; static void xl_first_kick(void);   /* the translator check (GSREF_XLATE=1, below): a call's first kicked vertex */
static int xl_in, sb_on, sb_fused; static uint64_t sb_h;   /* sb_fused: hash F only when the primitive is fogged (the shown-frames check) */   /* ... and the hash of what a call sends to the GS; sb_on: a trial run that draws nothing */
static long xlp_vb = -1; static int xlp_call = -1;   /* GSREF_XLPRIM=<vb>,<call>: print that call's drawn primitives in the trial and the real run */
static void sb_hash(const void *p, int n) { for (int k = 0; k < n; k++) sb_h = (sb_h ^ ((const uint8_t *)p)[k]) * 0x100000001b3ull;
  if (g_vb == xlp_vb && g_call == xlp_call) { printf("xlprim %s hash %d bytes:", sb_on ? "trial" : "real ", n); for (int k = 0; k < n; k++) printf(" %02x", ((const uint8_t *)p)[k]); printf("\n"); } }
/* the hash of a drawn primitive: its PRIM and the vertex fields it uses (colour of the last vertex only when flat; ST and Q or UV only
   when textured) */
/* the GS registers a primitive reads: its context's and the shared ones (the PRIM attributes are hashed apart) */
static const uint8_t SB_CTX[10] = { 0x06, 0x08, 0x14, 0x18, 0x40, 0x42, 0x47, 0x4a, 0x4c, 0x4e }, SB_SH[7] = { 0x1a, 0x1c, 0x3b, 0x3d, 0x45, 0x46, 0x49 };
static void sb_state(int ctx) { for (int k = 0; k < 10; k++) sb_hash(&gr[SB_CTX[k] + ctx], 8); for (int k = 0; k < 7; k++) sb_hash(&gr[SB_SH[k]], 8); }
static void sb_prim(int n, const Vtx *const *vv)
{
  if (g_vb == xlp_vb && g_call == xlp_call) { printf("xlprim %s prim %d:", sb_on ? "trial" : "real ", (int)(gr[0] & 7));
    for (int i = 0; i < n; i++) printf(" (%.2f %.2f z %08x f %d rgba %d %d %d %d st %g %g q %g)", vv[i]->x / 16.0, vv[i]->y / 16.0, vv[i]->z, vv[i]->f, vv[i]->r, vv[i]->g, vv[i]->b, vv[i]->a, vv[i]->s, vv[i]->t, vv[i]->q); printf("\n"); }
  uint64_t at = (gr[0x1a] & 1) ? gr[0x00] : gr[0x1b]; int iip = at >> 3 & 1, tme = at >> 4 & 1, fst = at >> 8 & 1;
  uint32_t w[2] = { (uint32_t)(gr[0x00] & 7), (uint32_t)(at & 0x7f8) }; sb_hash(w, 8);
  int fge = at >> 5 & 1;
  if (!sb_fused) sb_state((int)(at >> 9 & 1));
  else {   /* the shown-frames check: only the registers this primitive reads (texture ones when textured, ALPHA and PABE when blended or
              antialiased, FOGCOL when fogged) */
    int ctx = (int)(at >> 9 & 1), abe = (int)(at >> 6 & 3) != 0;
    for (int k = 0; k < 10; k++) { int r = SB_CTX[k]; if ((r == 0x06 || r == 0x08 || r == 0x14) && !tme) continue; if (r == 0x42 && !abe) continue; sb_hash(&gr[r + ctx], 8); }
    for (int k = 0; k < 7; k++) { int r = SB_SH[k]; if ((r == 0x1c || r == 0x3b) && !tme) continue; if (r == 0x3d && !fge) continue; if (r == 0x49 && !abe) continue; sb_hash(&gr[r], 8); } }
  for (int i = 0; i < n; i++) { const Vtx *c = vv[i]; int32_t e[12] = { c->x, c->y, (int32_t)c->z, (sb_fused && !fge) ? 0 : c->f }; int k = 4;
    if (iip || i == n - 1) { e[k++] = c->r; e[k++] = c->g; e[k++] = c->b; e[k++] = c->a; }
    if (tme && fst) { e[k++] = c->u; e[k++] = c->v; }
    else if (tme) { memcpy(&e[k++], &c->s, 4); memcpy(&e[k++], &c->t, 4); memcpy(&e[k++], &c->q, 4); }
    sb_hash(e, 4 * k); }
}
/* the shown-frames check (GSREF_SKIP, GSREF_SKIPCHK; renderer_notes 11.12, task #47).  The capture's vblank records say whether the
   vblank that ended had a full or a reduced call of the draw pipeline (runtime/ee_io.h), and a vblank's transfers carry the packets of
   the call before it (11.2).  GSREF_SKIP=1 leaves out the transfers of the vblanks whose packets came from a reduced call only, as a
   renderer that translates shown frames only would.  GSREF_SKIPCHK=1: a line per vblank ("skc vb"): its kind, the VU1 calls, the
   primitives drawn and a hash of them (sb_prim with sb_fused: PRIM, the registers each reads (the texture ones only when textured, ALPHA
   and PABE when blended or antialiased, FOGCOL and the vertex F when fogged), the vertex fields it uses), the textured primitives and a
   hash of what each can read (the blocks of its texture's clamp region in the bits its format reads (byte 3's low nibble for T4HL, its
   high nibble for T4HH), rehashed when written, and the CLUT entries of its CSA), apart for those that read a block a draw wrote
   (render targets: on the PSP the GE's own memory), the transfers that started inside a call and the records left out; GSREF_SKIPCHK=2
   adds a line per VU1 call ("skc call"), =3 also each textured primitive outside the calls ("skt": texture, region, render-target read,
   hash, its blocks' and CLUT's parts), =4 also each primitive outside the calls with its registers and vertices ("skr"; GSREF_SKCVB=<vb>:
   that vblank only).  GSREF_SKCW=<lo>,<hi> (hex blocks): who writes byte 3 of those blocks ("skw": vblank, call, draw or transfer,
   format).  A run with GSREF_SKIP=1 and one without compare on the shown vblanks (tools/dev/skc_cmp.py). */
#define SKC_H0 0xcbf29ce484222325ull
static uint64_t skc_hp = SKC_H0, skc_ht = SKC_H0, skc_hr = SKC_H0, skc_chp, skc_cht; static long skc_np, skc_tp, skc_rtp, skc_mip, skc_xin, skc_skipped;
static int skip_on, skc_kind = -1, skc_kind_end = -1, skc_incall; static long skc_dvb = -1;   /* skc_incall: inside vu1_run (GSREF_SKIPCHK=3 prints the textured primitives outside calls) */
static uint64_t skc_mix(uint64_t h, uint64_t v) { h ^= v; h *= 0x100000001b3ull; return h ^ h >> 29; }
static uint64_t blk_hash(uint32_t bn, int lanes)   /* the bytes of block bn in those lanes (1: bytes 0-2 of each word, 2: byte 3, 3: all,
                                                      4: byte 3's low nibble (T4HL), 5: its high nibble (T4HH)) */
{
  if (blk_dirty[bn]) { memset(blk_h[bn], 0, sizeof blk_h[bn]); blk_dirty[bn] = 0; }
  uint64_t *p = &blk_h[bn][lanes - 1];
  if (!*p) { const uint8_t *b = gsm + (bn << 8); uint64_t h = SKC_H0; int m = lanes == 4 ? 0x0f : lanes == 5 ? 0xf0 : 0xff;
    for (int i = 0; i < 256; i++) if (lanes == 3 || ((i & 3) == 3) == (lanes != 1)) { h ^= b[i] & m; h *= 0x100000001b3ull; }
    *p = h | 1; }
  return *p;
}
static void skc_tex(const Draw *d)
{
  int psm = d->tpsm, lanes = psm_lanes(psm), x0 = 0, x1 = (1 << d->tw) - 1, y0 = 0, y1 = (1 << d->th) - 1, bx = 8, by = 8, rt = 0;
  if (d->wms == 2) { x0 = d->minu; x1 = d->maxu; }
  if (d->wmt == 2) { y0 = d->minv; y1 = d->maxv; }
  if (x1 > 2047) x1 = 2047;
  if (y1 > 2047) y1 = 2047;
  if (psm == CT16 || psm == CT16S) bx = 16; else if (psm == T8) bx = by = 16; else if (psm == T4) { bx = 32; by = 16; }
  uint64_t h = skc_mix(SKC_H0, (uint64_t)psm << 48 | (uint64_t)x0 << 36 | (uint64_t)x1 << 24 | (uint64_t)y0 << 12 | (uint64_t)y1);
  for (int y = y0 & ~(by - 1); y <= y1; y += by) for (int x = x0 & ~(bx - 1); x <= x1; x += bx) {
    uint32_t bn = gs_baddr(psm, d->tbp[0], d->tbw[0], x, y) >> 8 & 0x3fff;
    if (blk_drawn[bn] & lanes) rt = 1;
    h = skc_mix(h, blk_hash(bn, psm == T4HL ? 4 : psm == T4HH ? 5 : lanes)); }
  uint64_t hb = h, hc = SKC_H0;   /* the blocks' part and the CLUT's (GSREF_SKIPCHK=3 prints them) */
  if (is_idx(psm)) for (int i = 0, n = (psm == T8 || psm == T8H) ? 256 : 16; i < n; i++) { uint32_t c = clut_rd(d->cpsm, d->csa, (uint32_t)i); h = skc_mix(h, c); hc = skc_mix(hc, c); }
  if (d->mxl) skc_mip++;
  if (rt) { skc_hr = skc_mix(skc_hr, h); skc_rtp++; } else { skc_ht = skc_mix(skc_ht, h); skc_tp++; }
  skc_cht = skc_mix(skc_cht, h ^ (uint64_t)rt);
  if (skc_on > 2 && !skc_incall) printf("skt vb %ld after call %d prim %ld psm %x tbp %x tbw %d region %d..%d %d..%d csa %d rt %d h %016llx blocks %016llx clut %016llx | %s\n", g_vb, g_call,
                                        skc_np, psm, d->tbp[0], d->tbw[0], x0, x1, y0, y1, d->csa, rt, (unsigned long long)h, (unsigned long long)hb,
                                        (unsigned long long)hc, g_st);
}
static void skc_prim(int n, const Vtx *const *vv)
{
  uint64_t s = sb_h; sb_h = SKC_H0; sb_fused = 1; sb_prim(n, vv); sb_fused = 0; skc_hp = skc_mix(skc_hp, sb_h); skc_chp = skc_mix(skc_chp, sb_h); uint64_t ph = sb_h; sb_h = s; skc_np++;
  if (skc_on > 3 && !skc_incall && (skc_dvb < 0 || g_vb == skc_dvb)) {   /* GSREF_SKIPCHK=4 (GSREF_SKCVB=<vb>: that vblank only): each primitive
                                                                             outside the calls, its registers as sb_prim hashes them */
    uint64_t at = (gr[0x1a] & 1) ? gr[0x00] : gr[0x1b]; int ctx = (int)(at >> 9 & 1);
    printf("skr vb %ld after call %d prim %ld p %d at %03x", g_vb, g_call, skc_np, (int)(gr[0] & 7), (unsigned)(at & 0x7f8));
    for (int k = 0; k < 10; k++) printf(" %02x:%016llx", SB_CTX[k] + ctx, (unsigned long long)gr[SB_CTX[k] + ctx]);
    for (int k = 0; k < 7; k++) printf(" %02x:%016llx", SB_SH[k], (unsigned long long)gr[SB_SH[k]]);
    for (int i = 0; i < n; i++) printf(" v(%d %d z %08x f %d rgba %d %d %d %d st %g %g q %g uv %d %d)", vv[i]->x, vv[i]->y, vv[i]->z, vv[i]->f, vv[i]->r, vv[i]->g,
                                       vv[i]->b, vv[i]->a, vv[i]->s, vv[i]->t, vv[i]->q, vv[i]->u, vv[i]->v);
    printf(" h %016llx\n", (unsigned long long)ph); }
  Draw d; setup(&d); if (d.tme) skc_tex(&d);
}
/* ================= front end check (GSREF_XLDIRECT=1 with GSREF_XLATE=1): ge/gsfront.c fed by the translator (renderer_notes 11.10) =====
   The translator feeds the GS front end every GIF path it sees (PATH2 and PATH3 as they come, PATH1 only a call's A+D packet); each
   primitive gsref draws from PATH2 or PATH3 is compared with the front end's, made from the same quadwords (the translator takes a
   DIRECT block word by word, gsref at its end, so the front end's primitives wait in order): the path, the state (GfDraw, field by
   field) and the vertex fields the primitive uses (x, y, z, colour; fog when fogged; S, T, Q or UV when textured).  The front end's
   uncertain primitives (an input written by a VU1 call it does not run: gf_unknown) are counted apart.  With GSREF_GEOUT the export's
   batches of those primitives are made from the front end's data, so the export's md5 shows whether anything it takes differs. */
static int xd_on, g_gpath; static XdRec *xd_q; static long xd_qn, xd_qr, xd_qcap;
static long xd_n[4], xd_unc[4], xd_bad[4], xd_badunc[4], xd_miss[4], xd_extra[4], xd_path1, xd_shown, xd_df[64], xd_vf[13];
#define XD_F X(prim) X(iip) X(tme) X(fge) X(abe) X(aa1) X(fst) X(ctxt) X(ofx) X(ofy) X(scx0) X(scx1) X(scy0) X(scy1) X(fbp) X(fbw) X(fbmsk) \
  X(fpsm) X(zbp) X(zpsm) X(zmsk) X(ate) X(atst) X(aref) X(afail) X(date) X(datm) X(zte) X(ztst) X(ba) X(bb) X(bc) X(bd) X(fix) X(pabe) X(fba) \
  X(colclamp) X(fcr) X(fcg) X(fcb) X(tpsm) X(tw) X(th) X(tcc) X(tfx) X(cpsm) X(csa) X(wms) X(wmt) X(minu) X(maxu) X(minv) X(maxv) X(lcm) \
  X(mxl) X(mmag) X(mmin) X(l)
#define X(f) #f,
static const char *XD_FN[64] = { XD_F "k", "tbp", "tbw" };
#undef X
static const char *XD_VN[13] = { "x", "y", "z", "f", "r", "g", "b", "a", "s", "t", "q", "u", "v" };
/* the state fields that differ (bit i: XD_FN[i]) */
static uint64_t xd_ddiff(const GfDraw *a, const GfDraw *b)
{
  uint64_t m = 0; int i = 0;
#define X(f) if (a->f != b->f) m |= 1ull << i; i++;
  XD_F
#undef X
  if (memcmp(&a->k, &b->k, 4)) m |= 1ull << i; i++;
  if (memcmp(a->tbp, b->tbp, sizeof a->tbp)) m |= 1ull << i; i++;
  if (memcmp(a->tbw, b->tbw, sizeof a->tbw)) m |= 1ull << i;
  return m;
}
/* the vertex fields that differ among those the primitive uses (bit i: XD_VN[i]) */
static int xd_vdiff(const GfDraw *d, const GfVtx *a, const GfVtx *b)
{
  int m = 0;
  if (a->x != b->x) m |= 1; if (a->y != b->y) m |= 2; if (a->z != b->z) m |= 4;
  if (d->fge && a->f != b->f) m |= 8;
  if (a->r != b->r) m |= 16; if (a->g != b->g) m |= 32; if (a->b != b->b) m |= 64; if (a->a != b->a) m |= 128;
  if (d->tme && !d->fst) { if (memcmp(&a->s, &b->s, 4)) m |= 256; if (memcmp(&a->t, &b->t, 4)) m |= 512; if (memcmp(&a->q, &b->q, 4)) m |= 1024; }
  if (d->tme && d->fst) { if (a->u != b->u) m |= 2048; if (a->v != b->v) m |= 4096; }
  return m;
}
static int xdg_on;   /* GSREF_XLDIG: ge/xldig.c's digest lines */
static void xd_cb(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  (void)ctx; if (xdg_on) xdg_prim(g, d, v, n);
  if (g->path != 2 && g->path != 3) { xd_path1++; return; }
  if (xd_qn == xd_qcap) { xd_qcap = xd_qcap ? 2 * xd_qcap : 4096; xd_q = realloc(xd_q, xd_qcap * sizeof *xd_q); }
  XdRec *r = &xd_q[xd_qn++]; r->d = *d; r->n = n; r->path = g->path; r->unk = g->unk_used;
  for (int i = 0; i < n; i++) { r->v[i] = *v[i]; r->vp[i] = &r->v[i]; }
}
/* the front end's primitives gsref did not draw (at a frame's end); the queue restarts */
static void xd_flush(void)
{
  for (long i = xd_qr; i < xd_qn; i++) { xd_extra[xd_q[i].path]++;
    if (xd_shown < 12) { xd_shown++; printf("xldirect vb %ld: the front end drew a path %d primitive %d gsref did not\n", g_vb, xd_q[i].path, xd_q[i].d.prim); } }
  xd_qn = xd_qr = 0;
}
static void xd_prim(int n, const Vtx *const *v)
{
  int p = g_gpath;
  if (sb_on || (p != 2 && p != 3)) return;
  if (n == 3 && (int64_t)(v[1]->x - v[0]->x) * (v[2]->y - v[0]->y) == (int64_t)(v[1]->y - v[0]->y) * (v[2]->x - v[0]->x)) return;   /* zero area: drawn by neither */
  if (xd_qr == xd_qn) { xd_miss[p]++; if (xd_shown < 12) { xd_shown++; printf("xldirect vb %ld: gsref drew a path %d primitive %d the front end did not\n", g_vb, p, (int)(gr[0] & 7)); } return; }
  const XdRec *r = &xd_q[xd_qr++];
  Draw d; setup(&d);
  uint64_t dm = xd_ddiff(&r->d, &d); int vm = 0;
  if (r->n == n) for (int i = 0; i < n; i++) vm |= xd_vdiff(&d, &r->v[i], v[i]);
  int bad = r->path != p || r->n != n || dm || vm, unc = r->unk != 0;
  xd_n[p]++; if (unc) xd_unc[p]++;
  if (bad) { if (unc) xd_badunc[p]++; else xd_bad[p]++;
    for (int i = 0; i < 64; i++) if (dm >> i & 1) xd_df[i]++;
    for (int i = 0; i < 13; i++) if (vm >> i & 1) xd_vf[i]++;
    if (xd_shown < 12) { xd_shown++; printf("xldirect vb %ld: path %d (front end %d), %d vertices (front end %d)%s, state", g_vb, p, r->path, n, r->n, unc ? ", uncertain" : "");
      for (int i = 0; i < 64; i++) if (dm >> i & 1) printf(" %s", XD_FN[i]);
      printf(", vertex"); for (int i = 0; i < 13; i++) if (vm >> i & 1) printf(" %s", XD_VN[i]); printf("\n"); } }
  if (!zm_n) xd_use = r;   /* the export takes the front end's data (not with GSREF_ZMAP, which maps gsref's depth first) */
}
static void xd_report(void)
{
  xd_flush();
  for (int p = 2; p <= 3; p++)
    fprintf(stderr, "xldirect path %d: %ld primitives compared, %ld uncertain; differ: %ld certain, %ld uncertain; gsref only %ld, front end only %ld\n",
            p, xd_n[p], xd_unc[p], xd_bad[p], xd_badunc[p], xd_miss[p], xd_extra[p]);
  fprintf(stderr, "xldirect: front end primitives on path 1 (A+D packets) %ld; fields that differ:", xd_path1);
  for (int i = 0; i < 64; i++) if (xd_df[i]) fprintf(stderr, " %s:%ld", XD_FN[i], xd_df[i]);
  for (int i = 0; i < 13; i++) if (xd_vf[i]) fprintf(stderr, " v.%s:%ld", XD_VN[i], xd_vf[i]);
  const GfGs *F = xl_front(); fprintf(stderr, "\nxldirect: front end totals: primitives by PRIM %ld %ld %ld %ld %ld %ld %ld, zero-area %ld, PRIM 7 %ld, uncertain %ld\n",
          F->prims[0], F->prims[1], F->prims[2], F->prims[3], F->prims[4], F->prims[5], F->prims[6], F->zero_area, F->prim7, F->uncertain);
}
#define SB_DRAW(n, a, b, c, CALL) do { if (xl_in || sb_on) { const Vtx *vv_[3] = { a, b, c }; sb_prim(n, vv_); } \
  if (skc_on && !sb_on) { const Vtx *vs_[3] = { a, b, c }; skc_prim(n, vs_); } \
  if (xd_on) { const Vtx *vx_[3] = { a, b, c }; xd_prim(n, vx_); } if (!sb_on) { CALL; prim_end(); } xd_use = 0; } while (0)
static void vkick(int draw)
{
  if (xl_first) xl_first_kick();
  int prim = gr[0x00] & 7; Vtx v = cur;
  if (vdump > 0 && prim >= 3 && prim <= 5 && ((gr[0x1a] & 1 ? gr[0] : gr[0x1b]) >> 4 & 1)) { vdump--;
    printf("v call %d prim %d kick %d x %.2f y %.2f z %08x f %d rgba %d %d %d %d s %g t %g q %g\n", g_call, prim, draw, (v.x - (int)(gr[0x18] & 0xffff)) / 16.0, (v.y - (int)(gr[0x18] >> 32 & 0xffff)) / 16.0, v.z, v.f, v.r, v.g, v.b, v.a, v.s, v.t, v.q); }
  switch (prim) {
    case 0: if (draw) SB_DRAW(1, &v, 0, 0, draw_point(&v)); break;
    case 1: vq[vqn++] = v; if (vqn == 2) { if (draw) SB_DRAW(2, &vq[0], &vq[1], 0, draw_line(&vq[0], &vq[1])); vqn = 0; } break;
    case 2: if (vqn) { if (draw) SB_DRAW(2, &vq[0], &v, 0, draw_line(&vq[0], &v)); } vq[0] = v; vqn = 1; break;
    case 3: vq[vqn++] = v; if (vqn == 3) { if (draw) SB_DRAW(3, &vq[0], &vq[1], &vq[2], draw_tri(&vq[0], &vq[1], &vq[2])); vqn = 0; } break;
    case 4: if (vqn == 2) { if (draw) SB_DRAW(3, &vq[0], &vq[1], &v, draw_tri(&vq[0], &vq[1], &v)); vq[0] = vq[1]; vq[1] = v; } else vq[vqn++] = v; break;
    case 5: if (vqn == 2) { if (draw) SB_DRAW(3, &vq[0], &vq[1], &v, draw_tri(&vq[0], &vq[1], &v)); vq[1] = v; } else vq[vqn++] = v; break;
    case 6: vq[vqn++] = v; if (vqn == 2) { if (draw) SB_DRAW(2, &vq[0], &vq[1], 0, draw_sprite(&vq[0], &vq[1])); vqn = 0; } break;
    default: count(1, "!prim 7");
  }
}
static float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static void gs_reg(int r, uint64_t v)
{
  if ((xl_in || sb_on) && r != 0x01 && r != 0x02 && r != 0x03 && r != 0x04 && r != 0x05 && r != 0x0a && r != 0x0c && r != 0x0d) {
    /* the hash takes the state each drawn primitive reads (sb_prim) and the state the call leaves (xl_call_end), not the writes; a
       trial keeps the state in gr (saved and restored around it) without side effects (CLUT loads, transfers) */
    if (r >= 0x50 && r <= 0x54) { uint64_t w[2] = { (uint64_t)r, v }; sb_hash(w, 16); }
    if (g_vb == xlp_vb && g_call == xlp_call) printf("xlprim %s reg %02x %016llx\n", sb_on ? "trial" : "real ", r, (unsigned long long)v);
    if (sb_on) {
      if (r == 0x16 || r == 0x17) { const uint64_t M = 0xFFFFFFE003F00000ull; gr[r - 0x10] = (gr[r - 0x10] & ~M) | (v & M); }
      if (r == 0x00) vqn = 0;
      if (r < 0x50 || r > 0x54) gr[r] = v;
      return;
    } }
  count(1, "reg %02x", r);
  switch (r) {
    case 0x00: gr[0] = v; vqn = 0; return;
    case 0x01: cur.r = v & 255; cur.g = v >> 8 & 255; cur.b = v >> 16 & 255; cur.a = v >> 24 & 255; cur.q = u2f(v >> 32); return;
    case 0x02: cur.s = u2f(v); cur.t = u2f(v >> 32); return;
    case 0x03: cur.u = v & 0x3fff; cur.v = v >> 16 & 0x3fff; return;
    case 0x04: case 0x0c: cur.x = v & 0xffff; cur.y = v >> 16 & 0xffff; cur.z = v >> 32 & 0xffffff; cur.f = v >> 56; vkick(r == 0x04); return;
    case 0x05: case 0x0d: cur.x = v & 0xffff; cur.y = v >> 16 & 0xffff; cur.z = v >> 32; vkick(r == 0x05); return;
    case 0x0a: cur.f = v >> 56; return;
    case 0x06: case 0x07: gr[r] = v; clut_load(v); return;
    case 0x16: case 0x17: { const uint64_t M = 0xFFFFFFE003F00000ull; gr[r - 0x10] = (gr[r - 0x10] & ~M) | (v & M); gr[r] = v; clut_load(gr[r - 0x10]); return; }
    case 0x53: gr[r] = v; xfer_start(v & 3); return;
    case 0x54: { uint8_t b[8]; memcpy(b, &v, 8); xfer_bytes(b, 8); return; }
  }
  gr[r] = v;
}

typedef struct { Vtx v; int adc, prim, tag; } DcRec;   /* df0 / fpos checks (below): the vertices the running call kicked; tag: its GIF tag (one per XGKICK) */
static DcRec dc_rec[2048]; static int dc_nr;
/* P06 dump (GSREF_P06DUMP=<file>; renderer_notes 11.13 step 4, task #63): each call of a P06 entry (+0, +358, +738, +ab0) as it starts
   (abs 0x00..0x0f, T+0 .. T+0x100 + 6 n) and what it kicked (each PATH1 GIF tag with the GS registers after it, each vertex), for
   tools/dev/p06_census.py.  Game data: personal use only, kept in the container. */
typedef struct { Vtx v; uint8_t adc, prim, tag, pad; } P6Rec;
static FILE *p6_f; static int p6_on, p6_tagn, p6_nr; static uint32_t p6_top; static uint8_t p6_vm[16384]; static P6Rec p6_rec[8192];
static const int P6R[12] = { 0x00, 0x06, 0x07, 0x08, 0x14, 0x42, 0x47, 0x3d, 0x1a, 0x1b, 0x18, 0x4e };   /* PRIM TEX0_1 TEX0_2 CLAMP_1 TEX1_1 ALPHA_1 TEST_1 FOGCOL PRMODECONT PRMODE XYOFFSET_1 ZBUF_1 */
static struct { uint64_t lo, hi, gr[12]; } p6_tag[64];
static int is_p06x(uint32_t e) { return e == 0x19fe08 || e == 0x1a0160 || e == 0x1a0540 || e == 0x1a08c0; }
/* ================= GIF (PATH1 from XGKICK, PATH2 from VIF1 DIRECT, PATH3 from DMA ch 2) ================= */
typedef struct { int nloop, flg, nreg, ri, eop, intag; uint64_t regs; } Gif;
static Gif gp[4]; static int g_qw_used;   /* gif_qw: 0 when the quadword only fed NOP registers */
/* one quadword; returns 1 when it ended a packet with EOP */
static int gif_qw(Gif *g, const uint32_t *q)
{
  uint64_t lo = q[0] | (uint64_t)q[1] << 32, hi = q[2] | (uint64_t)q[3] << 32;
  g_qw_used = 1; g_gpath = (int)(g - gp);
  if (!g->nloop) {
    if (dc_on && g == &gp[1]) dc_tagn++;
    g->nloop = lo & 0x7fff; g->eop = lo >> 15 & 1; g->flg = lo >> 58 & 3; g->nreg = (lo >> 60 & 15) ? (int)(lo >> 60 & 15) : 16; g->regs = hi; g->ri = 0;
    if ((lo >> 46 & 1) && g->flg == 0) gs_reg(0x00, lo >> 47 & 0x7ff);
    if (xc_p6 && g == &gp[1] && g->flg == 0 && (hi & 15) == 0xe) xc_p6ad++;   /* an A+D packet (P06's: NLOOP 1, NREG 7, all A+D) */
    if (p6_on && g == &gp[1]) { if (p6_tagn < 64) { p6_tag[p6_tagn].lo = lo; p6_tag[p6_tagn].hi = hi; for (int k = 0; k < 12; k++) p6_tag[p6_tagn].gr[k] = gr[P6R[k]]; } p6_tagn++; }
    count(1, "gif %s", (const char *[]){ "packed", "reglist", "image", "image" }[g->flg]);
    if (g->flg == 1) g->nloop *= g->nreg;   /* REGLIST: count 64-bit words */
    return !g->nloop && g->eop;
  }
  if (g->flg >= 2) {
    if (xf.on) { if (!xf.src0) xf.src0 = g_src; else if (g_src != xf.srcn) xf.jumps++; xf.srcn = g_src + 16;
      for (int k = 0; k < 16; k++) xf.hash = (xf.hash ^ ((const uint8_t *)q)[k]) * 0x100000001b3ull; }
    if (xl_in || sb_on) sb_hash(q, 16);
    if (!sb_on) xfer_bytes((const uint8_t *)q, 16);
    g->nloop--; return !g->nloop && g->eop; }
  if (g->flg == 1) {
    g_qw_used = 0;
    for (int h = 0; h < 2 && g->nloop; h++) { int r = g->regs >> (4 * g->ri) & 15; if (++g->ri == g->nreg) g->ri = 0; g->nloop--; if (r != 0xf) g_qw_used = 1;
      uint64_t v = h ? hi : lo; if (r == 0xe || r == 0xf) count(1, "!reglist reg %x", r); else gs_reg(r == 0x6 ? 0x06 : r, v); }
    return !g->nloop && g->eop;
  }
  int r = g->regs >> (4 * g->ri) & 15;
  switch (r) {
    case 0x0: gs_reg(0x00, lo & 0x7ff); break;
    case 0x1: cur.r = q[0] & 255; cur.g = q[1] & 255; cur.b = q[2] & 255; cur.a = q[3] & 255; cur.q = g_q; break;
    case 0x2: cur.s = u2f(q[0]); cur.t = u2f(q[1]); g_q = u2f(q[2]); break;
    case 0x3: cur.u = q[0] & 0x3fff; cur.v = q[1] & 0x3fff; break;
    case 0x4: cur.x = q[0] & 0xffff; cur.y = q[1] & 0xffff; cur.z = q[2] >> 4 & 0xffffff; cur.f = q[3] >> 4 & 0xff;
      if (dc_on && g == &gp[1] && dc_nr < 2048) { dc_rec[dc_nr].v = cur; dc_rec[dc_nr].adc = q[3] >> 15 & 1; dc_rec[dc_nr].prim = (int)(gr[0] & 7); dc_rec[dc_nr].tag = dc_tagn; dc_nr++; }
      if (p6_on && g == &gp[1] && p6_nr < 8192) { P6Rec *e = &p6_rec[p6_nr++]; e->v = cur; e->adc = q[3] >> 15 & 1; e->prim = (uint8_t)(gr[0] & 7); e->tag = (uint8_t)(p6_tagn - 1); e->pad = (uint8_t)r; }
      vkick(!(q[3] >> 15 & 1)); break;
    case 0x5: cur.x = q[0] & 0xffff; cur.y = q[1] & 0xffff; cur.z = q[2];
      if (p6_on && g == &gp[1] && p6_nr < 8192) { P6Rec *e = &p6_rec[p6_nr++]; e->v = cur; e->adc = q[3] >> 15 & 1; e->prim = (uint8_t)(gr[0] & 7); e->tag = (uint8_t)(p6_tagn - 1); e->pad = (uint8_t)r; }
      vkick(!(q[3] >> 15 & 1)); break;
    case 0xa: cur.f = q[3] >> 4 & 0xff; break;
    case 0xc: cur.x = q[0] & 0xffff; cur.y = q[1] & 0xffff; cur.z = q[2] >> 4 & 0xffffff; cur.f = q[3] >> 4 & 0xff; vkick(0); break;
    case 0xd: cur.x = q[0] & 0xffff; cur.y = q[1] & 0xffff; cur.z = q[2]; vkick(0); break;
    case 0xe: gs_reg(hi & 0xff, lo); break;
    case 0xf: g_qw_used = 0; break;
    default: gs_reg(r, lo);
  }
  if (++g->ri == g->nreg) { g->ri = 0; g->nloop--; }
  return !g->nloop && g->eop;
}

/* ================= VU1 (runtime/vu0.h's interpreter with VU1 sizes, XGKICK and the EFU) ================= */
typedef struct { uint32_t u[4]; } VQ;
static VQ VF[32]; static uint32_t VIr[16], VACC[4], VQr, VPr, VIreg, VRr = 0x3f800000u, VMAC, VSTAT, VCLIP, g_vmac;
static uint8_t VM[16384] __attribute__((aligned(16))); static uint32_t VP[4096]; static uint8_t vm_set[1024];
static int vu_tpc; static uint32_t vu_top, vu_itop; static long vu_steps; static int vu_trace_call = -1, vif_log;   /* GSREF_TRACE=<call>, GSREF_VIFLOG=1 (debug) */
static float VFL(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t VUI(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float vin(uint32_t u) { uint32_t e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; return VFL(u); }
static uint32_t vout(float f, int sh)
{
  uint32_t u = VUI(f), e = u & 0x7f800000, m = 0;
  if (!e) { m |= 1u << sh; if (u & 0x7fffff) m |= 0x100u << sh; u &= 0x80000000; }
  else if (e == 0x7f800000) { m |= 0x1000u << sh; u = (u & 0x80000000) | 0x7f7fffff; }
  if (u & 0x80000000) m |= 0x10u << sh;
  g_vmac |= m; return u;
}
static uint32_t vclamp(float f) { uint32_t u = VUI(f), e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; return u; }
static void vu_setflags(void)
{
  uint32_t z = (g_vmac & 0xf) != 0, s = (g_vmac & 0xf0) != 0, un = (g_vmac & 0xf00) != 0, o = (g_vmac & 0xf000) != 0, cur_ = z | s << 1 | un << 2 | o << 3;
  VMAC = g_vmac; VSTAT = (VSTAT & 0xfc0) | cur_ | (cur_ << 6) | (VSTAT & 0x30);
}
static void vwrite(int fd, int c, uint32_t u) { if (fd) VF[fd].u[c] = u; }
enum { O_ADD, O_SUB, O_MADD, O_MSUB, O_MAX, O_MINI, O_MUL, O_OPMSUB, O_OPMULA, O_ABS, O_ITOF, O_FTOI, O_CLIP, O_NOP };
static void vu_upper(uint32_t w)
{
  int op = w & 0x3f, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, sel = op & 3, idx = (w >> 6) & 31, fl = 1;
  int opn = O_NOP, src = -1, toacc = 0, tgt = fd, sh = 0;
  if (op < 0x1c) { opn = op >> 2; src = sel; }
  else if (op < 0x30) {
    static const signed char N[20] = { O_MUL, O_MAX, O_MUL, O_MINI, O_ADD, O_MADD, O_ADD, O_MADD, O_SUB, O_MSUB, O_SUB, O_MSUB, O_ADD, O_MADD, O_MUL, O_MAX, O_SUB, O_MSUB, O_OPMSUB, O_MINI };
    static const signed char S[20] = { 4, 5, 5, 5, 4, 4, 5, 5, 4, 4, 5, 5, -1, -1, -1, -1, -1, -1, -1, -1 };
    opn = N[op - 0x1c]; src = S[op - 0x1c];
  } else if (op >= 0x3c) {
    static const signed char N[4][12] = {
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_MUL, O_ADD, O_SUB, O_ADD, O_SUB },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_ABS, O_MADD, O_MSUB, O_MADD, O_MSUB },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_MUL, O_ADD, O_SUB, O_MUL, O_OPMULA },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_CLIP, O_MADD, O_MSUB, O_NOP, O_NOP } };
    static const signed char S[4][12] = {
      { 0, 0, 0, 0, -1, -1, 0, 4, 4, 4, -1, -1 }, { 1, 1, 1, 1, -1, -1, 1, -1, 4, 4, -1, -1 },
      { 2, 2, 2, 2, -1, -1, 2, 5, 5, 5, -1, -1 }, { 3, 3, 3, 3, -1, -1, 3, -1, 5, 5, -1, -1 } };
    if (idx >= 12) return;
    opn = N[sel][idx]; src = S[sel][idx]; toacc = 1;
    if (opn == O_ITOF || opn == O_FTOI || opn == O_ABS) { toacc = 0; tgt = ft; sh = (int[]){ 0, 4, 12, 15 }[sel]; }
    if (opn == O_OPMULA) src = -1;
  } else return;
  if (opn == O_NOP) return;
  if (opn == O_MAX || opn == O_MINI || opn == O_ABS || opn == O_ITOF || opn == O_FTOI) fl = 0;
  if (fl) g_vmac = 0;
  if (opn == O_CLIP) {
    float wv = fabsf(vin(VF[ft].u[3])), x = vin(VF[fs].u[0]), y = vin(VF[fs].u[1]), z = vin(VF[fs].u[2]); uint32_t bits = 0;
    if (x > wv) bits |= 1; if (x < -wv) bits |= 2; if (y > wv) bits |= 4; if (y < -wv) bits |= 8; if (z > wv) bits |= 16; if (z < -wv) bits |= 32;
    VCLIP = ((VCLIP << 6) | bits) & 0xffffff; return;
  }
  float A[4], B[4], Ac[4], Bv[4];
  for (int c = 0; c < 4; c++) { A[c] = vin(VF[fs].u[c]); B[c] = src >= 0 && src < 4 ? vin(VF[ft].u[src]) : src == 4 ? vin(VQr) : src == 5 ? vin(VIreg) : vin(VF[ft].u[c]); Ac[c] = vin(VACC[c]); Bv[c] = vin(VF[ft].u[c]); }
  uint32_t out[4]; int wr[4] = { 0, 0, 0, 0 };
  for (int c = 0; c < 4; c++) {
    if (!(dest & (8 >> c))) continue;
    float a = A[c], b = B[c], res = 0;
    switch (opn) {
      case O_ADD: res = a + b; break; case O_SUB: res = a - b; break;
      case O_MADD: res = Ac[c] + a * b; break; case O_MSUB: res = Ac[c] - a * b; break;
      case O_MAX: res = a > b ? a : b; break; case O_MINI: res = a < b ? a : b; break;
      case O_MUL: res = a * b; break;
      case O_OPMSUB: case O_OPMULA: { float p = c == 0 ? A[1] * Bv[2] : c == 1 ? A[2] * Bv[0] : A[0] * Bv[1]; res = opn == O_OPMSUB ? Ac[c] - p : p; break; }
      case O_ABS: res = fabsf(a); break;
      case O_ITOF: res = (float)(int32_t)VF[fs].u[c] / (float)(1 << sh); break;
      case O_FTOI: { float vv = a * (float)(1 << sh); out[c] = vv >= 2147483648.0f ? 0x7fffffff : vv <= -2147483648.0f ? 0x80000000u : (uint32_t)(int32_t)vv; wr[c] = 1; continue; }
    }
    out[c] = fl ? vout(res, 3 - c) : vclamp(res); wr[c] = 1;
  }
  for (int c = 0; c < 4; c++) if (wr[c]) { if (toacc || opn == O_OPMULA) VACC[c] = out[c]; else vwrite(tgt, c, out[c]); }
  if (fl) vu_setflags();
}
#define VI16(n) ((n) ? (VIr[n] & 0xffff) : 0)
static void vi_set(int n, uint32_t v) { if (n) VIr[n] = v & 0xffff; }
static long vm_uninit;
static uint8_t *vmem(uint32_t qa) { qa &= 0x3ff; return VM + (qa << 4); }
static int xl_kicking, g_vpc; static uint8_t xl_diff[1024], xl_wr[1024]; static void xl_badread(uint32_t qa, int kick);   /* the translator check (below) */
static uint8_t *vmem_rd(uint32_t qa) { qa &= 0x3ff; if (!vm_set[qa]) { vm_uninit++; vm_set[qa] = 2; } if (xl_in && !xl_kicking && xl_diff[qa] && !xl_wr[qa]) xl_badread(qa, 0); return VM + (qa << 4); }
static uint8_t *vmem_wr(uint32_t qa) { qa &= 0x3ff; vm_set[qa] = 1; if (xl_in) xl_wr[qa] = 1; return VM + (qa << 4); }
static int bk_reg = -1, bk_live; static uint32_t bk_old;   /* the branch-after-integer-op rule */
static void gif_kick(uint32_t qa);
static float efu_res; static int efu_lat;
/* a relative branch's target: the micro memory's 2,048 instructions wrap (PCSX2 masks the branch PC the same way).  Until 2026-10-27 a
   target below 0 was taken as no branch: P06's header.w = 0 test at +0 / +0x358 (to the no-clip loops at 0x4d7 / 0x52d) never jumped */
static int vu_br(int pc, int imm11)
{
  int t = pc + 1 + imm11;
  if (t < 0 || t > 2047) { count(1, "vu1 branch wraps (entry %06x)", g_entry); t &= 2047; }
  return t;
}
static int vu_lower(uint32_t w, int pc)
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, imm11 = w & 0x7ff;
  if (imm11 & 0x400) imm11 -= 0x800;
  int is = fs, it = ft, id = fd;
#define BVI(n) (bk_live && (n) == bk_reg ? bk_old : VI16(n))
  switch (op) {
    case 0x00: { uint32_t *m = (uint32_t *)vmem_rd(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); return -1; }
    case 0x01: { uint32_t *m = (uint32_t *)vmem_wr(VI16(it) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; return -1; }
    case 0x04: { uint32_t *m = (uint32_t *)vmem_rd(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { vi_set(it, m[c]); break; } return -1; }
    case 0x05: { uint32_t *m = (uint32_t *)vmem_wr(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VI16(it); return -1; }
    case 0x08: vi_set(it, VI16(is) + (((w >> 21) & 15) << 11 | (w & 0x7ff))); return -1;
    case 0x09: vi_set(it, VI16(is) - (((w >> 21) & 15) << 11 | (w & 0x7ff))); return -1;
    case 0x10: vi_set(1, (VCLIP & 0xffffff) == (w & 0xffffff)); return -1;
    case 0x11: VCLIP = w & 0xffffff; return -1;
    case 0x12: vi_set(1, (VCLIP & 0xffffff & (w & 0xffffff)) != 0); return -1;
    case 0x13: vi_set(1, ((VCLIP | w) & 0xffffff) == 0xffffff); return -1;
    case 0x14: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, (VSTAT & 0xfff) == imm); return -1; }
    case 0x15: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); VSTAT = (VSTAT & 0x3f) | (imm & 0xfc0); return -1; }
    case 0x16: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, VSTAT & imm & 0xfff); return -1; }
    case 0x17: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, (VSTAT | imm) & 0xfff); return -1; }
    case 0x18: vi_set(it, (VMAC & 0xffff) == VI16(is)); return -1;
    case 0x1a: vi_set(it, VMAC & VI16(is)); return -1;
    case 0x1b: vi_set(it, VMAC | VI16(is)); return -1;
    case 0x1c: vi_set(it, VCLIP & 0xfff); return -1;
    case 0x20: return vu_br(pc, imm11);
    case 0x21: vi_set(it, pc + 2); return vu_br(pc, imm11);
    case 0x24: return VI16(is);
    case 0x25: { uint32_t t = VI16(is); vi_set(it, pc + 2); return t; }
    case 0x28: return BVI(it) == BVI(is) ? vu_br(pc, imm11) : -1;
    case 0x29: return BVI(it) != BVI(is) ? vu_br(pc, imm11) : -1;
    case 0x2c: return (int16_t)BVI(is) < 0 ? vu_br(pc, imm11) : -1;
    case 0x2d: return (int16_t)BVI(is) > 0 ? vu_br(pc, imm11) : -1;
    case 0x2e: return (int16_t)BVI(is) <= 0 ? vu_br(pc, imm11) : -1;
    case 0x2f: return (int16_t)BVI(is) >= 0 ? vu_br(pc, imm11) : -1;
  }
  if (op >= 0x40) {
    int fn = w & 0x3f, i5 = (w >> 6) & 31; if (i5 & 16) i5 -= 32;
    switch (fn) {
      case 0x30: vi_set(id, VI16(is) + VI16(it)); return -1;
      case 0x31: vi_set(id, VI16(is) - VI16(it)); return -1;
      case 0x32: vi_set(it, VI16(is) + i5); return -1;
      case 0x34: vi_set(id, VI16(is) & VI16(it)); return -1;
      case 0x35: vi_set(id, VI16(is) | VI16(it)); return -1;
    }
    int idx = (w >> 6) & 31, sel = fn & 3, fsf = (w >> 21) & 3, ftf = (w >> 23) & 3;
    if (fn >= 0x3c) {
      if (sel == 0 && idx == 12) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VF[fs].u[c]); return -1; }
      if (sel == 1 && idx == 12) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VF[fs].u[(c + 1) & 3]); return -1; }
      if (sel == 0 && idx == 13) { uint32_t *m = (uint32_t *)vmem_rd(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); vi_set(is, VI16(is) + 1); return -1; }
      if (sel == 1 && idx == 13) { uint32_t *m = (uint32_t *)vmem_wr(VI16(it)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; vi_set(it, VI16(it) + 1); return -1; }
      if (sel == 2 && idx == 13) { vi_set(is, VI16(is) - 1); uint32_t *m = (uint32_t *)vmem_rd(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); return -1; }
      if (sel == 3 && idx == 13) { vi_set(it, VI16(it) - 1); uint32_t *m = (uint32_t *)vmem_wr(VI16(it)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; return -1; }
      if (sel == 0 && idx == 14) {
        float a = vin(VF[fs].u[fsf]), b = vin(VF[ft].u[ftf]); VSTAT &= ~0x30u;
        if (b == 0.0f) { if (a == 0.0f) VSTAT |= 0x10 | 0x400; else VSTAT |= 0x20 | 0x800; VQr = ((VF[fs].u[fsf] ^ VF[ft].u[ftf]) & 0x80000000) | 0x7f7fffff; }
        else VQr = vclamp(a / b);
        return -1; }
      if (sel == 1 && idx == 14) { float b = vin(VF[ft].u[ftf]); VSTAT &= ~0x30u; if (b < 0) VSTAT |= 0x10 | 0x400; VQr = VUI(sqrtf(fabsf(b))); return -1; }
      if (sel == 2 && idx == 14) {
        float a = vin(VF[fs].u[fsf]), b = fabsf(vin(VF[ft].u[ftf])); VSTAT &= ~0x30u;
        if (vin(VF[ft].u[ftf]) < 0) VSTAT |= 0x10 | 0x400;
        if (b == 0.0f) { VSTAT |= 0x20 | 0x800; VQr = (VF[fs].u[fsf] & 0x80000000) | 0x7f7fffff; } else VQr = vclamp(a / sqrtf(b));
        return -1; }
      if (sel == 3 && idx == 14) return -1;
      if (sel == 0 && idx == 15) { vi_set(it, VF[fs].u[fsf]); return -1; }
      if (sel == 1 && idx == 15) { int32_t v = (int16_t)VI16(is); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, (uint32_t)v); return -1; }
      if (sel == 2 && idx == 15) { uint32_t *m = (uint32_t *)vmem_rd(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { vi_set(it, m[c]); break; } return -1; }
      if (sel == 3 && idx == 15) { uint32_t *m = (uint32_t *)vmem_wr(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VI16(it); return -1; }
      if (idx == 16) {
        if (sel == 0) { uint32_t r = VRr; r = (r << 1) ^ (((r >> 4) ^ (r >> 22)) & 1); VRr = (r & 0x7fffff) | 0x3f800000; }
        if (sel <= 1) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VRr); return -1; }
        VRr = 0x3f800000 | ((sel == 2 ? VF[fs].u[fsf] : VRr ^ VF[fs].u[fsf]) & 0x7fffff); return -1; }
      if (idx == 25 && sel == 0) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VPr); return -1; }   /* MFP */
      if (idx == 26 && sel == 0) { vi_set(it, vu_top); return -1; }                                                   /* XTOP */
      if (idx == 26 && sel == 1) { vi_set(it, vu_itop); return -1; }                                                  /* XITOP */
      if (idx == 27 && sel == 0) { gif_kick(VI16(is)); return -1; }                                                   /* XGKICK */
      if (idx >= 28 && idx <= 31 && !(idx == 30 && sel == 3) && !(idx == 29 && sel == 3) && !(idx == 31 && sel == 3)) {   /* EFU: P lands later */
        float x = vin(VF[fs].u[0]), y = vin(VF[fs].u[1]), z = vin(VF[fs].u[2]), ww = vin(VF[fs].u[3]), e = vin(VF[fs].u[fsf]), sq = x * x + y * y + z * z, p = 0;
        static const int LAT[4][4] = { { 11, 18, 18, 24 }, { 54, 54, 12, 0 }, { 12, 18, 12, 0 }, { 29, 54, 44, 0 } };
        switch ((idx - 28) * 4 + sel) {
          case 0: p = sq; break; case 1: p = sq != 0 ? 1.0f / sq : sq; break; case 2: p = sqrtf(sq); break; case 3: p = sq > 0 ? 1.0f / sqrtf(sq) : 3.4028235e38f; break;
          case 4: p = x != 0 ? atanf(y / x) : 0; break; case 5: p = x != 0 ? atanf(z / x) : 0; break; case 6: p = x + y + z + ww; break;
          case 8: p = sqrtf(fabsf(e)); break; case 9: p = e != 0 ? 1.0f / sqrtf(fabsf(e)) : 3.4028235e38f; break; case 10: p = e != 0 ? 1.0f / e : 3.4028235e38f; break;
          case 12: p = sinf(e); break; case 13: p = atanf(e); break; case 14: p = expf(-e); break;
        }
        efu_res = p; efu_lat = LAT[idx - 28][sel]; return -1; }
      if (idx == 30 && sel == 3) return -1;                                                                           /* WAITP */
    }
  }
  if (w == 0x8000033c) return -1;
  count(1, "!vu1 unsupported lower %08x", w); return -1;
}
typedef struct { int tgt, wm, rd[2], rm[2], flags; } VuUp;
static void vu_up_info(uint32_t w, VuUp *u)
{
  int op = w & 0x3f, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, sel = op & 3, idx = (w >> 6) & 31;
  static const char bm[4] = { 8, 4, 2, 1 };
  u->tgt = 0; u->wm = 0; u->rd[0] = u->rd[1] = 0; u->rm[0] = u->rm[1] = 0; u->flags = 0;
  if (op < 0x1c) { u->tgt = fd; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; u->rd[1] = ft; u->rm[1] = bm[sel]; u->flags = (op >> 2) != 4 && (op >> 2) != 5; return; }
  if (op < 0x30) {
    int mm = op == 0x1d || op == 0x1f || op == 0x2b || op == 0x2f, vec = op >= 0x28;
    u->tgt = fd; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; if (vec) { u->rd[1] = ft; u->rm[1] = op == 0x2e ? 14 : dest; } u->flags = !mm; if (op == 0x2e) u->rm[0] = 14; return;
  }
  if (op < 0x3c || idx >= 12) return;
  if (idx == 4 || idx == 5 || (idx == 7 && sel == 1)) { u->tgt = ft; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; return; }
  if (idx == 7 && sel == 3) { u->rd[0] = fs; u->rm[0] = 14; u->rd[1] = ft; u->rm[1] = 1; u->flags = 2; return; }
  if (idx >= 10 && sel == 3) return;
  u->rd[0] = fs; u->rm[0] = (idx == 11 && sel == 2) ? 14 : dest;
  if (idx < 4 || idx == 6) { u->rd[1] = ft; u->rm[1] = bm[sel]; } else if (idx >= 10) { u->rd[1] = ft; u->rm[1] = (idx == 11 && sel == 2) ? 14 : dest; }
  u->flags = 1;
}
static int vu_low_q(uint32_t w)
{
  if ((w >> 25) != 0x40 || (w & 0x3f) < 0x3c || ((w >> 6) & 31) != 14) return 0;
  switch (w & 3) { case 0: case 1: return 7; case 2: return 13; } return 1;
}
static void vu_low_reads(uint32_t w, int r[2], int m[2])
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fsf = (w >> 21) & 3, ftf = (w >> 23) & 3;
  r[0] = r[1] = 0; m[0] = m[1] = 0;
  if (op == 0x01) { r[0] = fs; m[0] = dest; return; }
  if (op != 0x40 || (w & 0x3f) < 0x3c) return;
  int idx = (w >> 6) & 31, sel = w & 3;
  if (idx == 12) { r[0] = fs; m[0] = 15; return; }
  if (idx == 13 && (sel & 1)) { r[0] = fs; m[0] = dest; return; }
  if (idx == 14 && sel != 3) { if (sel != 1) { r[0] = fs; m[0] = 8 >> fsf; } r[1] = ft; m[1] = 8 >> ftf; return; }
  if ((idx == 15 && sel == 0) || (idx == 16 && sel >= 2)) { r[0] = fs; m[0] = 8 >> fsf; }
  if ((idx == 28 || (idx == 29 && sel < 3))) { r[0] = fs; m[0] = 15; }
  if ((idx == 30 && sel < 3) || (idx == 31 && sel < 3)) { r[0] = fs; m[0] = 8 >> fsf; }
}
static void vu_low_writes(uint32_t w, int *r, int *m)
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31;
  *r = 0; *m = 0;
  if (op == 0x00) { *r = ft; *m = dest; return; }
  if (op != 0x40 || (w & 0x3f) < 0x3c) return;
  int idx = (w >> 6) & 31, sel = w & 3;
  if (idx == 12 || (idx == 13 && !(sel & 1)) || (idx == 15 && sel == 1) || (idx == 16 && sel <= 1) || (idx == 25 && sel == 0)) { *r = ft; *m = dest; }
}
static int vu_bk_op(uint32_t w, int *reg)   /* the integer ops whose result a conditional branch right after reads as the old value */
{
  int op = w >> 25, it = (w >> 16) & 31, is = (w >> 11) & 31, id = (w >> 6) & 31;
  if (op == 0x08 || op == 0x09) { *reg = it; return 1; }
  if (op != 0x40) return 0;
  int fn = w & 0x3f;
  if (fn == 0x30 || fn == 0x31 || fn == 0x34 || fn == 0x35) { *reg = id; return 1; }
  if (fn == 0x32) { *reg = it; return 1; }
  if (fn >= 0x3c) { int idx = (w >> 6) & 31, sel = fn & 3;
    if (idx == 15 && sel == 0) { *reg = it; return 1; }
    if (idx == 13) { *reg = (sel & 1) ? it : is; return 1; } }
  return 0;
}
typedef struct { long t; int kind; uint32_t a, b; } VuPend;
static VuPend vu_pq[64]; static int vu_pn; static long vu_qready, vu_pready; static long vu_vfready[32][4];
static void vu_push(long t, int kind, uint32_t a, uint32_t b) { if (vu_pn < 64) { vu_pq[vu_pn].t = t; vu_pq[vu_pn].kind = kind; vu_pq[vu_pn].a = a; vu_pq[vu_pn].b = b; vu_pn++; } }
static void vu_commit(long now)
{
  int k = 0;
  for (int i = 0; i < vu_pn; i++) {
    VuPend *p = &vu_pq[i];
    if (p->t > now) { vu_pq[k++] = *p; continue; }
    if (p->kind == 0) { VMAC = p->a; VSTAT = (VSTAT & 0xff0) | p->b | (p->b << 6); }
    else if (p->kind == 1) VCLIP = ((VCLIP << 6) | p->a) & 0xffffff;
    else if (p->kind == 2) { VQr = p->a; VSTAT = (VSTAT & ~0x30u) | p->b | (p->b << 6); }
    else VPr = p->a;
  }
  vu_pn = k;
}
static void vu1_run(int pc)
{
  int pend = -1, endnext = 0; long cyc = 0; vu_pn = 0; vu_qready = vu_pready = 0; memset(vu_vfready, 0, sizeof vu_vfready); bk_live = 0;
  for (int guard = 0; guard < 2000000; guard++) {
    uint32_t lo = VP[(pc & 2047) * 2], hi = VP[(pc & 2047) * 2 + 1]; vu_steps++; g_vpc = pc;
    VuUp u; vu_up_info(hi & 0x07ffffff, &u); int lq = !(hi >> 31) ? vu_low_q(lo) : 0;
    long need = cyc;
    for (int k = 0; k < 2; k++) if (u.rd[k]) for (int c = 0; c < 4; c++) if (u.rm[k] & (8 >> c)) need = vu_vfready[u.rd[k]][c] > need ? vu_vfready[u.rd[k]][c] : need;
    if (!(hi >> 31)) { int lr[2], lm[2]; vu_low_reads(lo, lr, lm); for (int k = 0; k < 2; k++) if (lr[k]) for (int c = 0; c < 4; c++) if (lm[k] & (8 >> c)) need = vu_vfready[lr[k]][c] > need ? vu_vfready[lr[k]][c] : need; }
    if (lq && vu_qready > need) need = vu_qready;
    int is_efu = 0, waitp = 0;
    if (!(hi >> 31) && (lo >> 25) == 0x40 && (lo & 0x3f) >= 0x3c) { int idx = (lo >> 6) & 31, sel = lo & 3; is_efu = idx >= 28 && !(idx == 30 && sel == 3); waitp = idx == 30 && sel == 3; }
    if ((is_efu || waitp) && vu_pready > need) need = vu_pready;
    cyc = need; vu_commit(cyc);
    uint32_t vmac = VMAC, vst = VSTAT, vcl = VCLIP; VQ old = VF[u.tgt];
    vu_upper(hi & 0x07ffffff);
    VQ nv = VF[u.tgt]; VF[u.tgt] = old;
    if (u.flags == 1) vu_push(cyc + 4, 0, VMAC, VSTAT & 0xf); else if (u.flags == 2) vu_push(cyc + 4, 1, VCLIP & 0x3f, 0);
    VMAC = vmac; VSTAT = vst; VCLIP = vcl;
    int took = -1, bkr = -1; uint32_t bko = 0;
    if (hi >> 31) VIreg = lo;
    else {
      uint32_t q0 = VQr, s0 = VSTAT; int lw, lwm; vu_low_writes(lo, &lw, &lwm);
      if (vu_bk_op(lo, &bkr)) bko = VI16(bkr);
      efu_lat = 0;
      took = vu_lower(lo, pc);
      if (lq > 1) { vu_push(cyc + lq, 2, VQr, VSTAT & 0x30); vu_qready = cyc + lq; VQr = q0; VSTAT = (VSTAT & ~0x30u) | (s0 & 0x30); }
      if (efu_lat) { vu_push(cyc + efu_lat, 3, vclamp(efu_res), 0); vu_pready = cyc + efu_lat; }
      if (lw) for (int c = 0; c < 4; c++) if (lwm & (8 >> c)) vu_vfready[lw][c] = cyc + 4;
    }
    bk_live = bkr >= 0; bk_reg = bkr; bk_old = bko;
    if (u.tgt) for (int c = 0; c < 4; c++) if (u.wm & (8 >> c)) { VF[u.tgt].u[c] = nv.u[c]; vu_vfready[u.tgt][c] = cyc + 4; }
    if (vu_trace_call == g_call && (xlp_vb < 0 || g_vb == xlp_vb)) {
      printf("t%s %04x hi %08x lo %08x cyc %ld", sb_on ? "T" : "", pc, hi, lo, cyc);
      if (u.tgt) printf(" | vf%d %g %g %g %g", u.tgt, VFL(VF[u.tgt].u[0]), VFL(VF[u.tgt].u[1]), VFL(VF[u.tgt].u[2]), VFL(VF[u.tgt].u[3]));
      if (!(hi >> 31)) { int lw, lwm; vu_low_writes(lo, &lw, &lwm); if (lw) printf(" | vf%d %g %g %g %g", lw, VFL(VF[lw].u[0]), VFL(VF[lw].u[1]), VFL(VF[lw].u[2]), VFL(VF[lw].u[3])); }
      printf(" | vi"); for (int k = 1; k < 16; k++) printf(" %x", VIr[k]); printf(" | clip %06x q %g\n", VCLIP & 0xffffff, VFL(VQr));
    }
    cyc++;
    if (endnext) { vu_commit(1L << 40); vu_tpc = pc + 1; return; }
    if (hi >> 30 & 1) endnext = 1;
    int npc = pc + 1; if (pend >= 0) { npc = pend; pend = -1; }
    if (took >= 0) pend = took;
    pc = npc;
  }
  count(1, "!vu1 runaway");
}
/* ================= translator check (GSREF_XLATE=1): ge/xlate.c beside the VU1 interpreter (renderer_notes §11.10) ================
   The translator reads the same VIF1 and PATH3 words and runs no micro code.  It reports each call before gsref runs it; compared then are
   the entry and TOP, and the VU memory the converters read: abs 0x00..0x0b; for the P04 / P06 entries T+0x00..0x17; the A+D packet and
   the tag template of the entry's rule (xl_rule); for df0 and the effect entries the inputs T+0x100..+3n.  At the call's first kicked
   vertex: the GS registers a primitive reads (its context's and the shared ones).  Counted per entry: the program's loads, and the kicked
   quadwords the GS uses (not NOP-register data), of a qword that differs between the two memories at the call's start and that the call
   has not written yet; and where its first XGKICK reads.
   GSREF_XLRUN: trial runs, each a second run of the call that draws nothing, with gsref's state saved and restored around it; a call's
   GS output is the hash of every drawn primitive (PRIM attributes, the registers it reads, the vertex fields it uses) and of the GS
   registers it leaves.  1: the trial runs on the translator's VU memory; 2: on gsref's memory with VF1-31, VI1-15, ACC, Q, P and I
   scrambled (does the program read registers an earlier call left?); 3: both; 4: neither (checks the trial itself: no call may differ).
   8: no trial; every call runs on the translator's memory (the frames then show what that changes).  GSREF_XLSHOW=<entry>: examples of
   one entry only; GSREF_XLPRIM=<vb>,<call>: that call's primitives, registers and hashed data in both runs (and GSREF_TRACE limited to
   that vblank). */
typedef struct { uint32_t qa, pc, vu[4], tr[4]; int kick; } XlRd;
static XlRd xl_rb[64]; static int xl_nrb;   /* the current call's reads of qwords the translator lacks, printed when its trial run differs */
static int xl_chk, xl_kicked, xl_got; static XlCall xl_c; static uint64_t xl_gr[256];
static const uint8_t XLR[29] = { 0x00, 0x06, 0x07, 0x08, 0x09, 0x14, 0x15, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x3b, 0x3d, 0x40, 0x41, 0x42, 0x43, 0x45,
  0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f };
typedef struct { uint32_t e; long calls, cont, cont_unk, bad_entry, bad_top, vm[5], vmq[5], gs_calls, gs_used, gs[29], nokick, kick_n[5], dq_n[9], rd[2], rd_calls, rd_past, rq_n[2][9], run_n, run_bad; uint32_t kick_q[4], dq[8], rq[2][8]; int nk, ndq, nrq[2], rd_this, n, stride; } XlEnt;
static XlEnt xl_e[96]; static int xl_ne; static long xl_missed, xl_extra, xl_shown_vm, xl_shown_gs;
static XlEnt *xl_ent(uint32_t e)
{
  for (int i = 0; i < xl_ne; i++) if (xl_e[i].e == e) return &xl_e[i];
  if (xl_ne == 96) return &xl_e[95];
  xl_e[xl_ne].e = e; return &xl_e[xl_ne++];
}
static int xc_on, xc_cut, xc_cutnow, xc_pend, xc_r, xc_taken; static long xc_cn[3]; static uint8_t xc_vm[16384]; static uint64_t xc_gr[256]; static XlCall xc_c;   /* GSREF_XLCONV (below) */
static void xdg_cb(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n) { (void)ctx; xdg_prim(g, d, v, n); }
static void xl_cb(void *ctx, const XlCall *c)
{
  if (xdg_on) xdg_call(c);
  (void)ctx; if (xl_got) xl_extra++; xl_c = *c; xl_got = 1; memcpy(xl_gr, c->gr, sizeof xl_gr);
  if (xt_on) { xt_nps = 0; if (is_lit(c->entry)) xt_rec(); }
  if (xc_on && !c->cont && (c->entry == XL_DF0 || c->entry == XL_FX || c->entry == XL_FX2 || is_lit(c->entry))) {   /* the call as the translator hands it over, for its converter at the call's end */
    memcpy(xc_vm, c->vm, sizeof xc_vm); memcpy(xc_gr, c->gr, sizeof xc_gr); xc_c = *c; xc_c.vm = xc_vm; xc_c.gr = xc_gr; xc_pend = 1; xc_pmask = 0; }
}
/* the translator's P06 and P07 passes: the texture check's CLUT of the pass (GSREF_XLTEX), the pass's GS registers for the converter */
static void gs_pass_cb(void *ctx, const XlCall *c, int pass)
{
  (void)ctx;
  if (xt_on) xt_rec();
  if (xc_on && xc_pend && is_lit(c->entry) && pass >= 2 && pass <= 3) { memcpy(xc_pgr[pass], c->gr, sizeof xc_pgr[pass]); xc_pmask |= 1 << pass; }
}
static void xl_vmcmp(XlEnt *E, int reg, uint32_t qa, int n)
{
  const uint8_t *x = xl_vm(); int bad = 0;
  for (int k = 0; k < n; k++) { uint32_t a = (qa + k) & 0x3ff;
    if (memcmp(VM + a * 16, x + a * 16, 16)) { bad++;
      uint32_t r = reg == 1 || reg == 3 ? (a - vu_top) & 0x3ff : a | 0x1000; int i; for (i = 0; i < E->ndq && E->dq[i] != r; i++) {}
      if (i == E->ndq && i < 8) { E->dq[i] = r; E->ndq++; } E->dq_n[i < 8 ? i : 8]++;
      if (xl_shown_vm < 16) { const uint32_t *p = (const uint32_t *)(VM + a * 16), *t = (const uint32_t *)(x + a * 16); xl_shown_vm++;
        printf("xlate vb %ld call %d entry %06x region %d qword %03x (T+%03x): vu %08x %08x %08x %08x translator %08x %08x %08x %08x\n", g_vb, g_call, g_entry, reg, a, (a - vu_top) & 0x3ff,
               p[0], p[1], p[2], p[3], t[0], t[1], t[2], t[3]); } } }
  if (bad) { E->vm[reg]++; E->vmq[reg] += bad; }
}
static int xl_run, xl_got_run; static long xl_shown_run; static uint64_t xl_hb;   /* GSREF_XLRUN (above); xl_hb: the trial's hash */
static void vu1_run(int pc);
static void xl_trial(int pc)
{
  static VQ sVF[32]; static uint8_t sVM[16384], svs[1024]; uint32_t sVI[16], sACC[4], s1[10]; Gif sg = gp[1]; Vtx sc = cur, svq[3]; memcpy(svq, vq, sizeof vq); int svqn = vqn; float sq = g_q; uint32_t ssrc = g_src; static uint64_t sgr[256]; memcpy(sgr, gr, sizeof sgr);
  long sun = vm_uninit; int stpc = vu_tpc;
  memcpy(sVF, VF, sizeof VF); memcpy(sVI, VIr, sizeof sVI); memcpy(sACC, VACC, sizeof sACC); memcpy(sVM, VM, sizeof VM); memcpy(svs, vm_set, sizeof svs);
  uint32_t *R[10] = { &VQr, &VPr, &VIreg, &VRr, &VMAC, &VSTAT, &VCLIP, &g_vmac, 0, 0 }; for (int k = 0; k < 8; k++) s1[k] = *R[k];
  if (xl_run & 1) memcpy(VM, xl_vm(), sizeof VM);
  if (xl_run & 2) { for (int r = 1; r < 32; r++) for (int c = 0; c < 4; c++) VF[r].u[c] = 0x4b3c614eu ^ (r * 4 + c) * 0x9e3779b9u; for (int r = 1; r < 16; r++) VIr[r] = (0x5a5a + r * 0x1234) & 0xffff;
    for (int c = 0; c < 4; c++) VACC[c] = 0x3f9e0419u + c; VQr = 0x3fb504f3u; VPr = 0x3e8f5c29u; VIreg = 0x40490fdbu; }
  sb_on = 1; sb_h = 0xcbf29ce484222325ull; vu1_run(pc); for (int k = 0; k < 29; k++) sb_hash(&gr[XLR[k]], 8); sb_on = 0; xl_hb = sb_h;
  memcpy(VF, sVF, sizeof VF); memcpy(VIr, sVI, sizeof sVI); memcpy(VACC, sACC, sizeof sACC); memcpy(VM, sVM, sizeof VM); memcpy(vm_set, svs, sizeof svs);
  for (int k = 0; k < 8; k++) *R[k] = s1[k];
  gp[1] = sg; cur = sc; memcpy(vq, svq, sizeof vq); vqn = svqn; g_q = sq; g_src = ssrc; memcpy(gr, sgr, sizeof sgr); vm_uninit = sun; vu_tpc = stpc;
}
static void xl_call_begin(int cont, int pc)
{
  XlEnt *E = xl_ent(g_entry); E->calls++; if (cont) E->cont++;
  if (!xl_got) { xl_missed++; return; }
  xl_got = 0;
  if (cont && !xl_c.entry) E->cont_unk++;
  else if (xl_c.entry != g_entry) E->bad_entry++;
  if (xl_c.top != vu_top || xl_c.itop != vu_itop) E->bad_top++;
  xl_vmcmp(E, 0, 0x00, 12);
  const XlRule *r = xl_rule(g_entry);
  if (r && r->ad == 0xe8 && !r->ad_abs) xl_vmcmp(E, 1, vu_top, 0x18);
  if (xl_c.ad_qa != ~0u) {
    const uint32_t *t = (const uint32_t *)(VM + xl_c.ad_qa * 16); int nl = t[0] & 0x7fff, fl = t[1] >> 26 & 3, nr = (t[1] >> 28 & 15) ? (int)(t[1] >> 28 & 15) : 16;
    int len = 1 + (fl == 0 ? nl * nr : fl == 1 ? (nl * nr + 1) / 2 : nl); if (len > 24) len = 24;
    xl_vmcmp(E, 2, xl_c.ad_qa, len);
  }
  if (xl_c.tag_qa != ~0u) xl_vmcmp(E, 4, xl_c.tag_qa, 1);
  if (!cont && (g_entry == XL_DF0 || g_entry == XL_FX || g_entry == XL_FX2)) {
    int n = *(const uint32_t *)(VM + ((vu_top + 0x14) & 0x3ff) * 16) & 0xffff; if (n > 300) n = 300;
    xl_vmcmp(E, 3, vu_top + 0x100, 3 * n);
  }
  if (xl_run & 7) { xl_trial(pc); xl_got_run = 1; }
  sb_h = 0xcbf29ce484222325ull;
  for (int a = 0; a < 1024; a++) xl_diff[a] = memcmp(VM + a * 16, xl_vm() + a * 16, 16) != 0;
  memset(xl_wr, 0, sizeof xl_wr); E->rd_this = 0; E->n = -1; xl_nrb = 0;
  if (xl_run & 8) memcpy(VM, xl_vm(), sizeof VM);   /* GSREF_XLRUN=8: every call runs on the translator's memory (the images then show its effect) */
  if (!cont && r && r->ad == 0xe8 && !r->ad_abs) { E->n = *(const uint32_t *)(VM + ((vu_top + 0x14) & 0x3ff) * 16) & 0xffff; E->stride = (g_entry == 0x19fe08 || g_entry == 0x1a0160) ? 6 : 3; }
  xl_in = 1; xl_first = 1; xl_kicked = 0;
}
static long xl_shown_rd; static uint32_t xl_show;   /* GSREF_XLSHOW=<entry>: examples of that entry only */
static void xl_badread(uint32_t qa, int kick)
{
  XlEnt *E = xl_ent(g_entry); uint32_t r = (qa - vu_top) & 0x3ff; int i;
  int past = !kick && E->n >= 0 && r >= 0x100 && (int)(r - 0x100) / E->stride >= E->n;   /* a load past the call's input vertices */
  if (xl_nrb < 64) { XlRd *b = &xl_rb[xl_nrb++]; b->qa = qa; b->pc = g_vpc; b->kick = kick + 2 * past; memcpy(b->vu, VM + qa * 16, 16); memcpy(b->tr, xl_vm() + qa * 16, 16); }
  if (past) E->rd_past++;
  else {
    E->rd[kick]++; if (!E->rd_this) { E->rd_this = 1; E->rd_calls++; }
    for (i = 0; i < E->nrq[kick] && E->rq[kick][i] != r; i++) {}
    if (i == E->nrq[kick] && i < 8) { E->rq[kick][i] = r; E->nrq[kick]++; } E->rq_n[kick][i < 8 ? i : 8]++;
    if (xl_shown_rd < 40 && (!xl_show || xl_show == g_entry)) { xl_shown_rd++; const uint32_t *p = (const uint32_t *)(VM + qa * 16), *t = (const uint32_t *)(xl_vm() + qa * 16);
      printf("xlate read vb %ld call %d entry %06x %s pc %04x abs %03x T+%03x n %d: vu %08x %08x %08x %08x translator %08x %08x %08x %08x\n", g_vb, g_call, g_entry, kick ? "xgkick" : "load", g_vpc, qa, r, E->n,
             p[0], p[1], p[2], p[3], t[0], t[1], t[2], t[3]); }
  }
  xl_diff[qa] = 0;   /* count each qword once per call */
}
static void xl_call_end(void)
{
  XlEnt *E = xl_ent(g_entry);
  if (xl_first) E->nokick++;
  xl_first = 0; xl_in = 0;
  if (xl_run && xl_got_run) { for (int k = 0; k < 29; k++) sb_hash(&gr[XLR[k]], 8); E->run_n++; if (sb_h != xl_hb) { E->run_bad++; if (xl_shown_run < 12 && (!xl_show || xl_show == g_entry)) { xl_shown_run++; printf("xlate run vb %ld call %d entry %06x top %03x n %d: the GS output differs; its reads of qwords the translator lacks:\n", g_vb, g_call, g_entry, vu_top, E->n);
    for (int i = 0; i < xl_nrb; i++) { XlRd *b = &xl_rb[i]; printf("  %s pc %04x abs %03x T+%03x: vu %08x %08x %08x %08x translator %08x %08x %08x %08x\n", (const char *[]){ "load", "xgkick", "load past the inputs" }[b->kick], b->pc, b->qa, (b->qa - vu_top) & 0x3ff,
                                     b->vu[0], b->vu[1], b->vu[2], b->vu[3], b->tr[0], b->tr[1], b->tr[2], b->tr[3]); } } } }
  xl_got_run = 0;
}
static void xl_note_kick(uint32_t qa)
{
  XlEnt *E = xl_ent(g_entry); uint32_t r = (qa - vu_top) & 0x3ff; int i;
  for (i = 0; i < E->nk && E->kick_q[i] != r; i++) {}
  if (i == E->nk && i < 4) { E->kick_q[i] = r; E->nk++; }
  E->kick_n[i < 4 ? i : 4]++;
}
static int xl_used(int r, int ctx, int ac)
{
  if (r == 0x00) return 1;
  if (r == 0x1b) return !ac;
  switch (r) { case 0x1a: case 0x1c: case 0x3b: case 0x3d: case 0x45: case 0x46: case 0x49: return 1; }
  static const uint8_t C[10] = { 0x06, 0x08, 0x14, 0x18, 0x40, 0x42, 0x47, 0x4a, 0x4c, 0x4e };
  for (int k = 0; k < 10; k++) if (r == C[k] + ctx) return 1;
  return 0;
}
static void xl_first_kick(void)
{
  xl_first = 0; XlEnt *E = xl_ent(g_entry); E->gs_calls++;
  int ac = gr[0x1a] & 1, ctx = (int)((ac ? gr[0x00] : gr[0x1b]) >> 9 & 1), used = 0;
  for (int k = 0; k < 29; k++) { int r = XLR[k]; uint64_t a = gr[r], b = xl_gr[r]; if (!r) { a &= 0x7ff; b &= 0x7ff; }
    if (a == b) continue;
    E->gs[k]++; if (xl_used(r, ctx, ac)) used = 1;
    if (xl_shown_gs < 12 && xl_used(r, ctx, ac)) { xl_shown_gs++; printf("xlate vb %ld call %d entry %06x GS reg %02x: vu %016llx translator %016llx\n", g_vb, g_call, g_entry, r, (unsigned long long)a, (unsigned long long)b); } }
  if (used) E->gs_used++;
}
static int xl_cmp(const void *a, const void *b) { long d = ((const XlEnt *)b)->calls - ((const XlEnt *)a)->calls; return d > 0 ? 1 : d < 0 ? -1 : 0; }
static void xl_report(void)
{
  const XlStats *S = xl_stats(); long calls = 0;
  for (int i = 0; i < xl_ne; i++) calls += xl_e[i].calls;
  fprintf(stderr, "xlate: %ld VIF1 words, %ld calls seen (%ld MSCNT, %ld at never-uploaded code), gsref %ld calls; %ld calls the translator did not report, %ld reported twice; "
          "%ld A+D packets applied (%ld without an end); GIF tags path 1/2/3 %ld/%ld/%ld; %ld unknown VIF commands\n", S->words, S->calls, S->calls_cont, S->calls_noentry, calls, xl_missed, xl_extra,
          S->ad_kicks, S->ad_bad, S->gif_tags[1], S->gif_tags[2], S->gif_tags[3], S->unk);
  qsort(xl_e, xl_ne, sizeof *xl_e, xl_cmp);
  for (int i = 0; i < xl_ne; i++) { XlEnt *E = &xl_e[i];
    fprintf(stderr, "  entry %06x%s: %ld calls (%ld MSCNT, %ld of them not named), entry differs %ld, TOP differs %ld; VU memory differs (calls / qwords): abs %ld/%ld, T+0..17 %ld/%ld,"
            " A+D %ld/%ld, inputs %ld/%ld, tag template %ld/%ld; GS compared %ld, a used register differs %ld, no vertex %ld; first XGKICK at", E->e, xl_rule(E->e) ? " (rule)" : "", E->calls, E->cont,
            E->cont_unk, E->bad_entry, E->bad_top, E->vm[0], E->vmq[0], E->vm[1], E->vmq[1], E->vm[2], E->vmq[2], E->vm[3], E->vmq[3], E->vm[4], E->vmq[4], E->gs_calls, E->gs_used, E->nokick);
    for (int k = 0; k < E->nk; k++) fprintf(stderr, " T+%03x x%ld", E->kick_q[k], E->kick_n[k]);
    if (E->kick_n[4]) fprintf(stderr, " other x%ld", E->kick_n[4]);
    for (int k = 0; k < E->ndq; k++) fprintf(stderr, "%s %s%03x x%ld", k ? "" : "; differing qwords", E->dq[k] & 0x1000 ? "abs " : "T+", E->dq[k] & 0x3ff, E->dq_n[k]);
    if (E->dq_n[8]) fprintf(stderr, " other x%ld", E->dq_n[8]);
    fprintf(stderr, "; reads of qwords the translator lacks (in %ld calls): loads %ld", E->rd_calls, E->rd[0]);
    for (int k = 0; k < E->nrq[0]; k++) fprintf(stderr, " T+%03x x%ld", E->rq[0][k], E->rq_n[0][k]);
    if (E->rq_n[0][8]) fprintf(stderr, " other x%ld", E->rq_n[0][8]);
    fprintf(stderr, ", kicked %ld", E->rd[1]);
    for (int k = 0; k < E->nrq[1]; k++) fprintf(stderr, " T+%03x x%ld", E->rq[1][k], E->rq_n[1][k]);
    if (E->rq_n[1][8]) fprintf(stderr, " other x%ld", E->rq_n[1][8]);
    if (E->rd_past) fprintf(stderr, ", loads past the inputs %ld", E->rd_past);
    if (xl_run) fprintf(stderr, "; trial runs: GS output differs in %ld of %ld calls", E->run_bad, E->run_n);
    int any = 0; for (int k = 0; k < 29; k++) if (E->gs[k]) { fprintf(stderr, "%s %02x:%ld", any ? "" : "; registers that differ", XLR[k], E->gs[k]); any = 1; }
    fprintf(stderr, "\n"); }
}
static void gif_kick(uint32_t qa)
{
  if (xl_in && !xl_kicked) { xl_kicked = 1; xl_note_kick(qa); }
  count(1, "xgkick"); Gif *g = &gp[1];
  for (int n = 0; n < 1024; n++) { uint32_t q[4], a = (qa + n) & 0x3ff; xl_kicking = 1; memcpy(q, vmem_rd(a), 16); xl_kicking = 0; g_src = 0xf0000000u + a * 16;
    int e = gif_qw(g, q);
    if (xl_in && xl_diff[a] && !xl_wr[a] && g_qw_used) xl_badread(a, 1);   /* a kicked quadword whose data the GS uses */
    if (e) return; }
  count(1, "!xgkick without end");
}

/* ================= converter check (GSREF_XLCONV=1 with GSREF_XLATE=1): ge/xconv.c on the translator's calls (renderer_notes §11.10) =====
   Each df0 / 25f0 / 2870 call is converted from what the translator handed over at its start (a copy of its VU memory and GS registers),
   after gsref has run it.  On every effect call (all frames): the converter's clip positions and stored flag words against what the
   micro program stored back, and its strip triangles against the ADC bits the program kicked.  In the export's frame (GSREF_GEOUT):
   each batch gsref's export would append is compared field by field and byte by byte with the converter's, and the converter's is
   appended instead (the texture comes from gsref's cache by the translator's TEX0 / CLAMP and the EE source of their blocks; the pixel
   count is gsref's), so that file must equal the normal export.  A batch the converter sends where gsref's export has none (the PS2 drew
   nothing: df0 calls whose triangles are all off the screen or edge-on) is classed by what the GE does with each triangle (xc_df0_px). */
static GerBatch xc_b; static uint8_t xc_blob[1 << 19]; static uint32_t xc_size; static XcTrace xc_tr; static int xc_k;
static long xc_n[24]; static long xc_shown;
static const char *XCN[24] = { "calls converted", "batches", "no batch", "bad input", "texture not in gsref's cache", "effect calls traced", "vertices traced",
  "clip position differs", "stored flag word differs", "colour float differs", "UV float differs", "strip triangles compared", "strip triangle differs (drawn vs ADC kicked)",
  "export batches compared", "equal", "batch fields differ", "data differ", "gsref batch, converter none", "converter batch, gsref none (export frame)",
  "2870 calls with a strip triangle drawn", "converter-only batches: df0", "converter-only batches: 25f0", "converter-only batches: 2870", "" };
static int xc_tex_cb(void *ctx, const uint64_t *g, int ctxt, XcTex *t)
{
  (void)ctx;
  uint64_t t0 = g[0x06 + ctxt] & ((1ull << 61) - 1), cl = g[0x08 + ctxt]; int psm = t0 >> 20 & 63;   /* tex_use's key: TEX0 without CLD, CLAMP, EE sources */
  uint32_t ts = blk_src[t0 & 0x3fff], cs = is_idx(psm) ? blk_src[(t0 >> 37) & 0x3fff] : 0;
  int i; for (i = 0; i < ntu && !(tu[i].t0 == t0 && tu[i].cl == cl && tu[i].tsrc == ts && tu[i].csrc == cs); i++) {}
  if (i == ntu) { if (ge_on) xc_n[4]++; return 0; }
  int ti = ge_tex_of_tu[i]; if (ti < 0) return 0;
  t->index = ti; t->kind = G.t[ti].kind; t->tw = G.t[ti].tw; t->th = G.t[ti].th; t->u0 = G.u0[ti]; t->v0 = G.v0[ti]; t->minalpha = G.t[ti].minalpha;
  return 1;
}
static void xc_run(void)
{
  xc_pend = 0; xc_taken = 0;
  XcHost h = { xc_tex_cb, 0, ge_zk, ge_on ? 0.0f : -1.0f, (float)(65535.0 / ge_zk), &xc_tr, ge_live };
  h.cut = xc_cut;
  xc_k = xc_c.entry == XL_DF0 ? 0 : xc_c.entry == XL_FX ? 1 : xc_c.entry == XL_FX2 ? 2 : 3; xc_tr.n = 0;
  long cut0 = xc_stats()->cut[0];
  xc_r = xc_call(&xc_c, &h, &xc_b, xc_blob, sizeof xc_blob, &xc_size);
  xc_cutnow = xc_stats()->cut[0] != cut0;
  xc_n[0]++; xc_n[xc_r > 0 ? 1 : xc_r == 0 ? 2 : 3]++;
  if (xc_k == 3) {   /* P06 and P07: passes 2 and 3, each with the GS registers after its A+D packet (the texture scale goes into the blob) */
    for (int p = 2; p <= 3; p++) { xc_pr[p] = 0;
      if (xc_r == 1 && (xc_pmask >> p & 1)) { XlCall pc = xc_c; pc.gr = xc_pgr[p]; xc_pr[p] = xc_pass(&pc, p, &h, &xc_pb[p], xc_blob); } }
    return;
  }
  if (!xc_k || xc_tr.n <= 0) return;
  xc_n[5]++; xc_n[6] += xc_tr.n;
  for (int i = 0; i < xc_tr.n; i++) {   /* what the micro program stored back in the input slots (gsref's VU memory after the call) */
    const uint32_t *p = (const uint32_t *)(VM + ((vu_top + 0x100 + 3 * i) & 0x3ff) * 16), *c = (const uint32_t *)(VM + ((vu_top + 0x101 + 3 * i) & 0x3ff) * 16),
                   *u = (const uint32_t *)(VM + ((vu_top + 0x102 + 3 * i) & 0x3ff) * 16), *c0 = (const uint32_t *)(xc_vm + ((vu_top + 0x101 + 3 * i) & 0x3ff) * 16),
                   *u0 = (const uint32_t *)(xc_vm + ((vu_top + 0x102 + 3 * i) & 0x3ff) * 16);
    int bc = memcmp(p, xc_tr.c[i], 16) != 0, bf = (u[3] & 0xffff) != xc_tr.post[i], bcol = 0, buv = 0;
    for (int k = 0; k < 4; k++) { float f = (float)(int32_t)c0[k]; if (memcmp(&f, &c[k], 4)) bcol = 1; }
    for (int k = 0; k < 3; k++) { float f = (float)(int32_t)u0[k] / 4096.0f; if (memcmp(&f, &u[k], 4)) buv = 1; }
    xc_n[7] += bc; xc_n[8] += bf; xc_n[9] += bcol; xc_n[10] += buv;
    if ((bc || bf) && xc_shown < 12) { xc_shown++; float q[4]; memcpy(q, p, 16);
      printf("xlconv vb %ld call %d entry %06x vertex %d: vu clip %.9g %.9g %.9g %.9g flags %04x; converter %.9g %.9g %.9g %.9g flags %04x\n", g_vb, g_call, g_entry, i,
             q[0], q[1], q[2], q[3], u[3] & 0xffff, xc_tr.c[i][0], xc_tr.c[i][1], xc_tr.c[i][2], xc_tr.c[i][3], xc_tr.post[i]); }
  }
}
/* the export's batch b with its data (*bp, *bs): compared with the converter's, which takes its place; 0: nothing to append */
static int xc_take(GerBatch *b, const uint8_t **bp, uint32_t *bs)
{
  xc_n[13]++; xc_taken = 1;
  if (xc_cutnow) {   /* GSREF_XLCUT left the call out (the live renderer does the same): the PS2 must have drawn no pixel from it */
    xc_cn[0]++; xc_cn[2] += b->px != 0;
    if (b->px) { xc_cn[1]++; if (xc_shown < 24) { xc_shown++; printf("xlconv vb %ld call %d entry %06x: LEFT OUT off the scissor, but the PS2 wrote %u pixels from it\n", g_vb, g_call, g_entry, b->px); } }
    return 0; }
  if (xc_r != 1) { xc_n[17]++; if (xc_shown < 24) { xc_shown++; printf("xlconv vb %ld call %d entry %06x: gsref exports a batch, the converter none (%d)\n", g_vb, g_call, g_entry, xc_r); } return 0; }
  GerBatch x = xc_b, g = *b; x.px = g.px; x.first = g.first = 0;
  int fd = memcmp(&x, &g, sizeof x) != 0, dd = xc_size != *bs || memcmp(xc_blob, *bp, *bs);
  if (!fd && !dd) xc_n[14]++;
  xc_n[15] += fd; xc_n[16] += dd;
  if ((fd || dd) && xc_shown < 24) { xc_shown++;
    printf("xlconv vb %ld call %d entry %06x: batch fields %s, data %s (sizes %u / %u)", g_vb, g_call, g_entry, fd ? "differ" : "equal", dd ? "differ" : "equal", xc_size, *bs);
    if (fd) printf("; tex %d/%d flags %02x/%02x twopass %d/%d count %u/%u sc %d,%d,%d,%d/%d,%d,%d,%d", x.tex, g.tex, x.flags, g.flags, x.twopass, g.twopass, x.count, g.count,
                   x.sx0, x.sy0, x.sx1, x.sy1, g.sx0, g.sy0, g.sx1, g.sy1);
    if (dd) { uint32_t o = 0, m = xc_size < *bs ? xc_size : *bs; while (o < m && xc_blob[o] == (*bp)[o]) o++;
      printf("; first differing byte %u (GerXform %u bytes, then %s)", o, (unsigned)sizeof(GerXform), xc_k ? "vertices 24 B, polygon vertices 24 B, classes, fans" : "vertices 16 B, strips, indices"); }
    printf("\n"); }
  x.first = 0; x.px = b->px; *b = x; *bp = xc_blob; *bs = xc_size;
  return 1;
}
/* P06 in the export's frame: each pass's batch (pass 1 xc_b, passes 2 and 3 xc_pass's) against the state of the first primitive gsref
   drew in that pass (texture, flags, tests, blend, scissor, entry; two-pass on its own), then, for GSREF_GELIVE, the passes into the
   frame as the live renderer adds them (ge/gelive.c gl_call, gl_pass) */
static long xp_n[8]; static const char *XPN[8] = { "P06 and P07 calls in the export frame", "passes compared", "equal", "state differs", "two-pass differs",
  "gsref pass, converter none", "converter pass, gsref none", "" };
static void xc_p6_end(void)
{
  if (xc_r != 1) { if (xc_r < 0 && xc_shown < 24) { xc_shown++; printf("xlconv vb %ld call %d entry %06x: P06 / P07 converter refused the call (%d)\n", g_vb, g_call, g_entry, xc_r); } return; }
  xp_n[0]++;
  for (int p = 1; p <= 3; p++) {
    int cv = p == 1 || xc_pr[p] == 1; const GerBatch *x = p == 1 ? &xc_b : &xc_pb[p];
    if (!cv && !xc_p6have[p]) continue;
    if (!cv || !xc_p6have[p]) { xp_n[cv ? 6 : 5]++;
      if (xc_shown < 24) { xc_shown++; printf("xlconv vb %ld call %d entry %06x pass %d: %s\n", g_vb, g_call, g_entry, p, cv ? "the converter draws it, gsref drew nothing" : "gsref drew it, the converter does not"); }
      continue; }
    xp_n[1]++;
    GerBatch a = *x, g = xc_p6st[p];
    a.twopass = g.twopass = 0; a.kind = g.kind = 0; a.first = g.first = 0; a.count = g.count = 0; a.px = g.px = 0;
    int sd = memcmp(&a, &g, sizeof a) != 0, td = x->twopass != xc_p6twop[p];
    xp_n[2] += !sd && !td; xp_n[3] += sd; xp_n[4] += td;
    if ((sd || td) && xc_shown < 24) { xc_shown++;
      printf("xlconv vb %ld call %d entry %06x pass %d: converter / gsref tex %d/%d prim %d/%d flags %02x/%02x tests %d,%d,%d,%d/%d,%d,%d,%d blend %d%d%d%d,%d/%d%d%d%d,%d tfx %d/%d sc %d,%d,%d,%d/%d,%d,%d,%d two-pass %d/%d\n",
             g_vb, g_call, g_entry, p, a.tex, g.tex, a.prim, g.prim, a.flags, g.flags, a.ztst, a.atst, a.aref, a.afail, g.ztst, g.atst, g.aref, g.afail,
             a.ba, a.bb, a.bc, a.bd, a.fix, g.ba, g.bb, g.bc, g.bd, g.fix, a.tfx, g.tfx, a.sx0, a.sy0, a.sx1, a.sy1, g.sx0, g.sy0, g.sx1, g.sy1, x->twopass, xc_p6twop[p]); }
  }
  /* the live renderer draws it; px (pixels the PS2 wrote) here only 1 when gsref drew a primitive in the pass, else 0, for
     tools/dev/ger_cmp.py's rule on the texture of a call the PS2 drew nothing from */
  xc_b.px = (uint32_t)xc_p6have[1]; xc_pb[2].px = (uint32_t)xc_p6have[2]; xc_pb[3].px = (uint32_t)xc_p6have[3];
  if (ge_live && !gb_xform(&G, &xc_b, xc_blob, xc_size)) {
    uint32_t base = G.b[G.nb - 1].first - xc_b.first;
    for (int p = 2; p <= 3; p++) if (xc_pr[p] == 1) gb_xform_at(&G, &xc_pb[p], base);
  }
}
/* a df0 batch the converter sends and gsref's export has none for (the PS2 drew nothing from the call): what the GE does with each of
   its triangles, in df0chk's model (the near plane, then the guard band drops the whole primitive; screen = the batch's scissor):
   zero area / wholly behind the near plane / dropped by the guard band / outside the scissor (with a one-pixel margin) / may draw */
static float f24(float f);
static long xc_px[5], xc_px16[4]; static const char *XCPX[5] = { "zero area", "behind the near plane", "over the guard band", "off the scissor", "may draw" };
static void xc_df0_px(void)
{
  static VcVert v[1024]; static uint16_t fl[1024]; VcXform x; GerXform g;
  int n = vc_df0(xc_vm, xc_c.top, &x, v, fl, 1024), shown = 0;
  if (n < 0) return;
  memcpy(&g, xc_blob, sizeof g);
  for (int i = 2; i < n; i++) {
    if (fl[i] & 0x8000) continue;
    double cc[3][4], poly[8][4], sx[8], sy[8], x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30, area = 0; int np = 0, gb = 0;
    for (int k = 0; k < 3; k++) for (int c = 0; c < 4; c++) { const VcVert *p = &v[i - 2 + k];
      cc[k][c] = (double)x.m[c] * p->x + (double)x.m[4 + c] * p->y + (double)x.m[8 + c] * p->z + x.m[12 + c]; }
    for (int k = 0; k < 3; k++) { const double *P = cc[k], *Q = cc[(k + 1) % 3]; double dp = P[3] - P[2], dq = Q[3] - Q[2];
      if (dp >= 0) memcpy(poly[np++], P, sizeof poly[0]);
      if ((dp >= 0) != (dq >= 0)) { double t = dp / (dp - dq); for (int c = 0; c < 4; c++) poly[np][c] = P[c] + t * (Q[c] - P[c]); np++; } }
    if (np < 3) { xc_px[1]++; continue; }
    for (int k = 0; k < np; k++) {
      if (poly[k][3] <= 0) { gb = 1; break; }
      double X = g.vs[0] * poly[k][0] / poly[k][3] + g.vo[0], Y = g.vs[1] * poly[k][1] / poly[k][3] + g.vo[1];
      if (X < 0 || X >= 4096 || Y < 0 || Y >= 4096) gb = 1;
      sx[k] = X - (2048 - 240); sy[k] = Y - (2048 - 136);
      if (sx[k] < x0) x0 = sx[k]; if (sx[k] > x1) x1 = sx[k]; if (sy[k] < y0) y0 = sy[k]; if (sy[k] > y1) y1 = sy[k];
    }
    if (gb) { xc_px[2]++; continue; }
    for (int k = 0; k < np; k++) area += sx[k] * sy[(k + 1) % np] - sx[(k + 1) % np] * sy[k];
    if (area == 0) { xc_px[0]++; continue; }
    if (x1 < xc_b.sx0 - 1 || x0 > xc_b.sx1 + 1 || y1 < xc_b.sy0 - 1 || y0 > xc_b.sy1 + 1) { xc_px[3]++; continue; }
    xc_px[4]++;
    { /* the GE's own numbers (df0chk's float24 model with the batch's viewport): the rows in 1/16 pixel, truncated and rounded */
      float W[12]; for (int k = 0; k < 12; k++) W[k] = f24(g.world[k]);
      int r[2][3];
      for (int k = 0; k < 3; k++) { const VcVert *q = &v[i - 2 + k]; float p[3] = { q->x / 32768.0f, q->y / 32768.0f, q->z / 32768.0f }, e[3];
        for (int j = 0; j < 3; j++) e[j] = p[0] * W[j] + p[1] * W[3 + j] + p[2] * W[6 + j] + W[9 + j];
        float Y = f24(g.vs[1]) * (e[1] / e[2]) + f24(g.vo[1]), y16 = (Y - (2048 - 136)) * 16.0f;
        r[0][k] = (int)floorf(y16); r[1][k] = (int)lrintf(y16);
        if (shown < 4) printf("xlconv   vertex %d: GE y %.5f px, 1/16 row %.4f\n", k, Y - (2048 - 136), y16); }
      for (int m = 0; m < 2; m++) {
        int lo = r[m][0], hi = r[m][0]; for (int k = 1; k < 3; k++) { if (r[m][k] < lo) lo = r[m][k]; if (r[m][k] > hi) hi = r[m][k]; }
        if (lo == hi) xc_px16[m]++;
        else if ((hi & ~7) >= lo) { xc_px16[2 + m]++;   /* a row of pixel centres or corners (a multiple of 8 sixteenths) inside its span */
          printf("xlconv vb %ld call %d df0 tri %d: GE rows %d..%d (%s) hold a sample row\n", g_vb, g_call, i, lo, hi, m ? "rounded" : "truncated"); } }
    }
    if (shown++ < 4) {
      printf("xlconv vb %ld call %d df0 tri %d may draw: area %.6g px2, box %.3f..%.3f x %.3f..%.3f, scissor %d..%d x %d..%d; model xyz",
             g_vb, g_call, i, area / 2, x0, x1, y0, y1, xc_b.sx0, xc_b.sx1, xc_b.sy0, xc_b.sy1);
      for (int k = 0; k < 3; k++) printf(" (%d %d %d)", v[i - 2 + k].x, v[i - 2 + k].y, v[i - 2 + k].z);
      printf("\n");
    }
  }
}
static void xc_report(void)
{
  const XcStats *S = xc_stats();
  fprintf(stderr, "xlconv:");
  for (int k = 0; k < 23; k++) if (XCN[k][0]) fprintf(stderr, "%s %s %ld", k ? ";" : "", XCN[k], xc_n[k]);
  fprintf(stderr, "\nxlconv converter: calls df0/25f0/2870 %ld/%ld/%ld, batches %ld/%ld/%ld, nothing drawn %ld/%ld/%ld, bad input %ld/%ld/%ld; effect triangles by class 0-4 %ld %ld %ld %ld %ld (2870 culled %ld);"
          " polygons %ld with %ld vertices, empty %ld, back-face culled %ld, no room %ld; df0 calls left out off the scissor (GSREF_XLCUT) %ld (%ld vertices)\n", S->calls[0], S->calls[1], S->calls[2], S->batches[0], S->batches[1], S->batches[2],
          S->empty[0], S->empty[1], S->empty[2], S->bad[0], S->bad[1], S->bad[2], S->tris[0], S->tris[1], S->tris[2], S->tris[3], S->tris[4], S->tris[5],
          S->polys, S->poly_v, S->poly_empty, S->poly_culled, S->poly_full, S->cut[0], S->cut_v[0]);
  fprintf(stderr, "xlconv P06 and P07:"); for (int k = 0; k < 7; k++) fprintf(stderr, "%s %s %ld", k ? ";" : "", XPN[k], xp_n[k]);
  fprintf(stderr, "\nxlconv P06 converter: calls +0/+358/+738/+ab0 %ld/%ld/%ld/%ld, batches %ld, nothing drawn %ld, bad input %ld; vertices %ld, exact form %ld;"
          " passes 2/3 drawn %ld/%ld (pass 2 with its own vertices %ld), not drawn %ld/%ld; cull bit %ld; light ramps %ld\n", S->lit_kind[0], S->lit_kind[1], S->lit_kind[2],
          S->lit_kind[3], S->batches[3], S->empty[3], S->bad[3], S->lit_v, S->lit_exact, S->lit_pass[2], S->lit_pass[3], S->lit_own2, S->lit_nopass[2], S->lit_nopass[3],
          S->lit_cull, S->lit_ramp);
  fprintf(stderr, "xlconv P07 converter: calls %ld, batches %ld, nothing drawn %ld, bad input %ld (paths not seen %ld); vertices %ld, calls in the exact form"
          " %ld; passes 2/3 drawn %ld/%ld (pass 2 with its own vertices %ld), not drawn %ld/%ld\n", S->calls[4], S->batches[4], S->empty[4], S->bad[4],
          S->face_unseen, S->face_v, S->face_exact, S->face_pass[2], S->face_pass[3], S->face_own2, S->face_nopass[2], S->face_nopass[3]);
  if (xc_cut) fprintf(stderr, "xlconv cut (GSREF_XLCUT): df0 calls the converter left out %ld (every frame), of them with pixels the PS2 wrote %ld (must be 0)\n", xc_cn[0], xc_cn[1]);
  fprintf(stderr, "xlconv converter-only df0 batches, their triangles on the GE:");
  for (int k = 0; k < 5; k++) fprintf(stderr, "%s %s %ld", k ? ";" : "", XCPX[k], xc_px[k]);
  fprintf(stderr, "; of the may-draw ones, on one 1/16-pixel row in the GE model: truncated %ld, rounded %ld; spanning a row of pixel samples: truncated %ld, rounded %ld\n", xc_px16[0], xc_px16[1], xc_px16[2], xc_px16[3]);
}

/* ================= df0 converter check (GSREF_DF0CHK=1): ge/vu1conv.c beside the VU1 interpreter (renderer_notes §11.2, check 2) ======
   On every P04+df0 call: the converter reads the VU1 memory as the call starts; a model of the GE's transform (float24 matrices and
   viewport, float math, fog from the view z) gives each vertex's screen x / y, depth and fog, compared with what the micro program
   kicked; the strips are checked against the kicked ADC bits, and every triangle is classed by the PS2's clip test against what the GE
   does instead (near plane only, the guard band 0..4096 drops a whole primitive, nothing clipped at the far plane). */
#define DF0_ENTRY 0x1991d8u
static int dc_chk;
static uint8_t dc_vm[16384]; static uint32_t dc_top;
static double dc_max[8]; static long dc_maxvb[8]; static long dc_tot[24];
static const char *DCN[24] = { "calls", "vertices", "compared", "w<=0", "tris drawn by the data", "tris inside", "tris trivially rejected x/y",
  "tris trivially rejected near", "tris trivially rejected far only (GE draws them)", "tris clipped x/y only", "tris crossing near",
  "tris crossing far (GE draws past it)", "tris over the guard band (GE drops them)", "GE strips", "GE strip vertices", "kicked fan vertices",
  "adc lost", "adc set without a clip reason", "clip reason without adc", "count mismatch", "no-clip path calls", "bad input", "colour diff", "uv diff" };
static const char *DCM[8] = { "x px", "y px", "z16", "fog", "uv texels", "matrix resid", "", "" };
static float f24(float f) { uint32_t u; memcpy(&u, &f, 4); u &= 0xffffff00u; memcpy(&f, &u, 4); return f; }
static int dc_uv[4] = { 1 << 20, -(1 << 20), 1 << 20, -(1 << 20) }; static long dc_neguv;
static void dc_note(int k, double e) { if (e > dc_max[k]) { dc_max[k] = e; dc_maxvb[k] = g_vb; } }
static void dc_begin(int kind)
{
  memcpy(dc_vm, VM, sizeof dc_vm); dc_top = vu_top; dc_on = 1; dc_kind = kind; dc_tagn = 0; dc_nr = 0; dc_have = 0; dc_px = 0;
}
static void fp_end(void);
static void dc_check(const VcXform *xp, const VcVert *v, const uint16_t *fl, int n);
static void dc_export(const VcXform *x, const VcVert *v, const uint16_t *fl, int n);
/* the PS2 clip flags of a clip-space point: +x -x +y -y +z (near) -z (far) against |w| (VU CLIP) */
static int dc_cf(const double *c) { double w = fabs(c[3]); return (c[0] > w) | (c[0] < -w) << 1 | (c[1] > w) << 2 | (c[1] < -w) << 3 | (c[2] > w) << 4 | (c[2] < -w) << 5; }
static void dc_end(void)
{
  dc_on = 0;
  if (dc_kind) { fp_end(); return; }
  static VcVert v[1024]; static uint16_t fl[1024]; VcXform x;
  int n = vc_df0(dc_vm, dc_top, &x, v, fl, 1024);
  if (n < 0) { dc_tot[21]++; count(1, "!df0 converter: bad input (%d)", n); return; }
  if (dc_chk) dc_check(&x, v, fl, n);
  if (ge_on && dc_have) dc_export(&x, v, fl, n);
  else if (xc_on && xc_cutnow) {   /* GSREF_XLCUT outside the export (xc_take checks it there): the PS2 must have drawn no pixel */
    xc_cn[0]++;
    if (dc_px) { xc_cn[1]++; if (xc_shown < 24) { xc_shown++; printf("xlconv vb %ld call %d entry %06x: LEFT OUT off the scissor, but the PS2 wrote %ld pixels from it\n", g_vb, g_call, g_entry, (long)dc_px); } }
  }
}
static void dc_check(const VcXform *xp, const VcVert *v, const uint16_t *fl, int n)
{
  static uint16_t runs[2048]; VcXform x = *xp;
  dc_tot[0]++;
  dc_tot[1] += n; if (!x.clip) dc_tot[20]++;
  for (int i = 0; i < n; i++) { int u = (int16_t)v[i].u, w = (int16_t)v[i].v; if (u < dc_uv[0]) dc_uv[0] = u; if (u > dc_uv[1]) dc_uv[1] = u; if (w < dc_uv[2]) dc_uv[2] = w; if (w > dc_uv[3]) dc_uv[3] = w;
    if (u < 0 || w < 0) dc_neguv++; }
  dc_note(5, x.resid);
  /* the kicked strip: the first n vertices with PRIM tri-strip; fan vertices after it come from the clipper */
  int ns = 0, nf = 0; const DcRec *sv[1024];
  for (int i = 0; i < dc_nr; i++) { if (dc_rec[i].prim == 4 && ns < n) sv[ns++] = &dc_rec[i]; else if (dc_rec[i].prim == 5) nf++; }
  dc_tot[15] += nf;
  if (ns != n) { dc_tot[19]++; count(1, "!df0chk %d vertices, %d kicked in the strip", n, ns); return; }
  /* GE model: float24 world / projection / viewport, float math */
  float W[12], A = f24(x.pa), B = f24(x.pb), fog1 = f24(x.fog_b / x.fog_a), fog2 = f24(x.fog_a / 255.0f);
  for (int k = 0; k < 12; k++) W[k] = f24(x.world[k]);
  float vsx = f24(x.vs[0]), vox = f24(x.vo[0]), vsy = f24(x.vs[1]), voy = f24(x.vo[1]), vsz = f24(-x.vs[2] / 256.0f), voz = f24(x.vo[2] / 256.0f);
  int tw = 1 << (gr[0x06] >> 26 & 15), th = 1 << (gr[0x06] >> 30 & 15);
  double ofx = (gr[0x18] & 0xffff) / 16.0, ofy = (gr[0x18] >> 32 & 0xffff) / 16.0;
  static double cc[1024][4]; static int cf[1024];
  for (int i = 0; i < n; i++) {
    float p[3] = { v[i].x / 32768.0f, v[i].y / 32768.0f, v[i].z / 32768.0f }, e[3];
    for (int j = 0; j < 3; j++) e[j] = p[0] * W[j] + p[1] * W[3 + j] + p[2] * W[6 + j] + W[9 + j];
    float w = e[2], z = A * w - B;
    for (int c = 0; c < 4; c++) cc[i][c] = (double)x.m[c] * v[i].x + (double)x.m[4 + c] * v[i].y + (double)x.m[8 + c] * v[i].z + x.m[12 + c];
    cf[i] = dc_cf(cc[i]);
    const Vtx *k = &sv[i]->v;
    /* colour and UV need no transform: compare every vertex */
    if ((k->r | k->g << 8 | k->b << 16) != (int)(v[i].c & 0xffffff) || (k->a >= 0x80 ? 0xffu : (uint32_t)k->a * 2) != v[i].c >> 24) dc_tot[22]++;
    if (w <= 0) { dc_tot[3]++; continue; }
    if (cf[i]) continue;   /* outside the clip volume: the GS never gets this vertex unclipped (its X / Y can wrap) */
    dc_tot[2]++;
    float X = vsx * (e[0] / w) + vox, Y = vsy * (e[1] / w) + voy, Z = vsz * (z / w) + voz, F = (w + fog1) * fog2;
    F = (F < 0 ? 0 : F > 1 ? 1 : F) * 255.0f;
    dc_note(0, fabs(X - k->x / 16.0)); dc_note(1, fabs(Y - k->y / 16.0)); dc_note(2, fabs(Z - k->z / 256.0)); dc_note(3, fabs(F - k->f));
    if (k->q != 0) { double du = fabs(v[i].u / 32768.0 * 8 - k->s / k->q) * tw, dv = fabs(v[i].v / 32768.0 * 8 - k->t / k->q) * th;
      dc_note(4, du > dv ? du : dv); if (du > 0.01 || dv > 0.01) dc_tot[23]++; }
  }
  /* triangles */
  for (int i = 2; i < n; i++) {
    int adc_d = fl[i] >> 15 & 1, adc_k = sv[i]->adc, o = cf[i - 2] | cf[i - 1] | cf[i], a = cf[i - 2] & cf[i - 1] & cf[i];
    if (adc_d) { if (!adc_k) dc_tot[16]++; continue; }
    dc_tot[4]++;
    if (adc_k && !o) dc_tot[17]++;
    if (!adc_k && o) dc_tot[18]++;
    if (!o) { dc_tot[5]++; continue; }
    if (a) { dc_tot[(a & 15) ? 6 : (a & 16) ? 7 : 8]++; continue; }
    if (!(o & 0x30)) dc_tot[9]++;
    if (o & 16) dc_tot[10]++;
    if (o & 32) dc_tot[11]++;
    /* the GE's near clip (plane z = w, the PS2's +z), then the guard band on what is left, in the replay's screen (512 -> 480 x 272) */
    double poly[8][4]; int np = 0, gb = 0;
    for (int k = 0; k < 3; k++) {
      const double *P = cc[i - 2 + k], *Q = cc[i - 2 + (k + 1) % 3];
      double dp = P[3] - P[2], dq = Q[3] - Q[2];
      if (dp >= 0) memcpy(poly[np++], P, sizeof poly[0]);
      if ((dp >= 0) != (dq >= 0)) { double t = dp / (dp - dq); for (int c = 0; c < 4; c++) poly[np][c] = P[c] + t * (Q[c] - P[c]); np++; }
    }
    for (int k = 0; k < np; k++) {
      double sx = x.vs[0] * poly[k][0] / poly[k][3] + x.vo[0] - ofx, sy = x.vs[1] * poly[k][1] / poly[k][3] + x.vo[1] - ofy;
      double gx = 2048 - 240 + sx * 480.0 / 512.0, gy = 2048 - 136 + sy * 272.0 / 512.0;
      if (poly[k][3] <= 0 || gx < 0 || gx >= 4096 || gy < 0 || gy >= 4096) gb = 1;
    }
    if (gb) dc_tot[12]++;
  }
  int nst = vc_strips(fl, n, runs, 1024); dc_tot[13] += nst; for (int k = 0; k < nst; k++) dc_tot[14] += runs[2 * k + 1];
}
/* the call as a GERK_XFORM batch after its own (GERK_THROUGH) primitives; the current through batch stays current */
static void dc_export(const VcXform *x, const VcVert *v, const uint16_t *fl, int n)
{
  static uint16_t runs[2048], idx[4096]; static uint8_t blob[sizeof(GerXform) + 1024 * sizeof(VcVert) + 4096 + 8192];
  int nst = vc_strips(fl, n, runs, 1024), ni = 0;
  for (int k = 0; k < nst && !ge_live; k++) {
    if (k) { idx[ni] = idx[ni - 1]; ni++; idx[ni++] = runs[2 * k]; }
    for (int j = 0; j < runs[2 * k + 1]; j++) idx[ni++] = (uint16_t)(runs[2 * k] + j);
  }
  GerXform gx; memset(&gx, 0, sizeof gx);
  memcpy(gx.world, x->world, sizeof gx.world);
  gx.proj[0] = 1; gx.proj[5] = 1; gx.proj[10] = x->pa; gx.proj[11] = 1; gx.proj[14] = -x->pb;
  double ofx = dc_ofx / 16.0, ofy = dc_ofy / 16.0;
  gx.vs[0] = (float)(x->vs[0] * 480.0 / 512.0); gx.vo[0] = (float)(2048 - 240 + (x->vo[0] - ofx) * 480.0 / 512.0);
  gx.vs[1] = (float)(x->vs[1] * 272.0 / 512.0); gx.vo[1] = (float)(2048 - 136 + (x->vo[1] - ofy) * 272.0 / 512.0);
  gx.vs[2] = (float)(-x->vs[2] * ge_zk); gx.vo[2] = (float)(x->vo[2] * ge_zk);
  gx.fog1 = x->fog_b / x->fog_a; gx.fog2 = x->fog_a / 255.0f; gx.fogcol = (uint32_t)(gr[0x3d] & 0xffffff);
  gx.su = gx.sv = 1; gx.tu = gx.tv = 0;
  if (dc_ti >= 0 && G.t[dc_ti].kind == 1) {   /* PS2 u = 8 u16 / 32768 in PS2 texture widths; GE u in GE texture widths from the region's origin */
    double TW = 1 << dc_tw2, TH = 1 << dc_th2, tw = G.t[dc_ti].tw, th = G.t[dc_ti].th;
    gx.su = (float)(8 * TW / tw); gx.tu = (float)(-G.u0[dc_ti] / tw);
    gx.sv = (float)(8 * TH / th); gx.tv = (float)(-G.v0[dc_ti] / th);
  } else if (dc_ti >= 0) count(1, "!ge xform batch with a frame-buffer texture");
  gx.nv = n; gx.nst = nst; gx.nidx = ni;
  size_t o = 0;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, n * sizeof *v); o += n * sizeof *v;
  memcpy(blob + o, runs, nst * 4); o += nst * 4;
  memcpy(blob + o, idx, ni * 2); o += ni * 2;
  GerBatch b = dc_state; b.kind = GERK_XFORM; b.prim = 4; b.twopass = (uint8_t)dc_twop; b.count = nst; b.px = (uint32_t)dc_px;
  const uint8_t *bp = blob; uint32_t bs = (uint32_t)o;
  if (xc_on && !xc_take(&b, &bp, &bs)) return;
  gb_xform(&G, &b, bp, bs); dc_nx++; dc_nxv += n;
}
static void dc_report(void)
{
  fprintf(stderr, "df0chk:");
  for (int k = 0; k < 24; k++) fprintf(stderr, "%s %s %ld", k ? ";" : "", DCN[k], dc_tot[k]);
  fprintf(stderr, "\ndf0chk UV (4.12) u %d..%d v %d..%d, vertices with a negative u or v %ld", dc_uv[0], dc_uv[1], dc_uv[2], dc_uv[3], dc_neguv);
  fprintf(stderr, "\ndf0chk max differences (GE model - micro program):");
  for (int k = 0; k < 6; k++) fprintf(stderr, "%s %s %.4g (vb %ld)", k ? ";" : "", DCM[k], dc_max[k], dc_maxvb[k]);
  fprintf(stderr, "\n");
}

/* ================= fpos converter check (GSREF_DF0CHK=1 too): P04+25f0 / +2870 (float world positions) and the PS2 clipper port =======
   ge/vu1conv.c vc_fpos beside the VU1 interpreter: each vertex inside the clip volume as the GE would transform it (float24 matrices, as
   df0chk) against the strip the micro program kicked; each triangle by what the PS2 does (strip, trivially rejected, clipper) and what
   the GE would do with it under hardware transform (PPSSPP 1.20.4 GPU/Software Clipper.cpp ProcessTriangle and TransformUnit.cpp
   ClipToScreen, depth clamp on: a vertex outside the 0..4096 screen range drops the triangle; the GE clips at the near plane only;
   past the far plane it draws at the clamped depth); and vc_clip_tri, fed the clip positions, colours and UVs the micro program stored
   back into its input, against the fans the clipper kicked (same order, same vertices). */
#define FP_ENTRY 0x19a9f0u    /* P04+25f0 */
#define FP2_ENTRY 0x19ac78u   /* P04+2870 */
static long fp_tot[40]; static double fp_max[12]; static long fp_maxvb[12]; static int fp_uv[4] = { 1 << 30, -(1 << 30), 1 << 30, -(1 << 30) }, fp_amax, fp_nmax;
static const char *FPN[40] = { "calls", "calls of 2870", "vertices", "compared", "bad input", "count mismatch", "colour diff", "uv diff",
  "tris drawn by the data", "tris the strip draws", "tris trivially rejected", "tris to the clipper", "tris the clipper drops (all outside)",
  "clipper tris, GE draws them the same (x/y only, in range)", "clipper tris, GE drops them (a vertex out of the GE range)",
  "clipper tris, GE clips them at the near plane", "clipper tris, GE draws past the far plane", "clip flag disagreements (model vs VU)",
  "PS2 fans", "fans compared", "fan count mismatch (polygons vs fans)", "fan vertex count diff", "fan colour diff > 1", "back-face culled fans",
  "fog calls", "calls with a clipped triangle", "calls all GE-safe", "fan vertices", "zero-area PS2 fans after a plane emptied the polygon", "depth window: tris inside", "depth window: tris all nearer", "depth window: tris all farther", "depth window: tris across its edge", "calls without a depth window", "", "", "", "", "", "" };
static const char *FPM[12] = { "x px", "y px", "z16", "fog", "uv texels", "matrix resid", "fan x px", "fan y px", "fan z24", "fan fog", "fan uv texels", "fan colour" };
static void fp_note(int k, double e) { if (e > fp_max[k]) { fp_max[k] = e; fp_maxvb[k] = g_vb; } }
static float fp_rd(uint32_t qa, int k) { float f; memcpy(&f, VM + ((qa & 0x3ff) << 4) + 4 * k, 4); return f; }   /* the VU memory after the call */
static uint32_t fp_rdi(uint32_t qa, int k) { uint32_t u; memcpy(&u, VM + ((qa & 0x3ff) << 4) + 4 * k, 4); return u; }
/* the GE's outcome for a triangle with these PS2 clip coordinates under the replay's viewport (480 x 272 at 2048 - 240, 2048 - 136):
   0 drawn as the PS2 draws it, 1 dropped (a vertex outside the GE's 0..4096 range), 2 clipped by the GE at the near plane, 3 drawn past
   the far plane; 4 nothing drawn (all w < 0, or all three out on one side in z) */
static int ge_outcome(double cc[3][4], const VcXform *x)
{
  double ofx = (gr[0x18] & 0xffff) / 16.0, ofy = (gr[0x18] >> 32 & 0xffff) / 16.0;
  double xs = x->vs[0] * 480.0 / 512.0, xc = 2048 - 240 + (x->vo[0] - ofx) * 480.0 / 512.0;
  double ys = x->vs[1] * 272.0 / 512.0, yc = 2048 - 136 + (x->vo[1] - ofy) * 272.0 / 512.0;
  const double B = 4095.0 + 15.5 / 16.0;
  int neg = 0, pos = 0, nw = 0, near = 0, far = 0;
  for (int k = 0; k < 3; k++) {
    double w = cc[k][3], sx = cc[k][0] * xs / w + xc, sy = cc[k][1] * ys / w + yc, zg = -cc[k][2];   /* GE z = minus the PS2's clip z */
    if (!(sx >= 0 && sx < B && sy >= 0 && sy < B)) return 1;
    if (w < 0) nw++;
    if (zg / w >= 1.000030517578125) pos++; if (-zg / w >= 1.000030517578125) neg++;
    if (zg < -w) near = 1;
    if (zg > w) far = 1;
  }
  if (nw == 3 || pos == 3 || neg == 3) return 4;
  if (far) return 3;
  if (near) {   /* the GE clips at its z = -w and drops a part with a new vertex outside the range */
    for (int k = 0; k < 3; k++) { const double *P = cc[k], *Q = cc[(k + 1) % 3];
      double dp = P[3] - P[2], dq = Q[3] - Q[2];
      if ((dp >= 0) != (dq >= 0)) { double t = dp / (dp - dq), c[4]; for (int j = 0; j < 4; j++) c[j] = P[j] + t * (Q[j] - P[j]);
        double sx = c[0] * xs / c[3] + xc, sy = c[1] * ys / c[3] + yc; if (!(sx >= 0 && sx < B && sy >= 0 && sy < B)) return 1; } }
    return 2;
  }
  return 0;
}
/* the call as a GERK_XFORMF batch (ge/replay_fmt.h) after its own (GERK_THROUGH) primitives */
static void fp_export(const VcXform *x, const VcVertF *v, int n, const uint8_t *cls, const VcVertF *pv, int npv, uint16_t (*pfan)[2], int npf)
{
  static uint8_t blob[sizeof(GerXform) + (1024 + 8192) * sizeof(VcVertF) + 1024 + 4096];
  GerXform gx; memset(&gx, 0, sizeof gx);
  memcpy(gx.world, x->world, sizeof gx.world);
  gx.proj[0] = 1; gx.proj[5] = 1; gx.proj[10] = x->pa; gx.proj[11] = 1; gx.proj[14] = -x->pb;
  double ofx = dc_ofx / 16.0, ofy = dc_ofy / 16.0;
  gx.vs[0] = (float)(x->vs[0] * 480.0 / 512.0); gx.vo[0] = (float)(2048 - 240 + (x->vo[0] - ofx) * 480.0 / 512.0);
  gx.vs[1] = (float)(x->vs[1] * 272.0 / 512.0); gx.vo[1] = (float)(2048 - 136 + (x->vo[1] - ofy) * 272.0 / 512.0);
  gx.vs[2] = (float)(-x->vs[2] * ge_zk); gx.vo[2] = (float)(x->vo[2] * ge_zk);
  gx.fog1 = x->fog_b / x->fog_a; gx.fog2 = x->fog_a / 255.0f; gx.fogcol = (uint32_t)(gr[0x3d] & 0xffffff);
  gx.su = gx.sv = 1; gx.tu = gx.tv = 0;
  if (dc_ti >= 0 && G.t[dc_ti].kind == 1) {   /* u in PS2 texture widths; GE u in GE texture widths from the region's origin */
    double TW = 1 << dc_tw2, TH = 1 << dc_th2, tw = G.t[dc_ti].tw, th = G.t[dc_ti].th;
    gx.su = (float)(TW / tw); gx.tu = (float)(-G.u0[dc_ti] / tw);
    gx.sv = (float)(TH / th); gx.tv = (float)(-G.v0[dc_ti] / th);
  } else if (dc_ti >= 0) count(1, "!ge xform batch with a frame-buffer texture");
  gx.nv = n; gx.nfan = npf; gx.nfv = npv;
  size_t o = 0;
  memcpy(blob + o, &gx, sizeof gx); o += sizeof gx;
  memcpy(blob + o, v, n * sizeof *v); o += n * sizeof *v;
  memcpy(blob + o, pv, npv * sizeof *pv); o += npv * sizeof *pv;
  memcpy(blob + o, cls, n); memset(blob + o + n, 0, ((n + 3) & ~3) - n); o += (n + 3) & ~3;
  memcpy(blob + o, pfan, npf * 4); o += npf * 4;
  GerBatch b = dc_state; b.kind = GERK_XFORMF; b.prim = 4; b.twopass = (uint8_t)dc_twop; b.count = n; b.px = (uint32_t)dc_px;
  const uint8_t *bp = blob; uint32_t bs = (uint32_t)o;
  if (xc_on && !xc_take(&b, &bp, &bs)) return;
  gb_xform(&G, &b, bp, bs); fp_nx++; fp_nxv += n; fp_nxp += npv;
}
static void fp_end(void)
{
  static VcVertF v[1024]; static uint16_t fl[1024]; VcXform x;
  int r = vc_fpos(dc_vm, dc_top, &x, v, fl, 1024), n = r;
  fp_tot[0]++; if (dc_kind == 2) fp_tot[1]++;
  if (r == -4) { fp_tot[4]++; count(1, "!fpos converter: colour word above 255 or UV z not 1.0"); memcpy(&n, dc_vm + ((dc_top + 0x14) & 0x3ff) * 16, 4); n &= 0xffff;
    static int shown; if (getenv("GSREF_FPDBG") && shown++ < 6) for (int i = 0; i < n; i++) {
      const uint32_t *t = (const uint32_t *)(dc_vm + ((dc_top + 0x102 + 3 * i) & 0x3ff) * 16), *c = (const uint32_t *)(dc_vm + ((dc_top + 0x101 + 3 * i) & 0x3ff) * 16), *q = (const uint32_t *)(dc_vm + ((dc_top + 0x100 + 3 * i) & 0x3ff) * 16);
      if (c[0] > 255 || c[1] > 255 || c[2] > 255 || c[3] > 255 || t[2] != 0x1000) { printf("fpdbg vb %ld call %d kind %d n %d v %d pos %08x %08x %08x %08x col %08x %08x %08x %08x uv %08x %08x %08x %08x\n", g_vb, g_call, dc_kind, n, i, q[0], q[1], q[2], q[3], c[0], c[1], c[2], c[3], t[0], t[1], t[2], t[3]); break; } } }
  if (n < 0) { fp_tot[4]++; count(1, "!fpos converter: bad input (%d)", r); return; }
  fp_tot[2] += n; if (n > fp_nmax) fp_nmax = n;
  fp_note(5, x.resid);
  for (int i = 0; i < n; i++) {
    const uint32_t *t = (const uint32_t *)(dc_vm + ((dc_top + 0x102 + 3 * i) & 0x3ff) * 16), *c = (const uint32_t *)(dc_vm + ((dc_top + 0x101 + 3 * i) & 0x3ff) * 16);
    int u = (int32_t)t[0], w = (int32_t)t[1];
    if (u < fp_uv[0]) fp_uv[0] = u; if (u > fp_uv[1]) fp_uv[1] = u; if (w < fp_uv[2]) fp_uv[2] = w; if (w > fp_uv[3]) fp_uv[3] = w;
    if ((int)(c[3] & 0xff) > fp_amax) fp_amax = (int)(c[3] & 0xff);
  }
  if (gr[0] >> 5 & 1) fp_tot[24]++;
  { const uint32_t *t5 = (const uint32_t *)(dc_vm + ((dc_top + 0x15) & 0x3ff) * 16), *t6 = (const uint32_t *)(dc_vm + ((dc_top + 0x16) & 0x3ff) * 16);
    count(1, "fpos entry %d templates strip PRIM %03x pre %d / fan PRIM %03x pre %d", dc_kind, t5[1] >> 15 & 0x7ff, t5[1] >> 14 & 1, t6[1] >> 15 & 0x7ff, t6[1] >> 14 & 1); }
  /* the kicked strip and fans */
  int ns = 0; const DcRec *sv[1024]; int fan0[512], fann[512], nfan = 0;
  for (int i = 0; i < dc_nr; i++) {
    if (dc_rec[i].prim == 4 && ns < n) sv[ns++] = &dc_rec[i];
    else if (dc_rec[i].prim == 5) { if (!nfan || dc_rec[i].tag != dc_rec[fan0[nfan - 1]].tag) { if (nfan == 512) break; fan0[nfan] = i; fann[nfan++] = 0; } fann[nfan - 1]++; }
  }
  fp_tot[18] += nfan;
  if (ns != n) { fp_tot[5]++; count(1, "!fpchk %d vertices, %d kicked in the strip", n, ns); return; }
  if (xc_on && xc_tr.n == n) { int any = 0;   /* GSREF_XLCONV: the converter's strip triangles (class 1) against the ADC bits the program kicked */
    for (int i = 2; i < n; i++) { xc_n[11]++; if ((xc_tr.cls[i] == 1) != !sv[i]->adc) { xc_n[12]++; if (xc_shown < 24) { xc_shown++;
      printf("xlconv vb %ld call %d entry %06x triangle %d: converter class %d, kicked ADC %d\n", g_vb, g_call, g_entry, i, xc_tr.cls[i], sv[i]->adc); } }
      if (!sv[i]->adc) any = 1; }
    if (dc_kind == 2 && any) xc_n[19]++; }
  /* GE model: float24 world / projection / viewport, float math (as df0chk) */
  float W[12], A = f24(x.pa), B = f24(x.pb), fog1 = f24(x.fog_b / x.fog_a), fog2 = f24(x.fog_a / 255.0f);
  for (int k = 0; k < 12; k++) W[k] = f24(x.world[k]);
  float vsx = f24(x.vs[0]), vox = f24(x.vo[0]), vsy = f24(x.vs[1]), voy = f24(x.vo[1]), vsz = f24(-x.vs[2] / 256.0f), voz = f24(x.vo[2] / 256.0f);
  int tw = 1 << (gr[0x06] >> 26 & 15), th = 1 << (gr[0x06] >> 30 & 15);
  static double cc[1024][4]; static int cf[1024];
  for (int i = 0; i < n; i++) {
    float p[3] = { v[i].x, v[i].y, v[i].z }, e[3];
    for (int j = 0; j < 3; j++) e[j] = p[0] * W[j] + p[1] * W[3 + j] + p[2] * W[6 + j] + W[9 + j];
    float w = e[2], z = A * w - B;
    for (int c = 0; c < 4; c++) cc[i][c] = (double)x.m[c] * p[0] + (double)x.m[4 + c] * p[1] + (double)x.m[8 + c] * p[2] + x.m[12 + c];
    cf[i] = dc_cf(cc[i]);
    float vc[4]; for (int c = 0; c < 4; c++) vc[c] = fp_rd(dc_top + 0x100 + 3 * i, c);
    if (vc_cflags(vc) != cf[i]) fp_tot[17]++;
    const Vtx *k = &sv[i]->v;
    if ((k->r | k->g << 8 | k->b << 16) != (int)(v[i].c & 0xffffff) || (k->a >= 0x80 ? 0xffu : (uint32_t)k->a * 2) != v[i].c >> 24) fp_tot[6]++;
    if (w <= 0 || cf[i]) continue;
    fp_tot[3]++;
    float X = vsx * (e[0] / w) + vox, Y = vsy * (e[1] / w) + voy, Z = vsz * (z / w) + voz, F = (w + fog1) * fog2;
    F = (F < 0 ? 0 : F > 1 ? 1 : F) * 255.0f;
    fp_note(0, fabs(X - k->x / 16.0)); fp_note(1, fabs(Y - k->y / 16.0)); fp_note(2, fabs(Z - k->z / 256.0)); fp_note(3, fabs(F - k->f));
    if (k->q != 0) { double du = fabs(v[i].u - k->s / k->q) * tw, dv = fabs(v[i].v - k->t / k->q) * th;
      fp_note(4, du > dv ? du : dv); if (du > 0.01 || dv > 0.01) fp_tot[7]++; }
  }
  /* the frame's 16-bit depth window (renderer_notes 11.8, GSREF_ZMAP): where the GE would clamp each vertex's depth */
  double zlo = -1, zhi = -1;
  for (int k = 0; k < zm_n; k++) if (zm_t[k][0] == (uint32_t)g_vb && zm_t[k][1] == (uint32_t)(gr[0x4e] & 0x1ff) << 5) {   /* ZBUF_1 ZBP (pages) -> zbp (blocks) as zm_t holds it */
    double m = (zm_t[k][3] - (double)zm_t[k][2]) / 16; zlo = zm_t[k][2] - m; zhi = zm_t[k][3] + m; if (zlo < 0) zlo = 0; }
  if (zm_n && zlo < 0) fp_tot[33]++;
  if (ge_on) { zlo = 0; zhi = 65535.0 / ge_zk; }   /* the replay file's depth mapping: GE depth = Z24 x ge_zk */
  /* triangles; the clipper's polygons against its fans */
  int fi = 0, any = 0, unsafe = 0;
  static uint8_t cls[1024]; static VcVertF pv[8192]; static uint16_t pfan[1024][2]; int npf = 0, npv = 0;   /* the export's triangle classes and polygons */
  memset(cls, 0, n);
  if (zlo >= 0) for (int i = 2; i < n; i++) {
    if (fl[i] >> 15 & 1) continue;
    if (cf[i - 2] & cf[i - 1] & cf[i]) continue;
    int above = 0, below = 0, inr = 0, bad = 0;
    for (int k = 0; k < 3; k++) { const double *c = cc[i - 2 + k]; if (c[3] <= 0) { bad = 1; break; }
      double Z = x.vs[2] * c[2] / c[3] + x.vo[2]; if (Z > zhi) above++; else if (Z < zlo) below++; else inr++; }
    if (bad) continue;
    fp_tot[inr == 3 ? 29 : above == 3 ? 30 : below == 3 ? 31 : 32]++;
  }
  const float *K2 = (const float *)(dc_vm + 0x02 * 16);
  for (int i = 2; i < n; i++) {
    if (fl[i] >> 15 & 1) continue;
    fp_tot[8]++;
    uint32_t post = fp_rdi(dc_top + 0x102 + 3 * i, 3) & 0xffff;
    if (!(post >> 14 & 1)) { fp_tot[9]++; cls[i] = 1; continue; }
    any = 1;
    double t3[3][4]; for (int k = 0; k < 3; k++) memcpy(t3[k], cc[i - 2 + k], sizeof t3[k]);
    if (cf[i - 2] & cf[i - 1] & cf[i]) { fp_tot[10]++; cls[i] = 2; if (getenv("GSREF_FPDBG") && g_vb == 24056 && g_call == 2422) printf("fpdbg seq tri %d trivially rejected\n", i); continue; }
    fp_tot[11]++;
    int o = ge_outcome(t3, &x); if (o == 1 || o == 3) unsafe = 1;
    int across = 0;   /* across the frame's depth window: the GE's per-vertex depth clamp would bend the depth inside the triangle */
    if (zlo >= 0) for (int k = 0; k < 3; k++) { const double *c = t3[k]; double Z = c[3] > 0 ? x.vs[2] * c[2] / c[3] + x.vo[2] : 0; if (c[3] <= 0 || Z > zhi || Z < zlo) across = 1; }
    cls[i] = o == 0 && !across && (post & 0x30) == 0x30 ? 3 : 4;
    fp_tot[o == 0 ? 13 : o == 1 ? 14 : o == 2 ? 15 : o == 3 ? 16 : 12]++;
    VcCv in[3], out[16];
    for (int k = 0; k < 3; k++) { int j = i - 2 + k;
      for (int c = 0; c < 4; c++) { in[k].c[c] = fp_rd(dc_top + 0x100 + 3 * j, c); in[k].col[c] = fp_rd(dc_top + 0x101 + 3 * j, c); }
      for (int c = 0; c < 3; c++) in[k].uv[c] = fp_rd(dc_top + 0x102 + 3 * j, c);
      in[k].p[0] = v[j].x; in[k].p[1] = v[j].y; in[k].p[2] = v[j].z; }
    int m = vc_clip_tri(in, out);
    if (getenv("GSREF_FPDBG") && g_vb == 24056 && g_call == 2422) printf("fpdbg seq tri %d post %04x flags %02x %02x %02x polygon %d (next fan %d: %d vertices)\n", i, post, cf[i - 2], cf[i - 1], cf[i], m, fi, fi < nfan ? fann[fi] : -1);
    if (m < 3) { fp_tot[12]++;
      /* the PS2's plane routines go on after a plane empties the polygon and clip stale buffer data: a fan of points on one segment */
      if (fi < nfan) { double area = 0; const Vtx *a0 = &dc_rec[fan0[fi]].v;
        for (int k = 2; k < fann[fi]; k++) { const Vtx *b = &dc_rec[fan0[fi] + k - 1].v, *c = &dc_rec[fan0[fi] + k].v;
          area += fabs((double)(b->x - a0->x) * (c->y - a0->y) - (double)(c->x - a0->x) * (b->y - a0->y)); }
        if (area == 0) { fi++; fp_tot[28]++; } }
      continue; }
    /* the clipper's back-face test on the polygon's first three vertices (re/vu1_programs.md §2.3) */
    float e1[2], e2[2];
    for (int c = 0; c < 2; c++) { e1[c] = out[1].c[c] * out[0].c[3] - out[0].c[c] * out[1].c[3]; e2[c] = out[2].c[c] * out[1].c[3] - out[1].c[c] * out[2].c[3]; }
    int sgn = (e1[0] * e2[1] - e2[0] * e1[1]) < 0 ? 0x20 : 0;
    if ((int)(post & 0x30) == sgn) { fp_tot[23]++; continue; }
    if (npf < 1024 && npv + m <= 8192) {   /* the polygon for the export: world position, the colour the PS2 draws, UV */
      pfan[npf][0] = (uint16_t)i; pfan[npf][1] = (uint16_t)m; npf++;
      for (int k = 0; k < m; k++) { const VcCv *P = &out[k]; VcVertF *o2 = &pv[npv++]; float Q = 1.0f / P->c[3]; int cv[4];
        for (int c = 0; c < 4; c++) { cv[c] = (int)(P->col[c] * Q + 0.0005f); cv[c] = cv[c] < 0 ? 0 : cv[c] > 255 ? 255 : cv[c]; }
        o2->u = P->uv[0]; o2->v = P->uv[1]; o2->x = P->p[0]; o2->y = P->p[1]; o2->z = P->p[2];
        o2->c = (uint32_t)cv[0] | (uint32_t)cv[1] << 8 | (uint32_t)cv[2] << 16 | (uint32_t)(cv[3] >= 0x80 ? 0xff : cv[3] * 2) << 24; } }
    else count(1, "!fpos export: polygon buffer full");
    if (fi >= nfan) { fp_tot[20]++; continue; }
    int f0 = fan0[fi], fn = fann[fi]; fi++;
    fp_tot[19]++;
    if (fn != m) { fp_tot[21]++;
      if (getenv("GSREF_FPDBG")) { printf("fpdbg fan vb %ld call %d tri %d: polygon %d vertices, fan %d (fan %d of %d)\n", g_vb, g_call, i, m, fn, fi - 1, nfan);
        for (int k = 0; k < 3; k++) printf("  in %d c %g %g %g %g flags %02x\n", k, in[k].c[0], in[k].c[1], in[k].c[2], in[k].c[3], vc_cflags(in[k].c));
        for (int k = 0; k < m; k++) printf("  out %d c %g %g %g %g -> X %.2f Y %.2f\n", k, out[k].c[0], out[k].c[1], out[k].c[2], out[k].c[3], out[k].c[0] / out[k].c[3] * x.vs[0] + x.vo[0], out[k].c[1] / out[k].c[3] * x.vs[1] + x.vo[1]);
        for (int k = 0; k < fn; k++) printf("  ps2 %d X %.2f Y %.2f z %u\n", k, dc_rec[f0 + k].v.x / 16.0, dc_rec[f0 + k].v.y / 16.0, dc_rec[f0 + k].v.z); }
      continue; }
    fp_tot[27] += m;
    for (int k = 0; k < m; k++) {
      const Vtx *q = &dc_rec[f0 + k].v; const VcCv *P = &out[k];
      float Q = 1.0f / P->c[3], F = K2[0] * P->c[3] + K2[1]; F = F < 0 ? 0 : F > 255 ? 255 : F;
      double X = P->c[0] * Q * x.vs[0] + x.vo[0], Y = P->c[1] * Q * x.vs[1] + x.vo[1], Z = P->c[2] * Q * x.vs[2] + x.vo[2];
      fp_note(6, fabs(X - q->x / 16.0)); fp_note(7, fabs(Y - q->y / 16.0)); fp_note(8, fabs(Z - q->z)); fp_note(9, fabs(F - q->f));
      int cv[4] = { q->r, q->g, q->b, q->a }, dmax = 0;
      for (int c = 0; c < 4; c++) { int e = (int)(P->col[c] * Q + 0.0005f), d = abs(e - cv[c]); if (d > dmax) dmax = d; }
      fp_note(11, dmax); if (dmax > 1) fp_tot[22]++;
      if (q->q != 0) { double du = fabs(P->uv[0] - q->s / q->q) * tw, dv = fabs(P->uv[1] - q->t / q->q) * th; fp_note(10, du > dv ? du : dv); }
    }
  }
  if (fi != nfan) fp_tot[20]++;
  if (any) fp_tot[25]++;
  if (!unsafe) fp_tot[26]++;
  if (ge_on && dc_have) {
    if (dc_kind == 2) { if (dc_px) count(1, "!fpos export: P04+2870 drew pixels and is not exported"); return; }
    fp_export(&x, v, n, cls, pv, npv, pfan, npf);
  }
}
static void fp_report(void)
{
  fprintf(stderr, "fpchk:");
  for (int k = 0; k < 34; k++) if (FPN[k][0]) fprintf(stderr, "%s %s %ld", k ? ";" : "", FPN[k], fp_tot[k]);
  fprintf(stderr, "\nfpchk UV (4.12) u %d..%d v %d..%d; largest alpha %d; most vertices in a call %d", fp_uv[0], fp_uv[1], fp_uv[2], fp_uv[3], fp_amax, fp_nmax);
  fprintf(stderr, "\nfpchk max differences:");
  for (int k = 0; k < 12; k++) fprintf(stderr, "%s %s %.4g (vb %ld)", k ? ";" : "", FPM[k], fp_max[k], fp_maxvb[k]);
  fprintf(stderr, "\n");
}

/* ================= VIF1 ================= */
static struct { int cl, wl, mode; uint32_t mask, row[4], col[4], base, ofst, tops, itops, dbf; } vif = { 1, 1 };
static uint32_t vsrc0;   /* guest address of the first data word of the VIF command being collected */
static uint32_t vcmd, vbuf[1 << 18], VPsrc[2048]; static int vleft, vn;   /* VPsrc: guest address each micro instruction was uploaded from */
static void vif_unpack(uint32_t w, const uint32_t *d, int nw)
{
  int c = (w >> 24) & 0x7f, vn_ = (c >> 2) & 3, vl = c & 3, m = (c >> 4) & 1, num = (w >> 16) & 0xff; if (!num) num = 256;
  uint32_t imm = w & 0xffff, addr = (imm & 0x3ff) + ((imm >> 15 & 1) ? vif.tops : 0); int usn = imm >> 14 & 1;
  int bits = 32 >> vl, ne = vn_ + 1; if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; usn = 1; }
  count(1, "vif unpack V%d-%d%s mode%d cl%d wl%d", ne, bits, m ? " mask" : "", vif.mode, vif.cl, vif.wl);
  if (vif_log) printf("vif unpack V%d-%d%s mode%d cl%d wl%d addr %x%s num %d words %d mask %08x\n", ne, bits, m ? " mask" : "", vif.mode, vif.cl, vif.wl, addr, (imm >> 15 & 1) ? " (+tops)" : "", num, nw, vif.mask);
  if (vif_log > 1) { printf("  src %08x data", vsrc0); for (int k = 0; k < nw && k < 12; k++) printf(" %08x", d[k]); printf("\n"); }
  if (vif.wl > vif.cl) count(1, "!vif filling write");
  uint64_t bitpos = 0; int cyc = 0; uint32_t a = addr;
  for (int k = 0; k < num; k++) {
    uint32_t e[4];
    for (int j = 0; j < 4; j++) {
      int jj = vn_ == 0 ? 0 : vn_ == 1 ? (j & 1) : j;          /* S: xxxx, V2: xyxy, V3: the 4th is the next element (V4 logic), V4: xyzw */
      uint64_t bp = bitpos + (uint64_t)jj * bits; uint32_t wi = bp >> 5, sh = bp & 31, v;
      v = wi < (uint32_t)nw ? d[wi] >> sh : 0;
      if (bits < 32) { v &= (1u << bits) - 1; if (!usn && (v >> (bits - 1) & 1)) v |= ~0u << bits; }
      e[j] = v;
    }
    if (vn_ == 3 && vl == 3) { uint32_t s = e[0]; e[0] = (s & 0x1f) << 3; e[1] = (s >> 5 & 0x1f) << 3; e[2] = (s >> 10 & 0x1f) << 3; e[3] = (s >> 15 & 1) << 7; }
    bitpos += (uint64_t)bits * ne;
    uint32_t *q = (uint32_t *)vmem_wr(a); int row = cyc < 3 ? cyc : 3;
    for (int j = 0; j < 4; j++) {
      int mk = m ? (vif.mask >> (row * 8 + j * 2)) & 3 : 0;
      switch (mk) {
        case 0: { int md = (vn_ == 3 && vl == 3) ? 0 : vif.mode;
          if (md == 1) q[j] = e[j] + vif.row[j]; else if (md == 2) { vif.row[j] += e[j]; q[j] = vif.row[j]; } else if (md == 3) { vif.row[j] = e[j]; q[j] = e[j]; } else q[j] = e[j]; break; }
        case 1: q[j] = vif.row[j]; break;
        case 2: q[j] = vif.col[row]; break;
      }
    }
    a++; if (++cyc == vif.wl) { cyc = 0; a += vif.cl - vif.wl; }
  }
}
static void p6_write(void)
{
  uint32_t n = *(const uint32_t *)(p6_vm + ((p6_top + 0x14) & 0x3ff) * 16) & 0xffff, nq = 0x100 + 6 * n, nt = p6_tagn < 64 ? (uint32_t)p6_tagn : 64;
  if (nq > 1024) nq = 1024;
  uint32_t h[8] = { 0x32433650u, (uint32_t)g_vb, (uint32_t)g_call, g_entry, p6_top, nt, (uint32_t)p6_nr, nq };
  fwrite(h, 4, 8, p6_f); fwrite(p6_vm, 16, 16, p6_f);
  for (uint32_t k = 0; k < nq; k++) fwrite(p6_vm + ((p6_top + k) & 0x3ff) * 16, 16, 1, p6_f);
  for (uint32_t k = 0; k < nt; k++) fwrite(&p6_tag[k], sizeof p6_tag[k], 1, p6_f);
  for (int k = 0; k < p6_nr; k++) { const P6Rec *e = &p6_rec[k]; int32_t w[11] = { e->v.x, e->v.y, (int32_t)e->v.z, e->v.f, e->v.r, e->v.g, e->v.b, e->v.a, e->v.u, e->v.v, 0 };
    float fl[3] = { e->v.s, e->v.t, e->v.q }; uint8_t b4[4] = { e->adc, e->prim, e->tag, e->pad };
    fwrite(w, 4, 10, p6_f); fwrite(fl, 4, 3, p6_f); fwrite(b4, 1, 4, p6_f); }
}
static void vif_call(int pc, int cont)
{
  vu_top = vif.tops; vu_itop = vif.itops;
  if (vif.dbf) { vif.dbf = 0; vif.tops = vif.base; } else { vif.dbf = 1; vif.tops = vif.base + vif.ofst; }
  count(1, "vu1 call"); g_call++; g_entry = VPsrc[(cont ? vu_tpc : pc) & 2047]; count(1, "entry %06x calls", g_entry);
  if (vif_log) printf("vif call %d pc %x%s top %x itop %x\n", g_call, pc, cont ? " (cont)" : "", vu_top, vu_itop);
  if (xl_chk) xl_call_begin(cont, cont ? vu_tpc : pc);
  if ((dc_chk || ge_on || xc_on) && g_entry == DF0_ENTRY) dc_begin(0);
  else if ((dc_chk || ge_on || xc_on) && (g_entry == FP_ENTRY || g_entry == FP2_ENTRY)) dc_begin(g_entry == FP_ENTRY ? 1 : 2);
  if (p6_f && !cont && is_p06x(g_entry)) { p6_on = 1; memcpy(p6_vm, VM, sizeof p6_vm); p6_top = vu_top; p6_tagn = 0; p6_nr = 0; }
  xc_p6 = xc_on && ge_on && !cont && is_lit(g_entry); xc_p6ad = 0; memset(xc_p6have, 0, sizeof xc_p6have);
  if (xc_p6) xc_p6hy = g_entry == 0x1a31e0 ? *(const uint32_t *)(VM + (vu_top & 0x3ff) * 16 + 12) & 0xffff   /* P07: T+0.w */
                                           : *(const uint32_t *)(VM + ((vu_top + 0x14) & 0x3ff) * 16 + 4);
  long skc_np0 = skc_np; skc_chp = skc_cht = SKC_H0;
  skc_incall = 1; vu1_run(cont ? vu_tpc : pc); skc_incall = 0;
  if (skc_on > 1) printf("skc call vb %ld %d e%06x prims %ld hp %016llx ht %016llx\n", g_vb, g_call, (unsigned)g_entry, skc_np - skc_np0,
                         (unsigned long long)skc_chp, (unsigned long long)skc_cht);
  if (p6_on) { p6_on = 0; p6_write(); }
  if (xl_chk) xl_call_end();
  if (xc_pend) xc_run();
  if (xc_p6) { xc_p6 = 0; xc_p6_end(); }
  if (dc_on) dc_end();
  if (xc_on && ge_on && xc_r == 1 && !xc_taken && (g_entry == DF0_ENTRY || g_entry == FP_ENTRY || g_entry == FP2_ENTRY)) { xc_n[18]++; xc_n[20 + xc_k]++; if (g_entry == DF0_ENTRY) xc_df0_px(); if (xc_shown < 24) { xc_shown++;
    printf("xlconv vb %ld call %d entry %06x: the converter has a batch, gsref's export none\n", g_vb, g_call, g_entry); }
    if (ge_live) gb_xform(&G, &xc_b, xc_blob, xc_size); }   /* the live renderer draws it */
  xc_r = 0; xc_taken = 0; xc_tr.n = 0;
}
static void vif_word(uint32_t w, uint32_t wa)
{
  if (vleft) {
    if (!vn) vsrc0 = wa;
    vbuf[vn++] = w;
    if (--vleft) return;
    int c = (vcmd >> 24) & 0x7f;
    if (c == 0x20) vif.mask = vbuf[0];
    else if (c == 0x30) memcpy(vif.row, vbuf, 16);
    else if (c == 0x31) memcpy(vif.col, vbuf, 16);
    else if (c == 0x4a) { int at = vcmd & 0xffff; for (int k = 0; k < vn / 2; k++) { VP[((at + k) & 2047) * 2] = vbuf[2 * k]; VP[((at + k) & 2047) * 2 + 1] = vbuf[2 * k + 1]; VPsrc[(at + k) & 2047] = vsrc0 + 8 * k; } count(1, "vif mpg"); }
    else if (c == 0x50 || c == 0x51) { Gif *g = &gp[2]; for (int k = 0; k + 4 <= vn; k += 4) { g_src = vsrc0 + 4 * k; gif_qw(g, vbuf + k); } count(1, "vif direct"); }
    else if ((c & 0x60) == 0x60) vif_unpack(vcmd, vbuf, vn);
    return;
  }
  int c = (w >> 24) & 0x7f, num = (w >> 16) & 0xff; uint32_t imm = w & 0xffff;
  vcmd = w; vn = 0;
  switch (c) {
    case 0x00: case 0x10: case 0x11: case 0x13: case 0x06: case 0x07: return;   /* NOP FLUSHE FLUSH FLUSHA MSKPATH3 MARK */
    case 0x01: vif.cl = imm & 0xff; vif.wl = imm >> 8 & 0xff; return;
    case 0x02: vif.ofst = imm & 0x3ff; vif.dbf = 0; vif.tops = vif.base; return;
    case 0x03: vif.base = imm & 0x3ff; return;
    case 0x04: vif.itops = imm & 0x3ff; return;
    case 0x05: vif.mode = imm & 3; return;
    case 0x14: case 0x15: vif_call(imm, 0); return;
    case 0x17: vif_call(0, 1); return;
    case 0x20: vleft = 1; return;
    case 0x30: case 0x31: vleft = 4; return;
    case 0x4a: vleft = (num ? num : 256) * 2; return;
    case 0x50: case 0x51: vleft = (imm ? imm : 65536) * 4; return;
  }
  if ((c & 0x60) == 0x60) {
    int vn_ = (c >> 2) & 3, vl = c & 3, n = num ? num : 256, bits = 32 >> vl, ne = vn_ + 1; if (vn_ == 3 && vl == 3) { bits = 16; ne = 1; }
    if (vif.wl > vif.cl) n = (n / vif.wl) * vif.cl + (n % vif.wl < vif.cl ? n % vif.wl : vif.cl);
    vleft = (int)(((uint64_t)n * bits * ne + 31) / 32);
    if (!vleft) vif_unpack(w, vbuf, 0);
    return;
  }
  count(1, "!vif cmd %02x", c);
}

/* ================= output ================= */
static int no_img;   /* GSREF_NOIMG=1: no images */
static void ppm(const char *fn, int psm, uint32_t bp, uint32_t bw, int x0, int y0, int w, int h)
{
  if (no_img) return;
  FILE *f = fopen(fn, "wb"); if (!f) { perror(fn); return; }
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    uint32_t c = gs_rd(psm, bp, bw, x0 + x, y0 + y); uint8_t p[3];
    if (psm == CT16 || psm == CT16S) { p[0] = (c & 0x1f) << 3; p[1] = (c >> 5 & 0x1f) << 3; p[2] = (c >> 10 & 0x1f) << 3; }
    else { p[0] = c; p[1] = c >> 8; p[2] = c >> 16; }
    fwrite(p, 3, 1, f);
  }
  fclose(f);
}
static uint64_t priv[0x100];
static void frame_end(const char *dir, long vb)
{
  char fn[512];
  if (skc_on) { printf("skc vb %ld k %d calls %d prims %ld hp %016llx tex %ld ht %016llx rt %ld hr %016llx mip %ld xin %ld skipped %ld\n", vb,
                       skc_kind_end, g_call, skc_np, (unsigned long long)skc_hp, skc_tp, (unsigned long long)skc_ht, skc_rtp, (unsigned long long)skc_hr,
                       skc_mip, skc_xin, skc_skipped);
    skc_hp = skc_ht = skc_hr = SKC_H0; skc_np = skc_tp = skc_rtp = skc_mip = skc_xin = skc_skipped = 0; }
  if (xd_on) xd_flush();
  count(g_pix, "pixels written"); count(g_rej[0], "rejected: scissor"); count(g_rej[1], "rejected: alpha test"); count(g_rej[2], "rejected: dest alpha"); count(g_rej[3], "rejected: depth"); memset(g_rej, 0, sizeof g_rej); count(g_call, "vu1 calls total");
  if (vm_uninit) count(vm_uninit, "vu1 reads of never-written qwords");
  for (int i = 0; i < ntg; i++) {
    count(tg[i].prims, "target fbp %x psm %x fbw %d: %dx%d", tg[i].fbp / 32, tg[i].psm, tg[i].fbw, tg[i].maxx + 1, tg[i].maxy + 1);
    if (tg[i].maxx > 0 && tg[i].fbw) { snprintf(fn, sizeof fn, "%s/vb%ld_fb%x_%x.ppm", dir, vb, tg[i].fbp / 32, tg[i].psm); ppm(fn, tg[i].psm, tg[i].fbp, tg[i].fbw, 0, 0, tg[i].maxx + 1, tg[i].maxy + 1); }
  }
  uint64_t pm = priv[0x00 >> 4];
  for (int k = 0; k < 2; k++) {
    if (!(pm >> k & 1)) continue;
    uint64_t fb = priv[(0x70 + 0x20 * k) >> 4], dp = priv[(0x80 + 0x20 * k) >> 4];
    int fbp = fb & 0x1ff, fbw = fb >> 9 & 63, psm = fb >> 15 & 31, dbx = fb >> 32 & 0x7ff, dby = fb >> 43 & 0x7ff;
    int magh = (dp >> 23 & 15) + 1, magv = (dp >> 27 & 3) + 1, dw = (dp >> 32 & 0xfff) + 1, dh = (dp >> 44 & 0x7ff) + 1;
    count(1, "display %d fbp %x fbw %d psm %x at %d,%d size %dx%d (mag %d,%d)", k + 1, fbp, fbw, psm, dbx, dby, dw / magh, dh / magv, magh, magv);
    snprintf(fn, sizeof fn, "%s/vb%ld_disp%d.ppm", dir, vb, k + 1); ppm(fn, psm, fbp * 32, fbw, dbx, dby, dw / magh, dh / magv);
  }
  for (int i = 0; i < ntu; i++) { TexUse *t = &tu[i];
    printf("texuse vb %ld tsrc %08x csrc %08x tbp %04x tbw %d psm %02x %dx%d cbp %04x cpsm %d csa %d wrap %d/%d u %d-%d v %d-%d prims %ld taps %ld x %d..%d y %d..%d\n", vb, t->tsrc, t->csrc,
           (int)(t->t0 & 0x3fff), (int)(t->t0 >> 14 & 63), (int)(t->t0 >> 20 & 63), 1 << (t->t0 >> 26 & 15), 1 << (t->t0 >> 30 & 15), (int)(t->t0 >> 37 & 0x3fff), (int)(t->t0 >> 51 & 15), (int)(t->t0 >> 56 & 31),
           (int)(t->cl & 3), (int)(t->cl >> 2 & 3), (int)(t->cl >> 4 & 0x3ff), (int)(t->cl >> 14 & 0x3ff), (int)(t->cl >> 24 & 0x3ff), (int)(t->cl >> 34 & 0x3ff), t->prims, t->taps, t->x0, t->x1, t->y0, t->y1); }
  if (ge_f && vb == ge_vb) { ge_write(vb); ge_on = 0; }
  ntu = 0; cur_tu = -1;
  count_flush(vb); g_pix = 0; g_call = 0; ntg = 0; vm_uninit = 0;
}
int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: gsref <capture> <out dir> [-q]\n"); return 1; }
  quiet = argc > 3 && !strcmp(argv[3], "-q"); if (getenv("GSREF_VDUMP")) vdump = atoi(getenv("GSREF_VDUMP"));
  if (getenv("GSREF_SKIPTEX")) for (char *e = getenv("GSREF_SKIPTEX"), *x; nskip_tex < 8 && *e; e = *x ? x + 1 : x) skip_tex[nskip_tex++] = (uint32_t)strtoul(e, &x, 16);
  if (getenv("GSREF_TRACE")) vu_trace_call = atoi(getenv("GSREF_TRACE")); vif_log = getenv("GSREF_VIFLOG") ? atoi(getenv("GSREF_VIFLOG")) : 0; sprite_log = getenv("GSREF_SPRITES") != 0; no_fog = getenv("GSREF_NOFOG") != 0; xfer_log = getenv("GSREF_XFERLOG") != 0; dc_chk = getenv("GSREF_DF0CHK") != 0; if (getenv("GSREF_FOGONLY")) for (char *p = getenv("GSREF_FOGONLY"); *p && n_fog_only < 4; ) { fog_only[n_fog_only++] = (uint32_t)strtoul(p, &p, 16); if (*p != ',') break; p++; } tex_log = getenv("GSREF_TEXLOG") != 0;
  if (getenv("GSREF_ZQ")) { char *e = getenv("GSREF_ZQ"), *c; zq_sh = (int)strtol(e, &c, 10); if (*c == ',') zq_max = (uint32_t)strtoul(c + 1, 0, 16); }
  if (getenv("GSREF_ZCENSUS")) { zc_on = 1; zc_own = calloc(2048 * 2048, 4); }
  if (getenv("GSREF_ZSCALE")) ge_zk = atof(getenv("GSREF_ZSCALE"));
  if (gb_init(&G, ge_zk, 4096)) { fprintf(stderr, "gsref: no memory\n"); return 1; }
  if (getenv("GSREF_GELIVE") && atoi(getenv("GSREF_GELIVE"))) {
    if (!getenv("GSREF_GEOUT") || !getenv("GSREF_XLATE") || !getenv("GSREF_XLCONV") || !getenv("GSREF_XLDIRECT")) {
      fprintf(stderr, "gsref: GSREF_GELIVE needs GSREF_GEOUT, GSREF_XLATE, GSREF_XLCONV and GSREF_XLDIRECT\n"); return 1; }
    ge_live = 1; G.live = 1; }
  if (getenv("GSREF_RZ") && atoi(getenv("GSREF_RZ"))) _mm_setcsr(_mm_getcsr() | 0x6000u | 0x8000u);   /* the PSP's FCR31 0x01000001 as boot.c's MGS2_FPU_RZ */
  if (getenv("GSREF_XLATE")) { xl_chk = 1; xl_reset(); xl_on_call(xl_cb, 0); if (getenv("GSREF_XLRUN")) xl_run = atoi(getenv("GSREF_XLRUN")); if (getenv("GSREF_XLSHOW")) xl_show = (uint32_t)strtoul(getenv("GSREF_XLSHOW"), 0, 16);
    if (getenv("GSREF_XLPRIM")) sscanf(getenv("GSREF_XLPRIM"), "%ld,%d", &xlp_vb, &xlp_call); xc_on = getenv("GSREF_XLCONV") != 0; xc_cut = getenv("GSREF_XLCUT") && atoi(getenv("GSREF_XLCUT"));
    if (getenv("GSREF_XLDIRECT")) { xd_on = 1; xl_on_prim(xd_cb, 0); }
    if (getenv("GSREF_XLDIG")) { FILE *df = fopen(getenv("GSREF_XLDIG"), "w"); if (!df) { perror("GSREF_XLDIG"); return 1; } xdg_open(df); xdg_on = 1; if (!xd_on) xl_on_prim(xdg_cb, 0); }
    if (getenv("GSREF_XLTEX")) { xt_on = 1; xt_ram_img = calloc(32 << 20, 1); xt_map = gu_new(xt_ram, 0); if (!xt_ram_img || !xt_map) return 1; xl_uploads(xt_map); }
    if (xt_on || xc_on) xl_on_pass(gs_pass_cb, 0); }
  if (getenv("GSREF_ZMAP")) zm_load(getenv("GSREF_ZMAP"));
  if (getenv("GSREF_P06DUMP") && !(p6_f = fopen(getenv("GSREF_P06DUMP"), "wb"))) { perror("GSREF_P06DUMP"); return 1; }
  if (getenv("GSREF_PICK")) sscanf(getenv("GSREF_PICK"), "%d,%d,%ld", &pick_x, &pick_y, &pick_vb);
  if (getenv("GSREF_GEOUT")) { char *e = strdup(getenv("GSREF_GEOUT")), *c = strrchr(e, ':'); if (!c) { fprintf(stderr, "GSREF_GEOUT=<file>:<vblank>\n"); return 1; }
    *c = 0; ge_vb = atol(c + 1); if (!(ge_f = fopen(e, "wb"))) { perror(e); return 1; } tex_log = 1; }
  if (getenv("GSREF_RAM")) { FILE *rf = fopen(getenv("GSREF_RAM"), "rb"); ram_img = calloc(32 << 20, 1); if (!rf || fread(ram_img, 1, 32 << 20, rf) != 32u << 20) { fprintf(stderr, "gsref: cannot read GSREF_RAM\n"); return 1; } fclose(rf); tex_log = 1; }
  skip_on = getenv("GSREF_SKIP") && atoi(getenv("GSREF_SKIP")); if (getenv("GSREF_SKIPCHK")) skc_on = atoi(getenv("GSREF_SKIPCHK"));
  if (getenv("GSREF_SKCVB")) skc_dvb = atol(getenv("GSREF_SKCVB"));
  if (getenv("GSREF_SKCW")) sscanf(getenv("GSREF_SKCW"), "%x,%x", &skc_wlo, &skc_whi);
  no_img = getenv("GSREF_NOIMG") && atoi(getenv("GSREF_NOIMG"));
  mkdir(argv[2], 0755); VF[0].u[3] = 0x3f800000u;   /* VF0 = (0, 0, 0, 1) */
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 1; }
  uint8_t r[24]; int tte = 0; long vb = -1, ended = -1;   /* a vblank's draws end at the next vblank record, after its display registers (kind 4) */
  while (fread(r, 24, 1, f) == 1) {
    int kind = r[0], ch = r[1]; uint32_t a; memcpy(&a, r + 4, 4); const uint32_t *q = (const uint32_t *)(r + 8);
    if (kind == 4) { memcpy(&priv[(a & 0xff) >> 4], r + 8, 8); continue; }
    if (ended >= 0) { frame_end(argv[2], ended); ended = -1; }
    if (kind == 2) { if (xdg_on) xdg_vblank(a); if (vb >= 0) ended = vb; vb = a; g_vb = vb; ge_on = ge_f && vb == ge_vb; if (ge_on) { gb_reset(&G); memset(ge_tex_of_tu, 0xff, sizeof ge_tex_of_tu); }
      skc_kind_end = skc_kind; skc_kind = r[2]; continue; }
    if (kind == 3 && r[2]) skc_xin++;
    if (skip_on && skc_kind == 2) { skc_skipped++; continue; }   /* GSREF_SKIP: a vblank whose packets came from a reduced call */
    if (kind == 3) { if (ch == 1) tte = q[0] >> 6 & 1; continue; }
    if (xt_on && kind == 0 && (ch == 1 || ch == 2)) xt_store(a, q, 4);   /* the capture's data at its EE address: the texture sources' EE RAM */
    if (ch == 1) { if (kind == 1) { if (tte) { if (xl_chk) xl_vif1(q[2], a + 8); vif_word(q[2], a + 8); if (xl_chk) xl_vif1(q[3], a + 12); vif_word(q[3], a + 12); } }
      else for (int k = 0; k < 4; k++) { if (xl_chk) xl_vif1(q[k], a + 4 * k); vif_word(q[k], a + 4 * k); } }
    else if (ch == 2 && kind == 0) { if (xl_chk) xl_gif3(q, a); g_src = a; gif_qw(&gp[3], q); }
  }
  if (ended >= 0) frame_end(argv[2], ended);
  else if (vb >= 0) { skc_kind_end = skc_kind; frame_end(argv[2], vb); }
  fprintf(stderr, "gsref: %ld VU1 steps\n", vu_steps);
  if (dc_chk) { dc_report(); fp_report(); }
  if (zc_on) zc_report();
  if (xl_chk) xl_report();
  if (xd_on) xd_report();
  if (xt_on) xt_report();
  if (xc_on) xc_report();
  if (xdg_on) xdg_close();
  if (ram_img) fprintf(stderr, "texconv check: %ld textures match, %ld differ, %ld skipped; %ld of %ld texels differ\n", tc_ok, tc_bad, tc_skip, tc_diff, tc_texels);
  if (p6_f) fclose(p6_f);
  return 0;
}
