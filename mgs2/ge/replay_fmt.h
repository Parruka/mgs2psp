/* replay_fmt.h: the GE replay file. tools/gsref.c (GSREF_GEOUT=<file>:<vblank>) writes one PS2 frame's draws as GE batches;
   psp/ge_replay draws them on the PSP to measure the GE's cost of the frame. Game data: personal use only, never published.
   All little-endian; the PSP reads the file into memory as is. */
#ifndef MGS2_REPLAY_FMT_H
#define MGS2_REPLAY_FMT_H
#include <stdint.h>

#define GER_MAGIC 0x32524547u   /* "GER2" (GER1 had no transform batches) */
typedef struct { uint32_t magic, vb, nbatch, nvert, ntex, ndata; uint32_t off_batch, off_vert, off_tex, off_data; uint32_t w, h;
                 float zscale;   /* GE depth = the PS2's Z24 x zscale (1/256 maps all 24 bits; a frame's own range keeps more precision) */
                 float zlo, zhi; /* zscale 1 (the live renderer's raw depth, ge/gelive.h zraw): the Z24 range the frame's depth map covers
                                    (ge/gelist.h gx_depth_map; zlo > zhi: none); older files: 0 */
               } GerHeader;

/* GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D: u, v in texels of the GE texture; colour as the PS2's
   (0x80 = 1.0 for modulate) with alpha doubled (0x80 -> 0xff); x, y in PSP pixels (the PS2's 512 x 512 scaled to 480 x 272);
   z = the PS2's Z24 x zscale, at most 65535 */
typedef struct { float u, v; uint32_t c; float x, y, z; } GerVert;

enum { GERF_GOURAUD = 1, GERF_TEX = 2, GERF_BLEND = 4, GERF_ATEST = 8, GERF_ZTEST = 16, GERF_ZWRITE = 32, GERF_FOG = 64, GERF_TCC = 128 };
typedef struct {
  uint16_t tex;                        /* texture index, 0xffff: none */
  uint8_t prim;                        /* GU_POINTS (0) .. GU_SPRITES (6) */
  uint8_t flags;                       /* GERF_* */
  uint8_t ztst, atst, aref, afail;     /* the PS2's TEST_1 fields */
  uint8_t ba, bb, bc, bd, fix, tfx;    /* ALPHA_1 A B C D, FIX; TEX0 TFX */
  uint8_t twopass;                     /* 1: AFAIL FB_ONLY with Z write, and a texel or vertex alpha below AREF */
  uint8_t kind;                        /* GERK_THROUGH: first / count are GerVert; GERK_XFORM: see GerXform */
  int16_t sx0, sy0, sx1, sy1;          /* scissor in PSP pixels, inclusive */
  uint32_t entry, first, count, px;    /* VU1 entry (guest address), first vertex, vertices, pixels the PS2 wrote */
} GerBatch;

typedef struct {
  uint8_t kind;                        /* 1: indexed with a palette, 2: a frame buffer */
  uint8_t psm;                         /* kind 1: 4 (CLUT4) or 8 (CLUT8); kind 2: 0 = the buffer being drawn, 1 = the other one */
  uint8_t repeat;                      /* bit 0: u repeats, bit 1: v repeats; else clamp */
  uint8_t minalpha;                    /* lowest PS2 alpha among the palette entries the texture uses */
  uint16_t tw, th, bw, rows;           /* GE size (powers of two), stored width (texels) and rows; swizzled */
  uint32_t data, size, pal, npal;      /* byte offsets in the data block; npal 8888 entries (alpha doubled) */
} GerTex;
enum { GERK_THROUGH = 0, GERK_XFORM = 1, GERK_XFORMF = 2, GERK_LIT = 3 };

