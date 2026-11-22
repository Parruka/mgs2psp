#!/usr/bin/env python3
"""idiom_apply.py <gen header> <image> [...]: marks the functions of an existing generated header whose instruction words are a
recomp/idioms.py routine (the same "FN_ENTER(0x...); GEN_IDIOM_<NAME>();" line recomp.py now writes), in place.  <image>: the ELF
(SLES_503.83) for gen_elf.h, the stage code image (<stage>.bin, loaded at its first word) for gen_s_<stage>.h.  Idempotent."""
import os, re, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recomp'))
import idioms
hdr, img = sys.argv[1], sys.argv[2]
D = open(img, 'rb').read()
if D[:4] == b'\x7fELF': W = lambda a: struct.unpack_from('<I', D, a - 0x100000 + 0x80)[0]
else:
    base = struct.unpack_from('<I', D, 0)[0]
    W = lambda a: struct.unpack_from('<I', D, a - base)[0] if a >= base else (_ for _ in ()).throw(struct.error('below image'))
lines = open(hdr).read().split('\n'); n = 0; names = {}
for i, ln in enumerate(lines):
    m = re.match(r'FN_ENTER\(0x([0-9a-f]+)\);(.*)$', ln)
    if not m: continue
    idi = idioms.match(W, int(m.group(1), 16))
    if idi:
        lines[i] = 'FN_ENTER(0x%s); GEN_IDIOM_%s();' % (m.group(1), idi); n += 1; names[idi] = names.get(idi, 0) + 1
open(hdr, 'w').write('\n'.join(lines))
print('%s: %d functions marked %s' % (hdr, n, names))
