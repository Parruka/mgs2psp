#!/usr/bin/env python3
"""jalr_tab.py <gen_elf.h>: header post-pass, in place.  Appends gen_ftab: the cases of gen_call's switch that call one translated function
(`case 0xA: PA(); return 1;`), sorted by address, for boot/dispatch.h JALR's target cache: a call through a register to such a target
calls PA directly (the same call the switch makes) after one cache probe, instead of the switch's search.  Cases with an inlined body
stay with the switch.  Data only, after the last function, so the PGO profile's function numbers do not move.  Split builds only
(gen_pre.h GUEST_SPLIT_GEN).  Idempotent: an existing table is replaced in place.  Checks that every case lies below the stage image
(0x1f1c00): with OPT_STAGE_TAB, dispatch.h guest_call sends a stage address past gen_call."""
import re, sys
MARK = '/* tools/jalr_tab.py */'

def main():
    path = sys.argv[1]
    s = open(path).read()
    rest = ''
    if MARK in s:                       # (only the table goes: what later passes appended after it stays, e.g. native_elf_apply.py's include)
        i = s.index(MARK); j = s.index('#endif\n', i) + len('#endif\n')
        rest = s[j:]; s = s[:i].rstrip('\n') + '\n'
    m = re.search(r'^static int gen_call\(uint32_t a\).*?^}\n', s, re.M | re.S)
    if not m: sys.exit('jalr_tab: no gen_call in ' + path)
    body = m.group(0)
    plain = sorted((int(a, 16), a) for a, b in re.findall(r'^  case 0x([0-9a-f]+): P([0-9a-f]+)\(\); return 1;$', body, re.M) if a == b)
    ncase = len(re.findall(r'^  case 0x', body, re.M))
    top = max(int(a, 16) for a in re.findall(r'^  case 0x([0-9a-f]+):', body, re.M))
    if top >= 0x1f1c00: sys.exit('jalr_tab: gen_call has a case at %x, in the stage image: boot/dispatch.h guest_call may not skip it' % top)
    out = [MARK, '#ifdef GUEST_SPLIT_GEN   /* gen_call\'s %d plain cases of %d (boot/dispatch.h JALR) */' % (len(plain), ncase),
           'const struct GenFn gen_ftab[] = {']
    for i in range(0, len(plain), 4):
        out.append('  ' + ' '.join('{0x%s, P%s},' % (h, h) for _, h in plain[i:i + 4]))
    out += ['};', 'const int gen_ftab_n = %d;' % len(plain), '#endif', '']
    open(path, 'w').write(s + '\n'.join(out) + rest)
    print('%s: %d of %d gen_call cases in gen_ftab' % (path, len(plain), ncase))

if __name__ == '__main__': main()
