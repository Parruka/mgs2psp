/* vu1conv.h: one VU1 micro call's input (the VU1 data memory as the call starts) -> GE draws with hardware transform.
   renderer_notes §11.2; the micro programs' data layout: re/vu1_programs.md §2-3.  Plain C, no PSP dependencies: the
   harness's packet translator and the container tools (gsref) share it. */
#ifndef MGS2_VU1CONV_H
#define MGS2_VU1CONV_H
#include <stdint.h>
#include <string.h>   /* (vc_clip_in) */

/* GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT, 16 bytes.
   u, v: the PS2's 4.12 UV, 0 to 8 (the GE reads u / 32768, so the texture scale carries x 8; the deck's df0 UVs are 0..4094: a negative
         one is refused, since the GE's reading of 16-bit texture coordinates as signed or unsigned is not checked on the hardware yet);
   c: the PS2's colour (0x80 = 1.0 for modulate) with alpha doubled (0x80 -> 0xff);
   x, y, z: the PS2's s16 position (the GE reads s / 32768; the world matrix carries the x 32768). */
typedef struct { uint16_t u, v; uint32_t c; int16_t x, y, z, pad; } VcVert;

typedef struct {
  float world[12];       /* GE world matrix, 4 rows of 3 (sceGuSetMatrix order: the images of x, y, z and the translation):
                            the GE's view-space x, y, z are the PS2's clip x, clip y and clip w */
  float pa, pb;          /* projection: GE z = pa w - pb = minus the PS2's z_clip, so the PS2's near plane (z_clip = w) is the GE's z = -w */
  float resid;           /* max |M row 2 + pa M row 3 - pb e4| / max |M row 2|: how far the call's matrix is from that form */
  float fog_a, fog_b;    /* PS2 fog F = clamp(a w + b, 0, 255) (abs 0x02) */
  float vs[3], vo[3];    /* PS2 viewport: X = vs x / w + vo (abs 0x04 / 0x05) */
  float m[16];           /* the call's object -> clip matrix as the VU reads it: columns 0-3 (T+0..3, or T+0x10..0x13), xyzw each */
  int n, clip;           /* vertices; header.w (0: the program's no-clip path) */
  int alpha;             /* the constant vertex alpha (abs 0x03 y) */
} VcXform;

/* GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF, 24 bytes: the float-position entries (P04+25f0, +2870).
   u, v: the PS2's 4.12 UV / 4096 (1.0 = the PS2 texture's width / height; exact in a float); c: as VcVert; x, y, z: the PS2's float position. */
typedef struct { float u, v; uint32_t c; float x, y, z; } VcVertF;

/* P04+0xdf0 (pre-lit strips; +0x1088 has the same input): the call's transform and vertices.  vm: the 16 KB VU1 data memory, top: the
   call's TOP (qword address).  flags[i] = vertex i's 16-bit flag word (bit 15 ADC: the triangle that ends at vertex i is not drawn).
   Returns the vertex count; -1: the header's count is above max; -2: a position or UV outside 16 bits; -3: a negative UV. */
int vc_df0(const uint8_t *vm, uint32_t top, VcXform *x, VcVert *v, uint16_t *flags, int max);
/* sb1, Stage B lane 1: vc_df0 with the vertices read from the game's data (ge/xlate.h XlRec: p, kind, row of the streams at T + 0x100,
   0x101, 0x102), the rest from vm as vc_df0 reads it.  The same results as vc_df0 on the VU memory those streams leave */
int vc_df0_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, uint16_t *flags, int max);
/* sb14, Stage C step 3 (3v; stagec_notes.md step 3): vc_df0_rec in two passes, so that a call the cut leaves out (xconv.c xc_df0_off)
   never converts its UVs, colours and flags.  vc_df0_pos returns what vc_df0_rec returns (the count, -1, -2 or -3), writes the same
   VcXform and each vertex's x, y, z and pad, and when it returns a count above 0 the box of the positions (b[0..2] the least x, y, z,
   b[3..5] the largest); vc_df0_att then writes each vertex's u, v, c and its flag word.  Together the same bytes as vc_df0_rec.
   vc_df0_flag: vertex i's flag word as vc_df0_att writes it */
