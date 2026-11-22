/* The header at the start of a stage overlay (psp/Makefile, psp/ovl.ld): one stage image's translated code, loaded on demand into the
   arena in main_psp.c (boot/dispatch.h STAGE_OVL), so only one stage's code is in memory at a time.  -DSTAGE=<name> */
#include <stdint.h>
#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)
#define STR_(a) #a
#define STR(a) STR_(a)
void CAT(CAT(S_, STAGE), _init)(void); int CAT(CAT(S_, STAGE), _call)(uint32_t);
extern uint8_t __ovl_bss[], __ovl_end[]; extern void (*const __ovl_ctor_a[])(void), (*const __ovl_ctor_b[])(void);
extern uint8_t g_mem[]; void rt_poll(void); void P100010(void);   /* main_psp.c checks these against its own: the overlay must come from the same build */
struct OvlHdr { uint32_t magic; char name[12]; void (*init)(void); int (*call)(uint32_t); uint8_t *bss, *end; const void *chk[3];
  void (*const *ctor_a)(void), (*const *ctor_b)(void); };
const struct OvlHdr ovl_hdr __attribute__((section(".ovl_hdr"), used)) =
  { 0x314c564fu, STR(STAGE), CAT(CAT(S_, STAGE), _init), CAT(CAT(S_, STAGE), _call), __ovl_bss, __ovl_end, { g_mem, (const void *)rt_poll, (const void *)P100010 }, __ovl_ctor_a, __ovl_ctor_b };
