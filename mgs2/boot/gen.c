/* The ELF translation of the split x86 probe build (see boot.c, gen_pre.h); the call dispatch is gen_dispatch.c. */
#ifdef GEN_FN_HOOK                /* dev build (Makefile.probe boot_hook): every ELF function entry calls the host's g_fn_hook */
#include <stdint.h>
extern void (*g_fn_hook)(uint32_t);
#define FN_ENTER(a) do { if (g_fn_hook) g_fn_hook(a); } while (0)
#endif
#include "gen_pre.h"
#define static
#include GEN