/* GERK_XFORM: one VU1 call drawn with hardware transform (P04+df0 so far, ge/vu1conv.h). first = byte offset in the data block of a
   GerXform, followed by nv vertices (ge/vu1conv.h VcVert: GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT), nst strips (uint16 first
   vertex, uint16 count) and nidx uint16 indices (the same strips joined into one by repeated vertices); count = nst.  The PS2's own
   primitives of that call are also in the file, as GERK_THROUGH batches with the same entry: a player draws one or the other.
   The batch's state fields are the GS state of the call's first primitive; prim is GU_TRIANGLE_STRIP. */
typedef struct {
  float world[12];                     /* sceGuSetMatrix(GU_MODEL) rows x, y, z, w (3 values each) */
  float proj[16];                      /* sceGuSetMatrix(GU_PROJECTION) */
  float vs[3], vo[3];                  /* GE viewport scale and centre: x, y in GE coordinates (screen 480 x 272 at offset 1808, 1912), z 16-bit */
  float fog1, fog2; uint32_t fogcol;   /* GE FOG1 / FOG2 (fog factor = (view z + fog1) * fog2, 1 = no fog) and the PS2 FOGCOL */
  float su, sv, tu, tv;                /* sceGuTexScale / sceGuTexOffset for the batch's GE texture */
  uint32_t nv, nst, nidx, nfan, nfv, pad;
} GerXform;
/* GERK_XFORMF: one VU1 call with float world positions (P04+25f0 / +2870, ge/vu1conv.h vc_fpos) drawn with hardware transform.
   first = byte offset of a GerXform (world without the x 32768; su / sv for UVs in the PS2 texture's units), followed by nv + nfv
   VcVertF (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF): the call's vertices, then its clipped polygons' vertices (world
   positions, colours and UVs as the PS2's clipper makes them: ge/vu1conv.h vc_clip_tri); then nv uint8 triangle classes (padded to 4
   bytes), the class of the triangle that ends at vertex i: 0 none (vertex 0 or 1, or the strip's restart flag), 1 inside the clip volume
   (the PS2 draws it in its strip), 2 outside one clip plane (the PS2 drops it), 3 crossing the screen edge only, with every vertex inside
   the GE's 0..4096 range and the frame's depth window (the GE can draw it whole and let the scissor cut it), 4 any other crossing (the
   PS2's clipper polygon is needed); then nfan pairs uint16 (the triangle's last vertex i, its polygon's vertex count), for every class 3
   or 4 triangle whose polygon has 3 or more vertices, in order, the polygons' vertices following each other from index nv.
   nst = nidx = 0; count = nv.  The batch's state is the call's primitives' (strip and fan templates differ only in the primitive). */
/* GERK_LIT: one texture pass of a P06 call (ge/vu1conv.h vc_p06: Snake, the crew, the guards) or a P07 call (vc_p07: the codec faces)
   drawn with hardware transform, the GE clipping as for df0.  first = byte offset in the data block of the pass's GerLit, which says
   where, counted from itself, the call's GerXform (world, projection, viewport, fog: shared by its passes; su .. tv unused), its
   vertices and its strips (uint16 first vertex, uint16 count, as GERK_XFORM's) are: passes 1 and 2 draw the same vertices (pass 2's
   texture offset holds the call's UV2 - UV1) or pass 2 its own, pass 3 its own.  P06's fog is the call's constant F: fog1 = F / 255 x
   2^40, fog2 = 2^-40, so the GE's factor is F / 255 at any depth; P07's the PS2's per-vertex F = a w + b: fog1 = b / a, fog2 = a / 255
   (the constant form when a is 0).
   count = nst.  The batch's state is the pass's (its A+D packet's texture, clamp and blend). */
typedef struct {
  int32_t xf, vtx, run;                /* byte offsets from this GerLit: the GerXform, the vertices, the strips */
  uint32_t vtype;                      /* the vertices' GE vertex type (sceGuDrawArray's, GU_TRANSFORM_3D): VcVert, VcVertE, VcVertS, VcVertF */
  uint32_t stride, nv, nst, pass;      /* bytes a vertex; vertices; strips; the pass (1 .. 3) */
  float su, sv, tu, tv;                /* sceGuTexScale / sceGuTexOffset for the pass's GE texture */
} GerLit;
#endif
