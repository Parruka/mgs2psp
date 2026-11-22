/* dev (lean_plan.md 1b): the SIF command dispatcher's two table calls (P175cd0 at 0x175d98 / 0x175dd4), renamed JSIF by rc4/rc_jsif.py.
   regcache.py gives them the union of the ten handlers' sets, so the x86 build stops if the table hands it any other target. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
void gen_jalr(uint32_t a);
static inline void gen_jsif(uint32_t a)
{
  switch (a) {
  case 0x175780: case 0x175760: case 0x1761a8: case 0x176468: case 0x176658: case 0x176260: case 0x176f60: case 0x163bd0: case 0x15e600:
  case 0x15dd40: break;
  default: fprintf(stderr, "JSIF: target %08x is not in regcache's list\n", (unsigned)a); exit(7);
  }
  gen_jalr(a);
}
#define JSIF gen_jsif
