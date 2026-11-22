/* xlive_chk.c - dev, x86 check build only (boot.c and runtime/ee_io.h built with -DMGS2_XLIVE; tools/dev/build_xlive.sh): the packet
   translator live (renderer_notes 11.10, step 6).  io_dma_start hands each VIF1 / GIF transfer here before its exact walk; ge/xlive.c
   walks it a second time and feeds ge/xlate.c, whose GS front end tells ge/gsup.c of the texture uploads (the texels are read from guest
   RAM as it is when they are read).  Nothing of the game's is written.
   MGS2_XLIVE=<from vb>:<to vb>[:<digest file>] (vblanks as k_vblanks; the translator starts at <from> knowing nothing, as tools/gsref.c
   does on a capture that starts there; <from> 0: from the first transfer, before the first vblank, whose packets no MGS2_PKT_HASH line covers):
   - "xlive: vb <n> <hash>" for each vblank of the window: FNV-1a 64 of the quadwords this walk took, by ee_io.h's packet hash rules
     (PKT_HASH, PKT_TAG).  With MGS2_PKT_HASH=<from>:<to> in the same run each must equal the exact walk's "pkt: vb <n> <hash>";
   - the digest file (ge/xldig.c): a line per VU1 call and per vblank, for the same file from gsref (GSREF_XLATE=1 GSREF_XLDIG=<file>)
     on a GS capture of the window (MGS2_GSCAP=<file>:<from>:<to - from> in the same run);
   - totals when the window ends.
   MGS2_XLIVE_TEX=1 adds the texture check (renderer_notes 11.2 "Textures"; 11.10 step 5, "Open"):
   - each host -> local transfer, once all its IMAGE data is in: its key as the PSP's texture cache would take it (EE address, size, a hash
     of 64 words spread over it) and a hash of all of it; a known key with other content is a stale texture on the PSP;
   - the bytes of each upload as sent (a 32 MB copy by EE address); every word the texture sources read from guest RAM is compared with
     it: a texture is made from RAM when it is used, so any difference is a texel the GE would get wrong;
   - each draw that textures: the VU1 calls with a rule (TEX0 / CLAMP of the context their tag template's PRIM names, after their A+D
     packet, and after each P06 pass packet) and the front end's primitives.  A call's texels (once per vblank and upload per TEX0 /
     CLAMP, through ge/texconv.c as the PSP will convert them), a primitive's texels where it samples them (its vertices' texel box, one
     texel more each side; the whole texture when that box leaves the texture or the wrap mode is a region repeat) and the palette
     entries they use are read through the texture sources, from EE RAM now and as sent;
   - a line every 100 vblanks of the window, and totals;
   - at the end, by source (EE address and size): its uploads and keys, and what the rule "hash a source in full once it shows a second
     key" would leave stale and hash in full (bytes a vblank), with the uploads a vblank by size.
   MGS2_XLIVE_TEX=2 also prints, for the first 64 textures (by TBP, TBW, PSM) with a texel the map cannot tell or that texconv cannot
   convert, the texture and its first such texel's block list (gu_why).
   MGS2_XLIVE_WATCH=<block, hex>:<from vb>:<to vb> prints each entry the map pushes on that block's list in those vblanks (gu_watch).
   MGS2_XLIVE_RTLOG=<from vb>:<to vb> prints up to 80 textured front-end primitives of those vblanks with their texture registers, texel
   box and the map's flags for the texture's area (the whole, the box, the 64 x 64 squares holding a drawn area).
   With MGS2_XLIVE_TEX the totals also list the 24 textures (TBP, TBW, PSM) with the most texels the map cannot tell.
   MGS2_XLIVE_TRX=1 prints each GS local -> local transfer and each host -> local transfer in a format other than the 32-bit layout's
   (buffer, format, rectangle; a local copy's source too); MGS2_XLIVE_TRX=2 every host -> local transfer with its number and, with
   MGS2_XLIVE_TEX, the EE address of its data once all of it is in.
   MGS2_XLIVE_TK=1 also runs the PSP's texture path (ge/texcache.h) on every indexed texture use: the cache's texture and palette are
   compared texel by texel and entry by entry with the map, and the totals give its hit rate, its bytes and what it refused.
   MGS2_XLIVE_TK=<budget in KB> (over 1) sets the cache's budget, so eviction runs.
   MGS2_XLIVE_TKBLK=0 / 1 / 2: the texture path converts texel by texel / a GS block at a time (the default) / both, compared (tk_blk,
   task #59); a line after the texture totals counts them.
   The exact speed-ups of codec_speed.md steps 1 and 2 (on by default, as on the PSP) are switched off one by one with MGS2_XLIVE_GUMEMO=0
   (gu_area_memo, the PSP's OPT_GU_MEMO), MGS2_XLIVE_TKFLAGS=0 (tk_flags, OPT_TK_FLAGS), MGS2_XLIVE_GUCLUT=0 (gu_clut_memo, OPT_GU_CLUT)
   and MGS2_XLIVE_V48=0 (xl_ud_v48, OPT_XL_V48); ge/ built with -DGU_MEMO_CHECK checks the map's memos against its plain walks.
   MGS2_XLIVE_T16=0 (tk_t16) turns task #83's 16-bit textures and DIRECT primitive regions off (the pictures before; the PSP builds keep
   them on).
   The VU1 entry census (renderer_notes 11.2): the first call of each entry is printed with whether the translator has a rule for it;
   MGS2_XLIVE_CENSUS=<vb>[,<vb>...] prints the calls of each entry so far (and those without a tag template) after those vblanks (the
   route's stage borders), and the end of the window prints them too.  A run from vblank 0 also prints what each upload of the boot sent.
   MGS2_XLIVE_VCEN=1 (ge/ built with -DXL_CENSUS, as build_xlive.sh does): the volume census (renderer_notes 11.12), a line per vblank
   of the window, "xlive vc: vb <n>" with the VIF1 words (w), command words (cw), data words of UNPACK, MPG, DIRECT and the rest (up, mpg,
   dir, oth), GIF quadwords of paths 1-3 (g1-g3; path 1 = the A+D packets the translator kicks) and their IMAGE quadwords (i2, i3), the
   DMA quadwords the walk took (qw), VU1 calls (with a converter, a rule only, no rule), the front end's primitives, and the vectors the
   UNPACKs wrote by format (uq <command bits 0-4, hex>:<vectors>).
   MGS2_XLIVE_FAST=1: the walk hands the translator spans (xl_vif1_span: UNPACKs decoded when read, IMAGE runs in one piece; renderer_notes
   11.12 step 1) instead of single words; everything above prints as with single words (the digests hash the VU memory as it reads,
   xl_vm_logical), and the totals add a line "xlive lazy:" with the UNPACKs left to decode later and the quadwords decoded later.
   MGS2_XLIVE_SYNC=1 decodes all pending quadwords before each call's callbacks (the path the converters will take).
   MGS2_XLIVE_NOSEEN=1 leaves out the walk's packet hash (for timing).  The totals give the time spent in the walk with the translator
   and the callbacks ("xlive time:").
   MGS2_XLIVE_CG=1 (a build where valgrind/callgrind.h is found): callgrind's instrumentation on for the window only, for
   valgrind --tool=callgrind --instr-atstart=no --toggle-collect=xlv_dma (the translator's own profile).
   MGS2_XLIVE_DIGUSED=1: the digests take only what is read (ge/xldig.h xdg_used), to compare a run with MGS2_XLIVE_SKIP=1 against one
   without on the shown vblanks.
   MGS2_XLIVE_SKIP=1: shown frames only (renderer_notes 11.12 "Shown frames only"): the transfers of a vblank whose previous vblank had
   reduced calls of the draw pipeline only (boot.c g_dg_calls) are not handed over, as the PSP translator will skip them; their packet
   hash lines carry the empty hash, and the totals count what was skipped.
   MGS2_XLIVE_ONE=1 (with MGS2_XLIVE_FAST=1): the one walk (renderer_notes 11.12, task #47): a VIF1 transfer that ee_io.h's
   vif1_chain_fast walks is not walked again here; that walk hands the translator the same runs of words (XL_ONE) and the transfer's end
   (xlive_one_end).  GIF transfers keep the second walk.  The packet hash lines then cover the GIF transfers only, the walk's tag and
   quadword counts the GIF transfers only, and "xlive time:" holds the exact walk of the VIF1 transfers too.
   MGS2_XLIVE_GB=1 (renderer_notes 11.13, #30 step 3): the live renderer's frame (ge/gelive.h) made on each shown vblank of the window
   (one whose previous vblank had full draw-pipeline calls): the front end's path 2 / 3 primitives and the converters' VU1 calls into GE
   batches, textures from the PSP texture path (its own cache when MGS2_XLIVE_TK is not set; no budget).  A line per frame ("xlive gb:")
   and totals with the largest frame.  MGS2_XLIVE_GB=2 also asks the texture path when the last lookup's answer is taken (the memo) and
   counts the differences; MGS2_XLIVE_GB=3 also counts what each frame's depth map leaves outside the GE's range (gelive.h zchk; with
   MGS2_XLIVE_GB_ZK=0).  MGS2_XLIVE_GB_ZK=<k>: GE depth = Z24 x k (default 0.2, the replay files' since v4); 0: the PSP's depth (gelive.h
   zraw: Z24 as it is, the frame's range mapped by the GE list; the frame's depth range on each line).  MGS2_XLIVE_GER=<prefix>:<vb>[,<vb>...] saves those frames as replay files
   <prefix><vb>.ger (ge/replay_fmt.h, textures copied in) for psp/ge_replay and the check against gsref (GSREF_GELIVE).
   MGS2_XLIVE_GX=<a>:<b> (renderer_notes 11.13 "3e", task #56; with MGS2_XLIVE=0:<to>, and no MGS2_XLIVE_TK): the PSP harness's frames
   (psp/xl_psp.c MGS2_GL) made here the same way: the PSP's depth, its texture pool (MGS2_XLIVE_GX_TEXKB, default GL_PSP_TEXKB) and the
   cache's budget, its frame arena, texture table and list sizes (gelive.h GL_PSP_*), a frame begun only after the first transfer, and
   each frame's GE list (ge/gelist.c, the draw buffer alternating from the first frame); the frames whose last vblank lies in a..b print
   "picture:" lines with the list digest and the frame digest (gelive.h gl_digest) in the format of the PSP's log.txt, and the end a
   total, so the two can be compared line for line (the word counts differ by BASE words: the memory lies elsewhere).  A list that does
   not fit prints a line (the first 50), as on the PSP.  MGS2_XLIVE_GX_LIST=<KB>: another size of the list's block (the list and its aux
   share it, gelist.h), to measure what the lists need (those frames' lists then differ from the PSP's). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "../ge/xlive.h"
#include "../ge/xlate.h"
#include "../ge/gsfront.h"
#include "../ge/gsup.h"
#include "../ge/xldig.h"
#include "../ge/texconv.h"
#include "../ge/texcache.h"
#include "../ge/gslayout.h"
#include "../ge/gelive.h"
#if defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#endif
#endif
#ifndef CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_STOP_INSTRUMENTATION
#endif

#define GSZ (1u << 25)
static long from = -2, to; static int on, done; static XlvMem M; static GuMap *map; static FILE *dig;
extern uint32_t g_dg_calls[2];   /* boot.c: draw-pipeline calls, full / reduced */
static int skip_on, skip_now; static uint32_t sk_c0, sk_c1; static long sk_vb, sk_tr;   /* MGS2_XLIVE_SKIP */
extern int g_xl_one;                    /* runtime/ee_io.h: the exact walk feeds the translator (MGS2_XLIVE_ONE) */
static int one_on; static long one_tr; static struct timespec one_t0;
static uint64_t ph = 0xcbf29ce484222325ull;
static long cur_vb;
static const uint8_t *ram(void *ctx, uint32_t ee, uint32_t n) { (void)ctx; ee &= GSZ - 1; return M.mem && ee + n <= GSZ ? M.mem + ee : 0; }

