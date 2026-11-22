/* MGS2 port: the EE instructions the TT runtime did not need (emitted by mgs2/recomp/recomp.py).
   Needs guest.h first (R[] / RH[] = low / high 64 bits of the 128-bit GPRs, RD* / WR* guest memory).
   Unaligned loads / stores follow PCSX2 (little-endian EE): LWR of a full word sign-extends, otherwise it keeps bits 63..32.
   MMI lane order: lane 0 = least significant (R low bits). Each op reads its sources before writing rd (rd may equal rs or rt). */
#ifndef MGS2_MMI_H
#define MGS2_MMI_H

/* ---- unaligned loads / stores ---- */
#ifdef GEN_DG_TRACE   /* dev (boot.c MGS2_READ_HASH): the same, telling the tracer which bytes a partial load uses (g_dgt_w / g_dgt_o) and
                         that a partial store's read is its merge (width -1), not a load */
extern int g_dgt_w, g_dgt_o;
static inline int64_t GLWL(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t w; g_dgt_w = k + 1; g_dgt_o = 0; w = *(uint32_t *)MM(a & ~3u); g_dgt_w = g_dgt_o = 0;
  uint32_t v = ((uint32_t)r & (0x00ffffffu >> (8 * k))) | (w << (8 * (3 - k))); return (int64_t)(int32_t)v; }
static inline int64_t GLWR(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t w; g_dgt_w = 4 - k; g_dgt_o = k; w = *(uint32_t *)MM(a & ~3u); g_dgt_w = g_dgt_o = 0; uint32_t v = ((uint32_t)r & ~(0xffffffffu >> (8 * k))) | (w >> (8 * k));
  return k == 0 ? (int64_t)(int32_t)v : (int64_t)(((uint64_t)r & 0xffffffff00000000ull) | v); }
static inline void GSWL(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t m; g_dgt_w = -1; m = *(uint32_t *)MM(a & ~3u); g_dgt_w = 0; WR32(a & ~3u, (m & ~(0xffffffffu >> (8 * (3 - k)))) | ((uint32_t)r >> (8 * (3 - k)))); }
static inline void GSWR(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t m; g_dgt_w = -1; m = *(uint32_t *)MM(a & ~3u); g_dgt_w = 0; WR32(a & ~3u, (m & ~(0xffffffffu << (8 * k))) | ((uint32_t)r << (8 * k))); }
static inline int64_t GLDL(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t d; g_dgt_w = k + 1; g_dgt_o = 0; d = *(uint64_t *)MM(a & ~7u); g_dgt_w = g_dgt_o = 0; return (int64_t)(((uint64_t)r & (0x00ffffffffffffffull >> (8 * k))) | (d << (8 * (7 - k)))); }
static inline int64_t GLDR(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t d; g_dgt_w = 8 - k; g_dgt_o = k; d = *(uint64_t *)MM(a & ~7u); g_dgt_w = g_dgt_o = 0; return (int64_t)(((uint64_t)r & (0xffffffffffffff00ull << (8 * (7 - k)))) | (d >> (8 * k))); }
static inline void GSDL(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t m; g_dgt_w = -1; m = *(uint64_t *)MM(a & ~7u); g_dgt_w = 0; WR64(a & ~7u, (m & (0xffffffffffffff00ull << (8 * k))) | ((uint64_t)r >> (8 * (7 - k)))); }
static inline void GSDR(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t m; g_dgt_w = -1; m = *(uint64_t *)MM(a & ~7u); g_dgt_w = 0; WR64(a & ~7u, (m & (0x00ffffffffffffffull >> (8 * (7 - k)))) | ((uint64_t)r << (8 * k))); }
#else
static inline int64_t GLWL(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t w = RD32(a & ~3u); uint32_t v = ((uint32_t)r & (0x00ffffffu >> (8 * k))) | (w << (8 * (3 - k))); return (int64_t)(int32_t)v; }
static inline int64_t GLWR(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t w = RD32(a & ~3u); uint32_t v = ((uint32_t)r & ~(0xffffffffu >> (8 * k))) | (w >> (8 * k));
  return k == 0 ? (int64_t)(int32_t)v : (int64_t)(((uint64_t)r & 0xffffffff00000000ull) | v); }
