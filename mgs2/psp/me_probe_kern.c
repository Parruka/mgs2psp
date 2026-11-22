/* me_probe_kern.c - gl14mp (2026-11-02, task #132; mgs2/me_probe_notes.md): the kernels both cores run.  Compiled once, under the
   Media Engine's rules (hle_me.h: no system call, no libc, no function pointer or jump table; psp/gl14mp.sh: -fno-jump-tables
   -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns), and joined to the ME's unit; the main CPU calls the same functions at
   their user address, the ME at the kernel alias of the same physical code, so both caches see the same indexes.  The kernels time
   nothing: their callers do (main CPU: the microsecond clock; ME: its Count).  The in-cache kernels and mep_fp_run's ops are inline
   assembly, so the instructions are the same whatever the compiler does around them. */
#include <stdint.h>
#include "me_probe.h"

/* in cache: n rounds of 64 dependent integer instructions (32 addu / xor pairs) */
uint32_t mep_k_alu(uint32_t n)
{
  uint32_t a = 0x9e3779b9u, b = 0x7f4a7c15u;
  while (n--) __asm__ volatile(".set push\n.set noreorder\n.rept 32\naddu %0, %0, %1\nxor %1, %1, %0\n.endr\n.set pop" : "+r"(a), "+r"(b));
  return a ^ b;
}

/* in cache: n rounds of 16 dependent add.s / mul.s pairs (the values stay normal: x grows by 0.5 a pair, y stays 0.5) */
uint32_t mep_k_falu(uint32_t n)
{
  uint32_t r;
  __asm__ volatile(".set push\n.set noreorder\n"
                   "lui $t0, 0x3f80\nmtc1 $t0, $f12\nmtc1 $t0, $f14\nlui $t0, 0x3f00\nmtc1 $t0, $f13\nnop\n"   /* x 1.0, y 0.5, one 1.0 */
                   "beq %1, $zero, 2f\nnop\n"
                   "1:\n.rept 16\nadd.s $f12, $f12, $f13\nmul.s $f13, $f13, $f14\n.endr\n"
                   "addiu %1, %1, -1\nbne %1, $zero, 1b\nnop\n"
                   "2:\nmfc1 %0, $f12\nnop\n.set pop"
                   : "=r"(r), "+r"(n) :: "$t0", "$f12", "$f13", "$f14");
  return r;
}

/* one load per 64-byte line, four independent lines a round, summed after the four loads */
uint32_t mep_k_lines(const uint32_t *p, uint32_t nlines)
{
  const volatile uint32_t *q = (const volatile uint32_t *)p; uint32_t s = 0, i;
  for (i = 0; i + 4 <= nlines; i += 4) { uint32_t a = q[16 * i], b = q[16 * i + 16], c = q[16 * i + 32], d = q[16 * i + 48]; s += a + b + c + d; }
  return s;
}

/* dependent loads: p[k] holds the next index (hwprobe.c's chase) */
uint32_t mep_k_chase(const uint32_t *p, uint32_t start, uint32_t steps)
{
  const volatile uint32_t *q = (const volatile uint32_t *)p; uint32_t k = start;
  while (steps--) k = q[k];
  return k;
}

/* one store per 64-byte line */
void mep_k_stores(uint32_t *p, uint32_t nlines, uint32_t v)
{
  volatile uint32_t *q = (volatile uint32_t *)p; uint32_t i;
  for (i = 0; i < nlines; i++) q[16 * i] = v + i;
}

/* 64 bytes a round: the line's 16 words loaded, then stored (d, s and nbytes 64-byte aligned) */
void mep_k_copy(uint32_t *d, const uint32_t *s, uint32_t nbytes)
{
  uint32_t n = nbytes >> 6;
  while (n--) {
    uint32_t w0 = s[0], w1 = s[1], w2 = s[2], w3 = s[3], w4 = s[4], w5 = s[5], w6 = s[6], w7 = s[7];
    d[0] = w0; d[1] = w1; d[2] = w2; d[3] = w3; d[4] = w4; d[5] = w5; d[6] = w6; d[7] = w7;
    w0 = s[8]; w1 = s[9]; w2 = s[10]; w3 = s[11]; w4 = s[12]; w5 = s[13]; w6 = s[14]; w7 = s[15];
    d[8] = w0; d[9] = w1; d[10] = w2; d[11] = w3; d[12] = w4; d[13] = w5; d[14] = w6; d[15] = w7;
    d += 16; s += 16;
  }
}

