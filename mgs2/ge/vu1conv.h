/* vu1conv.h: one VU1 micro call's input (the VU1 data memory as the call starts) -> GE draws with hardware transform.
   renderer_notes §11.2; the micro programs' data layout: re/vu1_programs.md §2-3.  Plain C, no PSP dependencies: the
   harness's packet translator and the container tools (gsref) share it. */
#ifndef MGS2_VU1CONV_H
#define MGS2_VU1CONV_H
#include <stdint.h>

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

/* P04+0x25f0 / +0x2870 (pre-lit strips with float world positions and per-vertex alpha; 2870 adds a back-face test): the call's
   transform (the matrix at T+0x10..0x13; world rows without the x 32768) and vertices; flags as vc_df0 (from the UV qword's w).
   Returns the vertex count; -1: the header's count is above max; -4: a colour word above 255 or a UV z other than 1.0. */
int vc_fpos(const uint8_t *vm, uint32_t top, VcXform *x, VcVertF *v, uint16_t *flags, int max);

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
extern int vc_p06_exact;            /* checks: 1 computes every P06 and P07 call in the exact form (L->fast 0) */
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
#endif
