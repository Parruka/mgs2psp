/* gi.h: dev (x86) guest instruction counting for headers rewritten by tools/dev/gi_count.py (boot.c GEN_GI_COUNT, MGS2_GI);
   tools/dev/x86_gi.sh compiles every generated unit with -include this file. */
#include <stdint.h>
extern uint32_t g_gi_cnt[], g_gi_dgc[]; extern int g_gi_on, g_gi_dg;
#define GI_(a, k) do { if (g_gi_on) { if (g_gi_dg) g_gi_dgc[(a) >> 2] += (k); else g_gi_cnt[(a) >> 2] += (k); } } while (0)