int vc_df0_pos(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint32_t *row, VcXform *x, VcVert *v, int16_t *b, int max);
void vc_df0_att(const void *const *p, const uint8_t *kind, const uint32_t *row, const VcXform *x, VcVert *v, uint16_t *flags, int n);
static inline uint16_t vc_df0_flag(const void *const *p, const uint8_t *kind, const uint32_t *row, int i)
{
  typedef int16_t __attribute__((may_alias)) h16;
  return (uint16_t)((uint32_t)(int32_t)((const h16 *)p[1])[4 * i + 3] + (kind[1] == 3 ? row[3] : 0));
}

/* P04+0x25f0 / +0x2870 (pre-lit strips with float world positions and per-vertex alpha; 2870 adds a back-face test): the call's
   transform (the matrix at T+0x10..0x13; world rows without the x 32768) and vertices; flags as vc_df0 (from the UV qword's w).
   Returns the vertex count; -1: the header's count is above max; -4: a colour word above 255 or a UV z other than 1.0 (the UV z only when
   tme, the call's PRIM TME: an untextured call's UV quadword is not drawn; sb15 4x, the deck's flat triangles hold old data there). */
int vc_fpos(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, int max, int tme);
/* sb6, Stage B lane 3 (renderer_notes 11.33): vc_fpos with the vertices read from the game's data (ge/xlate.h XlRec lane 3: p[0] the
   positions, num[0] vectors of V4-32, kind[0] 1; p[1] the attributes, num[1] vectors of V4-16 signed, kind[1] 2, or 3 plus row; kind[2] 1:
   the layout vc_fpos reads, cl 3), the rest from vm as vc_fpos reads it.  The same results as vc_fpos on the VU memory those streams leave;
   -8 when they do not hold what vc_fpos would read (vc_fpos_rec_ok: another kind or layout, fewer vectors than the header's count n) */
static inline int vc_fpos_rec_ok(const void *const *p, const uint16_t *num, const uint8_t *kind, int n)
{
  return p[0] && p[1] && kind[2] == 1 && kind[0] == 1 && (kind[1] == 2 || kind[1] == 3) && n <= (int)num[0] && 2 * n <= (int)num[1];
}
int vc_fpos_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint16_t *num, const uint8_t *kind, const uint32_t *row, VcXform *x,
                VcVertF *v, uint16_t *flags, int max, int tme);

/* The P04 pre-lit clipper (micro address 0x1710, re/vu1_programs.md §2.5) on one triangle, as the PS2 runs it: the +z (near) plane if a
   vertex is outside it; then the colours times w; then -z, +x, -x, +y, -y, each when one of the (at most 4) vertices left after the near
   plane is outside it.  Outside is the VU's CLIP test (x > |w| ...).  Each plane is Sutherland-Hodgman from the first vertex: a new vertex
   = (a d_b - b d_a) / (d_b - d_a) with d = w - the coordinate (+ planes) or w + it (- planes), and that coordinate set to +w or -w.
   c: clip position; col: colour (times w after the near plane, so that a clipped edge keeps the GS's screen-linear colour); uv: u, v, 1;
   p: the world position, interpolated as uv (for drawing the polygon with hardware transform).  Returns the vertex count (0 to 9; below 3
   nothing is drawn); the PS2 then draws col / w (its RGBAQ = ftoi0(col / w + 0.0005)). */
typedef struct { float c[4], col[4], uv[3], p[3]; } VcCv;
int vc_clip_tri(const VcCv in[3], VcCv out[16]);
/* the VU's CLIP flags of a clip-space point: +x -x +y -y +z -z in bits 0..5 */
int vc_cflags(const float *c);

/* VU float arithmetic as the interpreters model it (tools/gsref.c, runtime/vu0.h): an input or a result with exponent 0 is a signed zero
   and one with exponent 255 the largest finite value of its sign; each product and each sum is rounded to float on its own (no fused
   multiply-add).  vc_vu: that flush and clamp of one value. */
