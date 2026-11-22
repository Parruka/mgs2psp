/* R5900 VU0 core (macro + micro mode) for the statically recompiled world collision (model/recomp.py emits VUM/VLQC2/... calls).
   Float model follows the VU: denormals flush to zero, Inf/NaN clamp to +-Fmax, MAC/status flags per component.  Needs guest.h (MM). */
#ifndef VU0_H
#define VU0_H
#include "guest.h"
typedef struct { uint32_t u[4]; } __attribute__((aligned(16))) VQ;   /* 16-byte aligned: the VFPU form loads them with lv.q */                               /* u[0]=x .. u[3]=w */
GSVS VQ VF[32] GREG_SD, VN[32] GREG_SD;   /* VN: the VU float form of loaded registers (model/vuemit.py mk_reader) */ GSVS uint32_t VIr[16] GREG_SD, VACC[4] __attribute__((aligned(16))) GREG_SD, VQr GREG_SD, VPr GREG_SD, VIreg GREG_SD, VRr GREG_SD, VMAC GREG_SD, VSTAT GREG_SD, VCLIP GREG_SD, VCMSAR0 GREG_SD;
GSVS uint8_t VUMEM[4096] __attribute__((aligned(16))) GREG_SD; GSVS uint32_t VUPROG[1024];            /* micro memory: 512 x (lower, upper) */
GSVS long vu_steps, vu_calls; GSVS int vu_trace; GSVS int vu_tpc; GSVS int g_unsup_vu; GSVS int g_vu_old_fcget;   /* GSV (guest.h): one copy across the split build units */
static inline float VFL(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t VUI(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline __attribute__((always_inline)) float vin(uint32_t u) { uint32_t e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; return VFL(u); }
GSVS uint32_t g_vmac GREG_SD;
static inline __attribute__((always_inline)) uint32_t vout(float f, int sh)                          /* clamp + accumulate MAC bits (sh: 3=x .. 0=w) */
{
  uint32_t u = VUI(f), e = u & 0x7f800000, m = 0;
  if (!e) { m |= 1u << sh; if (u & 0x7fffff) m |= 0x100u << sh; u &= 0x80000000; }
  else if (e == 0x7f800000) { m |= 0x1000u << sh; u = (u & 0x80000000) | 0x7f7fffff; }
  if (u & 0x80000000) m |= 0x10u << sh;
  g_vmac |= m; return u;
}
static inline __attribute__((always_inline)) uint32_t vclamp(float f) { uint32_t u = VUI(f), e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; return u; }   /* vout without the flags */
static inline __attribute__((always_inline)) void vu_setflags(void)
{
  uint32_t z = (g_vmac & 0xf) != 0, s = (g_vmac & 0xf0) != 0, un = (g_vmac & 0xf00) != 0, o = (g_vmac & 0xf000) != 0;
  uint32_t cur = z | s << 1 | un << 2 | o << 3;
  VMAC = g_vmac; VSTAT = (VSTAT & 0xfc0) | cur | (cur << 6) | (VSTAT & 0x30);   /* sticky bits 6..9; D/I (4,5) kept */
}
#define VX(r, c) VFL(VF[r].u[c])
static inline void vwrite(int fd, int c, uint32_t u) { if (fd) VF[fd].u[c] = u; }
/* upper instruction: w with bits 31..27 cleared */
enum { O_ADD, O_SUB, O_MADD, O_MSUB, O_MAX, O_MINI, O_MUL, O_OPMSUB, O_OPMULA, O_ABS, O_ITOF, O_FTOI, O_CLIP, O_NOP };
#define VU_INL static inline __attribute__((always_inline))
/* fl = 0: the MAC / status flags of this op are dead (model/vurecomp.py liveness: no flag read before the next flag write) */
VU_INL void vu_upper_fl(uint32_t w, int fl)
{
  int op = w & 0x3f, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, sel = op & 3, idx = (w >> 6) & 31;
  int opn = O_NOP, src = -1 /* -1 vector, 0..3 broadcast component, 4 Q, 5 I */, toacc = 0, tgt = fd, sh = 0;
  if (op < 0x1c) { opn = op >> 2; src = sel; }
  else if (op < 0x30) {
    static const signed char N[20] = { O_MUL, O_MAX, O_MUL, O_MINI, O_ADD, O_MADD, O_ADD, O_MADD, O_SUB, O_MSUB, O_SUB, O_MSUB, O_ADD, O_MADD, O_MUL, O_MAX, O_SUB, O_MSUB, O_OPMSUB, O_MINI };
    static const signed char S[20] = { 4, 5, 5, 5, 4, 4, 5, 5, 4, 4, 5, 5, -1, -1, -1, -1, -1, -1, -1, -1 };
    opn = N[op - 0x1c]; src = S[op - 0x1c];
  } else if (op >= 0x3c) {
    static const signed char N[4][12] = {
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_MUL, O_ADD, O_SUB, O_ADD, O_SUB },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_ABS, O_MADD, O_MSUB, O_MADD, O_MSUB },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_MUL, O_ADD, O_SUB, O_MUL, O_OPMULA },
      { O_ADD, O_SUB, O_MADD, O_MSUB, O_ITOF, O_FTOI, O_MUL, O_CLIP, O_MADD, O_MSUB, O_NOP, O_NOP } };
    static const signed char S[4][12] = {
      { 0, 0, 0, 0, -1, -1, 0, 4, 4, 4, -1, -1 }, { 1, 1, 1, 1, -1, -1, 1, -1, 4, 4, -1, -1 },
      { 2, 2, 2, 2, -1, -1, 2, 5, 5, 5, -1, -1 }, { 3, 3, 3, 3, -1, -1, 3, -1, 5, 5, -1, -1 } };
    if (idx >= 12) return;
    opn = N[sel][idx]; src = S[sel][idx]; toacc = 1;
    if (opn == O_ITOF || opn == O_FTOI || opn == O_ABS) { toacc = 0; tgt = ft; sh = (int[]){ 0, 4, 12, 15 }[sel]; }
    if (opn == O_OPMULA) src = -1;
  } else return;
  if (opn == O_NOP) return;
  if (opn == O_MAX || opn == O_MINI || opn == O_ABS || opn == O_ITOF || opn == O_FTOI) fl = 0;   /* these leave MAC / status alone */
  if (fl) g_vmac = 0;
  if (opn == O_CLIP) {
    float wv = fabsf(vin(VF[ft].u[3])), x = vin(VF[fs].u[0]), y = vin(VF[fs].u[1]), z = vin(VF[fs].u[2]); uint32_t bits = 0;
    if (x > wv) bits |= 1; if (x < -wv) bits |= 2; if (y > wv) bits |= 4; if (y < -wv) bits |= 8; if (z > wv) bits |= 16; if (z < -wv) bits |= 32;
    VCLIP = ((VCLIP << 6) | bits) & 0xffffff; return;
  }
  float A[4], B[4], Ac[4];
  for (int c = 0; c < 4; c++) { A[c] = vin(VF[fs].u[c]); B[c] = src >= 0 && src < 4 ? vin(VF[ft].u[src]) : src == 4 ? vin(VQr) : src == 5 ? vin(VIreg) : vin(VF[ft].u[c]); Ac[c] = vin(VACC[c]); }
  float Bv[4]; for (int c = 0; c < 4; c++) Bv[c] = vin(VF[ft].u[c]);                    /* vector ft for OPMULA/OPMSUB */
  uint32_t out[4]; int wr[4] = { 0, 0, 0, 0 };
  for (int c = 0; c < 4; c++) {
    if (!(dest & (8 >> c))) continue;
    float a = A[c], b = B[c], res = 0;
    switch (opn) {
      case O_ADD: res = a + b; break; case O_SUB: res = a - b; break;
      case O_MADD: res = Ac[c] + a * b; break; case O_MSUB: res = Ac[c] - a * b; break;
      case O_MAX: res = a > b ? a : b; break; case O_MINI: res = a < b ? a : b; break;
      case O_MUL: res = a * b; break;
      case O_OPMSUB: case O_OPMULA: { float p = c == 0 ? A[1] * Bv[2] : c == 1 ? A[2] * Bv[0] : A[0] * Bv[1]; res = opn == O_OPMSUB ? Ac[c] - p : p; break; }
      case O_ABS: res = fabsf(a); break;
      case O_ITOF: res = (float)(int32_t)VF[fs].u[c] / (float)(1 << sh); break;
      case O_FTOI: { float v = a * (float)(1 << sh); out[c] = v >= 2147483648.0f ? 0x7fffffff : v <= -2147483648.0f ? 0x80000000u : (uint32_t)(int32_t)v; wr[c] = 1; continue; }
    }
    out[c] = fl ? vout(res, 3 - c) : vclamp(res); wr[c] = 1;
  }
  for (int c = 0; c < 4; c++) if (wr[c]) { if (toacc || opn == O_OPMULA) VACC[c] = out[c]; else vwrite(tgt, c, out[c]); }
  if (fl) vu_setflags();
}
#define vu_upper_i(w) vu_upper_fl((w), 1)
#define VI16(n) ((n) ? (VIr[n] & 0xffff) : 0)
static inline void vi_set(int n, uint32_t v) { if (n) VIr[n] = v & 0xffff; }
static uint8_t *vmem(uint32_t qa) { return VUMEM + ((qa & 0xff) << 4); }
/* lower instruction; returns branch target (instruction index) or -1.  pc = index of this instruction (for branches) */
#ifdef GUEST_SPLIT_GEN   /* split build: one micro-program runner (interpreter + AOT programs) in the runtime unit, not a copy per generated unit */
void vu_run_shared(int pc);
#define vu_run vu_run_shared
#else
static void vu_run(int pc);
#endif
VU_INL int vu_lower_i(uint32_t w, int pc)
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, imm11 = w & 0x7ff;
  if (imm11 & 0x400) imm11 -= 0x800;
  int is = fs, it = ft, id = fd;
  switch (op) {
    case 0x00: { uint32_t *m = (uint32_t *)vmem(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); return -1; }
    case 0x01: { uint32_t *m = (uint32_t *)vmem(VI16(it) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; return -1; }
    case 0x04: { uint32_t *m = (uint32_t *)vmem(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { vi_set(it, m[c]); break; } return -1; }
    case 0x05: { uint32_t *m = (uint32_t *)vmem(VI16(is) + imm11); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { m[c] = VI16(it); break; } return -1; }
    case 0x08: vi_set(it, VI16(is) + (((w >> 21) & 15) << 11 | (w & 0x7ff))); return -1;
    case 0x09: vi_set(it, VI16(is) - (((w >> 21) & 15) << 11 | (w & 0x7ff))); return -1;
    case 0x10: vi_set(1, (VCLIP & 0xffffff) == (w & 0xffffff)); return -1;
    case 0x11: VCLIP = w & 0xffffff; return -1;                                   /* FCSET */
    case 0x12: vi_set(1, (VCLIP & 0xffffff & (w & 0xffffff)) != 0); return -1;
    case 0x13: vi_set(1, ((VCLIP | w) & 0xffffff) == 0xffffff); return -1;
    case 0x14: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, (VSTAT & 0xfff) == imm); return -1; }
    case 0x15: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); VSTAT = (VSTAT & 0x3f) | (imm & 0xfc0); return -1; }
    case 0x16: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, VSTAT & imm & 0xfff); return -1; }
    case 0x17: { uint32_t imm = (w >> 21 & 1) << 11 | (w & 0x7ff); vi_set(it, (VSTAT | imm) & 0xfff); return -1; }
    case 0x18: vi_set(it, (VMAC & 0xffff) == VI16(is)); return -1;
    case 0x1a: vi_set(it, VMAC & VI16(is)); return -1;
    case 0x1b: vi_set(it, VMAC | VI16(is)); return -1;
    case 0x1c: vi_set(it, (g_vu_old_fcget ? VSTAT : VCLIP) & 0xfff); return -1;   /* FCGET: the clip flags of the last two judgments (VU manual,
                                                                                   PCSX2 _vuFCGET); g_vu_old_fcget (boot.c MGS2_OLD_FCGET, dev): the status bits read before 2026-10-19 */
    case 0x20: return pc + 1 + imm11;
    case 0x21: vi_set(it, pc + 2); return pc + 1 + imm11;
    case 0x24: return VI16(is);                       /* JR / JALR: VI holds an instruction index (address / 8), as BAL writes it */
    case 0x25: vi_set(it, pc + 2); return VI16(is);
    case 0x28: return VI16(it) == VI16(is) ? pc + 1 + imm11 : -1;
    case 0x29: return VI16(it) != VI16(is) ? pc + 1 + imm11 : -1;
    case 0x2c: return (int16_t)VI16(is) < 0 ? pc + 1 + imm11 : -1;
    case 0x2d: return (int16_t)VI16(is) > 0 ? pc + 1 + imm11 : -1;
    case 0x2e: return (int16_t)VI16(is) <= 0 ? pc + 1 + imm11 : -1;
    case 0x2f: return (int16_t)VI16(is) >= 0 ? pc + 1 + imm11 : -1;
  }
  if (op >= 0x40) {
    int fn = w & 0x3f, i5 = (w >> 6) & 31; if (i5 & 16) i5 -= 32;
    switch (fn) {
      case 0x30: vi_set(id, VI16(is) + VI16(it)); return -1;
      case 0x31: vi_set(id, VI16(is) - VI16(it)); return -1;
      case 0x32: vi_set(it, VI16(is) + i5); return -1;
      case 0x34: vi_set(id, VI16(is) & VI16(it)); return -1;
      case 0x35: vi_set(id, VI16(is) | VI16(it)); return -1;
    }
    int idx = (w >> 6) & 31, sel = fn & 3, fsf = (w >> 21) & 3, ftf = (w >> 23) & 3;
    if (fn >= 0x3c) {
      if (sel == 0 && idx == 12) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VF[fs].u[c]); return -1; }                 /* MOVE */
      if (sel == 1 && idx == 12) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VF[fs].u[(c + 1) & 3]); return -1; }       /* MR32 */
      if (sel == 0 && idx == 13) { uint32_t *m = (uint32_t *)vmem(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); vi_set(is, VI16(is) + 1); return -1; }   /* LQI */
      if (sel == 1 && idx == 13) { uint32_t *m = (uint32_t *)vmem(VI16(it)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; vi_set(it, VI16(it) + 1); return -1; }   /* SQI */
      if (sel == 2 && idx == 13) { vi_set(is, VI16(is) - 1); uint32_t *m = (uint32_t *)vmem(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, m[c]); return -1; }       /* LQD */
      if (sel == 3 && idx == 13) { vi_set(it, VI16(it) - 1); uint32_t *m = (uint32_t *)vmem(VI16(it)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) m[c] = VF[fs].u[c]; return -1; }       /* SQD */
      if (sel == 0 && idx == 14) {                                                                                                            /* DIV */
        float a = vin(VF[fs].u[fsf]), b = vin(VF[ft].u[ftf]); VSTAT &= ~0x30u;
        if (b == 0.0f) { if (a == 0.0f) VSTAT |= 0x10 | 0x400; else VSTAT |= 0x20 | 0x800; VQr = ((VF[fs].u[fsf] ^ VF[ft].u[ftf]) & 0x80000000) | 0x7f7fffff; }
        else { float q = a / b; uint32_t u = VUI(q); uint32_t e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; VQr = u; }
        return -1; }
      if (sel == 1 && idx == 14) {                                                                                                            /* SQRT */
        float b = vin(VF[ft].u[ftf]); VSTAT &= ~0x30u; if (b < 0) VSTAT |= 0x10 | 0x400; VQr = VUI(sqrtf(fabsf(b))); return -1; }
      if (sel == 2 && idx == 14) {                                                                                                            /* RSQRT */
        float a = vin(VF[fs].u[fsf]), b = fabsf(vin(VF[ft].u[ftf])); VSTAT &= ~0x30u;
        if (vin(VF[ft].u[ftf]) < 0) VSTAT |= 0x10 | 0x400;
        if (b == 0.0f) { VSTAT |= 0x20 | 0x800; VQr = (VF[fs].u[fsf] & 0x80000000) | 0x7f7fffff; } else { float q = a / sqrtf(b); uint32_t u = VUI(q); uint32_t e = u & 0x7f800000; if (!e) u &= 0x80000000; else if (e == 0x7f800000) u = (u & 0x80000000) | 0x7f7fffff; VQr = u; }
        return -1; }
      if (sel == 3 && idx == 14) return -1;                                                                                                   /* WAITQ */
      if (sel == 0 && idx == 15) { vi_set(it, VF[fs].u[fsf]); return -1; }                                                                  /* MTIR */
      if (sel == 1 && idx == 15) { int32_t v = (int16_t)VI16(is); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, (uint32_t)v); return -1; }   /* MFIR */
      if (sel == 2 && idx == 15) { uint32_t *m = (uint32_t *)vmem(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { vi_set(it, m[c]); break; } return -1; }  /* ILWR */
      if (sel == 3 && idx == 15) { uint32_t *m = (uint32_t *)vmem(VI16(is)); for (int c = 0; c < 4; c++) if (dest & (8 >> c)) { m[c] = VI16(it); break; } return -1; }  /* ISWR */
      if (idx == 16) {        /* RNEXT / RGET / RINIT / RXOR: R holds 23 random bits under the exponent of 1.0; the RNEXT step (taps 4 and 22) follows PCSX2's interpreter */
        if (sel == 0) { uint32_t r = VRr; r = (r << 1) ^ (((r >> 4) ^ (r >> 22)) & 1); VRr = (r & 0x7fffff) | 0x3f800000; }
        if (sel <= 1) { for (int c = 0; c < 4; c++) if (dest & (8 >> c)) vwrite(ft, c, VRr); return -1; }
        VRr = 0x3f800000 | ((sel == 2 ? VF[fs].u[fsf] : VRr ^ VF[fs].u[fsf]) & 0x7fffff); return -1; }
    }
  }
  if (w == 0x8000033c) return -1;
  g_unsup_vu++; return -1;
}