/* ---- the texture check (MGS2_XLIVE_TEX=1; ge/gsup.c built with GU_SENT keeps each transfer's bytes as sent) ---- */
static int tx_on, tx_detail, trx_all, wbn = -1; static long wfrom, wto;
typedef struct {
  long xfers, bytes, keynew, keyhit, keybad;            /* finished uploads; their keys as the PSP's texture cache would take them */
  long scanned, scan_bad, scan_badx, scan_bytes;        /* each vblank: transfers the map can still read, those whose EE bytes now differ
                                                           from as sent (transfer-vblanks, distinct transfers, differing bytes) */
  long uses, calls, passes, prims, nonidx, notpl, mip;  /* texture uses */
  long conv, boxes, unsup, texels, tmiss, tone, tdiff;  /* conversions (calls), primitive boxes; texels read from EE RAM now and as sent:
                                                           unknown both ways, known one way only, different */
  long pchecks, pal, pmiss, pone, pdiff;                /* palettes, the same way */
  long direct, dtexels, dmiss, done1, ddiff;            /* direct-colour textures (CT32, CT24, CT16, CT16S): uses, texels the same way */
  long tk_uses, tk_tex, tk_rt, tk_lost, tk_unsup, tk_cmp, tk_bad, tk_pbad;   /* the PSP texture path (MGS2_XLIVE_TK): uses and their kind,
                                                           textures compared with the map, those with a differing texel or palette entry */
} TxCount;
static TxCount ck, ck_all;
static TkCache *tkc; static int tk_on, shown_tk;
/* MGS2_XLIVE_GB: the live renderer's frame */
static GlFrame glf; static int gb_on, gb_now; static char ger_pre[200]; static long ger_vb[64]; static int ger_n;
/* MGS2_XLIVE_GX: the PSP harness's frames and GE lists */
static int gx_on, gx_seen; static long gx_from = -1, gx_to = -1, gx_count, gx_n, gx_fail, gx_first = -1, gx_wmax, gx_amax, gx_bmax, gx_drop;
static uint64_t gx_all = 0xcbf29ce484222325ull; static GxList gxl; static GxHost gxh;
static uint8_t *gx_arena, *gx_pool, *gx_lb; static uint32_t gx_pool_n, gx_listn = GL_PSP_LIST;
static uint32_t gx_addr(void *ctx, const void *p)   /* a GE address space like the PSP's: the frame memory (the arena, then the list's
                                                       block), the pool */
{
  const uint8_t *q = p; (void)ctx;
  if (q >= gx_arena && q <= gx_arena + GL_PSP_ARENA) return 0x08600000u + (uint32_t)(q - gx_arena);
  if (q >= gx_lb && q <= gx_lb + gx_listn) return 0x08600000u + GL_PSP_ARENA + (uint32_t)(q - gx_lb);
  if (q >= gx_pool && q <= gx_pool + gx_pool_n) return 0x09000000u + (uint32_t)(q - gx_pool);
  fprintf(stderr, "xlive gx: an address outside the frame memory, the aux and the pool\n"); exit(2);
}
static void gx_list(long vb)   /* psp/xl_psp.c gl_list's work on the frame that ended (its last vblank vb) */
{
  int d = (int)(gx_count++ & 1), dg = vb >= gx_from && vb <= gx_to;
  GxFrame F = { glf.o.b, (uint32_t)glf.o.nb, glf.o.v, (uint32_t)glf.o.nv, glf.o.d, glf.o.t, (uint32_t)glf.o.nt, glf.tdata, glf.tpal, 1, 0 };
  gx_depth_map(glf.map_lo, glf.map_hi, 1.0f, &F.zs, &F.zo);
  gxl.dig_on = dg;
  if (gx_frame(&gxl, &gxh, &F, d) && ++gx_fail <= 50)
    fprintf(stderr, "picture: vb %ld: the list did not fit: %ld batches left out (%ld words, %ld aux bytes)\n", vb, gxl.c.dropped, gxl.c.words, gxl.c.aux);
  gx_drop += gxl.c.dropped; if (gxl.c.words > gx_wmax) gx_wmax = gxl.c.words; if (gxl.c.aux > gx_amax) gx_amax = gxl.c.aux;
  if (4 * gxl.c.words + gxl.c.aux > gx_bmax) gx_bmax = 4 * gxl.c.words + gxl.c.aux;
  if (dg) {
    uint64_t g = gl_digest(&glf, &gxl); gx_all = (gx_all ^ g) * 0x100000001b3ull; gx_n++;
    fprintf(stderr, "picture: vb %ld buffer %d list %016llx frame %016llx: %ld words, %ld aux bytes, %ld batches, %ld PRIMs, %d textures, "
            "%ld dropped\n", vb, d, (unsigned long long)gxl.dig, (unsigned long long)g, gxl.c.words, gxl.c.aux, gxl.c.batches, gxl.c.prims,
            glf.o.nt, gxl.c.dropped);
  }
}
static void gb_line(const GlCount *n, int64_t db, int64_t tb, const char *what)   /* db, tb: n's data and texture bytes (psp/xl_psp.c gl_line) */
{
  fprintf(stderr, "%s %ld batches (%ld transform), %ld vertices, %lld KB data, %ld textures (%lld KB, %ld new 16-bit); primitives %ld (%ld "
          "textured; left out: %ld uncertain, %ld path 1; state drawn otherwise %ld/%ld/%ld/%ld); calls %ld: converted %ld, nothing drawn %ld, "
          "bad %ld, no room %ld; other calls %ld; texture lookups %ld (memo %ld): frame buffers %ld, untextured: render targets %ld, not "
          "known %ld, refused %ld, table full %ld; dropped %ld", what, n->batches, n->xbatches, n->verts, (long long)(db >> 10), n->ntex,
          (long long)(tb >> 10), n->tex16, n->prims, n->prims_tex, n->uncertain, n->path1, n->unrep[0], n->unrep[1], n->unrep[2], n->unrep[3],
          n->calls, n->conv, n->conv_empty, n->conv_bad, n->conv_full, n->other, n->tex_asks, n->tex_memo, n->tex_fb, n->tex_rt,
          n->tex_lost, n->tex_unsup, n->tex_full, n->dropped);
  fprintf(stderr, "; P06 and P07 calls %ld, passes 2 and 3 drawn %ld, not drawn %ld", n->lit_calls, n->lit_passes, n->lit_nopass);
}
static void gb_frame_end(long vb)
{
  gl_end(&glf);
  if (gx_on) gx_list(vb);
  char w[64]; snprintf(w, sizeof w, "xlive gb: vb %ld:", vb); gb_line(&glf.n, glf.n.dbytes, glf.n.tbytes, w);
  if (glf.zraw) {
    const GlCount *n = &glf.n;
    if (glf.zr.hi >= glf.zr.lo) fprintf(stderr, "; depth: meshes %.0f..%.0f", glf.zr.lo, glf.zr.hi); else fprintf(stderr, "; depth: meshes none");
    fprintf(stderr, " (%ld calls, %ld vertices; %ld ZTST ALWAYS left out)", n->zr_calls, glf.zr.verts, n->zr_always);
    if (glf.thi >= glf.tlo) fprintf(stderr, ", GS draws %.0f..%.0f", glf.tlo, glf.thi); else fprintf(stderr, ", GS draws none");
    fprintf(stderr, " (vertices at 0 %ld, at the top %ld); map %.0f..%.0f; Z16 primitives %ld", glf.tz0, glf.tztop, glf.map_lo, glf.map_hi, n->z16);
    if (glf.zchk) fprintf(stderr, "; outside the map: class 3 effect triangles %ld, df0 triangles of depth writers %ld, of others %ld", n->bent, n->out_w, n->out_o);
  }
  fputc('\n', stderr);
  for (int k = 0; k < ger_n; k++) if (ger_vb[k] == vb) {
    char fn[256]; snprintf(fn, sizeof fn, "%s%ld.ger", ger_pre, vb); FILE *f = fopen(fn, "wb");
    if (!f || gl_write(&glf, f, (uint32_t)vb)) fprintf(stderr, "xlive gb: cannot write %s\n", fn); else fprintf(stderr, "xlive gb: vb %ld saved as %s\n", vb, fn);
    if (f) fclose(f);
  }
}
static int shown_key, shown_scan, shown_tex, shown_pal;
static uint64_t h64(uint64_t h, const uint8_t *b, uint32_t n)
{ for (uint32_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
/* the PSP texture cache's key for an upload (renderer_notes 11.2): its EE address and size and 64 words spread over it (all words when
   it has fewer) */
static uint64_t sparse64(const uint8_t *b, uint32_t n)
{
  uint32_t nw = n / 4, st = nw >= 64 ? nw / 64 : 1, k; uint64_t h = 0xcbf29ce484222325ull;
  for (k = 0; k < 64 && k * st < nw; k++) h = h64(h, b + 4 * (k * st), 4);
  return h;
}
typedef struct { uint32_t ee, n; uint64_t key, full; long vb; } Up;
#define UP_N (1u << 21)
static Up *up; static long up_n;
static int32_t *xvb; static uint8_t *xbad; static uint32_t xcap;   /* by transfer number: the vblank its data started in; it differed */
/* by source (EE address and size): the rule a PSP cache could keep for sources whose content changes: once a source has shown a second
   key it is hashed in full from then on; a stale key before that is one the rule misses */
typedef struct { uint32_t ee, n; long ups, keys, stale, pmiss, vol_vb, vbytes; } Src;
#define SRC_N (1u << 16)
static Src *srcs; static long src_n, pol_miss, pol_bytes, sz_bytes[4];
static Src *src_of(uint32_t ee, uint32_t n)
{
  uint32_t i = (uint32_t)((((uint64_t)ee << 20) ^ n) * 0x9e3779b97f4a7c15ull >> 48) & (SRC_N - 1);
  for (;; i = (i + 1) & (SRC_N - 1)) {
    Src *s = &srcs[i];
    if (!s->n) { if (src_n >= SRC_N / 2) { fprintf(stderr, "xlive tex: the source table is full\n"); exit(1); }
      s->ee = ee; s->n = n; s->vol_vb = -1; src_n++; return s; }
    if (s->ee == ee && s->n == n) return s;
  }
}
static void on_data(void *ctx, uint32_t no, uint32_t ee, uint32_t n, uint32_t got, uint32_t need)
{
  (void)ctx;
  if (no >= xcap) { uint32_t c = xcap ? xcap : 1u << 16; while (c <= no) c *= 2;
    int32_t *q = realloc(xvb, c * sizeof *q); uint8_t *f = q ? realloc(xbad, c) : 0; if (!q || !f) { fprintf(stderr, "xlive tex: no memory\n"); exit(1); }
    memset(q + xcap, 0, (c - xcap) * sizeof *q); memset(f + xcap, 0, c - xcap); xvb = q; xbad = f; xcap = c; }
  if (got == n) xvb[no] = (int32_t)cur_vb;
  if (got != need) return;
  uint32_t e0 = (ee + n - got) & (GSZ - 1); if (e0 + need > GSZ) return;
  if (trx_all) fprintf(stderr, "xlive trx: vb %ld: transfer %u data: EE %07x, %u bytes\n", cur_vb, no, e0, need);
  if (cur_vb == 0) { uint32_t nz = 0; for (uint32_t k = 0; k < need; k++) nz += M.mem[e0 + k] != 0;   /* the boot's uploads: what they sent */
    fprintf(stderr, "xlive boot: transfer %u: EE %07x, %u bytes, %u not zero\n", no, e0, need, nz); }
  uint64_t key = sparse64(M.mem + e0, need), full = h64(0xcbf29ce484222325ull, M.mem + e0, need);
  ck.xfers++; ck.bytes += need;
  Src *s = src_of(e0, need); s->ups++; sz_bytes[need <= 16384 ? 0 : need <= 65536 ? 1 : need <= 262144 ? 2 : 3] += need;
  uint32_t i = (uint32_t)((((uint64_t)e0 << 20) ^ need ^ key) * 0x9e3779b97f4a7c15ull >> 43) & (UP_N - 1);
  for (;; i = (i + 1) & (UP_N - 1)) {
    Up *u = &up[i];
    if (!u->n) { if (up_n >= UP_N / 2) { fprintf(stderr, "xlive tex: the upload table is full\n"); exit(1); }
      u->ee = e0; u->n = need; u->key = key; u->full = full; u->vb = cur_vb; up_n++; ck.keynew++; if (++s->keys == 2) s->vol_vb = cur_vb; break; }
    if (u->ee == e0 && u->n == need && u->key == key) {
      if (u->full != full) { ck.keybad++; s->stale++; if (s->vol_vb < 0) { s->pmiss++; pol_miss++; }
        if (shown_key++ < 20) fprintf(stderr, "xlive tex: vb %ld transfer %u: EE %07x, %u bytes: the same key as at vb %ld, other content "
                                      "(the source's upload %ld, its keys so far %ld)\n", cur_vb, no, e0, need, u->vb, s->ups, s->keys);
        u->full = full; }
      else ck.keyhit++;
      u->vb = cur_vb; break; }
  }
  if (s->vol_vb >= 0) { s->vbytes += need; pol_bytes += need; }
}
static int src_cmp(const void *a, const void *b) { const Src *x = a, *y = b; return (y->vbytes > x->vbytes) - (y->vbytes < x->vbytes); }
static void src_report(long nvb)
{
  long k = 0, shown = 0, nvol = 0; Src *l = malloc((src_n ? src_n : 1) * sizeof *l); if (!l || nvb < 1) { free(l); return; }
  for (uint32_t i = 0; i < SRC_N; i++) if (srcs[i].n) l[k++] = srcs[i];
  qsort(l, k, sizeof *l, src_cmp);
  for (long j = 0; j < k; j++) if (l[j].keys >= 2 || l[j].stale) { nvol += l[j].keys >= 2;
    if (shown++ < 40) fprintf(stderr, "xlive tex: source EE %07x, %u bytes: %ld uploads, %ld keys, hashed in full from vb %ld (%.1f KB a vblank), "
                              "%ld stale (%ld before that)\n", l[j].ee, l[j].n, l[j].ups, l[j].keys, l[j].vol_vb, l[j].vbytes / 1024.0 / nvb, l[j].stale, l[j].pmiss); }
  fprintf(stderr, "xlive tex: %ld sources, %ld with a second key; the rule \"hash a source in full once it shows a second key\" leaves %ld stale "
          "keys and hashes %.1f KB a vblank in full; uploads a vblank by size: to 16 KB %.1f KB, to 64 KB %.1f, to 256 KB %.1f, larger %.1f\n",
          k, nvol, pol_miss, pol_bytes / 1024.0 / nvb, sz_bytes[0] / 1024.0 / nvb, sz_bytes[1] / 1024.0 / nvb, sz_bytes[2] / 1024.0 / nvb,
          sz_bytes[3] / 1024.0 / nvb);
  free(l);
}
static void scan_fn(void *ctx, uint32_t no, uint32_t ee, uint32_t got, uint32_t first, uint32_t nd)
{
  (void)ctx; ck.scan_bad++; ck.scan_bytes += nd;
  if (no < xcap && !xbad[no]) { xbad[no] = 1; ck.scan_badx++;
    if (shown_scan++ < 20) fprintf(stderr, "xlive tex: vb %ld: transfer %u (data from vb %d), EE %07x, %u bytes: %u bytes differ from as sent, the first at +%u\n",
                                   cur_vb, no, no < xcap ? xvb[no] : -1, ee, got, nd, first); }
}
/* the textures the draws use: once per vblank and per state of what they read (transfers for the texels, CLUT loads for the palette) */
typedef struct { uint64_t a, b; long e, vb; } Seen;
#define SEEN_N 4096u
static Seen seen_tex[SEEN_N], seen_pal[SEEN_N];
static int seen_before(Seen *t, uint64_t a, uint64_t b, long e)
{
  uint64_t h = a * 0x9e3779b97f4a7c15ull ^ b * 0xff51afd7ed558ccdull ^ (uint64_t)e * 0xc4ceb9fe1a85ec53ull;
  Seen *s = &t[(h >> 40) & (SEEN_N - 1)];
  if (s->a == a && s->b == b && s->e == e && s->vb == cur_vb) return 1;
  s->a = a; s->b = b; s->e = e; s->vb = cur_vb; return 0;
}
static uint8_t txbuf[1 << 20];
typedef struct { int psm; uint32_t tbp, tbw; uint64_t t0, cl; int told, direct; long n, unk; } Fetch;
/* texels read and not known, by texture (TBP0 TBW PSM) */
typedef struct { uint32_t k; long n, unk, first; } KeyCnt;
#define KC_N 4096u
static KeyCnt kc[KC_N]; static long kc_used;
static void kc_add(const Fetch *f)
{
  uint32_t k = (uint32_t)(f->t0 & 0x3ffffff) | 1u << 31, i = (k * 0x9e3779b1u) >> 20 & (KC_N - 1);
  if (!f->n) return;
  for (;; i = (i + 1) & (KC_N - 1)) {
    KeyCnt *c = &kc[i];
    if (!c->k) { if (kc_used >= (long)KC_N / 2) return; c->k = k; c->first = -1; kc_used++; }
    if (c->k == k) { c->n += f->n; c->unk += f->unk; if (f->unk && c->first < 0) c->first = cur_vb; return; }
  }
}
static int kc_cmp(const void *a, const void *b) { const KeyCnt *x = a, *y = b; return (y->unk > x->unk) - (y->unk < x->unk); }
static void kc_report(void)
{
  KeyCnt *l = malloc(KC_N * sizeof *l); long n = 0; if (!l) return;
  for (uint32_t i = 0; i < KC_N; i++) if (kc[i].k && kc[i].unk) l[n++] = kc[i];
  qsort(l, (size_t)n, sizeof *l, kc_cmp);
  for (long j = 0; j < n && j < 24; j++)
    fprintf(stderr, "xlive tex: texture tbp %04x tbw %u psm %02x: %ld texels read, %ld not known (the first in vb %ld)\n", l[j].k & 0x3fff,
            l[j].k >> 14 & 63, l[j].k >> 20 & 63, l[j].n, l[j].unk, l[j].first);
  free(l);
}
/* MGS2_XLIVE_TEX=2: the first time a texture (TBP, TBW, PSM) has an unknown texel or cannot be converted */
static uint32_t told_k[64]; static int told_n;
static int first_tell(uint64_t t0)
{
  uint32_t k = (uint32_t)(t0 & 0x3ffffff); int i;   /* TBP0 TBW PSM */
  for (i = 0; i < told_n; i++) if (told_k[i] == k) return 0;
  if (told_n == 64) return 0;
  told_k[told_n++] = k; return 1;
}
static void tell(const char *what, uint64_t t0, uint64_t cl, int x, int y)
{
  fprintf(stderr, "xlive tex: vb %ld: %s: tbp %04x tbw %u psm %02x tw %u th %u cbp %04x cpsm %x csa %u; clamp wms %u wmt %u %u..%u %u..%u",
          cur_vb, what, (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 14 & 63), (unsigned)(t0 >> 20 & 63), (unsigned)(t0 >> 26 & 15), (unsigned)(t0 >> 30 & 15),
          (unsigned)(t0 >> 37 & 0x3fff), (unsigned)(t0 >> 51 & 15), (unsigned)(t0 >> 56 & 31), (unsigned)(cl & 3), (unsigned)(cl >> 2 & 3),
          (unsigned)(cl >> 4 & 1023), (unsigned)(cl >> 14 & 1023), (unsigned)(cl >> 24 & 1023), (unsigned)(cl >> 34 & 1023));
  if (x >= 0) {
    int psm = (int)(t0 >> 20 & 63); uint32_t tbp = (uint32_t)(t0 & 0x3fff), tbw = (uint32_t)(t0 >> 14 & 63), wa; char b[1024];
    x &= 2047; y &= 2047;
    wa = psm == GS_T8 ? gs_pa8(tbp, tbw, x, y) >> 2 : psm == GS_T4 ? gs_pa4(tbp, tbw, x, y) >> 3
       : psm == GS_CT16 || psm == GS_CT16S ? gs_pa16(tbp, tbw, x, y, psm == GS_CT16S, 0) >> 2 : gs_pa32(tbp, tbw, x, y, 0) >> 2;
    gu_why(map, wa, b, sizeof b); fprintf(stderr, "; texel (%d, %d), word %05x, %s", x, y, wa, b);
  }
  fprintf(stderr, "\n");
}
static int fetch(void *ctx, int x, int y)
{
  Fetch *f = ctx; int64_t v = gu_texel(map, f->psm, f->tbp, f->tbw, x, y), r;
  gu_read_sent(map, 1); r = gu_texel(map, f->psm, f->tbp, f->tbw, x, y); gu_read_sent(map, 0);
  long *n = f->direct ? &ck.dtexels : &ck.texels, *miss = f->direct ? &ck.dmiss : &ck.tmiss, *one = f->direct ? &ck.done1 : &ck.tone;
  ++*n; f->n++;
  if (v < 0 || r < 0) { if (v < 0 && r < 0) ++*miss; else ++*one;
    f->unk++;
    if (tx_detail && !f->told++ && first_tell(f->t0)) tell(v < 0 && r < 0 ? "a texel the map cannot tell" : "a texel known one way only", f->t0, f->cl, x, y);
    return v < 0 ? 0 : (int)v; }
  if (v != r) { if (f->direct) ck.ddiff++; else ck.tdiff++;
    if (shown_tex++ < 20) fprintf(stderr, "xlive tex: vb %ld: texel (%d, %d) of tbp %04x tbw %u psm %02x: %llx from EE RAM now, %llx as sent\n", cur_vb, x, y,
                                  f->tbp, f->tbw, f->psm, (unsigned long long)v, (unsigned long long)r); }
  return (int)v;
}
/* the texels a front-end primitive samples: its vertices' texel coordinates, one more each side (bilinear); 0: the whole texture */
static int prim_box(const GfDraw *d, const GfVtx *const *v, int n, int box[4])
{
  float a = 1e30f, b = 1e30f, c = -1e30f, e = -1e30f; int tw = 1 << d->tw, th = 1 << d->th;
  if (d->wms == 3 || d->wmt == 3) return 0;
  for (int i = 0; i < n; i++) {
    float s, t;
    if (d->fst) { s = v[i]->u / 16.0f; t = v[i]->v / 16.0f; }
    else { if (!(v[i]->q > 0)) return 0; s = v[i]->s / v[i]->q * (float)tw; t = v[i]->t / v[i]->q * (float)th; }
    if (!(s > -4096 && s < 4096 && t > -4096 && t < 4096)) return 0;
    if (s < a) a = s;
    if (s > c) c = s;
    if (t < b) b = t;
    if (t > e) e = t;
  }
  box[0] = (int)floorf(a) - 1; box[1] = (int)floorf(b) - 1; box[2] = (int)ceilf(c) + 1; box[3] = (int)ceilf(e) + 1;
  if (d->wms == 2) { if (box[0] < d->minu) box[0] = d->minu; if (box[2] > d->maxu) box[2] = d->maxu; }
  if (d->wmt == 2) { if (box[1] < d->minv) box[1] = d->minv; if (box[3] > d->maxv) box[3] = d->maxv; }
  if (box[0] < 0) box[0] = 0;
  if (box[1] < 0) box[1] = 0;
  if (box[2] >= tw) box[2] = tw - 1;
  if (box[3] >= th) box[3] = th - 1;
  return box[0] <= box[2] && box[1] <= box[3];
}
/* the PSP texture path (ge/texcache.h) on this use: its texture and palette against the map's (t, data: what tc_texture_cb made from
   the map; f: its unknown-texel count) */
static void tk_check(const uint64_t *gr, int ctxt, const TcTex *t, const uint8_t *data, const Fetch *f)
{
  uint64_t t0 = gr[0x06 + ctxt]; TkTex k; int kind = tk_get(tkc, gr, ctxt, &k);
  int n = (t->psm == 8) ? 256 : 16, csa = (int)(t0 >> 56 & 31), cpsm = (int)(t0 >> 51 & 15);
  if (t->psm == 16) n = 0;   /* direct colour: its texels carry the colours */
  ck.tk_uses++;
  switch (kind) {
    case TK_RT: ck.tk_rt++; return;
    case TK_LOST: ck.tk_lost++; return;
    case TK_UNSUP: ck.tk_unsup++; return;
    default: ck.tk_tex++;
  }
  if (f->unk) return;   /* the map could not give every texel of this region: the cache may hold an older, complete conversion */
  ck.tk_cmp++;
  int bad = 0;
  if (k.t.psm != t->psm || k.t.w != t->w || k.t.h != t->h || k.t.tw != t->tw || k.t.th != t->th || k.t.bw != t->bw || k.t.rows != t->rows
      || k.t.u0 != t->u0 || k.t.v0 != t->v0 || k.t.repeat != t->repeat) bad = -1;
  else for (int y = 0; y < t->rows && !bad; y++) for (int x = 0; x < t->bw; x++)   /* the texels it stores: the region plus its edge */
    if (tc_texel(&k.t, k.data, x, y) != tc_texel(t, data, x, y)) { bad = 1; if (shown_tk++ < 20)
        fprintf(stderr, "xlive tk: vb %ld: texture tbp %04x psm %02x: texel (%d, %d) %d in the cache, %d from the map\n", cur_vb,
                (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 20 & 63), x, y, tc_texel(&k.t, k.data, x, y), tc_texel(t, data, x, y)); break; }
  if (bad) { ck.tk_bad++; if (bad < 0 && shown_tk++ < 20)
      fprintf(stderr, "xlive tk: vb %ld: texture tbp %04x psm %02x: the cache's layout differs (%d x %d of %d x %d at %d,%d, stored %d x %d, repeat %d;"
              " the map's %d x %d of %d x %d at %d,%d, stored %d x %d, repeat %d)\n", cur_vb, (unsigned)(t0 & 0x3fff), (unsigned)(t0 >> 20 & 63),
              k.t.w, k.t.h, k.t.tw, k.t.th, k.t.u0, k.t.v0, k.t.bw, k.t.rows, k.t.repeat, t->w, t->h, t->tw, t->th, t->u0, t->v0, t->bw, t->rows, t->repeat); }
  if (!n) return;
  if (k.npal != n) { ck.tk_pbad++; return; }
  for (int i = 0; i < n; i++) {
    int64_t v = gu_clut(map, cpsm, csa * 16 + i); uint32_t want;
    if (v < 0) continue;   /* an entry the map does not know: as above */
    if (cpsm == GS_CT32 || cpsm == GS_CT24) { uint32_t a = cpsm == GS_CT24 ? (uint32_t)(gr[0x3b] & 0xff) : (uint32_t)v >> 24;
      want = ((uint32_t)v & 0xffffff) | (a >= 0x80 ? 0xffu : a * 2) << 24; }
    else { uint32_t c = (uint32_t)v, col = (c & 31) << 3 | (c >> 5 & 31) << 11 | (c >> 10 & 31) << 19, texa = (uint32_t)gr[0x3b],
             a = (c >> 15 & 1) ? (uint32_t)(gr[0x3b] >> 32 & 0xff) : ((texa >> 15 & 1) && !(c & 0x7fff)) ? 0 : (texa & 0xff);
           want = col | (a >= 0x80 ? 0xffu : a * 2) << 24; }
    if (k.pal[i] != want) {
      ck.tk_pbad++;
      if (shown_tk++ < 20)
        fprintf(stderr, "xlive tk: vb %ld: palette entry %d (cpsm %x csa %d): %08x in the cache, %08x from the map\n", cur_vb, i, cpsm, csa, k.pal[i], want);
      break;
    }
  }
}
static void tex_use(const uint64_t *gr, int ctxt, const int *box)
{
  uint64_t t0 = gr[0x06 + ctxt], cl = gr[0x08 + ctxt]; int psm = (int)(t0 >> 20 & 63), cpsm = (int)(t0 >> 51 & 15), csa = (int)(t0 >> 56 & 31);
  const GuStats *U = gu_stats(map);
  ck.uses++;
  if (psm == GS_CT32 || psm == GS_CT24 || psm == GS_CT16 || psm == GS_CT16S) {   /* direct colour: the texels its box samples, else the
                                                                                    whole texture (its clamp region) */
    int tw = 1 << (t0 >> 26 & 15), th = 1 << (t0 >> 30 & 15), wms = (int)(cl & 3), wmt = (int)(cl >> 2 & 3), all[4];
    Fetch f = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 1, 0, 0 };
    if (!box) { all[0] = wms == 2 ? (int)(cl >> 4 & 1023) : 0; all[2] = wms == 2 ? (int)(cl >> 14 & 1023) : tw - 1;
                all[1] = wmt == 2 ? (int)(cl >> 24 & 1023) : 0; all[3] = wmt == 2 ? (int)(cl >> 34 & 1023) : th - 1; box = all; }
    uint64_t bk = (uint64_t)box[0] | (uint64_t)box[1] << 11 | (uint64_t)box[2] << 22 | (uint64_t)box[3] << 33;
    if (seen_before(seen_tex, t0 & 0x3ffffffffull, bk ^ cl * 0x9e3779b97f4a7c15ull, U->xfers + U->xfer_local + U->drawn)) return;
    ck.direct++;
    for (int y = box[1]; y <= box[3]; y++) for (int x = box[0]; x <= box[2]; x++) fetch(&f, x, y);
    kc_add(&f);
    if (tk_on && (psm == GS_CT16 || psm == GS_CT16S)) {   /* the texture path takes 16-bit textures as GE 5551 */
      size_t sz = tc_texture_size(t0, cl); TcTex t; Fetch g = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 1, 0, 0 };
      if (sz && sz <= sizeof txbuf && !tc_texture_cb(t0, cl, fetch, &g, &t, txbuf)) tk_check(gr, ctxt, &t, txbuf, &g);
    }
    return;
  }
  if (psm != GS_T8 && psm != GS_T4 && psm != GS_T8H && psm != GS_T4HL && psm != GS_T4HH) { ck.nonidx++; return; }
  if (gr[0x14 + ctxt] >> 2 & 7) ck.mip++;   /* TEX1 MXL: only level 0 is read here */
  int n = (psm == GS_T8 || psm == GS_T8H) ? 256 : 16;
  if (!seen_before(seen_pal, (uint64_t)cpsm << 8 | (uint64_t)csa, (uint64_t)n, U->loads)) {   /* entries csa * 16 + i, as gsref reads them */
    ck.pchecks++;
    for (int i = 0; i < n; i++) {
      int k = csa * 16 + i; int64_t v = gu_clut(map, cpsm, k), r;
      gu_read_sent(map, 1); r = gu_clut(map, cpsm, k); gu_read_sent(map, 0);
      ck.pal++;
      if (v < 0 || r < 0) { if (v < 0 && r < 0) ck.pmiss++; else ck.pone++; continue; }
      if (v != r) { ck.pdiff++;
        if (shown_pal++ < 20) fprintf(stderr, "xlive tex: vb %ld: palette entry %d (CPSM %x): %llx from EE RAM now, %llx as sent\n", cur_vb, k, cpsm, (unsigned long long)v, (unsigned long long)r); }
    }
  }
  size_t sz = tc_texture_size(t0, cl); TcTex t; Fetch f = { psm, (uint32_t)(t0 & 0x3fff), (uint32_t)(t0 >> 14 & 63), t0, cl, 0, 0, 0, 0 };
  if (box) {   /* a primitive: the texels in its box */
    uint64_t bk = (uint64_t)box[0] | (uint64_t)box[1] << 11 | (uint64_t)box[2] << 22 | (uint64_t)box[3] << 33;
    if (seen_before(seen_tex, t0 & 0x3ffffffffull, bk ^ cl * 0x9e3779b97f4a7c15ull, U->xfers + U->xfer_local + U->drawn)) return;
    if (!sz) { ck.unsup++; if (tx_detail && first_tell(t0)) tell("not convertible", t0, cl, -1, -1); }
    ck.boxes++;
    for (int y = box[1]; y <= box[3]; y++) for (int x = box[0]; x <= box[2]; x++) fetch(&f, x, y);
    kc_add(&f); return;
  }
  if (seen_before(seen_tex, t0 & 0x3ffffffffull, cl, U->xfers + U->xfer_local + U->drawn)) return;   /* TBP0 TBW PSM TW TH, CLAMP */
  if (!sz || sz > sizeof txbuf || tc_texture_cb(t0, cl, fetch, &f, &t, txbuf)) { ck.unsup++; if (tx_detail && first_tell(t0)) tell("not convertible", t0, cl, -1, -1); kc_add(&f); return; }
  ck.conv++; kc_add(&f);
  if (tk_on) tk_check(gr, ctxt, &t, txbuf, &f);
}
static int call_ctxt = -1;   /* the current call's PRIM context, for its P06 passes (-1: the call does not texture) */
static void on_pass(void *ctx, const XlCall *c, int pass)
{
  (void)ctx;
  if (call_ctxt >= 0) { ck.passes++; tex_use(xl_front()->gr, call_ctxt, 0); }
  if (gb_now) gl_pass(&glf, c, pass);
}
static void tex_call(const XlCall *c)
{
  uint64_t prim = c->gr[0x00]; call_ctxt = -1;
  if (c->tag_qa == ~0u) { ck.notpl++; return; }   /* no tag template: the PRIM it draws with is not known */
  if (!(prim >> 4 & 1)) return;
  call_ctxt = (int)(prim >> 9 & 1); ck.calls++; tex_use(c->gr, call_ctxt, 0);
}
static void tx_line(const TxCount *k, long a, long b)
{
  fprintf(stderr, "xlive tex: vb %ld..%ld: uploads %ld (%ld KB), keys new %ld, seen %ld, seen with other content %ld; transfers compared %ld, "
          "differing %ld (%ld distinct, %ld bytes); texture uses %ld (calls %ld, P06 passes %ld, primitives %ld), other formats %ld, call without "
          "a template %ld, mipmapped %ld; conversions %ld, primitive boxes %ld, not convertible %ld: texels %ld, unknown %ld, known one way %ld, differing %ld; "
          "palettes %ld: entries %ld, unknown %ld, known one way %ld, differing %ld; direct colour %ld: texels %ld, unknown %ld, known one way %ld, "
          "differing %ld; the texture path: %ld uses (%ld textures, %ld render targets, %ld not known, %ld refused), %ld compared, %ld with a "
          "differing texel, %ld with a differing palette\n", a, b, k->xfers, k->bytes >> 10, k->keynew, k->keyhit,
          k->keybad, k->scanned, k->scan_bad, k->scan_badx, k->scan_bytes, k->uses, k->calls, k->passes, k->prims, k->nonidx, k->notpl, k->mip,
          k->conv, k->boxes, k->unsup, k->texels, k->tmiss, k->tone, k->tdiff, k->pchecks, k->pal, k->pmiss, k->pone, k->pdiff,
          k->direct, k->dtexels, k->dmiss, k->done1, k->ddiff,
          k->tk_uses, k->tk_tex, k->tk_rt, k->tk_lost, k->tk_unsup, k->tk_cmp, k->tk_bad, k->tk_pbad);
}
static void tx_add(void)
{
  long *s = (long *)&ck, *d = (long *)&ck_all;
  for (size_t i = 0; i < sizeof ck / sizeof (long); i++) d[i] += s[i];
  memset(&ck, 0, sizeof ck);
}

