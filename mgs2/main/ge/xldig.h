/* xldig.h: digests of what the packet translator (ge/xlate.h) hands its callers, one text line per VU1 call and per vblank, so two runs
   of the translator can be compared line by line (renderer_notes 11.10, step 6): the live x86 check build (boot/xlive_chk.c, fed by the
   second walk ge/xlive.c from guest RAM) and tools/gsref.c (GSREF_XLATE=1 GSREF_XLDIG=<file>, fed from a GS capture of the same
   vblanks).  The hash is FNV-1a over 64-bit words; equal lines mean equal inputs to the converters.  Portable C. */
#ifndef MGS2_XLDIG_H
#define MGS2_XLDIG_H
#include <stdio.h>
#include "xlate.h"

void xdg_open(FILE *f);              /* start writing (0: off) */
void xdg_vblank(long vb);            /* a vblank starts: the line of the vblank before it ("v <vb> ...") */
void xdg_call(const XlCall *c);      /* from the caller's xl_on_call callback: "c <vb> <n> ..." */
void xdg_prim(const GfGs *g, const GfDraw *d, const GfVtx *const *v, int n);   /* from the xl_on_prim callback (summed per vblank) */
void xdg_close(void);                /* the last vblank's line */
/* xdg_used(1): hash only what is read (renderer_notes 11.12 "Shown frames only"): a call line carries, instead of the whole VU memory
   and GS registers, its GS state as a primitive drawn with it reads it (when its A+D packet was applied, else 0), and the primitives'
   digest takes only the registers and vertex fields each reads */
void xdg_used(int on);
#endif