/* out-of-line copies for the interpreter; the recompiled code (EE macro ops, demo/gen_vu.h micro programs) inlines the cores with constant words */
static __attribute__((noinline)) void vu_upper(uint32_t w) { vu_upper_i(w); }
static __attribute__((noinline)) int vu_lower(uint32_t w, int pc) { return vu_lower_i(w, pc); }
#ifndef GUEST_SPLIT_GEN   /* split build: the micro-program runner (pipeline model, AOT dispatch) is compiled into the runtime unit only */
/* ---- micro-mode pipeline model (interpreter) ----
   An upper (FMAC) result and its MAC / status / clip flags land 4 cycles after issue; a VF register written by a lower op (LQ, MOVE,
   ...) too; DIV / SQRT land Q after 7 cycles, RSQRT after 13.  An instruction that reads a VF register still in flight stalls; flag reads
   (FMAND, FSAND, FCAND, ...) and Q reads see what has landed by their cycle.  The collision programs rely on this: they test the MAC flags
   of an instruction issued 4 slots earlier while newer ones are in flight.  ACC chains (OPMULA then OPMSUB) do not stall.  Latencies as in
   the VU manual / PCSX2's microVU (lower-op VF latency 4 as microVU models it; inferred, not measured). */
typedef struct { int tgt, wm, rd[2], rm[2], flags; } VuUp;     /* flags: 1 MAC / status, 2 clip */
static void vu_up_info(uint32_t w, VuUp *u)
{
  int op = w & 0x3f, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fd = (w >> 6) & 31, sel = op & 3, idx = (w >> 6) & 31;
  static const char bm[4] = { 8, 4, 2, 1 };
  u->tgt = 0; u->wm = 0; u->rd[0] = u->rd[1] = 0; u->rm[0] = u->rm[1] = 0; u->flags = 0;
  if (op < 0x1c) { u->tgt = fd; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; u->rd[1] = ft; u->rm[1] = bm[sel]; u->flags = (op >> 2) != 4 && (op >> 2) != 5; return; }
  if (op < 0x30) {
    int mm = op == 0x1d || op == 0x1f || op == 0x2b || op == 0x2f, vec = op >= 0x28;
    u->tgt = fd; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; if (vec) { u->rd[1] = ft; u->rm[1] = op == 0x2e ? 14 : dest; } u->flags = !mm; if (op == 0x2e) u->rm[0] = 14; return;
  }
  if (op < 0x3c || idx >= 12) return;
  if (idx == 4 || idx == 5 || (idx == 7 && sel == 1)) { u->tgt = ft; u->wm = dest; u->rd[0] = fs; u->rm[0] = dest; return; }   /* ITOF / FTOI / ABS */
  if (idx == 7 && sel == 3) { u->rd[0] = fs; u->rm[0] = 14; u->rd[1] = ft; u->rm[1] = 1; u->flags = 2; return; }           /* CLIP */
  if (idx >= 10 && sel == 3) return;                                                                                        /* NOP */
  u->rd[0] = fs; u->rm[0] = (idx == 11 && sel == 2) ? 14 : dest;                                                              /* ACC ops */
  if (idx < 4 || idx == 6) { u->rd[1] = ft; u->rm[1] = bm[sel]; } else if (idx >= 10) { u->rd[1] = ft; u->rm[1] = (idx == 11 && sel == 2) ? 14 : dest; }
  u->flags = 1;
}
static int vu_low_q(uint32_t w)                                  /* Q latency of a lower op (DIV 7, SQRT 7, RSQRT 13), 1 for WAITQ, else 0 */
{
  if ((w >> 25) != 0x40 || (w & 0x3f) < 0x3c || ((w >> 6) & 31) != 14) return 0;
  switch (w & 3) { case 0: case 1: return 7; case 2: return 13; } return 1;
}
static void vu_low_reads(uint32_t w, int r[2], int m[2])
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31, fs = (w >> 11) & 31, fsf = (w >> 21) & 3, ftf = (w >> 23) & 3;
  r[0] = r[1] = 0; m[0] = m[1] = 0;
  if (op == 0x01) { r[0] = fs; m[0] = dest; return; }                                       /* SQ */
  if (op != 0x40 || (w & 0x3f) < 0x3c) return;
  int idx = (w >> 6) & 31, sel = w & 3;
  if (idx == 12) { r[0] = fs; m[0] = 15; return; }                                          /* MOVE / MR32 */
  if (idx == 13 && (sel & 1)) { r[0] = fs; m[0] = dest; return; }                           /* SQI / SQD */
  if (idx == 14 && sel != 3) { if (sel != 1) { r[0] = fs; m[0] = 8 >> fsf; } r[1] = ft; m[1] = 8 >> ftf; return; }   /* DIV / SQRT / RSQRT */
  if ((idx == 15 && sel == 0) || (idx == 16 && sel >= 2)) { r[0] = fs; m[0] = 8 >> fsf; }   /* MTIR, RINIT, RXOR */
}
static void vu_low_writes(uint32_t w, int *r, int *m)
{
  int op = w >> 25, dest = (w >> 21) & 15, ft = (w >> 16) & 31;
  *r = 0; *m = 0;
  if (op == 0x00) { *r = ft; *m = dest; return; }                                           /* LQ */
  if (op != 0x40 || (w & 0x3f) < 0x3c) return;
  int idx = (w >> 6) & 31, sel = w & 3;
  if (idx == 12 || (idx == 13 && !(sel & 1)) || (idx == 15 && sel == 1) || (idx == 16 && sel <= 1) || (idx == 32 && sel == 0)) { *r = ft; *m = dest; }
}
typedef struct { long t; int kind; uint32_t a, b; } VuPend;      /* kind 0: MAC a, status bits b; 1: clip bits a; 2: Q a, D / I bits b */
static VuPend vu_pq[64]; static int vu_pn; static long vu_qready; static long vu_vfready[32][4];
static void vu_push(long t, int kind, uint32_t a, uint32_t b) { if (vu_pn < 64) { vu_pq[vu_pn].t = t; vu_pq[vu_pn].kind = kind; vu_pq[vu_pn].a = a; vu_pq[vu_pn].b = b; vu_pn++; } }
static void vu_commit(long now)
{
  int k = 0;
  for (int i = 0; i < vu_pn; i++) {
    VuPend *p = &vu_pq[i];
    if (p->t > now) { vu_pq[k++] = *p; continue; }
    if (p->kind == 0) { VMAC = p->a; VSTAT = (VSTAT & 0xff0) | p->b | (p->b << 6); }
    else if (p->kind == 1) VCLIP = ((VCLIP << 6) | p->a) & 0xffffff;
    else { VQr = p->a; VSTAT = (VSTAT & ~0x30u) | p->b | (p->b << 6); }
  }
  vu_pn = k;
}
/* ---- program execution ---- */
#ifdef VU_DUMP
/* collects the microprogram images and their entry points (model/vurecomp.py input): /tmp/vu/<hash>.bin + /tmp/vu/entries.txt */
static int vu_dump_dirty = 1; static uint32_t vu_dump_hash; static uint32_t vu_dump_seen[4096]; static int vu_dump_n;
static void vu_dump_entry(int pc)
{
  if (vu_dump_dirty) { uint32_t h = 2166136261u; for (int i = 0; i < 1024; i++) { h ^= VUPROG[i]; h *= 16777619u; } vu_dump_hash = h; vu_dump_dirty = 0;
    char fn[64]; snprintf(fn, sizeof fn, "/tmp/vu/%08x.bin", h); FILE *f = fopen(fn, "rb"); if (!f) { f = fopen(fn, "wb"); fwrite(VUPROG, 4, 1024, f); } fclose(f); }
  uint32_t key = vu_dump_hash ^ ((uint32_t)pc * 0x9e3779b1u);
  for (int i = 0; i < vu_dump_n; i++) if (vu_dump_seen[i] == key) return;
  if (vu_dump_n < 4096) vu_dump_seen[vu_dump_n++] = key;
  FILE *f = fopen("/tmp/vu/entries.txt", "a"); fprintf(f, "%08x %d\n", vu_dump_hash, pc); fclose(f);
}
#endif
/* the micro memory image (FNV-1a of the 1024 words, recomputed after an MPG upload): selects the AOT-compiled program (demo/gen_vu.h) */
#ifndef VU_TAG_ENTER
#define VU_TAG_ENTER(h, pc)
#define VU_TAG_LEAVE()
#endif
#endif   /* !GUEST_SPLIT_GEN */
GSVS int vu_prog_clean; GSVS uint32_t vu_prog_hash;   /* clean: vu_prog_hash is the hash of VUPROG (0 after an MPG upload) */
#ifndef GUEST_SPLIT_GEN
static uint32_t vu_hash(void) { if (!vu_prog_clean) {
#ifdef VU_STATS
  g_hash_n++;
#endif
 uint32_t h = 2166136261u; for (int i = 0; i < 1024; i++) { h ^= VUPROG[i]; h *= 16777619u; } vu_prog_hash = h; vu_prog_clean = 1; } return vu_prog_hash; }
