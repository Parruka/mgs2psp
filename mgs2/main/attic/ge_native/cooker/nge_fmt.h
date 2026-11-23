/* nge_fmt.h: the native GE renderer's cooked format, step 0 (native_ge_renderer_design.md sections 1.3 and 2.1).
   Models are cooked once (at stage load, or on a miss in the stub) into GE vertex buffers and one display list per group; the main CPU
   then writes, per mesh, WORLD and, per group, a material bind and a CALL into the group's list.
   Plain C99: stdint and string.h only, no allocation, no system calls, no floating point beyond the transform helper in nge_list.c.
   Builds for the x86 twin (gcc) and for the PSP (psp-gcc).  Layouts are offsets in one arena, so the arena can be moved (VRAM / RAM)
   with nge_relocate.  What it holds is game data: personal use only. */
#ifndef NGE_FMT_H
#define NGE_FMT_H
#include <stdint.h>

/* GE commands used here (pspsdk guInternal.h numbering) */
enum {
  NGE_C_NOP = 0x00, NGE_C_VADDR = 0x01, NGE_C_IADDR = 0x02, NGE_C_PRIM = 0x04, NGE_C_JUMP = 0x08, NGE_C_CALL = 0x0a, NGE_C_RET = 0x0b,
  NGE_C_END = 0x0c, NGE_C_FINISH = 0x0f, NGE_C_BASE = 0x10, NGE_C_VTYPE = 0x12, NGE_C_OFFSET = 0x13, NGE_C_ORIGIN = 0x14,
  NGE_C_WORLDN = 0x3a, NGE_C_WORLDD = 0x3b, NGE_C_PROJN = 0x3e, NGE_C_PROJD = 0x3f
};
#define NGE_W(c, a) ((uint32_t)(c) << 24 | ((uint32_t)(a) & 0xffffffu))
#define NGE_PRIM_STRIP 4u

/* GE vertex types (GU_TRANSFORM_3D) */
#define NGE_VT_PRE 0x11eu    /* GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT: 16 B, byte for byte ge/vu1conv.h VcVert (today's df0
                                batches): u, v (PS2 4.12, the texture scale carries x 8), colour r g b (PS2 0x80 = 1.0) and alpha (doubled),
                                x, y, z (s16; WORLD carries x 32768), pad 0 */
#define NGE_VT_LIT 0x15eu    /* + GU_NORMAL_16BIT: u v, colour (d, d, d, a), nx ny nz, x y z: 20 B (lit P04, GE lighting: design 2.1) */
#define NGE_VT_SEL 0x435eu   /* + GU_WEIGHT_8BIT | GU_WEIGHTS(2): w0 w1, then as LIT (u v at 2): 24 B (lit P04 with the 2-matrix selector) */

typedef struct { uint16_t u, v; uint32_t c; int16_t x, y, z, pad; } NgeVPre;   /* 16 B (= VcVert) */

enum {                       /* NgeGroup.kind: the group's cooked form */
  NGE_K_PRE = 1,             /* pre-lit (mesh+0xC0 bit 0): P04+df0 (or +1088, the same input with a back-face test) */
  NGE_K_LIT = 2,             /* lit, one matrix: P04+780 */
  NGE_K_SEL = 3              /* lit, desc0 bit 0: the 2-matrix selector, P04+a70 */
};
enum {                       /* NgeGroup.flags */
  NGE_GF_NOSTRIP = 1,        /* every triangle has ADC: today's converter makes no batch; the writer makes no CALL */
  NGE_GF_NEGUV = 2,          /* a negative UV (ge/vu1conv.c df0_pos returns -3: no batch today); the writer makes no CALL */
  NGE_GF_NOSTREAM = 4,       /* a stream not sent (count 0 or no pointer): not cooked; the group stays on today's path */
  NGE_GF_BIG = 8             /* more vertices than the streams hold, or above 255: not cooked */
};

typedef struct {             /* one cooked group: 28 B */
  uint32_t list;             /* arena offset of its display list: VTYPE, BASE, VADDR, PRIM .. (VADDR again where a strip does not follow
                                the last), RET */
  uint32_t vb;               /* arena offset of its first vertex */
  uint16_t nw, nv;           /* list words; vertices */
  uint8_t kind, nst;         /* NGE_K_*; strips (one PRIM each) */
  uint16_t flags;            /* NGE_GF_* */
  int16_t box[6];            /* positions' least x y z, largest x y z: the cut's box (ge/xconv.c xc_df0_off) and the depth range's bound */
} NgeGroup;

typedef struct {             /* one cooked mesh (a DG_OBJ: obj + 0xC0 + k 0x130): 48 B */
  uint32_t next;             /* arena offset of the mesh cooked before it (the arena's chain, for nge_relocate); 0: none */
  uint32_t mesh;             /* its guest address */
  uint32_t desc, sa, sb, sc; /* the check at each use: mesh+0xF4 and the stream bases mesh+0xDC / E0 / E4 at cook time */
  uint32_t dsum;             /* a sum of the group descriptors' words (design 2.1 "header word sum") */
  uint32_t grp;              /* arena offset of NgeGroup[ng] */
  uint32_t bytes;            /* arena bytes this mesh took (groups, vertices, lists) */
  uint16_t ng, pre;          /* groups; 1 pre-lit */
  uint32_t alpha;            /* the vertex alpha cooked in (GE scale, 0..255): VU1 abs 0x03 y doubled, today's rule */
  uint32_t live;             /* 1 valid; 0 invalidated (its bytes stay until the arena is reset) */
} NgeMesh;

typedef struct {             /* the cooked arena: one block, GE address ge (the cooked lists hold absolute addresses; the block must not
                                cross a 16 MB boundary of the GE's address space, so one BASE serves every list) */
  uint8_t *mem; uint32_t ge, cap, used, peak, last;   /* last: arena offset of the newest mesh (the chain's head), 0: none */
} NgeArena;
enum { NGE_OK = 0, NGE_E_ROOM = -1, NGE_E_BAD = -2, NGE_E_OVR = -3 };

#endif
