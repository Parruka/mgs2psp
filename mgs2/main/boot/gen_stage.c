/* One stage image's translation (-DSTAGE_GEN='"gen_s_<name>.h"') in the split x86 probe build (see boot.c, gen_pre.h). */
#define GEN_UNIT STAGE_GEN
#include "gen_pre.h"
#define static
#include STAGE_GEN
