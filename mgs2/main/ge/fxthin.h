/* fxthin.h (task #114): the effects the picture may draw thinner.  Ffm (2026-10-31): the rain's big sheets of speckled drops close to
   the camera and the water splash at Snake's feet are "too much for the PSP screen"; the picture draws 1 in N of their strips (N from the
   cfg: MGS2_GL_RAIN, MGS2_GL_SPLASH; 1 = as the PS2).  Picture only: the game's state and every hash stay as they are.

   Each frame, before its first effect call is converted, fx_thin_scan walks the actor lists of the PAL game (SLES-50383): 11 levels in
   the ELF's data from 0x1b9d94, 0x64 bytes each, a head actor and a tail actor 0x30 apart; an actor's +0 is the next one, +4 its act
   function, +0x28 its file name.  It picks the actors by file name: d_splash_camera.c (the sheets) and ft_splsh.c (the splash), whose
   DG object (the actor's +0x40, +0x860) holds the records the DG system draws from and their vertex data, in one block of the game's
   allocator (header 0x10 bytes before it: previous block, next block, 0, size).  The block's range goes to the converter, which thins an
   effect call whose vertex data comes from it (ge/xconv.h XcHost.thin, XlCall.src).  The names are cached by act function and name
   pointer (open addressing, every pair kept: a name is read once), and the cache is emptied when the stage image's header (0x1f1c00, 32
   bytes) changes or it is 3/4 full.  It only reads game memory.

   Task #154 (step 4, C4: the effects the PSP did not draw before): the rain's streaks (P04+2b60 line strips) 1 strip in N too (cfg
   MGS2_GL_STREAKS).  Their actors are rain.c (the rain) and rain_slow.c (all 64 sets for 42 frames on both routes); each keeps a table of
   entries of 0x20 bytes from its +0xb0 to its block's end (rain.c's block 0x5b0 bytes: 40 entries; rain_slow.c's 0x8b0: 64): two floats,
   then the address of a block of the allocator at the entry's +0x10, each block the vertex data of one call (4,736 bytes, in a row 0x1290
   apart in the dumps).  The scan takes the blocks whose header looks right, sorts
   them by address and joins neighbours (a block that starts right after the last one's end and the next header): one range a run of
   blocks, marked for the streaks (XcThin.line).  Owners from the x86 census (MGS2_FX_WHO=1, stagec_notes.md 4.9).

   Task #158 (Ffm 2026-11-03 13:27): thinning the streaks by strips saves no time (each call is decoded and its vertices clipped before
   the strips are dropped), so they can also be drawn 1 call in N (cfg MGS2_GL_STREAK_CALLS): a call is left out whole, before its decode,
   unless its block's place in the range, (XlCall.src - lo) / FX_STREAK_BLOCK, is a multiple of N.  Each call is 32 drops spread over the
   whole screen (x86 probe, deck route: a call's on-screen box spans 1.72 x 1.76 of the 2 x 2 clip square, median) and a block keeps its
   place from frame to frame, so the same drops stay drawn.

   Task #161 (deck_costs.md, 2026-11-03): with C4 on, the rain's sprites (P04+3740) can be left out whole, before their decode (cfg
   MGS2_GL_RAIN_SPRITES=0; 1 or unset: drawn).  Their actors by the x86 census (sb15 MGS2_FX_WHO=3, both routes: no block two actors
   point at): body_sph.c (its block's pointer at +0x78), charaspread.c (+0xa8), rain_gas_pers.c (+0x184), line_sph.c (a table of 0x50-byte
   entries from +0xa0, the pointer first; seen up to +0xb40, 35 entries, read only inside the actor's own block).  The scan keeps their
   blocks sorted and joined (a block that starts at the last one's end + its header joins it); a sprite call whose XlCall.src lies in one
   is left out (fx_drop_has).  The x86 check (MGS2_THIN_CHK=1) counts the sprite and quad calls by the census's owner, left out or not. */
#ifndef MGS2_FXTHIN_H
#define MGS2_FXTHIN_H
#include <stdint.h>
#include "xconv.h"

