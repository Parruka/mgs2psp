# ed_rh.py: sbg0 (task #165) the read-hash proof of G0H in a GEN_DG_TRACE build: boot.c marks, native_w00a.c/.h hooks
import hashlib, sys
T = '/home/claude/sbg0/'
def edit(fn, md5, reps):
    s = open(T + fn).read()
    assert hashlib.md5(s.encode()).hexdigest() == md5, fn
    for a, b in reps:
        assert s.count(a) == 1, (fn, a[:60])
        s = s.replace(a, b)
    open(T + fn, 'w').write(s)
    print(fn, hashlib.md5(s.encode()).hexdigest())

edit('boot/boot.c', 'e22b8045df2b3308066407f64192fd31', [
('static void dgt_wr(uint32_t a, int n, uintptr_t ra)\n',
 '''/* sbg0 (task #165, G0H): the hide bits line_sph's native G0 form sets on a prim (+164 |= 0x3000: the draw pipeline then skips it) are
   the one word G0 changes that game code outside the renderer loads: the act's next turn loads it and ORs the bits (hidden, 0x333400)
   or ANDs them off (visible, 0x333434) before any use.  rt_g0h_mark (nat/native_w00a.c) marks the word; any store to it by game code or a
   copy unmarks it; a load of a marked word by line_sph's act (MGS2_READ_G0=<lo>:<hi>, host offsets from rt_dgt_rd as MGS2_READ_SKIP) is
   hashed with those two bits clear, as the translation leaves them, and unmarks it; any other load of a marked word is hashed as it is
   and reported ("g0h:" lines, the first 64). */
static uint8_t *g0h_mk; static unsigned long g0h_fn[2]; static int g0h_on = -1; static long g0h_rep, g0h_n, g0h_set;
void rt_g0h_mark(uint32_t a) { if (!g0h_mk) g0h_mk = calloc(GUEST_SIZE / 4, 1); g0h_mk[(a & (GUEST_SIZE - 1)) >> 2] = 1; g0h_set++; }
static void dgt_wr(uint32_t a, int n, uintptr_t ra)
'''),
('  for (; w <= e && w < GUEST_SIZE / 4; w++) { dgt_w[w] = v; if (v) {',
 '  for (; w <= e && w < GUEST_SIZE / 4; w++) { if (g0h_mk) g0h_mk[w] = 0; dgt_w[w] = v; if (v) {'),
('    dgt_w[d] = v;\n',
 '    dgt_w[d] = v; if (g0h_mk) g0h_mk[d] = 0;\n'),
('  if (dgt_rl_on) {\n',
 '''  if (g0h_mk) {
    uint32_t m = a & (GUEST_SIZE - 1), w;
    for (w = m >> 2; w <= ((m + n - 1) & (GUEST_SIZE - 1)) >> 2; w++) if (g0h_mk[w]) {
      if (g0h_on < 0) { const char *e2 = getenv("MGS2_READ_G0"); g0h_on = e2 && sscanf(e2, "%lx:%lx", &g0h_fn[0], &g0h_fn[1]) == 2; }
      if (g0h_on && (unsigned long)(ra - b) >= g0h_fn[0] && (unsigned long)(ra - b) < g0h_fn[1]) {
        int j = (int)((w << 2) + 1 - m);   /* bits 12-13 of the word are 0x30 in its byte 1 */
        if (j >= 0 && j < n) v[j] &= (uint8_t)~0x30u;
        g0h_mk[w] = 0; g0h_n++;
      } else if (g0h_rep++ < 64) fprintf(stderr, "g0h: vb %ld word %07x loaded by r%lx while hidden\\n", k_vblanks, w << 2, (unsigned long)(ra - b));
    }
  }
  if (dgt_rl_on) {
'''),
])

edit('nat/native_w00a.h', '8f94ecc84f72ceaf4a3214eac3a1e762', [
('#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && !defined(GEN_DG_TRACE) && !defined(GEN_MM_CHECK)',
 '#if defined(GEN_MM_FAST) && defined(GUEST_SPR0) && (!defined(GEN_DG_TRACE) || defined(NAT_IN_TRACE)) && !defined(GEN_MM_CHECK)'),
])

edit('nat/native_w00a.c', 'd077b36665516435bcbf262dcfe1a854', [
('#define NAT_MASK(a, n) ((void)0)\n#endif\n',
 '''#define NAT_MASK(a, n) ((void)0)
#endif
#if defined(GEN_DG_TRACE) && defined(NAT_IN_TRACE)   /* sbg0 x86 dev (-DNAT_IN_TRACE): the read-hash proof of G0H (boot.c rt_g0h_mark) */
void rt_g0h_mark(uint32_t a);
#define NAT_G0H_MARK(a) rt_g0h_mark(a)
#else
#define NAT_G0H_MARK(a) ((void)0)
#endif
'''),
('    if (g0h) { *(uint32_t *)GEN_RAM(pw) |= 0x3000u; nat_ls_n[4]++; }',
 '    if (g0h) { *(uint32_t *)GEN_RAM(pw) |= 0x3000u; NAT_G0H_MARK(pw); nat_ls_n[4]++; }'),
])
