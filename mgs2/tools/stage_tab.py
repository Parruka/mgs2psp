#!/usr/bin/env python3
"""stage_tab.py <gen_s_<stage>.h> [out]: header post-pass (in place without out).  S_<stage>_call, the stage's dispatch of calls through a
register, is a switch over every translated function's address (all plain cases: `case 0xA: S_<stage>_PA(); return 1;`).  Compiled with a
PGO profile, GCC orders its compares by the cases' measured probabilities, and the cases the training run never saw (probability 0) come
out as one long chain of compares: in free play on the deck, w00a's calls walked up to about 980 instructions over 18 KB of code each
(speed_notes §43).  This pass replaces the switch by a sorted table of the same cases (S_<stage>_ftab) looked up through a small
direct-mapped cache (S_<stage>_jc, 256 entries): the same function is called for every address the switch had, and 0 is returned for any
other, as the switch's default did.  The cache holds only this stage's own entries: dispatch.h stage_call checks the stage image's
signature before it calls here, and on the PSP an overlay's cache is in its bss, zeroed whenever the overlay loads.  Table and cache are
data at file scope without `static` (gen_stage.c defines it away), placed just before the function, so every function keeps its place
(PGO numbering); only S_<stage>_call's own profile no longer matches.  (v26; the PSP harness's OPT_STAGE_TAB bit covers the rest of the
stage call path, runtime/host_time.h.)"""
import re, sys
MARK = '/* tools/stage_tab.py */'

def main():
    src = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else src
    s = open(src).read()
    if MARK in s: sys.exit('stage_tab: %s already has the table' % src)
    m = re.search(r'^static int S_(\w+)_call\(uint32_t a\)(.*)\n\{\n  switch \(a\) \{\n(.*?)\n  \}\n  return 0;\n\}\n', s, re.M | re.S)
    if not m: sys.exit('stage_tab: no S_<stage>_call switch in ' + src)
    st, comment, body = m.group(1), m.group(2), m.group(3)
    tab = []
    for c in body.split('\n'):
        k = re.match(r'^  case 0x([0-9a-f]+): S_%s_P([0-9a-f]+)\(\); return 1;$' % st, c)
        if not k or int(k.group(1), 16) != int(k.group(2), 16): sys.exit('stage_tab: not a plain case in S_%s_call: %s' % (st, c[:120]))
        tab.append((int(k.group(1), 16), k.group(2)))
    a = [t[0] for t in tab]
    if a != sorted(set(a)): sys.exit('stage_tab: S_%s_call cases not sorted and unique' % st)
    n = len(tab)
    o = [MARK + '   /* S_%s_call\'s %d cases as a sorted table, looked up through a direct-mapped cache */' % (st, n),
         'const struct GenFn S_%s_ftab[%d] = {' % (st, n)]
    for i in range(0, n, 4):
        o.append('  ' + ' '.join('{0x%xu, S_%s_P%s},' % (x, st, h) for x, h in tab[i:i + 4]))
    o += ['};', 'struct GenFn S_%s_jc[256];' % st,
          'static int S_%s_call(uint32_t a)%s' % (st, comment), '{',
          '  struct GenFn *e = &S_%s_jc[((a >> 3) ^ (a >> 11)) & 255];' % st,
          '  if (e->a != a || !e->f) {',
          '    int lo = 0, hi = %d; void (*f)(void) = 0;' % (n - 1),
          '    while (lo <= hi) { int k = (lo + hi) >> 1; uint32_t x = S_%s_ftab[k].a; if (x == a) { f = S_%s_ftab[k].f; break; } if (x < a) lo = k + 1; else hi = k - 1; }' % (st, st),
          '    if (!f) return 0;',
          '    e->a = a; e->f = f;',
          '  }',
          '  e->f(); return 1;',
          '}', '']
    s = s[:m.start()] + '\n'.join(o) + s[m.end():]
    open(out, 'w').write(s)
    print('%s: S_%s_call, %d cases in S_%s_ftab' % (out, st, n, st))

if __name__ == '__main__': main()
