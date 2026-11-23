/* nge_list.c: see nge_list.h. */
#include <string.h>
#include "nge_list.h"

/* a float as the VU reads it (ge/vu1conv.c vfl): no denormals, an exponent of 255 the largest finite value */
static float vfl(uint32_t u)
{
  uint32_t e = u & 0x7f800000u; float f;
  if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
  memcpy(&f, &u, 4); return f;
}

int nge_xf_block(const uint8_t *ram, uint32_t blk, NgeXf *x)
{
  uint32_t seg = blk >> 28, a = blk & 0x0ffffff0u;
  if ((seg > 3) || a < 0x100000u || a + 64 > 0x2000000u) return 0;
  float m[16];
  for (int k = 0; k < 16; k++) { uint32_t u; memcpy(&u, ram + (a & NGE_RAM_MASK) + 4 * (uint32_t)k, 4); m[k] = vfl(u); }
  const float s = 32768.0f;
  float *w = x->world;   /* ge/vu1conv.c vc_xform's rows, the same operations in the same order */
  w[0] = m[0] * s; w[1] = m[1] * s; w[2] = m[3] * s; w[3] = m[4] * s; w[4] = m[5] * s; w[5] = m[7] * s;
  w[6] = m[8] * s; w[7] = m[9] * s; w[8] = m[11] * s; w[9] = m[12] * 1.0f; w[10] = m[13] * 1.0f; w[11] = m[15] * 1.0f;
  const float r20 = m[2], r21 = m[6], r22 = m[10], r23 = m[14], r30 = m[3], r31 = m[7], r32 = m[11], r33 = m[15];
  float d = 0, nn = 0;
  d += r20 * r30; nn += r30 * r30; d += r21 * r31; nn += r31 * r31; d += r22 * r32; nn += r32 * r32;
  const float pa = nn > 0 ? -d / nn : 0, pb = r23 + pa * r33;
  x->pa = pa; x->pb = pb;
  return 1;
}

uint32_t nge_put_world(uint32_t *w, const NgeXf *x)
{
  w[0] = NGE_W(NGE_C_WORLDN, 0);
  for (int k = 0; k < 12; k++) { uint32_t u; memcpy(&u, &x->world[k], 4); w[1 + k] = NGE_W(NGE_C_WORLDD, u >> 8); }
  return 13;
}

uint32_t nge_put_call(uint32_t *w, const NgeArena *a, const NgeGroup *g, uint32_t base_back)
{
  if (!g->list || (g->flags & (NGE_GF_NOSTRIP | NGE_GF_NEGUV | NGE_GF_NOSTREAM | NGE_GF_BIG))) return 0;
  const uint32_t ad = a->ge + g->list;
  w[0] = NGE_W(NGE_C_BASE, (ad >> 8) & 0xf0000u);
  w[1] = NGE_W(NGE_C_CALL, ad & 0xffffffu);
  w[2] = NGE_W(NGE_C_BASE, base_back & 0xf0000u);
  return 3;
}
