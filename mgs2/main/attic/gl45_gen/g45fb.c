/* gl45 batch 3: the counters of gen_lev.h's GEN_LEV_FBTEST (x86 twin): guarded calls whose guard held, calls whose guard was false
   (the fallback by itself), calls forced to the fallback by the test.  Printed at the hash lines of every 5000th vblank and the last. */
#include <stdio.h>
long gen_fb[3];
void n45_mark(long vb)
{
  if (vb % 5000 == 0 || vb >= 37800) fprintf(stderr, "gen_fb vb %ld: guard held %ld, guard false %ld, forced to the fallback %ld\n", vb, gen_fb[0], gen_fb[1], gen_fb[2]);
}
