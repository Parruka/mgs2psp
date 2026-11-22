#include <stdio.h>
#include <stdint.h>
static uint32_t old_(uint32_t num, uint32_t cl, uint32_t wl) { return wl ? (num - 1) / wl * cl + (wl > cl ? wl - 1 : (num - 1) % wl) : 1023; }
static uint32_t new_(uint32_t num, uint32_t cl, uint32_t wl)
{
  if (wl == cl) return wl ? num - 1 : 1023;
  if (wl == 1) return (num - 1) * cl;
  if (wl == 2) return ((num - 1) >> 1) * cl + (cl < 2 ? 1 : (num - 1) & 1);
  return wl ? (num - 1) / wl * cl + (wl > cl ? wl - 1 : (num - 1) % wl) : 1023;
}
int main(void) { long n = 0, bad = 0;
  for (uint32_t num = 1; num <= 256; num++) for (uint32_t cl = 0; cl < 256; cl++) for (uint32_t wl = 0; wl < 256; wl++) { n++; if (old_(num, cl, wl) != new_(num, cl, wl)) { if (bad++ < 5) printf("num %u cl %u wl %u: %u %u\n", num, cl, wl, old_(num, cl, wl), new_(num, cl, wl)); } }
  printf("%ld cases, %ld differ\n", n, bad); return 0; }