static inline void GSWL(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t m = RD32(a & ~3u); WR32(a & ~3u, (m & ~(0xffffffffu >> (8 * (3 - k)))) | ((uint32_t)r >> (8 * (3 - k)))); }
static inline void GSWR(int64_t r, uint32_t a)
{ unsigned k = a & 3; uint32_t m = RD32(a & ~3u); WR32(a & ~3u, (m & ~(0xffffffffu << (8 * k))) | ((uint32_t)r << (8 * k))); }
static inline int64_t GLDL(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t d = RD64(a & ~7u); return (int64_t)(((uint64_t)r & (0x00ffffffffffffffull >> (8 * k))) | (d << (8 * (7 - k)))); }
static inline int64_t GLDR(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t d = RD64(a & ~7u); return (int64_t)(((uint64_t)r & (0xffffffffffffff00ull << (8 * (7 - k)))) | (d >> (8 * k))); }
static inline void GSDL(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t m = RD64(a & ~7u); WR64(a & ~7u, (m & (0xffffffffffffff00ull << (8 * k))) | ((uint64_t)r >> (8 * (7 - k)))); }
static inline void GSDR(int64_t r, uint32_t a)
{ unsigned k = a & 7; uint64_t m = RD64(a & ~7u); WR64(a & ~7u, (m & (0x00ffffffffffffffull >> (8 * (7 - k)))) | ((uint64_t)r << (8 * k))); }
#endif
/* tools/ldu_fuse.py: an LDL a+7 / LDR a pair (rl: the LDR first) or an SDL a+7 / SDR a pair as one call (boot/gen_dispatch.c: one host
   access when both ends are plain RAM, else the pair in its order).  Declared only: a definition here would renumber the functions of
   every unit and void their PGO profile */
int64_t GLDU(int64_t r, uint32_t a, int rl); void GSDU(int64_t r, uint32_t a, int rl);

/* ---- 128-bit lanes ---- */
typedef union { uint64_t d[2]; uint32_t w[4]; uint16_t h[8]; uint8_t b[16]; int32_t sw[4]; int16_t sh[8]; } Q128;
static inline Q128 QG(int r) { Q128 q; q.d[0] = (uint64_t)R[r]; q.d[1] = (uint64_t)RH[r]; return q; }
static inline void QS(int r, Q128 q) { if (r) { R[r] = (int64_t)q.d[0]; RH[r] = (int64_t)q.d[1]; } }

static inline void M_PLZCW(int rd, int rs, int rt)   /* per word of the low 64 bits: leading bits equal to the sign bit, minus one */
{ uint64_t s = (uint64_t)R[rs], o = 0; int i; (void)rt;
  for (i = 0; i < 2; i++) { uint32_t w = (uint32_t)(s >> (32 * i)); if (w & 0x80000000u) w = ~w; uint32_t n = w ? (uint32_t)__builtin_clz(w) - 1 : 31; o |= (uint64_t)n << (32 * i); }
  if (rd) R[rd] = (int64_t)o; }
