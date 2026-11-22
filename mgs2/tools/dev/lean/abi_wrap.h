/* dev (x86, lean_plan.md §5): the EE ABI premise of regcache.py, checked at every return of every translated function.  A caller keeps
   s0-s7, gp, sp and fp in its locals across a call and never reloads them, so a callee must leave R[16..23], R[28..30] (and f20-f31)
   at its return as they were at its entry.  Included in place of the unit's header: -DABI_GEN='"<header>"' -DABI_UNIT='"<unit>"'. */
#undef static
#include <stdio.h>
#include <string.h>
struct AbiSnap { uint32_t a; int64_t r[11]; uint32_t f[12]; };
static const int abi_r[11] = {16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30};
static unsigned long abi_n_ret, abi_n_bad;
static inline struct AbiSnap abi_take(uint32_t a)
{ struct AbiSnap s; s.a = a; for (int k = 0; k < 11; k++) s.r[k] = R[abi_r[k]]; memcpy(s.f, &F[20], sizeof s.f); return s; }
static void abi_check(struct AbiSnap *s)
{
    abi_n_ret++;
    for (int k = 0; k < 11; k++)
        if (R[abi_r[k]] != s->r[k] && ++abi_n_bad <= 60)
            fprintf(stderr, "abi error: %s P%x r%d entry %llx return %llx\n", ABI_UNIT, s->a, abi_r[k], (unsigned long long)s->r[k], (unsigned long long)R[abi_r[k]]);
    for (int k = 0; k < 12; k++) {
        uint32_t v; memcpy(&v, &F[20 + k], 4);
        if (v != s->f[k] && ++abi_n_bad <= 60) fprintf(stderr, "abi error: %s P%x f%d entry %08x return %08x\n", ABI_UNIT, s->a, 20 + k, s->f[k], v);
    }
#ifdef ABI_POISON   /* lean_plan.md 1c premise: no caller reads at, a0-a3, t0-t9, ra (and v1, f1-f19) after a call before writing it, and no
                       poll does: each return leaves them as a pattern in R[] / F[], so a reader changes the game state hashes */
    for (int n = 1; n < 32; n++)
        if (n == 1 || n == 3 || (n >= 4 && n <= 15) || n == 24 || n == 25 || n == 31) R[n] = (int64_t)(0x5a5a5a5a00000000ull | (0x9e3779b9u * (uint32_t)n));
    for (int n = 1; n < 20; n++) { uint32_t v = 0x7fbadb00u | n; memcpy(&F[n], &v, 4); }
#endif
}
__attribute__((destructor)) static void abi_fini(void)
{ fprintf(stderr, "abichk %s: %lu returns checked, %lu with a changed register (error lines: the first 60)\n", ABI_UNIT, abi_n_ret, abi_n_bad); }
#undef FN_ENTER
#ifdef GEN_LITE
#define FN_ENTER(a) struct AbiSnap __attribute__((cleanup(abi_check))) abi_s_ = abi_take(a); do { if (GEN_LITE_FN(a) && g_dg_lite) return; } while (0)
#else
#define FN_ENTER(a) struct AbiSnap __attribute__((cleanup(abi_check))) abi_s_ = abi_take(a)
#endif
#define static
#include ABI_GEN