float vc_vu(float f);
/* clip = M (p, 1) as P04's loops compute it (MULAx, MADDAy, MADDAz, MADDw; re/vu1_programs.md 3.7): m = VcXform.m (columns), p the
   position as the VU reads it.  For the float-position entries this is the clip position the micro program stores back in the input. */
void vc_vu_clip(const float *m, const float *p, float *c);
/* sb3 (renderer_notes 11.28): vc_vu_clip without its clamps, the same bits when every input is 0 or within 2^-28 .. 2^60 in size
   (vc_clip_in): then no product or sum can overflow or be denormal (each is 0 or at least 2^-125, as a sum of two floats is a whole
   multiple of the finer one's last bit), so each clamp leaves its value as it is and the same IEEE single steps run in the same order */
static inline int vc_clip_in(float f) { uint32_t u; memcpy(&u, &f, 4); return !(u & 0x7fffffffu) || (u >> 23 & 0xffu) - 99u <= 87u; }
static inline void vc_vu_clip_nc(const float *m, const float *p, float *c)
{ for (int k = 0; k < 4; k++) c[k] = ((m[k] * p[0] + m[4 + k] * p[1]) + m[8 + k] * p[2]) + m[12 + k]; }

/* The triangles a PS2 strip with these flags draws (vertex i >= 2 without ADC: triangle i-2, i-1, i) as GE strips:
   runs[2k] = first vertex, runs[2k + 1] = vertex count of GE strip k.  Returns the number of strips (at most max). */
int vc_strips(const uint16_t *flags, int n, uint16_t *runs, int max);

/* ---- P06, the lit meshes (re/vu1_programs.md 3.4, 3.8, 3.14; renderer_notes 11.14): Snake, the crew, the guards ----
   The four entries by the guest address their micro code was uploaded from: +0 (lit), +358 (two-matrix skin), +738 (a per-vertex
   colour factor, the guards), +ab0 (both). */
#define VC_P06 0x19fe08u
#define VC_P06S 0x1a0160u
#define VC_P06T 0x1a0540u
#define VC_P06ST 0x1a08c0u
/* GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF, 20 bytes: VcVert's UV with a float position (the skinned entries) */
typedef struct { uint16_t u, v; uint32_t c; float x, y, z; } VcVertS;
/* GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_16BIT, 20 bytes: a float UV with VcVert's position (pass 3 of +0 / +738) */
typedef struct { float u, v; uint32_t c; int16_t x, y, z, pad; } VcVertE;
typedef struct {
  VcXform x;             /* the call's transform (T+0..3; world rows x 32768 for 16-bit positions, x 1 for floats) */
  int kind;              /* 0 +0, 1 +358 (skinned), 2 +738 (colour factor), 3 +ab0 (both) */
  int n;
  uint32_t hy;           /* header y: the pass flags */
  int fog;               /* the call's constant fog F (0..255): bits 4-11 of header z - 0x8000 */
  int pos16, uv16;       /* the vertex type of passes 1 and 2: 16-bit positions (the unskinned entries, every position in 16 bits; else
                            floats) and 16-bit UVs (every UV1 in 0..32767; else floats, UV / 4096): VcVert, VcVertE, VcVertS or VcVertF */
  int uv2;               /* pass 2 (header y 0x10): 1 drawn with pass 1's vertices, UV2 = UV1 + (du, dv) on every vertex (4.12); 2 the
                            differences vary: drawn with vertices of its own (vc_p06_uv2); -1 a UV2 z other than 1.0 (not drawn; none
                            seen); 0 no pass 2 */
  int du, dv;            /* uv2 1: the constant (4.12); else 0 */
  int p3;                /* pass 3: 1 the normal projection (0x200), 2 the light ramp (0x400), -1 from T+0x104 (0x100: not drawn),
                            0 none.  Its vertices: VcVertE (16-bit positions) or VcVertF */
  int amin;              /* the lowest vertex alpha (PS2 scale, 128 = 1.0) */
  int fast;              /* 1: the constants the vertices use are below 2^20 in size, so the vertices were computed without the
                            per-operation flush (the same bits: vu1conv.c p6_vertex; P07: p7_loop) */
  int cull;              /* 1: header y 0x4 with a vertex whose winding bits are not 0x30: the PS2 culls triangles, the GE draws them all */
} VcLit;
/* A P06 call (entry VC_P06..VC_P06ST) as the micro program computes it: each vertex's colour (three directional lights and ambient,
   clamped; the +738 / +ab0 factor), skinned position, UVs and pass-3 texture coordinates, in the program's order of operations.  v1:
   n vertices of passes 1 and 2 (type by pos16, uv16); v3: n vertices of pass 3 (when p3 > 0; may be 0 otherwise); flags[i]: the data's
   flag word, with ADC (bit 15) on every vertex when header z gives it; rgba (0: not wanted): the RGBAQ colour the program kicks,
   r | g << 8 | b << 16 | a << 24.  The GE draws with the same transform as df0 (VcXform), with a constant fog.  Returns n; -1: n above
   max; -5: an entry that is not P06; -6: a UV1 z other than 1.0 (the GS's texel is then u / z; none seen) */