static inline void M_PSUBW(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 4; i++) o.w[i] = a.w[i] - b.w[i]; QS(rd, o); }
static inline void M_PADDH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.h[i] = (uint16_t)(a.h[i] + b.h[i]); QS(rd, o); }
static inline void M_PSUBB(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 16; i++) o.b[i] = (uint8_t)(a.b[i] - b.b[i]); QS(rd, o); }
static inline void M_PEXTLW(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; o.w[0] = b.w[0]; o.w[1] = a.w[0]; o.w[2] = b.w[1]; o.w[3] = a.w[1]; QS(rd, o); }
static inline void M_PEXTUW(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; o.w[0] = b.w[2]; o.w[1] = a.w[2]; o.w[2] = b.w[3]; o.w[3] = a.w[3]; QS(rd, o); }
static inline void M_PEXTLH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 4; i++) { o.h[2 * i] = b.h[i]; o.h[2 * i + 1] = a.h[i]; } QS(rd, o); }
static inline void M_PEXTUH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 4; i++) { o.h[2 * i] = b.h[4 + i]; o.h[2 * i + 1] = a.h[4 + i]; } QS(rd, o); }
static inline void M_PEXTLB(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) { o.b[2 * i] = b.b[i]; o.b[2 * i + 1] = a.b[i]; } QS(rd, o); }
static inline void M_PEXTUB(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) { o.b[2 * i] = b.b[8 + i]; o.b[2 * i + 1] = a.b[8 + i]; } QS(rd, o); }
static inline void M_PPACW(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; o.w[0] = b.w[0]; o.w[1] = b.w[2]; o.w[2] = a.w[0]; o.w[3] = a.w[2]; QS(rd, o); }
static inline void M_PPACH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 4; i++) { o.h[i] = b.h[2 * i]; o.h[4 + i] = a.h[2 * i]; } QS(rd, o); }
static inline void M_PPACB(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) { o.b[i] = b.b[2 * i]; o.b[8 + i] = a.b[2 * i]; } QS(rd, o); }
static inline void M_PXOR(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; o.d[0] = a.d[0] ^ b.d[0]; o.d[1] = a.d[1] ^ b.d[1]; QS(rd, o); }
static inline void M_PROT3W(int rd, int rs, int rt) { Q128 b = QG(rt), o; (void)rs; o.w[0] = b.w[1]; o.w[1] = b.w[2]; o.w[2] = b.w[0]; o.w[3] = b.w[3]; QS(rd, o); }
static inline void M_PCPYH(int rd, int rs, int rt) { Q128 b = QG(rt), o; int i; (void)rs; for (i = 0; i < 4; i++) { o.h[i] = b.h[0]; o.h[4 + i] = b.h[4]; } QS(rd, o); }
static inline void M_PEXEH(int rd, int rs, int rt)
{ Q128 b = QG(rt), o; (void)rs; o.h[0] = b.h[2]; o.h[1] = b.h[1]; o.h[2] = b.h[0]; o.h[3] = b.h[3]; o.h[4] = b.h[6]; o.h[5] = b.h[5]; o.h[6] = b.h[4]; o.h[7] = b.h[7]; QS(rd, o); }
/* shifts by the instruction's sa field: (rd, rt, sa) */
static inline void M_PSLLH(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.h[i] = (uint16_t)(b.h[i] << (sa & 15)); QS(rd, o); }
static inline void M_PSRLH(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.h[i] = (uint16_t)(b.h[i] >> (sa & 15)); QS(rd, o); }
static inline void M_PSRAH(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.h[i] = (uint16_t)(b.sh[i] >> (sa & 15)); QS(rd, o); }
static inline void M_PSLLW(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 4; i++) o.w[i] = b.w[i] << (sa & 31); QS(rd, o); }
static inline void M_PSRLW(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 4; i++) o.w[i] = b.w[i] >> (sa & 31); QS(rd, o); }
static inline void M_PSRAW(int rd, int rt, int sa) { Q128 b = QG(rt), o; int i; for (i = 0; i < 4; i++) o.w[i] = (uint32_t)(b.sw[i] >> (sa & 31)); QS(rd, o); }
static inline int16_t SAT16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }
static inline void M_PADDSH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.sh[i] = SAT16((int32_t)a.sh[i] + b.sh[i]); QS(rd, o); }
static inline void M_PADDUH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) { uint32_t v = (uint32_t)a.h[i] + b.h[i]; o.h[i] = v > 0xffff ? 0xffff : (uint16_t)v; } QS(rd, o); }
static inline void M_PMINH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.sh[i] = a.sh[i] < b.sh[i] ? a.sh[i] : b.sh[i]; QS(rd, o); }
/* PMULTH: eight 16x16 -> 32 products p0..p7; LO = {p0, p1, p4, p5}, HI = {p2, p3, p6, p7} (LO1 / HI1 = upper 64 bits), rd = {p0, p2, p4, p6} */
static inline void M_PMULTH(int rd, int rs, int rt)
{ Q128 a = QG(rs), b = QG(rt), o; uint32_t p[8]; int i; for (i = 0; i < 8; i++) p[i] = (uint32_t)((int32_t)a.sh[i] * b.sh[i]);
  LO = (int64_t)(((uint64_t)p[1] << 32) | p[0]); HI = (int64_t)(((uint64_t)p[3] << 32) | p[2]);
  LO1 = (int64_t)(((uint64_t)p[5] << 32) | p[4]); HI1 = (int64_t)(((uint64_t)p[7] << 32) | p[6]);
  o.w[0] = p[0]; o.w[1] = p[2]; o.w[2] = p[4]; o.w[3] = p[6]; QS(rd, o); }