/* each quadword as ee_io.h's pkt_fnv / pkt_tag hash it (a tag: only the fields that act) */
static void seen(void *ctx, int ch, uint32_t a, const uint8_t *q, int tag, uint32_t chcr)
{
  uint32_t w[4]; const uint8_t *p = q; (void)ctx; (void)a;
  if (tag) { uint32_t id; memcpy(w, q, 16); id = (w[0] >> 28) & 7;
    w[0] &= 0xf000ffffu; if (id == 1 || id == 6 || id == 7) w[1] = 0; if (ch != 1 || !(chcr & 0x40)) w[2] = w[3] = 0; p = (const uint8_t *)w; }
  for (int i = 0; i < 16; i++) { ph ^= p[i]; ph *= 0x100000001b3ull; }
}
static long ncall, nprim; static int sync_on, dig_on, cg_on; static double t_walk; static uint8_t lvm[16384] __attribute__((aligned(16)));
#define EC_N 256
typedef struct { uint32_t entry; long calls, notpl, first; } EntCnt;
static EntCnt ec[EC_N]; static int ec_n, ec_last, census_n; static long census_at[32];
static void ec_add(const XlCall *c)
{
  int i = ec_last;
  if (i >= ec_n || ec[i].entry != c->entry) {
    for (i = 0; i < ec_n && ec[i].entry != c->entry; i++) {}
    if (i == ec_n) { if (ec_n == EC_N) return; ec[ec_n++] = (EntCnt){ c->entry, 0, 0, cur_vb };
      fprintf(stderr, "xlive: vb %ld: entry %06x called first (%s)\n", cur_vb, (unsigned)c->entry, xl_rule(c->entry) ? "a rule" : "no rule"); }
    ec_last = i;
  }
  ec[i].calls++; if (c->tag_qa == ~0u) ec[i].notpl++;
}
static void ec_report(long vb)
{
  for (int i = 0; i < ec_n; i++) fprintf(stderr, "xlive census: to vb %ld: entry %06x (%s): %ld calls, %ld without a tag template, the first in vb %ld\n",
                                         vb, (unsigned)ec[i].entry, xl_rule(ec[i].entry) ? "a rule" : "no rule", ec[i].calls, ec[i].notpl, ec[i].first);
}
static void on_trx(void *ctx, const uint64_t *gr)
{
  uint64_t b = gr[0x50], p = gr[0x51], r = gr[0x52]; int dir = (int)(gr[0x53] & 3), dpsm = (int)(b >> 56 & 63); (void)ctx;
  if (dir == 2)
    fprintf(stderr, "xlive trx: vb %ld: local copy: sbp %04x sbw %u spsm %02x at %u,%u -> dbp %04x dbw %u dpsm %02x at %u,%u, %ux%u, dir %u\n", cur_vb,
            (unsigned)(b & 0x3fff), (unsigned)(b >> 16 & 63), (unsigned)(b >> 24 & 63), (unsigned)(p & 0x7ff), (unsigned)(p >> 16 & 0x7ff),
            (unsigned)(b >> 32 & 0x3fff), (unsigned)(b >> 48 & 63), (unsigned)dpsm, (unsigned)(p >> 32 & 0x7ff), (unsigned)(p >> 48 & 0x7ff),
            (unsigned)(r & 0xfff), (unsigned)(r >> 32 & 0xfff), (unsigned)(p >> 59 & 3));
  else if (trx_all || (dpsm != GS_CT32 && dpsm != GS_CT24 && dpsm != GS_T8H && dpsm != GS_T4HL && dpsm != GS_T4HH))
    fprintf(stderr, "xlive trx: vb %ld: upload %ld: dbp %04x dbw %u dpsm %02x at %u,%u, %ux%u\n", cur_vb, gu_stats(map)->xfers + 1, (unsigned)(b >> 32 & 0x3fff), (unsigned)(b >> 48 & 63),
            (unsigned)dpsm, (unsigned)(p >> 32 & 0x7ff), (unsigned)(p >> 48 & 0x7ff), (unsigned)(r & 0xfff), (unsigned)(r >> 32 & 0xfff));
}
/* the volume census (MGS2_XLIVE_VCEN=1) */
static int vc_on; static XlStats vc_prev; static long vc_calls[3], vc_pcalls[3], vc_pprim, vc_pqw;
static void vc_line(long vb)
{
  const XlStats *S = xl_stats(); long cw = 0, d[4] = { 0, 0, 0, 0 };   /* data words: UNPACK, MPG, DIRECT, the rest */
  for (int c = 0; c < 128; c++) {
    cw += S->cmds[c] - vc_prev.cmds[c];
    d[(c & 0x60) == 0x60 ? 0 : c == 0x4a ? 1 : (c == 0x50 || c == 0x51) ? 2 : 3] += S->wd[c] - vc_prev.wd[c]; }
  fprintf(stderr, "xlive vc: vb %ld w %ld cw %ld up %ld mpg %ld dir %ld oth %ld g1 %ld g2 %ld g3 %ld i2 %ld i3 %ld qw %ld calls %ld %ld %ld prims %ld uq",
          vb, S->words - vc_prev.words, cw, d[0], d[1], d[2], d[3], S->gq[1] - vc_prev.gq[1], S->gq[2] - vc_prev.gq[2], S->gq[3] - vc_prev.gq[3],
          S->img_qw[2] - vc_prev.img_qw[2], S->img_qw[3] - vc_prev.img_qw[3], M.qw - vc_pqw, vc_calls[0] - vc_pcalls[0], vc_calls[1] - vc_pcalls[1],
          vc_calls[2] - vc_pcalls[2], nprim - vc_pprim);
  for (int f = 0; f < 32; f++) if (S->uq[f] != vc_prev.uq[f]) fprintf(stderr, " %02x:%ld", f, S->uq[f] - vc_prev.uq[f]);
  fputc('\n', stderr);
  vc_prev = *S; memcpy(vc_pcalls, vc_calls, sizeof vc_calls); vc_pprim = nprim; vc_pqw = M.qw;
}
static void vc_start(void) { vc_prev = *xl_stats(); memcpy(vc_pcalls, vc_calls, sizeof vc_calls); vc_pprim = nprim; vc_pqw = M.qw; }
static void on_call(void *ctx, const XlCall *c)
{
  (void)ctx; ncall++; ec_add(c);
  if (sync_on) xl_vm_sync();
  if (dig) { XlCall l = *c; xl_vm_logical(lvm); l.vm = lvm; xdg_call(&l); }   /* the VU memory as it reads (spans: pending quadwords decoded) */
  if (tx_on) tex_call(c);
  if (vc_on) vc_calls[gl_conv_entry(c->entry) ? 0 : xl_rule(c->entry) ? 1 : 2]++;
  if (gb_now) gl_call(&glf, c);
}
/* MGS2_XLIVE_RTLOG (dev): a textured primitive's TEX0 / CLAMP / TEXA / FRAME / ZBUF, texel box and what the map says of its texture's
   area: the whole, the box, and the 64 x 64 squares that hold a drawn area */