int vc_p06(const uint8_t *vm, uint32_t top, uint32_t entry, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max);
/* Pass 2's own vertices (L->uv2 2): v1's colours and positions with the call's UV2 (T+0x103 + 6i), 16-bit UVs when every UV2 is in
   0..32767, else floats (UV2 / 4096).  v2: L->n vertices; returns their type bits (bit 0 a float UV, bit 1 a float position: VcVert,
   VcVertE, VcVertS, VcVertF) */
int vc_p06_uv2(const uint8_t *vm, uint32_t top, const VcLit *L, const void *v1, void *v2);
/* sb2, Stage B lane 2: vc_p06 with the vertices read from the game's data (ge/xlate.h XlRec p, kind, num, row: the streams of slots A to
   F at T + 0x100 + s + 6 i), the rest from vm as vc_p06 reads it.  The same results as vc_p06 on the VU memory those streams leave;
   -8: the call needs that VU memory (a stream it reads not sent in its group, short or of another kind, or a call vc_p06 gives to its
   general form): the caller decodes the VU memory and calls vc_p06.  vc_p06_uv2_rec: vc_p06_uv2 with UV2 from stream D (d) */
int vc_p06_rec(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *p, const uint8_t *kind, const uint16_t *num,
               const uint32_t *row, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max);
int vc_p06_uv2_rec(const void *d, const VcLit *L, const void *v1, void *v2);
extern int vc_p06_exact;           /* checks: 1 computes every P06 and P07 call in the exact form (L->fast 0) */
int vc_p06_size1(const VcLit *L);   /* the size of a vertex of passes 1 and 2 */
int vc_p06_size3(const VcLit *L);   /* of pass 3 */

/* ---- P07, the codec faces (re/vu1_programs.md P07; renderer_notes 11.17): skinned with 1 to 4 bones per vertex (matrices at abs
   32 + the vertex's bone offsets), lit by three directional lights, drawn in 1 to 3 texture passes.  Uploaded from 0x1a31e0.  Its
   input: T+0 (x vertices, z bones, w flags), 7 quadwords a vertex from T+0x100 (position 12.4, normal 4.12, UV1, UV2, -, weights x 128,
   bone offsets) ---- */
#define VC_P07 0x1a31e0u
/* A P07 call as the micro program computes it, in its order of operations, each result through the VU's flush and clamp: each
   vertex's skinned position (floats: the GE's world is the call's V, abs 8..11), its normal (normalised with ERLENG for 2 to 4 bones,
   as the matrix gives it for 1), its colour (lights abs 24..26 (transposed) and 28..30, ambient abs 31, clamped at abs 3 x; alpha
   abs 3 y) and pass 3's texture coordinates (the normal through abs 16..18, then T+0xfe scale and T+0xff offset).  L as vc_p06's,
   with kind 4, hy the flags (T+0.w: 0x10 pass 2, 0x200 pass 3), pos16 0, fast 1 (p7_loop) or 0, cull 0, fog -1 (the PS2's
   F = clamp(a w + b, 0, abs 3 x) per vertex, which the GE's fog makes from its view z) or, when a is 0, the constant F; p3 1 (0x200),
   -1 (0x100 or 0x400 alone or with it: paths not seen, not drawn) or 0.  v1: n vertices of passes 1 and 2 (VcVertS or VcVertF by
   uv16); v3: n vertices of pass 3 (VcVertF, when p3 > 0); flags[i]: in[0].w (bit 15 ADC); rgba (0: not wanted): the RGBAQ colour as
   kicked.  Returns n; -1 n above max; -6 a UV1 z other than 1.0; -7 a path the captures never took (flags 0x800 or 0x4, T+1.x not 0,
   bones outside 1..4).  Pass 2's own vertices (uv2 2): vc_p06_uv2 with this L. */
