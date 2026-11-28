/* gl45 batch 3: the counters of tools/g45_count.py and their dump (x86 twin).  boot_x86.c's state_hash calls n45_mark at every hash
   line; G45_BC_AT=<vb>,<vb>,... names the hash lines at which the whole array is appended to G45_BC_OUT as one line "<vb> c0 c1 ...". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef G45_N
#define G45_N 120000
#endif
long g45_bc[G45_N];
void n45_mark(long vb)
{
  const char *e = getenv("G45_BC_AT"), *o = getenv("G45_BC_OUT"); char buf[32]; FILE *f; long i;
  if (!e || !o) return;
  snprintf(buf, sizeof buf, ",%ld,", vb);
  { char t[256]; snprintf(t, sizeof t, ",%s,", e); if (!strstr(t, buf)) return; }
  f = fopen(o, "a"); if (!f) return;
  fprintf(f, "%ld", vb); for (i = 0; i < G45_N; i++) fprintf(f, " %ld", g45_bc[i]); fprintf(f, "\n"); fclose(f);
}