static long rt_from = -1, rt_to = -1; static int rt_shown;
static void rt_log(const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  const uint64_t *gr = g->gr; int c = d->ctxt, box[4], fw = 0, fx = -1, nd = 0;
  uint64_t t0 = gr[0x06 + c], cl = gr[0x08 + c], texa = gr[0x3b];
  int psm = (int)(t0 >> 20 & 63), tw = 1 << (t0 >> 26 & 15), th = 1 << (t0 >> 30 & 15), hb = prim_box(d, v, n, box);
  uint32_t tbp = (uint32_t)(t0 & 0x3fff), tbw = (uint32_t)(t0 >> 14 & 63);
  gu_area_id(map, psm, tbp, tbw, 0, 0, tw - 1, th - 1, &fw);
  if (hb) gu_area_id(map, psm, tbp, tbw, box[0] < 0 ? 0 : box[0], box[1] < 0 ? 0 : box[1], box[2], box[3], &fx);
  fprintf(stderr, "xlive rt: vb %ld path %d prim %d: TEX0 tbp %04x tbw %u psm %02x %dx%d tcc %d tfx %d, CLAMP %d/%d, TEXA ta0 %02x aem %d ta1 %02x, "
          "FRAME fbp %03x fbw %u psm %02x fbmsk %08x, ZBUF zbp %03x psm %02x zmsk %d, blend %d %d %d %d fix %d, box %d..%d %d..%d (%s); map: whole %d, box %d;",
          cur_vb, g->path, d->prim, (unsigned)tbp, (unsigned)tbw, psm, tw, th, d->tcc, d->tfx, (int)(cl & 3), (int)(cl >> 2 & 3),
          (unsigned)(texa & 0xff), (int)(texa >> 15 & 1), (unsigned)(texa >> 32 & 0xff), (unsigned)(d->fbp / 32), (unsigned)d->fbw, d->fpsm,
          (unsigned)d->fbmsk, (unsigned)(d->zbp / 32), d->zpsm, d->zmsk, d->ba, d->bb, d->bc, d->bd, d->fix, box[0], box[2], box[1], box[3],
          hb ? "valid" : "none", fw, fx);
  for (int y = 0; y < th; y += 64) for (int x = 0; x < tw; x += 64) {
    int f2 = 0; gu_area_id(map, psm, tbp, tbw, x, y, x + 63 < tw ? x + 63 : tw - 1, y + 63 < th ? y + 63 : th - 1, &f2);
    if ((f2 & GU_ID_DRAWN) && nd++ < 24) fprintf(stderr, " drawn %d,%d", x, y);
  }
  fprintf(stderr, " (%d squares drawn)\n", nd); rt_shown++;
}
static void on_prim(void *ctx, const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n)
{
  int box[4]; (void)ctx; nprim++; xdg_prim(g, d, v, n);
  if (tx_on && d->tme) { ck.prims++; tex_use(g->gr, d->ctxt, prim_box(d, v, n, box) ? box : 0); }
  if (d->tme && cur_vb >= rt_from && cur_vb <= rt_to && rt_shown < 80) rt_log(g, d, v, n);
  if (gb_now) gl_prim(&glf, g, d, v, n);
}

