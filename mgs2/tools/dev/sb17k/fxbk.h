/* fxbk.h (task #160, Ffm 2026-11-03 16:32 "what cost more?"): measurement builds only (-DFX_BUCKET).  Names the picture's work for the
   emulator profiler's buckets (PspProf PSPPROF_BUCKET = the address of g_emuactor, psp/main_psp.c): a converter call counts under
   0xfe000000 | its entry's index << 16 | its owner's index.  The owner of an effect call (P04+25f0 fans, +2870 strips, +3740 sprites,
   +3a38 quads, +2b60 streaks) is the actor whose own block points at the block holding the call's data (XlCall.src), as the x86 census
   (fxthin.c MGS2_FX_WHO); owners are told apart by their file name's pointer (actor +0x28).  Other calls have owner 0; 0xfff1..3: no
   source, no block, no owner.  The look-up itself counts under 0xfd000000.  fxbk_print writes the tables (entries, owners, and the
   actors alive at the end by act function, for GEN_ACTOR_PROF's keys).  Reads game memory only. */
#ifndef MGS2_FXBK_H
#define MGS2_FXBK_H
#include <stdint.h>
#include <stdio.h>
uint32_t fxbk_key(const uint8_t *ram, uint32_t entry, uint32_t src, int fx);   /* fx: look the owner up */
uint32_t fxbk_ekey(uint32_t entry);   /* the key of the entry alone (0xfe000000 | index << 16), not counted */
void fxbk_print(FILE *f, const uint8_t *ram);
#endif
