"""Translate stage code images for the boot probe and write stages.h: each distinct image as its own unit of functions (survey.py
SPLIT; identical images, like the 2 KB r_* ones, share one), plus a table the host uses to tell which image is in memory and to
call into it.  The signature is the header's end / bss words, the first exported (hash, address) pairs at +0x14, and one code word.
With STAGES_EXTERN defined, stages.h only declares each image's entry points (split build: boot/gen_stage.c compiles each image alone).
Usage: SLUS=SLES_503.83 [REUSE=1] python3 mkstages.py <stage dir root> <out dir> <stage> [<stage> ...]"""
import os, re, sys, struct, subprocess, hashlib
here = os.path.dirname(os.path.abspath(__file__)); survey = os.path.join(here, '..', 'recomp', 'survey.py')
root, out, names = sys.argv[1], sys.argv[2], sys.argv[3:]
inc, rows, done = [], [], {}
for n in names:
    b = os.path.join(root, n, 'nocache', n + '.bin'); d = open(b, 'rb').read(); md = hashlib.md5(d).hexdigest()
    if md not in done:
        h = os.path.join(out, 'gen_s_%s.h' % n)
        if not (os.environ.get('REUSE') and os.path.exists(h)):          # REUSE=1: keep translations already written
            r = subprocess.run([sys.executable, survey, h], env=dict(os.environ, STAGE=b, SPLIT=n), capture_output=True, text=True)
            print(n, r.stdout.strip().split('\n')[0])
        inc.append('#ifndef STAGES_EXTERN\n#include "gen_s_%s.h"\n#else\nvoid S_%s_init(void); int S_%s_call(uint32_t);\n#endif\n' % (n, n, n))
        done[md] = n
    rep = done[md]
    # the code word: the first instruction of the first translated function at +0x8000 or later (a fixed +0x8000 hit data in w00a,
    # which the game writes after loading, so the image stopped matching)
    fns = sorted(int(x, 16) - 0x1f1c00 for x in re.findall(r'FN_ENTER\(0x([0-9a-f]+)\)', open(os.path.join(out, 'gen_s_%s.h' % rep)).read()))
    code = next((o for o in fns if 0x8000 <= o < len(d) - 3), next((o for o in fns if 0x30 <= o < len(d) - 3), min(0x8000, len(d) - 4)))
    offs = [4, 8] + list(range(0x14, 0x30, 4)) + [code]
    rows.append('  { "%s", { %s }, { %s }, S_%s_init, S_%s_call },\n' % (n, ', '.join('0x%x' % o for o in offs),
                ', '.join('0x%08xu' % struct.unpack_from('<I', d, o)[0] for o in offs), rep, rep))
open(os.path.join(out, 'stages.h'), 'w').write(''.join(inc) + '#define STAGE_SIG_N 10\n'
    'static const struct StageCode { const char *name; uint32_t off[STAGE_SIG_N], sig[STAGE_SIG_N]; void (*init)(void); int (*call)(uint32_t); } stage_code[] = {\n'
    + ''.join(rows) + '};\n')