static void setup(void);
void xlive_chk_dma(const uint8_t *mem, const uint8_t *spr, int ch, uint32_t chcr, uint32_t madr, uint32_t qwc, uint32_t tadr, int one)
{
  if (from == -2) setup();
  if (!on) return;
  gx_seen = 1;                            /* MGS2_XLIVE_GX: frames begin from here on (the PSP's translator starts at its first transfer) */
  if (skip_now) { static long last = -1; sk_tr++; if (cur_vb != last) { sk_vb++; last = cur_vb; } return; }
  M.mem = mem; M.mem_size = GSZ; M.spr = spr;
  if (one_on && one) { one_tr++; g_xl_one = 1; clock_gettime(CLOCK_MONOTONIC, &one_t0); return; }   /* the exact walk feeds it */
  struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
  xlv_dma(&M, ch, chcr, madr, qwc, tadr);
  clock_gettime(CLOCK_MONOTONIC, &b); t_walk += (double)(b.tv_sec - a.tv_sec) * 1e3 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
}

void xlive_one_end(void)   /* runtime/ee_io.h: the end of a VIF1 transfer the exact walk fed to the translator */
{
  struct timespec b;
  xl_flush(); g_xl_one = 0;   /* the game may change this RAM before the next transfer */
  clock_gettime(CLOCK_MONOTONIC, &b); t_walk += (double)(b.tv_sec - one_t0.tv_sec) * 1e3 + (double)(b.tv_nsec - one_t0.tv_nsec) / 1e6;
}