#define FX_KINDS 9      /* (gl38: 4..8 the rain owners the bound turns tell apart, never thinned: see FxThin.all) 0: the sheets of drops close to the camera (d_splash_camera.c), 1: the splash at the feet (ft_splsh.c), 2: the
                           rain's streaks (rain.c, rain_slow.c; task #154), 3: the rain's sprites (task #161: drawn or left out whole) */
#define FX_DROPS 512    /* task #161: the rain's sprite blocks a frame (more are drawn: counted in spr_over) */
#define FX_RANGES 16    /* blocks a frame (more instances are not thinned: counted in over) */
#define FX_STREAK_BLOCK 0x1290u   /* task #158: a streak block's place: its data (4,736 bytes) and the next block's header, in a row */
#define FX_CACHE 512    /* (act function, name pointer) pairs (a power of two; the deck has about 140) */
typedef struct { uint32_t lo, hi, kind; } FxBd;   /* gl38: a rain owner's block and its kind */
typedef struct FxThin {
  int keep[FX_KINDS];                 /* 1 strip in keep[k] of kind k's calls drawn (1: all; the streaks' 0: none, task #158) */
  int keep_calls;                     /* task #158: 1 in keep_calls of the rain's streak calls drawn, by block (1: all) */
  XcThin r[FX_RANGES]; int n;         /* this frame's blocks */
  struct { uint32_t ak, np; } c[FX_CACHE]; int nc;   /* ak: the act function | 0 an empty entry, 1 another actor, 2 one of ours (its
                                         kind in ck; act functions are 4-byte aligned); nc: entries in use */
  uint8_t ck[FX_CACHE];               /* task #154: the kind of an entry with 2 */
  uint32_t sig[8];                    /* the stage image's header the cache was filled under */
  long scans, actors, named, flushes, found[FX_KINDS], bad, over;   /* scans; actors walked; names read; cache emptied; blocks found by
                                         kind (the streaks: their actors); walks, objects or streak actors that did not look right (the
                                         frame is not thinned past a walk's); blocks over FX_RANGES (the streaks: ranges) */
  long line_none;                     /* task #158: streak calls of the rain's ranges left out whole (MGS2_GL_STREAKS=0, or
                                         MGS2_GL_STREAK_CALLS above 1) */
  long tab_blocks, tab_empty, tab_bad, tab_ranges;   /* task #154: the streak actors' tables: blocks taken, entries 0, entries whose
                                         block's header did not look right (not thinned), ranges made of the blocks taken */
  struct { uint32_t lo, hi; } d[FX_DROPS]; int nd;   /* task #161: this frame's rain sprite blocks, sorted and joined (keep[3] 0) */
  /* gl38 (ge/bound.h): all 1 (the host sets it before a frame's scan): the scan also lists the blocks of the rain's effect owners, each
     with its kind: 0 the drop sheets, 1 the foot splash, 4 d_splash_fall.c (falling splashes), 5 d_splash_floor.c (floor splashes),
     6 ripple.c, 7 d_splash_parts_slow.c, 8 d_thunder_parts.c (owners and offsets: rain_options_2026-11-21.md, the rain study's census).
     Nothing else of the scan changes with it */
  int all, nb; FxBd b[FX_RANGES]; long b_over, b_bad;
  long spr_none, spr_blocks, spr_bad, spr_over, spr_ranges;   /* task #161: sprite calls left out whole; blocks taken; pointers whose
                                         block's header did not look right (drawn); blocks over FX_DROPS (drawn); ranges after joining */
} FxThin;
void fx_thin_init(FxThin *t, int keep_sheets, int keep_splash, int keep_streaks);   /* shares 1..8 (streaks: task #154; 0: none, task #158) */
void fx_thin_calls(FxThin *t, int keep_calls);                    /* task #158: the streaks' calls 1 in keep_calls (1..8), after init */
void fx_thin_rainspr(FxThin *t, int keep);                        /* task #161: 0: the rain's sprites left out whole (with C4); 1: drawn */
int fx_drop_has(const FxThin *t, uint32_t src);                   /* task #161: 1: src lies in one of this frame's rain sprite blocks */
int fx_thin_on(const FxThin *t);                                  /* 1: a share above 1, or the streaks none (or the rain's sprites out) */
int fx_thin_scan(FxThin *t, const uint8_t *ram, int lines);       /* ram: the guest's 32 MB; this frame's blocks into t->r, their number;
                                                                     lines (task #154): 1: the streaks' too (C4 on: they are drawn) */
#ifndef __psp__
void fx_who_note(const uint8_t *ram, uint32_t entry, uint32_t src);   /* x86 census (MGS2_FX_WHO=1): see fxthin.c */
int fx_who_name(const uint8_t *ram, uint32_t src, char *name, int cap);   /* x86 (task #154 check): the census's owner of the block
                                         holding src, its file name into name; 0, or -1 no source, -2 no block, -3 no actor points at it */
#endif
#endif