int vc_p07(const uint8_t *vm, uint32_t top, VcLit *L, void *v1, void *v3, uint16_t *flags, uint32_t *rgba, int max);
/* sb5, Stage B lane 4: vc_p07 with the vertices read from the game's data (ge/xlate.h XlRec p, kind, num: streams A to F of a P07 slot), the
   rest from vm as vc_p07 reads it.  The same results as vc_p07 on the VU memory those streams leave, float bit for float bit (one loop,
   p7_loop, for both); -8: the call needs that VU memory (a stream it reads not sent, short or of another kind, stream E sent, a bone
   matrix outside abs 32 .. 63, the vertices wrapping past the memory's end, or a call vc_p07 gives to its exact form): the caller decodes
   the VU memory and calls vc_p07.  vc_p07_uv2_rec: vc_p06_uv2 with this L and UV2 from stream D (d) */
int vc_p07_rec(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint16_t *num, VcLit *L, void *v1, void *v3,
               uint16_t *flags, uint32_t *rgba, int max);
int vc_p07_uv2_rec(const void *d, const VcLit *L, const void *v1, void *v2);
/* sb11 C1 (stagec_notes.md 1.7-1.9; ge/xconv.c xc_lit_c1): a lane 4 P07 call drawn by the GE's own skinning, lighting and texture
   matrix.  vc_p07_c1: the call's header and constants as p07_main reads them, without its vertex loop.  -8: p07_main's fast form would
   not take the call (or a stream it reads is missing: vc_p07_rec's -8); -9: it would, but the GE cannot draw it (the clamp abs 3 x not
   255, a light colour times its direction's length or the ambient outside 0..255); else n, with L as vc_p07_rec gives it but for uv2,
   du, dv (vc_p07_c1v's) and k the GE's constants: the bones a vertex (nb), each light's direction (column j of abs 24..26, normalised)
   and colour (row j of abs 28..30 times that column's length, rounded; 0: off), the ambient (abs 31, rounded; the GE's alpha, the PS2's
   doubled, in bits 24-31), pass 3's matrix (TGEN rows: s R[.][j], translation o, q 1) */
typedef struct {
  const float (*ldir)[3]; const uint32_t *lcol; uint32_t amb; const float *tgen; int nb;   /* vc_p07_c1 (c1opt2: ldir, lcol and tgen
                                                     point at its constants, which the next call may change) */
  int nw, stride; uint32_t v2; uint8_t slot[8], idx[8];              /* vc_p07_c1s (idx: each slot's place among slot[]) */
  int16_t blo[3], bhi[3]; uint16_t smin, smax;                        /* vc_p07_c1s (c1opt2): the box of the positions, the least and
                                                     largest sum of a vertex's weights (a 1-bone call's 128) */
} VcP7C1;
int vc_p07_c1(const uint8_t *vm, uint32_t top, const void *const *p, const uint8_t *kind, const uint16_t *num, VcLit *L, VcP7C1 *k);
/* vc_p07_c1s and vc_p07_c1b: the call's GE vertices from its streams (p as vc_p07_rec's, after vc_p07_c1 took the call: L->n, L->p3 and
   k->nb set), in two steps so the bones can be read between them (c1opt, 2026-11-02; the first form, vc_p07_c1v, did both in one).
   vc_p07_c1s reads the slots and the checks: k->slot[0 .. k->nw - 1] the slots the call names in ascending order (offset / 4, with a
   weight above 0 or the one bone), k->idx each slot's place among them, k->stride = align2(nw) + 16 bytes, k->v2 the offset of pass 2's
   own copy (uv2 2; else 0); L->uv2, du, dv as p07_main gives them.  Returns n, or -8 where p07_main would not take the call (a UV1 above
   32767 or its z other than 1.0) or the GE cannot (a bone offset not a multiple of 4 or above 28, a UV2 above 32767 for uv2 2).
   vc_p07_c1b writes the vertices: its weights by slot place (a 1-bone call's 128, the others' bytes summed by slot), UV1 (16-bit), the
   normal and the position (16-bit, as the streams hold them: the GE reads s / 32768, the bones carry the rest), at v, and with uv2 2
   pass 2's own copy with UV2 at v + k->v2 (n x stride rounded up to 16; the pads zeroed); flags[i]: stream A's w (the strip flags).
   With zf (each slot place's bone w row, 4 floats, and the translation zt), each vertex's w = zt + sum over its slots in ascending
   order of weight x (zf[j] . (p, 1)), and of those above 0 the least and largest in zw[0], zw[1] and their count in *zn (xconv.c's
   depth range).  Returns n, or -8 (a slot's weights above 255 in a vertex) */