static inline void M_PADDUB(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 16; i++) { uint32_t v = (uint32_t)a.b[i] + b.b[i]; o.b[i] = v > 0xff ? 0xff : (uint8_t)v; } QS(rd, o); }
/* PDIVBW: the four words of rs divided by the halfword rt.h0; LO = quotients, HI = remainders (LO1 / HI1 = words 2, 3); rd not written.
   Divide by zero and 0x80000000 / -1 as PCSX2 does (not yet checked against the hardware) */
static inline void M_PDIVBW(int rd, int rs, int rt)
{ Q128 a = QG(rs), b = QG(rt); uint32_t q[4], r[4]; int i; int32_t d = b.sh[0]; (void)rd;
  for (i = 0; i < 4; i++) { int32_t n = (int32_t)a.w[i];
    if (n == (int32_t)0x80000000 && d == -1) { q[i] = 0x80000000u; r[i] = 0; }
    else if (d) { q[i] = (uint32_t)(n / d); r[i] = (uint32_t)(n % d); }
    else { q[i] = n < 0 ? 1u : 0xffffffffu; r[i] = (uint32_t)n; } }
  LO = (int64_t)(((uint64_t)q[1] << 32) | q[0]); HI = (int64_t)(((uint64_t)r[1] << 32) | r[0]);
  LO1 = (int64_t)(((uint64_t)q[3] << 32) | q[2]); HI1 = (int64_t)(((uint64_t)r[3] << 32) | r[2]); }
/* PMFHI / PMFLO: rd = the 128-bit HI (HI1:HI) / LO (LO1:LO) */
static inline void M_PMFHI(int rd, int rs, int rt) { Q128 o; (void)rs; (void)rt; o.d[0] = (uint64_t)HI; o.d[1] = (uint64_t)HI1; QS(rd, o); }
static inline void M_PMFLO(int rd, int rs, int rt) { Q128 o; (void)rs; (void)rt; o.d[0] = (uint64_t)LO; o.d[1] = (uint64_t)LO1; QS(rd, o); }
GSVS uint32_t SAR;   /* the EE shift amount register (mtsab / mtsah / mtsa), in bits, for QFSRV */
static inline void M_PMAXH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.sh[i] = a.sh[i] > b.sh[i] ? a.sh[i] : b.sh[i]; QS(rd, o); }
/* QFSRV: rd = low 128 bits of the 256-bit value rs:rt (rs high, rt low) shifted right by SAR bits (0..127) */
static inline void M_QFSRV(int rd, int rs, int rt)
{ Q128 a = QG(rs), b = QG(rt), o; uint64_t x[4] = { b.d[0], b.d[1], a.d[0], a.d[1] }; unsigned sh = SAR & 127, q = sh >> 6, r = sh & 63;
  o.d[0] = r ? (x[q] >> r) | (x[q + 1] << (64 - r)) : x[q];
  o.d[1] = r ? (x[q + 1] >> r) | (x[q + 2] << (64 - r)) : x[q + 1];
  QS(rd, o); }
/* PMFHL.LW: rd = {LO.w0, HI.w0, LO.w2, HI.w2} */
static inline void M_PMFHL_LW(int rd, int rs, int rt)
{ Q128 o; (void)rs; (void)rt; o.w[0] = (uint32_t)LO; o.w[1] = (uint32_t)HI; o.w[2] = (uint32_t)LO1; o.w[3] = (uint32_t)HI1; QS(rd, o); }
/* PMFHL.UW: rd = {LO.w1, HI.w1, LO.w3, HI.w3} */
static inline void M_PMFHL_UW(int rd, int rs, int rt)
{ Q128 o; (void)rs; (void)rt; o.w[0] = (uint32_t)((uint64_t)LO >> 32); o.w[1] = (uint32_t)((uint64_t)HI >> 32); o.w[2] = (uint32_t)((uint64_t)LO1 >> 32); o.w[3] = (uint32_t)((uint64_t)HI1 >> 32); QS(rd, o); }
static inline void M_PCGTH(int rd, int rs, int rt) { Q128 a = QG(rs), b = QG(rt), o; int i; for (i = 0; i < 8; i++) o.h[i] = a.sh[i] > b.sh[i] ? 0xffff : 0; QS(rd, o); }
#endif
