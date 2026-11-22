/* me_rt.h (task #149, 2026-11-03; mgs2/me_handoff_design.md 4.8, the ME picture's build order step 2): what the picture code (ge/
   xlive, xlate, gsfront, texconv, texcache, gelive, gebatch, xconv, vu1conv, gelist, gsup2) calls instead of the C library, so that the
   same code can run on the Media Engine, which may call nothing outside its own unit (no libc, no system call; hle_me.h's rules):
   - me_memcpy, me_memset, me_memcmp: the C library's semantics (memcpy's regions must not overlap: the x86 build counts any that do);
   - me_floorf, me_ceilf: fdlibm's (newlib's) rules, bit for bit (me_rt_test.c checks every float against the C library's);
   - the picture heap: malloc, calloc, realloc, free and aligned_alloc over a region the host gives at set-up (me_heap_init), with
     two-level segregated fit (TLSF, Masmano et al. 2004, written here): no system call and no lock, deterministic, so a run of the
     same build on the emulator proves the PSP's; its peak, extent and failures are counted for the log.  More regions can join it
     as pools of their own (me_heap_add), and on the main CPU a hook of the host's gives one when nothing fits (me_heap_grow);
   - ME_CALLP(f): a call through a pointer.  The ME runs its code at the kernel alias of the EBOOT's addresses (0x80000000 | address;
     v30me6c), so a call through a pointer made at a user address must go to the alias there: on the PSP the address is ORed with
     the top bit of $gp, which is 0 on the main CPU (the EBOOT's _gp) and which the ME's entry sets to 1; on x86 nothing changes.
   Built with -DMGS2_ME_SAFE, the picture files include this header after the C library's, and its macros send their calls here;
   the memcpy family by a rename in the objects (below). */
#ifndef MGS2_ME_RT_H
#define MGS2_ME_RT_H
#include <stddef.h>
#include <stdint.h>

void *me_memcpy(void *d, const void *s, size_t n);
void *me_memset(void *d, int c, size_t n);
int me_memcmp(const void *a, const void *b, size_t n);
float me_floorf(float x);
float me_ceilf(float x);

int me_heap_init(void *base, size_t n);   /* the region (any alignment; at least 4 KB): 0, or -1 */
int me_heap_add(void *base, size_t n);    /* one more region (a pool of its own, kept; up to 32 pools in all): 0, or -1 */
/* the host's hooks: more, when no free block fits a request, gives a region of *got bytes, at least want, or 0; less takes back
   a region more gave, once no block in it is in use.  Only the main CPU may set them; on the main CPU they call the C library's
   malloc and free, on the ME (task #180 step 4d-2: ge/xme.c xme_heap_ask) they ask the main CPU through the loop's ctx and wait. */
void me_heap_grow(void *(*more)(size_t want, size_t *got), void (*less)(void *region));
/* a third hook, the main CPU's too: resize, for a realloc whose block cannot grow where it is and is the only block in use in a region
   the hook gave (a free block may follow it):
   the region made want bytes long, in place when the host can (as the C library's realloc does at the top of its heap) or moved with
   its bytes, as realloc does; or 0, the region as it was (the block then moves as before).  On the ME it stays 0. */
void me_heap_resize(void *(*resize)(void *region, size_t want));
void *me_malloc(size_t n);
void *me_calloc(size_t a, size_t b);
void *me_realloc(void *p, size_t n);
void me_free(void *p);
void *me_aligned_alloc(size_t al, size_t n);   /* al a power of two */
typedef struct MeHeapStats {
  size_t size;                /* the bytes the heap manages, in all its regions */
  int pools; size_t added;    /* its regions; the bytes of those added after the first (me_heap_add and the hook) */
  long grows, grow_fails;     /* regions the hook gave when no free block fitted; times it gave none */
  long returned; size_t added_peak;   /* regions given back to the host once empty; the most bytes added at one time */
  long resized, resized_tail;         /* regions the host resized for a realloc (me_heap_resize); of them, with a free block after the
                                         realloc's (the region a little larger than the block it was first given for) */
  size_t used, peak;          /* bytes in blocks in use (headers included) now and at most */
  size_t extent;              /* the highest end of a block in use in the first region, from its start, at most */
  long allocs, frees, reallocs, moved;   /* calls; reallocs that moved the block */
  long fails; size_t fail_first;         /* allocations refused (no room), the size of the first */
  long blocks, blocks_peak;              /* blocks in use now and at most */
} MeHeapStats;
const MeHeapStats *me_heap_stats(void);
int me_heap_check(void);   /* walks the region: 0, or the number of faults found (dev) */

/* faults the picture code met that a host prints (the ME prints nothing) */
typedef struct MeFaults {
  long overlap;               /* x86: me_memcpy with overlapping regions */
  long zk;                    /* gb_vert asked for depth by zk (only the replay tools use it: not the live picture) */
} MeFaults;
extern MeFaults me_faults;

/* task #180 step 4c: the ME's heartbeat inside long work (the chain walks' tags, the translator's VIF commands and GIF tags, a local
   copy's rows), so that a long walk is not taken for a hang: me_beat_p is the loop's heartbeat word (xme.h XmeCtx.heart), set by the
   host; a word of its own until then.  Step 4d-4: the word is shared through the uncached alias (xme.h XS), so a beat counts in a cached
   word of the unit's (me_beat_n, turn-owned like the rest of its memory) and moves the shared word every 64th time. */
#ifdef MGS2_ME_SAFE
extern volatile uint32_t *me_beat_p;
extern uint32_t me_beat_n;
#define ME_BEAT() ((void)((++me_beat_n & 63u) || ((*me_beat_p)++, 0)))
#else
#define ME_BEAT() ((void)0)
#endif

#if defined(__psp__) && defined(MGS2_ME_SAFE)
static inline uint32_t me_seg(void) { uint32_t g; __asm__("srl %0, $gp, 31" : "=r"(g)); return g; }
/* ins: bit 31 of the address from me_seg (a form the ME unit's check looks for before every jalr and jr through a register) */
#define ME_CALLP(f) ({ __typeof__(f) me_f_ = (f); uint32_t me_a_ = (uint32_t)(uintptr_t)me_f_; \
                       __asm__("ins %0, %1, 31, 1" : "+r"(me_a_) : "r"(me_seg())); (__typeof__(f))(uintptr_t)me_a_; })
#else
#define ME_CALLP(f) (f)
#endif

/* memcpy, memset and memcmp stay the compiler's (it copies small fixed sizes inline); the calls it leaves are renamed to me_memcpy,
   me_memset and me_memcmp in the objects (objcopy --redefine-sym), on the PSP and in the x86 twin alike */
#ifdef MGS2_ME_SAFE
#define floorf me_floorf
#define ceilf me_ceilf
#define malloc me_malloc
#define calloc me_calloc
#define realloc me_realloc
#define free me_free
#define aligned_alloc me_aligned_alloc
#endif
#endif
