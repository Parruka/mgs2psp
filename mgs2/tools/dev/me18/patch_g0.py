#!/usr/bin/env python3
"""patch_g0.py <out dir>: task #180 step 4b-2 (mgs2/me_step4_notes.md): copies of the five generated headers that hold the 12 G0
functions (me_handoff_design.md 4.3: the game code that writes into bytes in flight in its own step), with each of those bodies opened by
`{ enum { gen_g0_fn = <n> };` in place of its `{` line, so that boot/gen_pre.h's guards (GEN_G0_PRE) call rt_g0_pre in those bodies
and compile away elsewhere.  No line is added or removed (the profile's line checksums stay as they were).  Each body is checked for
store forms the guards do not cover (inline functions defined before the bodies, whose expansions cannot see the enum): it stops if one
is used.  It prints each body's store forms and counts."""
import os, re, sys, hashlib

G0 = [  # (number, header, function): the census's 12 (me_handoff_design.md 2, me_step1_notes.md 3)
    (1, '/home/claude/v31/WPL/gen_s_d00t.h', 'S_d00t_P299fb8'),
    (2, '/home/claude/v31/WPL/gen_elf.h', 'P11de00'),
    (3, '/home/claude/v31/WPL/gen_s_d01t.h', 'S_d01t_P2a1200'),
    (4, '/home/claude/sbg0/wpl/gen_s_w00a.h', 'S_w00a_P31c0d0'),
    (5, '/home/claude/sbg0/wpl/gen_s_w00a.h', 'S_w00a_P3a4138'),
    (6, '/home/claude/sbg0/wpl/gen_s_w00a.h', 'S_w00a_P11e0a8'),
    (7, '/home/claude/v31/WPL/gen_elf.h', 'P16d1ac'),
    (8, '/home/claude/v31/WPL/gen_s_w01a.h', 'S_w01a_P30c8b8'),
    (9, '/home/claude/v31/WPL/gen_s_w01a.h', 'S_w01a_P322198'),
    (10, '/home/claude/v31/WPL/gen_s_w01a.h', 'S_w01a_P30d6f8'),
    (11, '/home/claude/v31/WPL/gen_s_w01a.h', 'S_w01a_P350f40'),
    (12, '/home/claude/v31/WPL/gen_s_w01a.h', 'S_w01a_P3926e0'),
]
GUARDED = ['WR8B', 'WR16B', 'WR32B', 'WR64B', 'WRFB', 'WR8S', 'WR16S', 'WR32S', 'WR64S', 'WRFS', 'WR8', 'WR16', 'WR32', 'WR64',
           'GUEST_STORE', 'GUEST_STORE16', 'GSDU', 'GSDUB', 'VSQC2B']
UNGUARDED = ['WRF', 'GSWL', 'GSWR', 'GSDL', 'GSDR', 'VSQC2', 'rt_store', 'rt_st8', 'rt_st16', 'rt_st32', 'rt_st64', 'rt_stf', 'memcpy',
             'memset', 'memmove', 'VU_CP16', 'SQ', 'SQC2', 'GSQ', 'WR128', 'MM', 'MMS', 'GEN_RAM', 'gen_hb', 'gen_mm']

def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    by_h = {}
    for n, h, f in G0:
        by_h.setdefault(h, []).append((n, f))
    for h, fs in by_h.items():
        lines = open(h, encoding='latin-1').read().split('\n')
        md5 = hashlib.md5(open(h, 'rb').read()).hexdigest()
        for n, f in fs:
            d = [i for i, l in enumerate(lines) if l == 'static void %s(void)' % f]
            if len(d) != 1:
                sys.exit('%s: %d definitions of %s' % (h, len(d), f))
            i = d[0]
            if lines[i + 1] != '{':
                sys.exit('%s: %s: the line after the definition is %r' % (h, f, lines[i + 1][:40]))
            e = i + 2
            while lines[e] != '}':
                e += 1
            body = '\n'.join(lines[i + 2:e])
            names = {}
            for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', body):
                names[m.group(1)] = names.get(m.group(1), 0) + 1
            host = 0   # memcpy / memset / memmove into the host's own state (the VU registers and their save area), not guest memory
            for m in re.finditer(r'\b(memcpy|memset|memmove)\(', body):
                j, dep = m.end(), 0
                while not (body[j] == ',' and dep == 0):
                    dep += body[j] in '([' ; dep -= body[j] in ')]'; j += 1
                dst = body[m.end():j].strip()
                if re.match(r'^(vu_sv\[\d+\]|VF\[\d+\]\.u|&?vu_[a-z_]+)$', dst):
                    host += 1; names[m.group(1)] -= 1
                    if not names[m.group(1)]: del names[m.group(1)]
                else:
                    sys.exit('%s: %s: %s into %r' % (h, f, m.group(1), dst))
            bad = sorted(k for k in names if k in UNGUARDED)
            if bad:
                sys.exit('%s: %s uses store forms the guards do not cover: %s' % (h, f, ', '.join('%s x%d' % (k, names[k]) for k in bad)))
            st = sorted((k, names[k]) for k in names if k in GUARDED)
            print('%2d %-16s %s lines %d-%d (%d): %s' % (n, f, os.path.basename(h), i + 1, e + 1, e - i + 1,
                                                       (' '.join('%s %d' % kv for kv in st) or 'no stores') +
                                                       (' (and %d copies into the host\'s VU state)' % host if host else '')))
            lines[i + 1] = '{ enum { gen_g0_fn = %d };   /* task #180 step 4b-2: a G0 function (tools/dev/me18/patch_g0.py) */' % n
        o = os.path.join(out, os.path.basename(h))
        open(o, 'w', encoding='latin-1').write('\n'.join(lines))
        print('%s (%s) -> %s (%s)' % (h, md5, o, hashlib.md5(open(o, 'rb').read()).hexdigest()))

main()