static void totals(void)
{
  const XlStats *S = xl_stats(); const GfGs *F = xl_front(); const GuStats *U = gu_stats(map);
  fprintf(stderr, "xlive: vb %ld..%ld: %ld transfers, %ld tags, %ld quadwords (%ld chains cut at 1,000,000 tags); translator: %ld VIF1 words, "
          "%ld calls (%ld MSCNT, %ld without an entry), %ld callbacks, A+D packets %ld (%ld without an end), GIF tags path 1/2/3 %ld/%ld/%ld, "
          "IMAGE quadwords %ld, unknown VIF commands %ld, filling writes %ld, MSCNT after MSCNT %ld (the first after entry %06x)\n", from, to - 1,
          M.transfers, M.tags, M.qw, M.guard_stops, S->words, S->calls, S->calls_cont, S->calls_noentry, ncall, S->ad_kicks, S->ad_bad,
          S->gif_tags[1], S->gif_tags[2], S->gif_tags[3], S->gif_image_qw, S->unk, S->fills, S->cont2, (unsigned)S->cont2_after);
  fprintf(stderr, "xlive: front end primitives by PRIM %ld %ld %ld %ld %ld %ld %ld (%ld to the callback), zero-area %ld, PRIM 7 %ld, uncertain %ld\n",
          F->prims[0], F->prims[1], F->prims[2], F->prims[3], F->prims[4], F->prims[5], F->prims[6], nprim, F->zero_area, F->prim7, F->uncertain);
  fprintf(stderr, "xlive: texture sources: %ld transfers (%ld other layout, %ld not modelled, %ld in pieces, %ld past 2048, %ld local, %ld local "
          "not known), %ld IMAGE without a transfer, %ld CLUT loads (%ld wide), %ld block entries dropped; areas drawn %ld (%ld draws kept "
          "them); live %d transfers, %d loads\n", U->xfers, U->xfer_other, U->xfer_unmod, U->xfer_split, U->xfer_wrap, U->xfer_local,
          U->local_unknown, U->image_orphan, U->loads, U->load_wide, U->dropped, U->drawn, U->drawn_kept, U->live_xfers, U->live_loads);
  if (M.fast) fprintf(stderr, "xlive lazy: %ld UNPACKs (%ld vectors) left to decode later; quadwords decoded later: %ld when read, %ld before an "
                      "UNPACK wrote over them, %ld at %ld flushes (%ld for a full table of UNPACKs, %ld of ROW / COL copies)\n", S->lz_cmds,
                      S->lz_vec, S->dec_read, S->dec_write, S->dec_flush, S->flushes, S->full_ud, S->full_rc);
  fprintf(stderr, "xlive time: %.3f ms a vblank in the walk, the translator and the callbacks (%s%s%s)\n", t_walk / (to - from > 0 ? to - from : 1),
          M.fast ? "spans" : "words", M.seen ? ", packet hash" : "", dig_on ? ", digests" : "");
  if (skip_on) fprintf(stderr, "xlive skip: shown frames only: %ld vblanks after reduced calls only, %ld transfers not handed over\n", sk_vb, sk_tr);
  if (one_on) fprintf(stderr, "xlive one: %ld VIF1 transfers fed by the exact walk\n", one_tr);
  if (gb_on) { char w[64]; snprintf(w, sizeof w, "xlive gb: %ld frames:", glf.all.frames); gb_line(&glf.all, glf.all_dbytes, glf.all_tbytes, w);
    fprintf(stderr, "; memo answers that differed %ld\n", glf.memo_bad); gb_line(&glf.max, glf.max.dbytes, glf.max.tbytes, "xlive gb: the largest of each in one frame:"); fputc('\n', stderr);
    if (gx_on) { const TkStats *k = tk_stats(tkc);
      fprintf(stderr, "picture: %ld frames made (the first began at vb %ld); lists: %ld that did not fit, %ld batches left out, the largest %ld "
              "words, the largest aux %ld bytes, the largest list and aux together %ld bytes (of %u); digests %ld (vb %ld..%ld), all %016llx\n",
              gx_count, gx_first, gx_fail, gx_drop, gx_wmax, gx_amax, gx_bmax, gx_listn, gx_n, gx_from, gx_to, (unsigned long long)gx_all);
      fprintf(stderr, "picture: textures: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, "
              "refused %ld (no room in the pool %ld), texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak "
              "%lld KB), evicted %ld, the heaviest frame %lld KB; pool %u KB, budget %u KB\n", k->uses, k->hits, k->misses, k->pal_hits,
              k->pal_misses, k->rt, k->lost, k->unsup, k->nomem, k->unknown, (long long)(k->conv_bytes >> 10), k->live,
              (long long)(k->live_bytes >> 10), (long long)(k->peak_bytes >> 10), k->evicted, (long long)(k->peak_frame_bytes >> 10),
              gx_pool_n >> 10, gx_pool_n / 8 * 7 >> 10);
      fprintf(stderr, "picture: texture conversions: %ld a block at a time, %ld handed back to the texel path, %ld differing from it; GS blocks "
              "read %ld, %ld of them whole from one upload\n", k->blk, k->blk_back, k->blk_diff, gu_stats(map)->blocks, gu_stats(map)->blocks_one); }
    const XcStats *x = xc_stats(); fprintf(stderr, "xlive gb: converters: calls %ld / %ld / %ld, batches %ld / %ld / %ld, empty %ld / %ld / %ld, bad %ld / %ld / %ld; "
              "effect triangles by class %ld %ld %ld %ld %ld (2870 culled %ld); polygons %ld (%ld vertices), empty %ld, culled %ld, no room %ld; "
              "df0 calls left out off the scissor %ld (%ld vertices)\n",
              x->calls[0], x->calls[1], x->calls[2], x->batches[0], x->batches[1], x->batches[2], x->empty[0], x->empty[1], x->empty[2],
              x->bad[0], x->bad[1], x->bad[2], x->tris[0], x->tris[1], x->tris[2], x->tris[3], x->tris[4], x->tris[5], x->polys, x->poly_v,
              x->poly_empty, x->poly_culled, x->poly_full, x->cut[0], x->cut_v[0]);
    fprintf(stderr, "xlive gb: P06: calls %ld (+0 %ld, +358 %ld, +738 %ld, +ab0 %ld), batches %ld, empty %ld, bad %ld; vertices %ld, exact "
              "form %ld; passes 2 and 3 drawn %ld / %ld (pass 2 with its own vertices %ld), not drawn %ld (a UV2 z not 1.0) / %ld (from "
              "T+0x104); cull bit %ld; light ramps %ld\n", x->calls[3], x->lit_kind[0], x->lit_kind[1], x->lit_kind[2], x->lit_kind[3],
              x->batches[3], x->empty[3], x->bad[3], x->lit_v, x->lit_exact, x->lit_pass[2], x->lit_pass[3], x->lit_own2, x->lit_nopass[2],
              x->lit_nopass[3], x->lit_cull, x->lit_ramp);
    fprintf(stderr, "xlive gb: P07: calls %ld, batches %ld, empty %ld, bad %ld (paths not seen %ld); vertices %ld, calls in the exact form %ld; "
              "passes 2 and 3 drawn %ld / %ld (pass 2 with its own vertices %ld), not drawn %ld (a UV2 z not 1.0) / %ld (flags 0x100 or 0x400); "
              "fast form checked against the exact form (XC_P7CHK) %ld calls, differing %ld\n", x->calls[4], x->batches[4], x->empty[4], x->bad[4],
              x->face_unseen, x->face_v, x->face_exact, x->face_pass[2], x->face_pass[3], x->face_own2, x->face_nopass[2], x->face_nopass[3],
              x->face_chk, x->face_chkdiff); }
}