static int vu_aot(uint32_t hash, int pc);
#ifdef VU_PROFILE
void vu_prof(uint32_t h, int pc, int end);   /* boot.c: interpreter calls and steps per image and entry (dev) */
#endif
typedef struct { VQ vf[32]; uint32_t vi[16], acc[4], q, ir, clip, stat, mac; int tpc; uint8_t mem[4096]; } VuSnap;
static void vu_snap(VuSnap *s) { memcpy(s->vf, VF, sizeof VF); memcpy(s->vi, VIr, sizeof VIr); memcpy(s->acc, VACC, sizeof VACC); s->q = VQr; s->ir = VIreg; s->clip = VCLIP; s->stat = VSTAT; s->mac = VMAC; s->tpc = vu_tpc; memcpy(s->mem, VUMEM, 4096); }
static void vu_restore(const VuSnap *s) { memcpy(VF, s->vf, sizeof VF); memcpy(VIr, s->vi, sizeof VIr); memcpy(VACC, s->acc, sizeof VACC); VQr = s->q; VIreg = s->ir; VCLIP = s->clip; VSTAT = s->stat; VMAC = s->mac; vu_tpc = s->tpc; memcpy(VUMEM, s->mem, 4096); }
#ifdef YO_PROF
static uint64_t yo_prof_now(void); static uint64_t g_prof_vu; static unsigned g_prof_vucalls;
#endif
#ifdef VU_CHECK
/* differential test of the AOT-compiled programs: the interpreter reruns every compiled call on the saved entry state; any difference in the
   VF / VI / ACC / Q / I / CLIP registers, the micro memory or the next pc is reported (MAC / status are not compared: dead flags are skipped) */