int vc_p07_c1s(const void *const *p, VcLit *L, VcP7C1 *k);
int vc_p07_c1b(const void *const *p, const VcLit *L, const VcP7C1 *k, uint8_t *v, uint16_t *flags, const float (*zf)[4], float zt, float *zw,
               long *zn);
/* the bone matrix at abs m (32 + 4 s) as the GE takes it: GE rows (the images of x, y, z, then the translation), x 2048 in the 3 x 3 (the
   GE reads 16-bit positions as s / 32768, the PS2 12.4); 0 where p7_loop would not take it (m above 60, or p7_mok) */
int vc_p07_bone(const uint8_t *vm, uint32_t m, float *b12);

/* ---- sb12 C2 (stagec_notes.md step 2; ge/xconv.c xc_lit_c2; ge/replay_fmt.h GERL_C2): a lane 2 P06 call drawn by the GE's own skinning
   (the skinned entries: two bones), lighting and (pass 3) texture matrix, by mapping (b): the PS2's ambient as the emissive colour, the
   global ambient (0, 0, 0, 255), the material's ambient and diffuse from the vertex colour (d, d, d, a), with d the PS2's normal length
   in 8 bits (vc_p06_c2v) and a the GE alpha (the call's, or the factor entries' per vertex).
   vc_p06_c2: the call's header, streams and constants as vc_p06_rec and p06_main read them, without the vertex loop.  -8 where p06_main's
   fast form would not take the call (a stream it reads not sent in its group, short or of another kind; n 0 or above 64; the light ramp;
   pass 3 drawn without pass 2 or the reverse; ROW z not 1.0; a constant from 2^20 in size; a lit colour below 0; no lit light); -9 where
   it would but the GE cannot draw it (the clamp abs 3 x not 255; a lit light's colour times its direction's length from 255.5; the
   ambient outside 0..255); else n, with L as vc_p06_rec gives it but for uv2, du, dv, amin and cull (vc_p06_c2v's), and k: each light's
   direction (its column of T + 4..6, normalised) and colour (row j of T + 8..0xa times that column's length, rounded; 0: off), the
   emissive (T + 0xb, rounded), pass 3's matrix (rows R[j][0] s0, R[j][1] s1, 0, then o0, o1, 1: the normal's scale is the list's), and
   for the skinned entries the skin matrix as p6_loop reads it and the GE's bone 1 (32768 B3 as GE rows, then B's translation) */
typedef struct {
  const float (*ldir)[3]; const uint32_t *lcol; uint32_t emis; const float *tgen;   /* vc_p06_c2 (ldir, lcol and tgen point at its kept
                                                     constants, which the next call may change) */
  float bone1[12], b[4][3];                         /* skinned: the GE's bone 1; the skin matrix (p6_loop's P6F.b) */
  uint32_t a1, a2, adc; float acl; int skin, tint, chk2;   /* the alpha as kicked (PS2 scale, 128 = 1.0) and the GE's; ADC on every
                                                     vertex (header z); the clamped alpha (the factor entries' A1); the entry; pass 2 */
  int16_t blo[3], bhi[3];                           /* vc_p06_c2box: the box of the positions */
} VcP6C2;
int vc_p06_c2(const uint8_t *vm, uint32_t top, uint32_t entry, const void *const *p, const uint8_t *kind, const uint16_t *num,
              const uint32_t *row, VcLit *L, VcP6C2 *k);