/* the options, at the first transfer or vblank */
static void setup(void)
{
  const char *e = getenv("MGS2_XLIVE"); char fn[256] = ""; from = -1;
  if (e && sscanf(e, "%ld:%ld:%255s", &from, &to, fn) < 2) from = -1;
  if (from >= 0) {
    M.seen = seen;
    if (!(map = gu_new(ram, 0))) { fprintf(stderr, "xlive: no memory for the texture sources\n"); exit(1); }
    xl_on_call(on_call, 0); xl_on_prim(on_prim, 0); xl_uploads(map); xl_on_pass(on_pass, 0);
    M.fast = getenv("MGS2_XLIVE_FAST") && atoi(getenv("MGS2_XLIVE_FAST"));
    sync_on = getenv("MGS2_XLIVE_SYNC") && atoi(getenv("MGS2_XLIVE_SYNC"));
    if (getenv("MGS2_XLIVE_NOSEEN") && atoi(getenv("MGS2_XLIVE_NOSEEN"))) M.seen = 0;
    if (getenv("MGS2_XLIVE_TEX") && atoi(getenv("MGS2_XLIVE_TEX"))) {
      tx_on = 1; tx_detail = atoi(getenv("MGS2_XLIVE_TEX")) >= 2; up = calloc(UP_N, sizeof *up); srcs = calloc(SRC_N, sizeof *srcs);
      if (!up || !srcs) { fprintf(stderr, "xlive: no memory for the texture check\n"); exit(1); }
      gu_on_data(map, on_data, 0); gu_keep_sent(map, 1);
      if (getenv("MGS2_XLIVE_TK") && atoi(getenv("MGS2_XLIVE_TK"))) {   /* the PSP texture path on top of the map */
        long bud = atoi(getenv("MGS2_XLIVE_TK")); gu_keys(map, 1); if (from == 0) gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, bud > 1 ? (uint32_t)(bud * 1024) : 0))) { fprintf(stderr, "xlive: no memory for the texture cache\n"); exit(1); }
        tk_on = 1;
      }
    }
    const char *gxe = getenv("MGS2_XLIVE_GX");
    if ((getenv("MGS2_XLIVE_GB") && atoi(getenv("MGS2_XLIVE_GB"))) || gxe) {
      int m = getenv("MGS2_XLIVE_GB") && atoi(getenv("MGS2_XLIVE_GB")) ? atoi(getenv("MGS2_XLIVE_GB")) : 1;
      double zk = gxe ? 0 : getenv("MGS2_XLIVE_GB_ZK") ? atof(getenv("MGS2_XLIVE_GB_ZK")) : 0.2;
      if (gxe) {   /* the PSP harness's sizes and order (psp/xl_psp.c gl_setup) */
        long kb = getenv("MGS2_XLIVE_GX_TEXKB") ? atol(getenv("MGS2_XLIVE_GX_TEXKB")) : GL_PSP_TEXKB;
        if (sscanf(gxe, "%ld:%ld", &gx_from, &gx_to) != 2 || from != 0 || tkc || kb < 256) {
          fprintf(stderr, "xlive: MGS2_XLIVE_GX=<a>:<b> needs MGS2_XLIVE=0:<to>, no MGS2_XLIVE_TK and a pool of 256 KB or more\n"); exit(1); }
        gx_pool_n = (uint32_t)kb << 10;
        if (getenv("MGS2_XLIVE_GX_LIST")) gx_listn = (uint32_t)atol(getenv("MGS2_XLIVE_GX_LIST")) << 10;   /* measurement only: the lists then
                                                                                                             differ from the PSP's */
        if (posix_memalign((void **)&gx_pool, 64, gx_pool_n) || posix_memalign((void **)&gx_arena, 64, GL_PSP_ARENA)
            || posix_memalign((void **)&gx_lb, 64, gx_listn)) {
          fprintf(stderr, "xlive: no memory for the PSP frames\n"); exit(1); }
        memset(gx_pool, 0, gx_pool_n); memset(gx_arena, 0, GL_PSP_ARENA); memset(gx_lb, 0, gx_listn);
        gu_keys(map, 1); gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, gx_pool_n / 8 * 7)) || tk_pool(tkc, gx_pool, gx_pool_n)) { fprintf(stderr, "xlive: no texture cache\n"); exit(1); }
        if (gl_init(&glf, tkc, map, 0, GL_PSP_MAXTEX) || gb_arena(&glf.o, gx_arena, GL_PSP_ARENA, GL_PSP_NB, GL_PSP_NV)) {
          fprintf(stderr, "xlive: no memory for the live frame\n"); exit(1); }
        gx_init(&gxl, (uint32_t *)gx_lb, gx_listn / 4, gx_lb, gx_listn); gxl.ret = 1;   /* one block, as the PSP's */
        if (gx_listn != GL_PSP_LIST) fprintf(stderr, "xlive mode: a list block of %u KB (MGS2_XLIVE_GX_LIST: not the PSP's)\n", gx_listn >> 10);
        gxh.addr = gx_addr; gxh.ctx = 0; gxh.fb[0] = 0x000000; gxh.fb[1] = 0x088000; gxh.zb = 0x110000; gx_on = 1;
        fprintf(stderr, "xlive mode: the PSP harness's frames and GE lists (MGS2_XLIVE_GX), texture pool %ld KB (budget %u KB); digests of the "
                "frames whose last vblank is in %ld..%ld\n", kb, gx_pool_n / 8 * 7 >> 10, gx_from, gx_to);
      } else {
      if (!tkc) { gu_keys(map, 1); if (from == 0) gu_zero_empty(map, 1);
        if (!(tkc = tk_new(map, 0))) { fprintf(stderr, "xlive: no memory for the texture cache\n"); exit(1); } }
      if (gl_init(&glf, tkc, map, zk, 4096)) { fprintf(stderr, "xlive: no memory for the live frame\n"); exit(1); }
      }
      glf.memo_check = m >= 2; glf.zchk = m >= 3; gb_on = 1;
      if (getenv("MGS2_XLIVE_CUT") && !atoi(getenv("MGS2_XLIVE_CUT"))) { glf.h.cut = 0;   /* the PSP's OPT_GL_CUT off */
        fprintf(stderr, "xlive mode: every df0 call converted (MGS2_XLIVE_CUT=0)\n"); }
      const char *e2 = getenv("MGS2_XLIVE_GER"), *c2 = e2 ? strrchr(e2, ':') : 0;
      if (c2 && (size_t)(c2 - e2) < sizeof ger_pre) { memcpy(ger_pre, e2, (size_t)(c2 - e2)); ger_pre[c2 - e2] = 0;
        for (c2++; *c2 && ger_n < 64; ) { ger_vb[ger_n++] = strtol(c2, (char **)&c2, 10); if (*c2 == ',') c2++; else break; } }
      if (zk > 0) fprintf(stderr, "xlive mode: the live renderer's frame on shown vblanks (MGS2_XLIVE_GB=%d), GE depth = Z24 x %g%s\n", m, zk,
                          m >= 2 ? ", memo checked" : "");
      else fprintf(stderr, "xlive mode: the live renderer's frame on shown vblanks (MGS2_XLIVE_GB=%d), the PSP's depth (Z24, each frame's "
                   "range mapped)%s%s\n", m, m >= 2 ? ", memo checked" : "", m >= 3 ? ", map checked" : "");
    }
    { static const char *const sw[5] = { "MGS2_XLIVE_GUMEMO", "MGS2_XLIVE_TKFLAGS", "MGS2_XLIVE_GUCLUT", "MGS2_XLIVE_V48", "MGS2_XLIVE_T16" };
      for (int i = 0; i < 5; i++) {
        const char *e = getenv(sw[i]); if (!e) continue;
        int v = atoi(e) != 0;
        if (i == 0) gu_area_memo(map, v); else if (i == 1) { if (tkc) tk_flags(tkc, v); } else if (i == 2) gu_clut_memo(map, v); else if (i == 3) xl_ud_v48(v); else if (tkc) tk_t16(tkc, v);
        fprintf(stderr, "xlive mode: %s=%d\n", sw[i], v); } }
    if (tkc && getenv("MGS2_XLIVE_TKBLK")) { tk_blk(tkc, atoi(getenv("MGS2_XLIVE_TKBLK")));
      fprintf(stderr, "xlive mode: texture conversion %s (MGS2_XLIVE_TKBLK)\n", atoi(getenv("MGS2_XLIVE_TKBLK")) == 0 ? "texel by texel" :
              atoi(getenv("MGS2_XLIVE_TKBLK")) == 1 ? "a block at a time" : "both ways, compared"); }
    if (getenv("MGS2_XLIVE_WATCH") && sscanf(getenv("MGS2_XLIVE_WATCH"), "%x:%ld:%ld", &wbn, &wfrom, &wto) != 3) wbn = -1;
    if (getenv("MGS2_XLIVE_RTLOG") && sscanf(getenv("MGS2_XLIVE_RTLOG"), "%ld:%ld", &rt_from, &rt_to) != 2) rt_from = rt_to = -1;
    if (getenv("MGS2_XLIVE_TRX") && atoi(getenv("MGS2_XLIVE_TRX")) >= 1) { trx_all = atoi(getenv("MGS2_XLIVE_TRX")) >= 2; gu_on_trx(map, on_trx, 0); }
    for (const char *c = getenv("MGS2_XLIVE_CENSUS"); c && *c && census_n < 32; ) { census_at[census_n++] = strtol(c, (char **)&c, 10); if (*c == ',') c++; else break; }
    vc_on = getenv("MGS2_XLIVE_VCEN") && atoi(getenv("MGS2_XLIVE_VCEN"));
    if (fn[0] && !(dig = fopen(fn, "w"))) { fprintf(stderr, "xlive: cannot write %s\n", fn); exit(1); }
    dig_on = dig != 0;
    if (getenv("MGS2_XLIVE_DIGUSED") && atoi(getenv("MGS2_XLIVE_DIGUSED"))) { xdg_used(1); fprintf(stderr, "xlive mode: digests of what is read (MGS2_XLIVE_DIGUSED)\n"); }
    fprintf(stderr, "xlive: the translator live from vb %ld to %ld%s%s\n", from, to - 1, dig ? ", digests to " : "", fn);
    if (M.fast || sync_on) fprintf(stderr, "xlive mode:%s%s\n", M.fast ? " spans (MGS2_XLIVE_FAST)" : "", sync_on ? " sync before each call (MGS2_XLIVE_SYNC)" : "");
    cg_on = getenv("MGS2_XLIVE_CG") && atoi(getenv("MGS2_XLIVE_CG"));
    skip_on = getenv("MGS2_XLIVE_SKIP") && atoi(getenv("MGS2_XLIVE_SKIP")); sk_c0 = g_dg_calls[0]; sk_c1 = g_dg_calls[1];
    if (skip_on) fprintf(stderr, "xlive mode: shown frames only (MGS2_XLIVE_SKIP)\n");
    one_on = M.fast && getenv("MGS2_XLIVE_ONE") && atoi(getenv("MGS2_XLIVE_ONE"));
    if (one_on) fprintf(stderr, "xlive mode: one walk (MGS2_XLIVE_ONE)\n");
    if (from == 0) { on = 1; xl_reset(); xdg_open(dig); vc_start(); if (cg_on) CALLGRIND_START_INSTRUMENTATION; }
  }
}
/* each vblank start (boot.c k_vblank_hook, after pkt_vblank): the line of the vblank that ended, then the window */
void xlive_chk_vblank(long vb)
{
  if (from == -2) setup();
  if (from < 0 || done) return;
  if (on) fprintf(stderr, "xlive: vb %ld %016llx\n", vb - 1, (unsigned long long)ph);
  if (gb_now) { gb_frame_end(vb - 1); gb_now = 0; }
  { int k = (g_dg_calls[0] != sk_c0) | (g_dg_calls[1] != sk_c1) << 1; sk_c0 = g_dg_calls[0]; sk_c1 = g_dg_calls[1];   /* the vblank that ended */
    skip_now = skip_on && k == 2;
    if (gb_on && (k & 1) && ((on && vb < to) || (!on && vb == from)) && (!gx_on || gx_seen)) { gb_now = 1; gl_begin(&glf);
      if (gx_on && gx_first < 0) { gx_first = vb; fprintf(stderr, "picture: the first frame begins at vb %ld\n", vb); } } }
  if (on && vc_on) vc_line(vb - 1);
  if (on) for (int k = 0; k < census_n; k++) if (census_at[k] == vb - 1) ec_report(vb - 1);
  ph = 0xcbf29ce484222325ull;
  if (!on && vb == from) { on = 1; xl_reset(); xdg_open(dig); vc_start(); if (cg_on) CALLGRIND_START_INSTRUMENTATION; }
  if (tkc) tk_frame(tkc);
  if (map && wbn >= 0) gu_watch(map, vb >= wfrom && vb <= wto ? wbn : -1);
  if (on && tx_on) {   /* the vblank that ended: transfers whose EE bytes changed since they were sent; every 100 vblanks a line */
    ck.scanned += gu_sent_scan(map, scan_fn, 0);
    if (vb > from && ((vb - from) % 100 == 0 || vb >= to)) { tx_line(&ck, vb - 1 - (vb - 1 - from) % 100, vb - 1); tx_add(); }
  }
  cur_vb = vb;
  if (on && vb >= to) { on = 0; done = 1; if (cg_on) CALLGRIND_STOP_INSTRUMENTATION; xdg_close(); if (dig) fclose(dig); dig = 0; totals();
    ec_report(vb - 1);
    if (vc_on) { const XlStats *S = xl_stats();   /* the UNPACK kinds of the window */
      for (int i = 0; i < S->nuk; i++) { uint64_t k = S->uk[i];
        fprintf(stderr, "xlive vc kind: format %02x mode %u usn %u flg %u cl %u wl %u mask %08x: %ld commands, %ld vectors\n", (unsigned)(k & 0x1f),
                (unsigned)(k >> 5 & 3), (unsigned)(k >> 7 & 1), (unsigned)(k >> 8 & 1), (unsigned)(k >> 16 & 0xff), (unsigned)(k >> 24 & 0xff),
                (unsigned)(k >> 32), S->uk_cmds[i], S->uk_vec[i]); }
      for (int i = 0; i < S->nent; i++) fprintf(stderr, "xlive vc entry: %06x (%s): %ld calls, %ld vectors unpacked before them\n", (unsigned)S->ent[i],
                                                  S->ent[i] == XL_DF0 || S->ent[i] == XL_FX || S->ent[i] == XL_FX2 ? "converter" : xl_rule(S->ent[i]) ? "rule" : "no rule",
                                                  S->ent_n[i], S->ent_uq[i]); }
    if (tkc) { const TkStats *k = tk_stats(tkc);
      fprintf(stderr, "xlive tk: the texture path: %ld uses, textures %ld hits / %ld conversions, palettes %ld / %ld; render targets %ld, not known %ld, "
              "refused %ld, texels or entries the map could not give %ld; converted %lld KB, kept %ld (%lld KB, peak %lld KB), evicted %ld, "
              "the heaviest frame %lld KB\n", k->uses, k->hits, k->misses, k->pal_hits, k->pal_misses, k->rt, k->lost, k->unsup, k->unknown,
              (long long)(k->conv_bytes >> 10), k->live, (long long)(k->live_bytes >> 10), (long long)(k->peak_bytes >> 10), k->evicted,
              (long long)(k->peak_frame_bytes >> 10));
      fprintf(stderr, "xlive tk: conversions: %ld a block at a time, %ld handed back to the texel path, %ld differing from it; GS blocks read %ld, "
              "%ld of them whole from one upload\n", k->blk, k->blk_back, k->blk_diff, gu_stats(map)->blocks, gu_stats(map)->blocks_one); }
    if (tx_on) { fprintf(stderr, "xlive tex: totals, "); tx_line(&ck_all, from, vb - 1); fprintf(stderr, "xlive tex: %ld distinct upload keys\n", up_n); src_report(vb - from); kc_report(); }
    return; }
  if (on) xdg_vblank(vb);
}