static long vu_check_calls, vu_check_bad;
static void vu_run_interp(int pc);
static void vu_run(int pc)
{
  static VuSnap in, outc, outi; uint32_t h = vu_hash(); vu_snap(&in);
  if (!vu_aot(h, pc)) { vu_run_interp(pc); return; }
  vu_snap(&outc); vu_restore(&in); vu_run_interp(pc); vu_snap(&outi); vu_check_calls++;
#ifndef VU_CHECK_FLAGS   /* VU_CHECK_FLAGS: compare them too (recomp/vupipe.py with -DVP_EXACT_FLAGS) */
  outc.stat = outi.stat; outc.mac = outi.mac;   /* MAC / status are not compared (dead flags are skipped by the compiled forms) */
#endif
  if (memcmp(&outc, &outi, sizeof outc)) {
    if (vu_check_bad++ < 20) {
      fprintf(stderr, "VU_CHECK mismatch: image %08x entry %d\n", h, pc);
      for (int r = 0; r < 32; r++) for (int c = 0; c < 4; c++) if (outc.vf[r].u[c] != outi.vf[r].u[c]) fprintf(stderr, "  vf%d.%d compiled %08x interp %08x\n", r, c, outc.vf[r].u[c], outi.vf[r].u[c]);
      for (int r = 0; r < 16; r++) if (outc.vi[r] != outi.vi[r]) fprintf(stderr, "  vi%d %x %x\n", r, outc.vi[r], outi.vi[r]);
      for (int c = 0; c < 4; c++) if (outc.acc[c] != outi.acc[c]) fprintf(stderr, "  acc%d %08x %08x\n", c, outc.acc[c], outi.acc[c]);
      if (outc.q != outi.q) fprintf(stderr, "  q %08x %08x\n", outc.q, outi.q); if (outc.ir != outi.ir) fprintf(stderr, "  I %08x %08x\n", outc.ir, outi.ir);
      if (outc.stat != outi.stat) fprintf(stderr, "  status %x %x\n", outc.stat, outi.stat); if (outc.mac != outi.mac) fprintf(stderr, "  mac %x %x\n", outc.mac, outi.mac);
      if (outc.clip != outi.clip) fprintf(stderr, "  clip %x %x\n", outc.clip, outi.clip); if (outc.tpc != outi.tpc) fprintf(stderr, "  tpc %d %d\n", outc.tpc, outi.tpc);
      if (memcmp(outc.mem, outi.mem, 4096)) fprintf(stderr, "  micro memory differs\n");
    }
  }
}
static void vu_run_interp(int pc)
{
#else
static void vu_run(int pc)
{
#if defined(YO_PROF) && !defined(NOVUTIME)   /* NOVUTIME: no timer around every program (on the PSP the two syscalls per call were ~8% of the frame) */
  uint64_t t0_ = yo_prof_now(); g_prof_vucalls++;
  { uint32_t h_ = vu_hash(); VU_TAG_ENTER(h_, pc); int ok_ = vu_aot(h_, pc); VU_TAG_LEAVE();
    if (ok_) { g_prof_vu += yo_prof_now() - t0_; return; } }
#elif defined(YO_PROF)
  { g_prof_vucalls++; uint32_t h_ = vu_hash(); VU_TAG_ENTER(h_, pc); int ok_ = vu_aot(h_, pc); VU_TAG_LEAVE(); if (ok_) return; }
#elif defined(VU_STATS)
  { static int lk = -1; static long ls; static long st[64]; if (lk >= 0) st[lk] += vu_steps - ls; lk = -1;
    static uint32_t mh[64], mp[64], mn[64], hn[64]; static int nm; uint32_t h = vu_hash();
#ifdef VU_NOAOT
    int ok = 0;
#else
    int ok = vu_aot(h, pc);
#endif
    int k; for (k = 0; k < nm; k++) if (mh[k] == h && mp[k] == (uint32_t)pc) break; if (k == nm && nm < 64) { mh[k] = h; mp[k] = pc; nm++; }
    if (k < 64) (ok ? hn : mn)[k]++;
    if (pc < 0) { for (k = 0; k < nm; k++) printf("  vu %08x pc %4d compiled %u interp %u steps/call %ld\n", mh[k], mp[k], hn[k], mn[k], mn[k] ? st[k] / mn[k] : 0); return; }
    if (ok) return; lk = k < 64 ? k : -1; ls = vu_steps; }
#else
  if (vu_aot(vu_hash(), pc)) return;
#endif
#endif
  int pend = -1, endnext = 0; vu_calls++;
#ifdef GEN_DG_TRACE
  { void rt_dgt_vu(uint32_t h, int pc); rt_dgt_vu(vu_hash(), pc); }   /* dev (boot.c MGS2_DG_TRACE): micro calls outside the draw pipeline */
#endif
#ifdef VU_PROFILE
  vu_prof(vu_hash(), pc, 0);
#endif
#ifdef VU_DUMP
  vu_dump_entry(pc);
#endif
  long cyc = 0; vu_pn = 0; vu_qready = 0; memset(vu_vfready, 0, sizeof vu_vfready);
  for (int guard = 0; guard < 200000; guard++) {
    uint32_t lo = VUPROG[(pc & 511) * 2], hi = VUPROG[(pc & 511) * 2 + 1]; vu_steps++; if (vu_trace) printf("%04x ", pc);
    VuUp u; vu_up_info(hi & 0x07ffffff, &u); int lq = !(hi >> 31) ? vu_low_q(lo) : 0;
    long need = cyc;                                                       /* stall until the registers this pair reads are written */
    for (int k = 0; k < 2; k++) if (u.rd[k]) for (int c = 0; c < 4; c++) if (u.rm[k] & (8 >> c)) need = vu_vfready[u.rd[k]][c] > need ? vu_vfready[u.rd[k]][c] : need;
    if (!(hi >> 31)) { int lr[2], lm[2]; vu_low_reads(lo, lr, lm); for (int k = 0; k < 2; k++) if (lr[k]) for (int c = 0; c < 4; c++) if (lm[k] & (8 >> c)) need = vu_vfready[lr[k]][c] > need ? vu_vfready[lr[k]][c] : need; }
    if (lq && vu_qready > need) need = vu_qready;                          /* DIV / SQRT / RSQRT / WAITQ wait for the divider */
    cyc = need; vu_commit(cyc);
    uint32_t vmac = VMAC, vst = VSTAT, vcl = VCLIP; VQ old = VF[u.tgt];
    vu_upper(hi & 0x07ffffff);                                             /* its results and flags land 4 cycles later: the lower op of this pair sees the old ones */
    VQ nv = VF[u.tgt]; VF[u.tgt] = old;
    if (u.flags == 1) vu_push(cyc + 4, 0, VMAC, VSTAT & 0xf); else if (u.flags == 2) vu_push(cyc + 4, 1, VCLIP & 0x3f, 0);
    VMAC = vmac; VSTAT = vst; VCLIP = vcl;
    int took = -1;
    if (hi >> 31) VIreg = lo;
    else {
      uint32_t q0 = VQr, s0 = VSTAT; int lw, lwm; vu_low_writes(lo, &lw, &lwm);
      took = vu_lower(lo, pc);
      if (lq > 1) { vu_push(cyc + lq, 2, VQr, VSTAT & 0x30); vu_qready = cyc + lq; VQr = q0; VSTAT = (VSTAT & ~0x30u) | (s0 & 0x30); }
      if (lw) for (int c = 0; c < 4; c++) if (lwm & (8 >> c)) vu_vfready[lw][c] = cyc + 4;
    }
    if (u.tgt) for (int c = 0; c < 4; c++) if (u.wm & (8 >> c)) { VF[u.tgt].u[c] = nv.u[c]; vu_vfready[u.tgt][c] = cyc + 4; }
    cyc++;
    if (endnext) { vu_commit(cyc + 64); vu_tpc = pc + 1; break; }   /* all writes still in flight (latency <= 13); 1L << 40 was 0 on the PSP (32-bit long) */
    if (hi >> 30 & 1) endnext = 1;
    int npc = pc + 1; if (pend >= 0) { npc = pend; pend = -1; }
    if (took >= 0) pend = took;
    pc = npc;
  }
#ifdef VU_PROFILE
  vu_prof(0, 0, 1);
#endif
}
#ifdef GUEST_SPLIT_RT
void vu_run_shared(int pc) { vu_run(pc); }   /* the generated units call the micro programs through this one copy */
#endif
#endif   /* !GUEST_SPLIT_GEN */
/* ---- COP2 macro mode (EE side) ---- */
VU_INL void VUM(uint32_t w)
{
  int fn = w & 0x3f;
  if (fn == 0x38) { vu_run((w >> 6) & 0x7fff); return; }                                  /* VCALLMS imm15 */
  if (fn == 0x39) { vu_run(VCMSAR0); return; }                                            /* VCALLMSR: CMSAR0 in instruction units, as TPC (the game resumes with CMSAR0 = TPC) */
  if (fn >= 0x30 && fn <= 0x35) { vu_lower_i(0x80000000u | (w & 0x01ffffff), 0); return; }  /* VIADD.. */
  if (fn >= 0x3c) { int idx = (w >> 6) & 31; if (idx >= 12 && (idx <= 16 || idx >= 25)) { if (idx != 25 && idx <= 16) { vu_lower_i(0x80000000u | (w & 0x01ffffff), 0); return; } } }
  vu_upper_i(w & 0x01ffffff);
}
/* VUM with the flags of an upper op known dead (model/recomp.py: no cfc2 of MAC / status in the calling function) */
VU_INL void VUM0(uint32_t w)
{
  int fn = w & 0x3f;
  if (fn == 0x38 || fn == 0x39 || (fn >= 0x30 && fn <= 0x35)) { VUM(w); return; }
  if (fn >= 0x3c) { int idx = (w >> 6) & 31; if (idx >= 12 && (idx <= 16 || idx >= 25)) { if (idx != 25 && idx <= 16) { VUM(w); return; } } }
  vu_upper_fl(w & 0x01ffffff, 0);
}
/* lqc2 / sqc2 addresses are 16-byte aligned (the recompiler masks them; guest memory is 64-byte aligned): word moves, not unaligned pairs */
#if defined(__psp__) && defined(VU_QMOVE_VFPU)   /* one 16-byte VFPU move instead of four word loads and four stores (C700: scratch, as
                                                    recomp/vuemit.py vfpu_group's constant operand, whose value nothing reads) */
#define VU_CP16(d, s) __asm__ volatile ("lv.q C700, 0(%1)\n\tsv.q C700, 0(%0)" : : "r"(d), "r"(s) : "memory")
#else
#define VU_CP16(d, s) memcpy(__builtin_assume_aligned((d), 16), __builtin_assume_aligned((s), 16), 16)
#endif
#ifdef GEN_DG_TRACE   /* dev (boot.c MGS2_READ_HASH): a 16-byte load */
static inline void VLQC2(int ft, uint32_t a) { extern int g_dgt_w; if (ft) { uint8_t *p_; g_dgt_w = 16; p_ = MM(a); g_dgt_w = 0; VU_CP16(VF[ft].u, p_); } }
#else
static inline void VLQC2(int ft, uint32_t a) { if (ft) VU_CP16(VF[ft].u, MM(a)); }
#endif
#ifdef GUEST_STORE16                     /* gen_pre.h GEN_MM_FAST: GUEST_STORE16 with the 16-byte move */
static inline void VSQC2(int ft, uint32_t a) { uint32_t a_ = a; if (GEN_FAST(a_)) { GEN_ODD_W(a_, 16); VU_CP16(GEN_RAM(a_), VF[ft].u); } else rt_store(a_, VF[ft].u, 16); }
#else
static inline void VSQC2(int ft, uint32_t a) { memcpy(__builtin_assume_aligned(MM(GUEST_WR_HOOK(a, 16)), 16), VF[ft].u, 16); }
#endif
static uint32_t VCFC2(int n)
{
  if (n < 16) return VI16(n);
  switch (n) { case 16: return VSTAT; case 17: return VMAC; case 18: return VCLIP; case 20: return VRr; case 21: return VIreg; case 22: return VQr; case 26: return vu_tpc; case 27: return VCMSAR0; }   /* 29 VPU-STAT: never busy (0) */
  return 0;
}
static void VCTC2(int n, uint32_t v)
{
  if (n < 16) { vi_set(n, v); return; }
  switch (n) { case 16: VSTAT = v; break; case 17: VMAC = v; break; case 18: VCLIP = v; break; case 20: VRr = v; break; case 21: VIreg = v; break; case 22: VQr = v; break; case 27: VCMSAR0 = v & 0xffff; break; }   /* COP2 control 27 is CMSAR0 */
}
static void VQMFC2(int rt, int rd) { if (rt) { R[rt] = (int64_t)(((uint64_t)VF[rd].u[1] << 32) | VF[rd].u[0]); RH[rt] = (int64_t)(((uint64_t)VF[rd].u[3] << 32) | VF[rd].u[2]); } }
static void VQMTC2(int rt, int rd) { if (rd) { VF[rd].u[0] = (uint32_t)R[rt]; VF[rd].u[1] = (uint32_t)((uint64_t)R[rt] >> 32); VF[rd].u[2] = rt ? (uint32_t)RH[rt] : 0; VF[rd].u[3] = rt ? (uint32_t)((uint64_t)RH[rt] >> 32) : 0; } }
static void vu_reset(void) { memset(VF, 0, sizeof VF); VF[0].u[3] = 0x3f800000; memset(VIr, 0, sizeof VIr); memset(VACC, 0, sizeof VACC); VQr = VPr = VIreg = VRr = VMAC = VSTAT = VCLIP = VCMSAR0 = 0; }
#include "vif.h"
#ifdef VU_FAST
/* fast form of the AOT programs (demo/gen_vu.h): results without the VU float model, checked with the FPU sticky flags. No overflow and no
   underflow (with flush-to-zero on) means every result was a normal value or an exact zero, on which the model is the identity; otherwise
   the call is undone and the exact form runs. g_vu_fast = 0 runs only the exact form; g_vu_fcheck = 1 runs both on every call and compares. */
static int g_vu_fast = 1, g_vu_fcheck, g_vu_vfpu, g_vf_chk;
#ifndef VU_VFPU_BADREPORT
#define VU_VFPU_BADREPORT(h, pc, k, q)
#endif
#ifndef VU_FCHECK_REPORT
#define VU_FCHECK_REPORT(h, pc, a, b)
#endif
static long vu_fast_calls, vu_fast_fallbacks, vu_fcheck_calls, vu_fcheck_bad;
static uint32_t vuf_last; static uint32_t vuf_stat, vuf_mac, vuf_clip, vuf_vmac, vuf_fenv; static int vuf_tpc;
#if defined(__psp__)
static inline uint32_t vu_fcr_get(void) { uint32_t r; __asm__ volatile("cfc1 %0, $31" : "=r"(r) :: "memory"); return r; }
static inline void vu_fcr_set(uint32_t r) { __asm__ volatile("ctc1 %0, $31" :: "r"(r) : "memory"); }
/* FCR31: flags 2..6, enables 7..11, cause 12..17, FS (flush to zero) 24; underflow / overflow flags 0x08 / 0x10 */
/* underflow (0x08) is not a fallback: with FS the hardware gives the flushed signed zero the VU model gives, and it also raises underflow
   for exact zero differences (1 - 1), which made a quarter of the calls fall back */
#ifndef VU_FBAD_MASK
#define VU_FBAD_MASK 0x10u
#endif
#define VU_FENV_BEGIN() do { vuf_fenv = vu_fcr_get(); vu_fcr_set((vuf_fenv & ~0x3ff7cu) | 0x1000000u); } while (0)
/* the fast form's entry check of one value on the FPU flags (VU_FENV_BEGIN cleared them; FS on): x * 1 is exact for a normal value or a
   zero and underflows (U) for a denormal (flushed); x * 0 is invalid (V) for +-Inf; the signaling compare c.lt is invalid for a NaN.
   VU_ECHK_END reads U | V once (hardware flag rules: psp_bench FCR31 table) */
static inline __attribute__((always_inline)) void vu_fchk(float x, float one, float zero)
{ float t_; __asm__ volatile("mul.s %0, %1, %2\n\tmul.s %0, %1, %3\n\tc.lt.s %1, %1" : "=&f"(t_) : "f"(x), "f"(one), "f"(zero) : "$fcc0"); }
#define VU_FENV_BAD() ({ uint32_t f_ = vu_fcr_get(); vu_fcr_set(vuf_fenv); vuf_last = f_; (f_ & VU_FBAD_MASK) != 0; })
#define VU_FSYNC(x) __asm__ volatile("" :: "f"(x))
#else
#include <xmmintrin.h>
/* MXCSR: flags 0..5 (overflow 0x08, underflow 0x10), FTZ 0x8000 */
#ifndef VU_FBAD_MASK
#define VU_FBAD_MASK 0x08u     /* overflow (underflow 0x10 ignored, as on the PSP) */
#endif
#define VU_FENV_BEGIN() do { vuf_fenv = _mm_getcsr(); _mm_setcsr((vuf_fenv & ~0x3fu) | 0x8000u); } while (0)
#define VU_FENV_BAD() ({ uint32_t f_ = _mm_getcsr(); _mm_setcsr(vuf_fenv); (f_ & VU_FBAD_MASK) != 0; })
#define VU_FSYNC(x) __asm__ volatile("" :: "x"(x))
#endif
/* undo log of the micro memory qwords stored by a fast call (VU_UNDO at SQI) */
static uint8_t vuf_dirty[256], vuf_list[256], vuf_save[256][16]; static int vuf_n;
static inline void vu_undo_q(uint32_t a) { a &= 0xff; if (!vuf_dirty[a]) { vuf_dirty[a] = 1; vuf_list[vuf_n++] = (uint8_t)a; memcpy(vuf_save[a], VUMEM + (a << 4), 16); } }
static inline void vu_undo_end(int undo) { while (vuf_n) { uint32_t a = vuf_list[--vuf_n]; vuf_dirty[a] = 0; if (undo) memcpy(VUMEM + (a << 4), vuf_save[a], 16); } }
static inline __attribute__((always_inline)) void vu_fast_begin(void) { vuf_stat = VSTAT; vuf_mac = VMAC; vuf_clip = VCLIP; vuf_vmac = g_vmac; vuf_tpc = vu_tpc; VU_FENV_BEGIN(); }
/* entry check of the fast form: a register value read before any write must already be in VU float form (not a denormal, Inf or NaN) */
static inline __attribute__((always_inline)) uint32_t vu_ebad(uint32_t u) { uint32_t t = u & 0x7fffffffu; return (t - 1u < 0x7fffffu) | (t >= 0x7f800000u); }
static inline __attribute__((always_inline)) int vu_fast_abort(void) { (void)VU_FENV_BAD(); return 0; }
/* a fast call abandoned after running (the VFPU form found an Inf / NaN result): the flag state and the FPU environment as on entry (no
   micro memory stores in the VFPU programs: nothing else to undo) */
static inline void vu_fast_undo(void) { (void)VU_FENV_BAD(); VSTAT = vuf_stat; VMAC = vuf_mac; VCLIP = vuf_clip; g_vmac = vuf_vmac; vu_tpc = vuf_tpc; }
/* a fast call abandoned in the middle (a raw micro-memory value that is not in VU float form, VU_LCHK): the state as on entry (micro memory
   stores undone, flags / clip / tpc restored, FPU environment back; registers live in the call's locals until its exit) */
static __attribute__((noinline, cold)) int vu_fast_fail(void)
{
  (void)VU_FENV_BAD(); if (vuf_n) vu_undo_end(1);
  VSTAT = vuf_stat; VMAC = vuf_mac; VCLIP = vuf_clip; g_vmac = vuf_vmac; vu_tpc = vuf_tpc; return 0;
}
static inline __attribute__((always_inline)) int vu_fast_bad(void)
{
  if (!VU_FENV_BAD()) { if (vuf_n) vu_undo_end(0); return 0; }
  if (vuf_n) vu_undo_end(1);
  VSTAT = vuf_stat; VMAC = vuf_mac; VCLIP = vuf_clip; g_vmac = vuf_vmac; vu_tpc = vuf_tpc; return 1;
}
#endif
/* VU0 macro ops on the PSP's VFPU (recomp/vuemit.py macro_vfpu, VU_VFPU_MACRO=1; Ffm's consent 2026-10-20: results may differ from the PS2
   in the last bit).  The VFPU is IEEE single with round to nearest, denormal inputs and results flushed to zero (hwprobe, harness v8), not
   the VU's round toward zero.  vu_vfpu(A, C): on the PSP the asm statement (A: its operands in parentheses); on x86, the reference the
   PSP's state hashes are checked against, the statement expression C: the same operations in the same order with the VFPU's rounding
   (vu_rnadd / vu_rnsub / vu_rnmul switch MXCSR to round to nearest, FTZ, DAZ around one operation).  "vu_" names: recomp/regcache.py
   reads them as pure calls.  (PPSSPP runs VFPU arithmetic in the FPU's rounding mode: an emulator run does not show the PSP's results.) */
/* vu_vgrp(A, B, X, K, R, F): a run of macro ops on the VFPU with a check (recomp/vuemit.py vfpu_group, VU_VFPU=1).  PSP: the asm A (loads,
   ops, the NaN / infinity check into vcc_: bit 4 set = a lane failed), then the asm B (stores) or, when the check failed, F (cold): the C
   forms of the same ops (the exact VU0 model) on the registers nothing has written yet.  x86: X (the same ops with the VFPU's rounding,
   the registers it writes saved in vu_sv first), K the same check (vu_fin: not NaN / infinity), on a failure R (restore) and F.
   g_vgrp_fb (boot.c) counts the fallbacks.
   F runs with the VFPU's rounding (VU_FB_ON / VU_FB_OFF: FCR31 / MXCSR rounding to nearest, flush kept; toward zero under the dev
   MGS2_VFPU_RZ=1), so a lane holding ordinary values gets the same bits whether its run took the VFPU or fell back, and only the lanes
   that hold a NaN / infinity / huge value depend on the check.  Why: v15 on the PSP took the VFPU where x86 fell back because a garbage
   lane differed (an FPU rule x86 did not model, guest.h FADD), and every other lane of the run then differed in the last bit (vb 15794).
   recomp/vupipe.py VPF does the same for the micro programs' VFPU forms. */
extern uint32_t g_vgrp_fb;
#define vu_fin(u) (((u) & 0x7f800000u) != 0x7f800000u)
#if defined(__psp__)
#define vu_vfpu(A, C) __asm__ volatile A
#define VU_FB_ON(m) do { __asm__ volatile("nop\n\tcfc1 %0, $31" : "=r"(m) : : "memory"); __asm__ volatile("ctc1 %0, $31\n\tnop\n\tnop" : : "r"((m) & ~3u) : "memory"); } while (0)
#define VU_FB_OFF(m) __asm__ volatile("ctc1 %0, $31\n\tnop\n\tnop" : : "r"(m) : "memory")
#if defined(VU_RZ_ALL)
/* E1 "Everything exact" (Ffm 2026-11-08 17:46, task #178, DEVIATIONS.md E1): a unit built -DVU_RZ_ALL runs F alone, the exact VU0
   model, in the game's FPU mode (FCR31 0x01000001, main_psp.c: round toward zero + FS), as the PS2's VU0 rounds; no VFPU, no check.
   The x86 twin with MGS2_VFPU_RZ=1 is its reference: X runs toward zero with FTZ / DAZ, and where its check fails, F.  The micro
   programs' C forms must run in the same mode (built without VP_VFPU). */
#if defined(VP_VFPU)
#error "VU_RZ_ALL: build without VP_VFPU (the micro programs' VFPU forms round to nearest)"
#endif
#define vu_vgrp(A, B, X, K, R, F) do { F; } while (0)
#else
#define vu_vgrp(A, B, X, K, R, F) do { uint32_t vcc_, vct_; __asm__ volatile A; if (__builtin_expect(vcc_ & 0x10u, 0)) { uint32_t fm_; g_vgrp_fb++; VU_FB_ON(fm_); F; VU_FB_OFF(fm_); } \
  else __asm__ volatile B; } while (0)