/* vc_p06_c2box: the box of the call's positions (stream A) into k->blo, k->bhi (for xconv.c's depth range of a skinned call) */
void vc_p06_c2box(const void *const *p, int n, VcP6C2 *k);
/* vc_p06_c2v: the call's GE vertices from its streams (after vc_p06_c2 took it): skinned, two 16-bit weights (w x 8 and 32768 - w x 8, w
   the position's 4th halfword, 4.12); UV1 (16-bit); the colour (d, d, d, a); the normal and the position (16-bit, as the streams hold
   them): 20 bytes a vertex unskinned, 24 skinned, at v, and with uv2 2 pass 2's own copy with UV2 at v + a1 (a1 = n x stride rounded up to
   16; the pads zeroed).  d: 255 sqrt(S) rounded, S the squared length of the PS2's normal (the data's, or for a skinned vertex the blend
   p6_loop computes, n w + (B3 n)(1 - w), in its order and float mode) as a float in that mode, (x^2 + y^2) + z^2; rounded by VC_C2T's
   thresholds (vu1conv.c), with no square root (the same d on any machine whose single-precision sums and products are IEEE in that mode).
   a: the GE alpha (the call's k->a2; the factor entries' per vertex, as p6_loop makes it).  flags[i]: stream B's w with ADC as p06_main;
   L->uv2, du, dv, amin and cull as p06_main gives them.  With zr (the depth range's w row: zr[0..2] times the position, plus zr[3]) each
   vertex's w as xconv.c xc_zr_lit takes it (unskinned the position as the stream holds it; skinned p6_loop's blend of it), and of those
   above 0 the least and largest in zw[0], zw[1] and their count in *zn.  Returns n, or -8 where p06_main would not take the call (a UV1
   above 32767) or the GE cannot draw it (a weight outside 0..4096, a factor's colour other than 4096 (1.0), with uv2 2 a UV2 above 32767) */
int vc_p06_c2v(const void *const *p, VcLit *L, const VcP6C2 *k, uint8_t *v, uint16_t *flags, const float *zr, float *zw, long *zn);

/* ---- Stage C step 4 (stagec_notes.md step 4): the effects P12d910 sends that the GE did not draw before ---- */
#define VC_SPR 0x19bb50u    /* P04+0x3740: sprites (the rain's mist and spray) */
#define VC_QUAD 0x19be50u   /* P04+0x3a38: rotated quads */
#define VC_LINE 0x19af68u   /* P04+0x2b60: line strips (the rain's streaks) */
/* P04+0x3740 (p4.lst 0x3740-0x3a30): count (T+0x14 x) sprites, records of 6 quadwords from T+0x100: centre (float x, y, z), RGBA,
   UV0 (4.12 s, t, q), -, size (ints x, y), UV1.  The PS2 (each step rounded as vc_vu): c = M centre (M at T+0x10..0x13, as
   vc_vu_clip); corner 0 = c - size f, corner 1 = c + size f in x and y (f: abs 7 x, y; z and w are c's); both CLIP-tested against
   |c.w|; a sprite with corner 1 beyond -x or -y, corner 0 beyond +x or +y, or either outside in z is dropped (ADC); else a corner beyond
   x or y = -w (corner 0) or +w (corner 1) is moved onto it and its UV moved in proportion, in the order x0, y0, x1, y1; fog F =
   clamp(fog_a w + fog_b, 0, abs 3 x) from corner 1.  Out: x as vc_xform makes it but with the GE world the identity (the GE's view
   space is then the PS2's clip x, y, w: the GE divides by w and takes depth and fog from it), and the drawn sprites' corners as VcVertF
   pairs, in order: x, y the corner's clip x, y (a moved one +-w), z the clip w, u, v the UV in PS2 texture widths after the moves, c the
   colour as VcVert's; drop[i] = 1 for each sprite the PS2 drops (i below the count), else 0.  Returns the count (drawn and dropped);
   -1: the count above max; -4: a colour word above 255, or UV0's q (the GS's Q for both corners) other than 1.0 */
