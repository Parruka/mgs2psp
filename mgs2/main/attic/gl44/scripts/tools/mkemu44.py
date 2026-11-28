"""mkemu44.py (gl44): the emulator test variants of cfg/mgs2_gl44.cfg in cfg_emu/, as build_gl42/cfg_emu (each change marked 'gl44 test key'):
  mgs2_gl44_xpchk.cfg  + MGS2_XP_CHECK=1 (the XP check in every plan)
  mgs2_gl44_h1.cfg     digests in every frame from 24,000 (MGS2_GL_DIG=24000:31400, MGS2_DIG_CYCLE all 0), the package's turns
  mgs2_gl44_h0.cfg     h1 with nothing native (MGS2_NGE_CYCLE all 0) and option B and V44 off in every turn (MGS2_OPT2_CYCLE all ff9ff9ff):
                       gl38's state hash (hashref_gl38), the heap gate's pair"""
import re
R = '/home/claude/main/build_gl44/'
src = open(R + 'cfg/mgs2_gl44.cfg').read().split('\n')
def sub(lines, key, val, note='was'):
    out = []; n = 0
    for l in lines:
        if l.startswith(key + '='):
            out.append('# gl44 test key (%s: %s)' % (note, l)); out.append(key + '=' + val); n += 1
        else: out.append(l)
    assert n == 1, (key, n); return out
def val(lines, key): return [l for l in lines if l.startswith(key + '=')][0].split('=', 1)[1]
x = list(src); i = max(k for k, l in enumerate(x) if l.strip()); x[i + 1:i + 1] = ['# gl44 test key', 'MGS2_XP_CHECK=1']
open(R + 'cfg_emu/mgs2_gl44_xpchk.cfg', 'w').write('\n'.join(x))
h1 = sub(src, 'MGS2_GL_DIG', '24000:31400'); h1 = sub(h1, 'MGS2_DIG_CYCLE', '0' * len(val(src, 'MGS2_DIG_CYCLE')))
open(R + 'cfg_emu/mgs2_gl44_h1.cfg', 'w').write('\n'.join(h1))
h0 = sub(h1, 'MGS2_NGE_CYCLE', '0' * len(val(src, 'MGS2_NGE_CYCLE')), 'the delivery file has')
h0 = sub(h0, 'MGS2_OPT2_CYCLE', ','.join(['ff9ff9ff'] * len(val(src, 'MGS2_OPT2_CYCLE').split(','))), 'the delivery file has')
open(R + 'cfg_emu/mgs2_gl44_h0.cfg', 'w').write('\n'.join(h0))