#endif
#else
static uint32_t vu_sv[33][4];
#ifdef VGRP_TRACE   /* dev: the fallbacks counted per host code address (boot.c vgrp_note, MGS2_VGRP_TRACE=<first vb>:<last vb>) */
void vgrp_note(const void *pc, const uint32_t (*sv)[4]);   /* sv: the unit's vu_sv (the values the run's written registers held before it) */
#define VGRP_NOTE() vgrp_note(({ __label__ l_; l_: ; &&l_; }), (const uint32_t (*)[4])vu_sv)
#else
#define VGRP_NOTE() ((void)0)
#endif
#define VU_FB_ON(m) do { if (!vu_rn_bits) vu_rn_bits = getenv("MGS2_VFPU_RZ") ? 0xe040u : 0x8040u; (m) = _mm_getcsr(); \
  _mm_setcsr(((m) & ~0x6000u) | (vu_rn_bits & 0x6000u)); __asm__ volatile("" ::: "memory"); } while (0)
#define VU_FB_OFF(m) do { __asm__ volatile("" ::: "memory"); _mm_setcsr(m); } while (0)
#define vu_vgrp(A, B, X, K, R, F) do { X; if (!(K)) { uint32_t fm_; g_vgrp_fb++; VGRP_NOTE(); R; VU_FB_ON(fm_); F; VU_FB_OFF(fm_); } } while (0)
#include <xmmintrin.h>
static uint32_t vu_rn_bits;   /* dev MGS2_VFPU_RZ=1: the VFPU ops in the reference's mode (round toward zero, FTZ, DAZ: the VU reads a denormal as zero), so a build must give the round-toward-zero hashes */
static inline uint32_t vu_rn_on(void)
{ uint32_t m = _mm_getcsr(); if (!vu_rn_bits) vu_rn_bits = getenv("MGS2_VFPU_RZ") ? 0xe040u : 0x8040u; _mm_setcsr((m & ~0x6000u) | vu_rn_bits); return m; }
static void vu_rn_chk(int op, float a, float b, float r);   /* dev MGS2_VFPU_CHK=1 (end of this file) */
#define VU_RNOP(name, op) static __attribute__((noinline, unused)) float name(float a, float b) \
  { uint32_t m = vu_rn_on(); __asm__ volatile("" : "+x"(a), "+x"(b)); float r = a op b; __asm__ volatile("" : "+x"(r)); _mm_setcsr(m); vu_rn_chk(#op[0], a, b, r); return r; }
VU_RNOP(vu_rnadd, +)
VU_RNOP(vu_rnsub, -)
VU_RNOP(vu_rnmul, *)
#define vu_vfpu(A, C) C
#endif
#ifdef MGS2_GEN_VU
#include MGS2_GEN_VU   /* the AOT VU0 micro programs (vurecomp.py); MGS2: not generated yet */
#else   /* no AOT programs: every micro call runs on the interpreter (vu_aot returns 0); the helpers the macro ops use, as vurecomp.py emits them */
static inline float vu_f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t vu_u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
#ifdef VU_MEASURE_NOCLAMP   /* dev measurement only, NOT exact: the macro ops without their input / result clamps (the cost of the clamps) */
static inline __attribute__((always_inline)) float vu_in(float f) { return f; }
#else
static inline __attribute__((always_inline)) float vu_in(float f) { uint32_t u = vu_u(f), e = u & 0x7f800000u; if (!e) u &= 0x80000000u; else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu; return vu_f(u); }
#endif
#define vu_cl vu_in
static inline __attribute__((always_inline)) float vu_clf(float f, int sh) { return vu_f(vout(f, sh)); }   /* + MAC bits (g_vmac) */
static inline __attribute__((always_inline)) float vu_max(float a, float b) { return a > b ? a : b; }
static inline __attribute__((always_inline)) float vu_min(float a, float b) { return a < b ? a : b; }
static inline __attribute__((always_inline)) float vu_ftoi(float v) { return vu_f(v >= 2147483648.0f ? 0x7fffffffu : v <= -2147483648.0f ? 0x80000000u : (uint32_t)(int32_t)v); }
static int vuc_entry(uint32_t hash, int pc) { (void)hash; (void)pc; return 0; }
static int vuc_dispatch(uint32_t hash, int pc) { (void)hash; (void)pc; return 0; }
#ifdef VU_FAST
static int vucf_dispatch(uint32_t hash, int pc) { (void)hash; (void)pc; return 0; }
#endif
#endif
/* the result clamp of the VU0 macro ops (model/vuemit.py, the lanes whose MAC / status flags are dead).  PSP: the game runs with
   flush-to-zero (main: FCR31 FS, the PS2 FPU rule), so from inputs in VU float form (finite normal or signed zero) a result is a normal
   value, a signed zero (an underflow, flushed by the hardware with its sign: the VU float model's mapping) or +-Inf (overflow), never a
   NaN; the per-lane clamp (+-Inf -> +-Fmax) runs only when the sticky overflow flag is set, read once per op after its lanes (the asm
   takes the results as inputs: they are computed before the read).  Elsewhere (x86 reference): the clamp on every lane. */
#if defined(VU_MEASURE_NOCLAMP)
#define VU_MCL1(a) ((void)0)
#define VU_MCL2(a, b) ((void)0)
#define VU_MCL3(a, b, c) ((void)0)
#define VU_MCL4(a, b, c, d) ((void)0)
#elif defined(__psp__) && defined(VU_MCL_RZ)
/* PSP with FCR31 0x01000001 (main_psp.c: round toward zero + FS, never changed: guest cfc1 / ctc1 are no-ops in recomp.py): from VU
   float form inputs an op cannot overflow to +-Inf (round toward zero gives +-Fmax, the clamp's value) nor give a denormal (FS), so the
   result is already in VU float form and the clamp is the identity (the x86 reference runs the same rounding: MGS2_FPU_RZ=1) */
#define VU_MCL1(a) ((void)0)
#define VU_MCL2(a, b) ((void)0)
#define VU_MCL3(a, b, c) ((void)0)
#define VU_MCL4(a, b, c, d) ((void)0)
#elif defined(__psp__) && !defined(VU_MCL_EXACT)
static __attribute__((noinline, cold)) void vu_mo_clear(void) { uint32_t r; __asm__ volatile("cfc1 %0, $31" : "=r"(r)); __asm__ volatile("ctc1 %0, $31" :: "r"(r & ~0x3f010u)); }
#define VU_MO_(...) ({ uint32_t r_; __asm__ volatile("cfc1 %0, $31" : "=r"(r_) : __VA_ARGS__); (r_ & 0x10u) != 0; })
#define VU_MCL1(a) do { if (__builtin_expect(VU_MO_("f"(a)), 0)) { a = vu_cl(a); vu_mo_clear(); } } while (0)
#define VU_MCL2(a, b) do { if (__builtin_expect(VU_MO_("f"(a), "f"(b)), 0)) { a = vu_cl(a); b = vu_cl(b); vu_mo_clear(); } } while (0)
#define VU_MCL3(a, b, c) do { if (__builtin_expect(VU_MO_("f"(a), "f"(b), "f"(c)), 0)) { a = vu_cl(a); b = vu_cl(b); c = vu_cl(c); vu_mo_clear(); } } while (0)
#define VU_MCL4(a, b, c, d) do { if (__builtin_expect(VU_MO_("f"(a), "f"(b), "f"(c), "f"(d)), 0)) { a = vu_cl(a); b = vu_cl(b); c = vu_cl(c); d = vu_cl(d); vu_mo_clear(); } } while (0)
#else
#define VU_MCL1(a) ((a) = vu_cl(a))
#define VU_MCL2(a, b) ((a) = vu_cl(a), (b) = vu_cl(b))
#define VU_MCL3(a, b, c) ((a) = vu_cl(a), (b) = vu_cl(b), (c) = vu_cl(c))
#define VU_MCL4(a, b, c, d) ((a) = vu_cl(a), (b) = vu_cl(b), (c) = vu_cl(c), (d) = vu_cl(d))
#endif
/* the result clamp in a vu_vgrp fallback (F; tools/vgrp_fcl.py): F runs with the VFPU's rounding (VU_FB_ON), where an overflow gives
   +-Inf, so the VU_MCL_RZ identity does not hold there (v16 emulator, vb 15739: +-Inf stored where x86 had +-Fmax).  PSP: from VU float
   form inputs (F clamps them first) a result is finite, a signed zero (FS) or +-Inf, so |u| is capped at Fmax by min, branch-free (the
   functions' control flow, and so their PGO profile, stay the same).  x86: VU_MCL (the full clamp on every lane).
   PSP -DVU_RZ_ALL (E1): F runs in the game's mode (toward zero + FS), where an op cannot overflow to +-Inf, so VU_MCL's rule holds. */
#if defined(__psp__) && defined(VU_RZ_ALL)
#define VU_FCL1 VU_MCL1
#define VU_FCL2 VU_MCL2
#define VU_FCL3 VU_MCL3
#define VU_FCL4 VU_MCL4
#elif defined(__psp__)
static inline __attribute__((always_inline)) float vu_ofl(float f)
{ uint32_t u = vu_u(f), m; __asm__("min %0, %1, %2" : "=r"(m) : "r"(u & 0x7fffffffu), "r"(0x7f7fffffu)); return vu_f(m | (u & 0x80000000u)); }
#define VU_FCL1(a) ((a) = vu_ofl(a))
#define VU_FCL2(a, b) ((a) = vu_ofl(a), (b) = vu_ofl(b))
#define VU_FCL3(a, b, c) ((a) = vu_ofl(a), (b) = vu_ofl(b), (c) = vu_ofl(c))
#define VU_FCL4(a, b, c, d) ((a) = vu_ofl(a), (b) = vu_ofl(b), (c) = vu_ofl(c), (d) = vu_ofl(d))
#else
#define VU_FCL1 VU_MCL1
#define VU_FCL2 VU_MCL2
#define VU_FCL3 VU_MCL3
#define VU_FCL4 VU_MCL4
#endif
#ifdef VU_TRACEH
static uint32_t g_vth = 2166136261u; static long g_vtn;
static void vu_traceh(void) { static VuSnap s; vu_snap(&s); const uint32_t *p = (const uint32_t *)&s; for (unsigned i = 0; i < sizeof s / 4; i++) { g_vth ^= p[i]; g_vth *= 16777619u; } if (++g_vtn <= 2200 && (g_vtn % 100 == 0 || (g_vtn > 900 && g_vtn <= 1000))) VU_TRACEH(g_vtn, g_vth); }
#define VU_AOT_RET(x) do { int r_ = (x); vu_traceh(); return r_; } while (0)
#else
#define VU_AOT_RET(x) return (x)
#endif
static int vu_aot_(uint32_t h, int pc);
static int vu_aot(uint32_t h, int pc) { VU_AOT_RET(vu_aot_(h, pc)); }
static int vu_aot_(uint32_t h, int pc)
{
  if (!vuc_entry(h, pc)) return 0;
#ifdef VU_FAST
#ifdef VU_VFPU
  if (g_vu_vfpu) {
    if (g_vu_fcheck) {       /* VFPU form against the exact form */
      static VuSnap in, of, oe; vu_snap(&in); vu_fcheck_calls++;
      if (!vucv_dispatch(h, pc)) { vu_fast_fallbacks++; vu_snap(&of); if (memcmp(&of, &in, sizeof of)) { vu_fcheck_bad++; VU_FCHECK_REPORT(h, pc | 0x10000, &of, &in); } vuc_dispatch(h, pc); return 1; }
      vu_snap(&of); vu_restore(&in); vuc_dispatch(h, pc); vu_snap(&oe);
      if (memcmp(&of, &oe, sizeof of)) { vu_fcheck_bad++; VU_FCHECK_REPORT(h, pc, &of, &oe); }
      return 1;
    }
#ifdef VU_VFPU_DBG   /* the VFPU results are used; a fallback is checked to have left the entry state */
    { static VuSnap in, of; vu_snap(&in); vu_fast_calls++; if (vucv_dispatch(h, pc)) return 1; vu_fast_fallbacks++;
      vu_snap(&of); if (memcmp(&of, &in, sizeof of)) { vu_fcheck_bad++; VU_FCHECK_REPORT(h, pc | 0x10000, &of, &in); } vuc_dispatch(h, pc); return 1; }
#endif
    vu_fast_calls++; if (vucv_dispatch(h, pc)) return 1; vu_fast_fallbacks++; vuc_dispatch(h, pc); return 1;
  }
#endif
  if (g_vu_fcheck) {
    static VuSnap in, of, oe; vu_snap(&in); vu_fcheck_calls++;
    if (!vucf_dispatch(h, pc)) { vu_fast_fallbacks++; vuc_dispatch(h, pc); return 1; }
    vu_snap(&of); vu_restore(&in); vuc_dispatch(h, pc); vu_snap(&oe);
    if (memcmp(&of, &oe, sizeof of)) vu_fcheck_bad++;
    return 1;
  }
  if (g_vu_fast) { vu_fast_calls++; if (vucf_dispatch(h, pc)) return 1; vu_fast_fallbacks++;
#ifdef VU_FSTATS
    VU_FSTATS(h, pc, vuf_last);
#endif
  }
#endif
  return vuc_dispatch(h, pc);   /* 0: not run (vupipe.py -DVP_FAST outside its FPU mode): the interpreter runs it */
}
#if !defined(__psp__)
/* dev MGS2_VFPU_CHK=1 (x86): each VFPU-form op against the VU model on the same inputs in the current FPU mode (vu_in on the inputs,
   vu_cl on the result); the first 30 differences to stderr.  With MGS2_VFPU_RZ=1 every difference is an input or a result the VU float
   model treats differently from IEEE (a denormal, Inf / NaN pattern, an overflow), not a rounding difference. */
static void vu_rn_chk(int op, float a, float b, float r)
{
  static int on = -1; static long n, bad;
  if (on < 0) on = getenv("MGS2_VFPU_CHK") != 0;
  if (!on) return;
  n++;
  uint32_t u[4]; float f[4] = { a, b, r, 0 };
  memcpy(u, f, 12);
  for (int i = 0; i < 2; i++) { uint32_t ex = u[i] & 0x7f800000u; if (!ex) u[i] &= 0x80000000u; else if (ex == 0x7f800000u) u[i] = (u[i] & 0x80000000u) | 0x7f7fffffu; }
  memcpy(f, u, 8);
  f[3] = op == '+' ? f[0] + f[1] : op == '-' ? f[0] - f[1] : f[0] * f[1];
  memcpy(&u[3], &f[3], 4);
  { uint32_t ex = u[3] & 0x7f800000u; if (!ex) u[3] &= 0x80000000u; else if (ex == 0x7f800000u) u[3] = (u[3] & 0x80000000u) | 0x7f7fffffu; }
  if (u[3] != u[2] && bad++ < 30) { uint32_t ab[2]; float fa[2] = { a, b }; memcpy(ab, fa, 8); fprintf(stderr, "vfpu chk: op %ld %c %08x %08x -> %08x, VU model %08x\n", n, op, ab[0], ab[1], u[2], u[3]); }
}
#endif

#endif