int vc_spr(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint8_t *drop, int max);
/* P04+0x3a38 (p4.lst 0x3a38-0x3d90): count (T+0x14 x) quads, records of 12 quadwords from T+0x100: centre (float x, y, z), RGBA, UV0
   (4.12 s, t, q), side (ints x, y), UV1.  The PS2 (each step rounded as vc_vu): c = M centre; corners c - f s, (c.x + fx sy, c.y - fy sx),
   (c.x - fx sy, c.y + fy sx), c + f s (f: abs 7 x, y; z and w are c's), UVs (u0, v0), (u1, v0), (u0, v1), (u1, v1); each corner
   CLIP-tested against |c.w|; all four outside one plane: dropped; another flag: the strip triangles (0, 1, 2) and (1, 2, 3) go to the
   pre-lit clipper (micro 0x1710, vc_clip_tri; the micro program rewrites T+0x14 x to 4 x the count for it); else drawn as a strip of the
   four corners.  Out: x as vc_spr's (the GE world the identity); for quad i st[i] = 0 drawn whole, 1 dropped, 2 to the clipper;
   v[4 i + k]: corner k as vc_spr's (x, y: clip x, y; z: clip w; u, v in PS2 texture widths; c the colour as VcVert's); with st[i] = 2,
   cv[4 i + k]: corner k as the clipper reads it (c: clip x, y, z, w; col: the PS2's colour, 0..255 floats; uv: u, v, q; p: clip x, y, w,
   the GE's view position).  Returns the count; -1: above max; -4: a colour word above 255, or UV0's or UV1's q other than 1.0 */
int vc_quad(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, VcCv *cv, uint8_t *st, int max);
/* P04+0x2b60 (p4.lst 0x2b60-0x2de0; its clipper 0x2de8-0x3190 with the plane routines 0x31a0-0x3738 and 0x2508): count (T+0x14 x)
   vertices of line strips, records of 3 quadwords from T+0x100 as P04+25f0's (float world position, RGBA, UV whose w holds the flags:
   bit 15, a strip starts at the vertex).  The PS2: c = M p (vc_vu_clip) CLIP-tested for each vertex; the segment that ends at vertex i
   (i >= 1, no strip start there) is drawn in the strip when neither end has a flag, else marked for the line clipper, which drops it when
   both ends lie outside one plane and else clips it plane by plane (vc_clip_seg) and draws what is left as a line of its own (tag template
   T+0x16).  tme: the call's PRIM TME (untextured, the UV quadword's x, y, q are not read).  Out: x as vc_fpos's (the GE world the call's matrix rows), v[i] as vc_fpos's (world position, UV in PS2 texture widths,
   colour), flags[i] the UV's w, cc[i] the clip position c, cls[i] for the segment that ends at vertex i: 0 none (i = 0 or a strip start),
   1 drawn in the strip, 2 dropped at the clipper's first test, 4 to be clipped (vc_clip_seg).  Returns the count; -1: above max; -4: a
   colour word above 255 or, textured, a UV q other than 1.0 */
int vc_line(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, float (*cc)[4], uint8_t *cls, int max, int tme);
/* The P04 line clipper on one segment (in[0], in[1]: VcCv as vc_clip_tri's, uv's q 1.0): the +z plane if an end is outside it; the
   colours times w; then -z, +x, -x, +y, -y in turn, each when an end is outside it as the CLIP flags stand after the plane before: the
   end outside replaced in place by (a d_b - b d_a) / (d_b - d_a) (d = w - the coordinate, or w + it for the minus planes; a the first
   end) with that coordinate set to +w or -w, each product, difference and quotient rounded as the VU's; after each plane, both ends
   outside one plane drop the segment.  Returns 2 (out: the two ends, colours times w as the PS2 holds them: it draws ftoi0(col / w +
   0.0005)) or 0 (dropped) */
int vc_clip_seg(const VcCv in[2], VcCv out[2]);
#endif
