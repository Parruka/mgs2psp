#!/usr/bin/env python3
"""pxcmp.py (python3 -I): the experimental relink px (the final build + XL_PAD 0x210 in front of ge_xlate.o's tables; model/hka39, hkf39,
hdn39) against the final build's own windows of the same turns (model/ka39, kf39, dn39): whole windows, ms a PSP step [C model]."""
R = '/home/claude/main/build_gl39/model/'
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
def head(p):
    f = open(p).readline().split(); k = dict(zip(f[1::2], f[2::2])); return [int(k['total']) / 80.0, int(k['imiss']) / 80.0, int(k['dmiss']) / 80.0]
for a, b, n in (('hka39', 'ka39', '29,510 K0 digests on'), ('hka39.1', 'ka39.1', '29,610 K0 digest-off'), ('hka39.2', 'ka39.2', '29,710 K7'), ('hka39.3', 'ka39.4', '30,210 K0 digest-off'),
                ('hka39.4', 'ka39.6', '30,910 walk K0 digest-off'), ('hkf39', 'kf39', '29,610 K0 digest-off, F1 off'), ('hkf39.1', 'kf39.2', '30,710 K7 (t67)'),
                ('hdn39', 'dn39', '29,810 guard cfg, ME turn'), ('hdn39.1', 'dn39.1', '29,910 guard cfg, main-drawing turn')):
    x, y = head(R + a), head(R + b)
    print('%-36s final %7.2f (instr %9.0f I %6.0f D %6.0f) | px - final %+5.2f (instr %+7.0f I %+5.0f D %+6.0f)' % (n, ms(y), y[0], y[1], y[2], ms(x) - ms(y), x[0] - y[0], x[1] - y[1], x[2] - y[2]))