/* the same, each destination line claimed in the cache first (cache 0x18, create dirty exclusive: no read of the old line), so every
   word of it must be written; main_psp.c's cde_selftest checks the op on the main CPU at every start, me_probe.c checks it on the ME */
void mep_k_copy_cde(uint32_t *d, const uint32_t *s, uint32_t nbytes)
{
  uint32_t n = nbytes >> 6;
  while (n--) {
    uint32_t w0 = s[0], w1 = s[1], w2 = s[2], w3 = s[3], w4 = s[4], w5 = s[5], w6 = s[6], w7 = s[7];
    __asm__ volatile("cache 0x18, 0(%0)" :: "r"(d) : "memory");
    d[0] = w0; d[1] = w1; d[2] = w2; d[3] = w3; d[4] = w4; d[5] = w5; d[6] = w6; d[7] = w7;
    w0 = s[8]; w1 = s[9]; w2 = s[10]; w3 = s[11]; w4 = s[12]; w5 = s[13]; w6 = s[14]; w7 = s[15];
    d[8] = w0; d[9] = w1; d[10] = w2; d[11] = w3; d[12] = w4; d[13] = w5; d[14] = w6; d[15] = w7;
    d += 16; s += 16;
  }
}

/* the float ops, one instruction each between moves (the nops are the same on both cores); the caller set FCR31 */
#define FP_BEGIN ".set push\n.set noreorder\nmtc1 %1, $f12\nmtc1 %2, $f13\nnop\nnop\n"
#define FP_END "\nnop\nmfc1 %0, $f14\nnop\n.set pop"
#define FP2(ins) __asm__ volatile(FP_BEGIN ins " $f14, $f12, $f13" FP_END : "=r"(r) : "r"(a), "r"(b) : "$f12", "$f13", "$f14")
#define FP1(ins) __asm__ volatile(FP_BEGIN ins " $f14, $f12" FP_END : "=r"(r) : "r"(a), "r"(b) : "$f12", "$f13", "$f14")
#define FPC(ins) __asm__ volatile(FP_BEGIN ins " $f12, $f13\nnop\nnop\ncfc1 %0, $31\nnop\n.set pop" : "=r"(r) : "r"(a), "r"(b) : "$f12", "$f13", "$f14")
#define FP_LOOP(stmt) for (i = 0; i < n; i++) { uint32_t a = in[2 * i], b = in[2 * i + 1], r; stmt; out[i] = r; } break
#define FPC_LOOP(stmt) for (i = 0; i < n; i++) { uint32_t a = in[2 * i], b = in[2 * i + 1], r; stmt; out[i] = (r >> 23) & 1u; } break

uint32_t mep_fp_run(uint32_t op, const uint32_t *in, uint32_t n, uint32_t *out)
{
  uint32_t i, f;
  switch (op) {                         /* -fno-jump-tables: a chain of compares */
  case 0: FP_LOOP(FP2("add.s"));
  case 1: FP_LOOP(FP2("sub.s"));
  case 2: FP_LOOP(FP2("mul.s"));
  case 3: FP_LOOP(FP2("div.s"));
  case 4: FP_LOOP(FP1("sqrt.s"));
  case 5: FP_LOOP(FP1("abs.s"));
  case 6: FP_LOOP(FP1("neg.s"));
  case 7: FP_LOOP(FP1("cvt.s.w"));
  case 8: FP_LOOP(FP1("cvt.w.s"));
  case 9: FP_LOOP(FP1("trunc.w.s"));
  case 10: FP_LOOP(FP1("round.w.s"));
  case 11: FP_LOOP(FP1("ceil.w.s"));
  case 12: FP_LOOP(FP1("floor.w.s"));
  case 13: FPC_LOOP(FPC("c.eq.s"));
  case 14: FPC_LOOP(FPC("c.lt.s"));
  case 15: FPC_LOOP(FPC("c.le.s"));
  case 16: FPC_LOOP(FPC("c.un.s"));
  default: break;
  }
  __asm__ volatile("cfc1 %0, $31" : "=r"(f));
  return f;
}
