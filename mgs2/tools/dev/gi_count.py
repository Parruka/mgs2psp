"""gi_count.py <in header> <out header>: dev copy of a generated header that counts guest instructions (boot.c GEN_GI_COUNT, MGS2_GI).
Each guest instruction line gets GI_(pc, k): k = 2 on lines holding a control transfer (its delay slot shares the line), else 1, plus the
instructions of functions inlined into the line (their own markers inside it; counted at the caller's pc, as if straight-line).  The call of
P11da90 (the draw pipeline, dispatch.h) is wrapped so its instructions count apart.  Build the unit with -DGEN_GI_COUNT -include gi.h."""
import re, sys
src = open(sys.argv[1]).read()
MARK = re.compile(r'^/\*([0-9a-f]{6})\*/', re.M)
INNER = re.compile(r'/\*[0-9a-f]{6}\*/')
XFER = re.compile(r'\bgoto\b|\breturn\b|\bJALR\(|\bJR\(|\b(?:S_\w+_)?P[0-9a-f]{5,6}\(\)')
out = []
for ln in src.split('\n'):
    m = MARK.match(ln)
    if m:
        k = (2 if XFER.search(ln) else 1) + len(INNER.findall(ln, m.end()))
        ln = ln[:m.end()] + ' GI_(0x%s, %d);' % (m.group(1), k) + ln[m.end():]
    out.append(ln)
s = '\n'.join(out)
d = 'static void P11da90(void)\n{'
if s.count(d) == 1:
    s = s.replace(d, 'static void P11da90_gi_real(void);\nstatic void P11da90(void) { g_gi_dg++; P11da90_gi_real(); g_gi_dg--; }\n'
                     'static void P11da90_gi_real(void)\n{')
    sys.stderr.write('P11da90 wrapped\n')
open(sys.argv[2], 'w').write(s)
