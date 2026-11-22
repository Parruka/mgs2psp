/* Guest call dispatch of the split x86 probe build (see boot.c, gen_pre.h): the ELF's gen_call, then the stage image in memory. */
#include "gen_pre.h"
int gen_call(uint32_t a); void gen_init(void); void P100010(void);
#define STAGES_EXTERN
#define DISPATCH_FN
#include "dispatch.h"
int gen_guest_call(uint32_t a) { return guest_call(a); }
void gen_start_x(void) { gen_start(); }
/* runtime/mmi.h GLDU / GSDU (tools/ldu_fuse.py), after the unit's other functions so their PGO profile keeps its numbering.  With the
   PSP access path (gen_pre.h) an access whose 8 bytes sit in one 32 MB view outside the register space is plain RAM at both ends */
#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_MM_CHECK)
#define LDU_RAM(a) (((a) ^ ((a) + 7)) >> 25 == 0 && GEN_FAST(a))
#else
#define LDU_RAM(a) 0
#endif
int64_t GLDU(int64_t r, uint32_t a, int rl)
{
  if (LDU_RAM(a)) { uint64_t v; memcpy(&v, GEN_RAM(a), 8); return (int64_t)v; }
  return rl ? GLDL(GLDR(r, a), a + 7) : GLDR(GLDL(r, a + 7), a);
}
void GSDU(int64_t r, uint32_t a, int rl)
{
  if (LDU_RAM(a)) { uint64_t v = (uint64_t)r; memcpy(GEN_RAM(a), &v, 8); return; }
  if (rl) { GSDR(r, a); GSDL(r, a + 7); } else { GSDL(r, a + 7); GSDR(r, a); }
}
