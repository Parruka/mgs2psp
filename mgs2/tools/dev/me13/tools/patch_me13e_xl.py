# patch_me13e_xl.py <xlate.c>: me13 e (task #147, the take-back's x86 check): sb14's ge/xlate.c + xl_keep_save / xl_keep_load, the
# translator's state that lasts from frame to frame saved whole and put back after xl_reset: the GS registers (gr), the GIF paths (G), the
# VIF state (V), VU1's data memory as the converters read it (VM, every pending UNPACK decoded first) and the sources of its micro program
# (VPsrc), and the GS front end (F: its registers and vertex queue; its callbacks and map stay those of the moment).  The caches the
# translator rebuilds by itself (the rule cache EC, the descriptors, the generations) start afresh.  Built into me13/xe only.
import sys
p = sys.argv[1]
s = open(p).read()
add = '''
/* me13 e (task #147): the take-back's x86 check, the translator's lasting state saved and put back (tools/patch_me13e_xl.py) */
static struct { uint64_t gr[256]; Gif G[4]; __typeof__(V) V; uint8_t VM[sizeof VM]; uint32_t VPsrc[2048]; GfGs F; } KS;
void xl_keep_save(void)
{
  vm_all(); memcpy(KS.gr, gr, sizeof gr); memcpy(KS.G, G, sizeof G); KS.V = V; memcpy(KS.VM, VM, sizeof VM); memcpy(KS.VPsrc, VPsrc, sizeof VPsrc); KS.F = F;
}
void xl_keep_load(void)
{
  GfPrimFn fn = F.fn; void *ctx = F.ctx; struct GuMap *up = F.up;
  memcpy(gr, KS.gr, sizeof gr); memcpy(G, KS.G, sizeof G); V = KS.V; memcpy(VM, KS.VM, sizeof VM); memcpy(VPsrc, KS.VPsrc, sizeof VPsrc);
  F = KS.F; F.fn = fn; F.ctx = ctx; F.up = up; F.dirty = 1;
}
'''
if 'xl_keep_save' in s: sys.exit('patch: already patched')
if s.count('const XlStats *xl_stats(void) { return &St; }\n') != 1: sys.exit('patch: anchor not found')
s = s.replace('const XlStats *xl_stats(void) { return &St; }\n', 'const XlStats *xl_stats(void) { return &St; }\n' + add)
open(p, 'w').write(s)
print('patched')
