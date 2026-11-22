# xme_rest_chk.py <xme_rest.o> [me_rest.S] (gl28, 2026-11-19): psp/xme_rest.S, the picture loop's ME rest (MGS2_ME_REST), must be
# v30me5's psp/me_rest.S (mgs2/psp/me_rest.S: 76,548 rests and 0 lost wakes on Ffm's PSP in v30me5 run 2) with its symbols renamed
# me_rest* -> xme_rest* and nothing else.  The reference is assembled here as v30me5.sh did (psp-gcc -c) into a temporary folder; both
# objects are disassembled with every word shown (psp-objdump -d -r -z) and compared instruction by instruction: the same count, the
# same mnemonic, the same operands and the same relocations (type and symbol), with the reference's names renamed.  Raw words are
# compared too and only reported (a relocated word may differ).  Also: .text alignment 2**2 and size equal, no data or bss bytes, the
# same global symbols at the same offsets, and a list of the special instructions (CP0 moves, eret, HALT, stores).  Each disassembly
# must give one instruction per 4 bytes of .text and at least one, and the globals must not be empty (an empty read never passes).  Prints
# "xme_rest_chk: PASS" (exit 0) or "xme_rest_chk: FAIL (n)".  Run it with python3 -I.
import sys, os, re, subprocess, tempfile
BIN = '/home/claude/pspsdk/pspdev/bin/'
OBJ = sys.argv[1]
REF = sys.argv[2] if len(sys.argv) > 2 else '/mnt/project-files/mgs2/psp/me_rest.S'
bad = []

def run(*a):
    return subprocess.run(list(a), capture_output=True, text=True, check=True).stdout

def rename(s):
    return re.sub(r'\bme_rest', 'xme_rest', s)

def insns(o, ren):
    """[(offset, word, mnemonic, operands, [relocations])] of .text, names renamed when ren"""
    out = []
    for l in run(BIN + 'psp-objdump', '-d', '-r', '-z', '-j', '.text', o).splitlines():
        m = re.match(r'^\s+([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)\s*(.*)$', l)
        if m:
            ops = m.group(4).split('#')[0].strip()
            out.append([int(m.group(1), 16), m.group(2), m.group(3), rename(ops) if ren else ops, []]); continue
        m = re.match(r'^\s+([0-9a-f]+):\s+(R_\S+)\s+(\S+)', l)
        if m and out:
            out[-1][4].append((m.group(2), rename(m.group(3)) if ren else m.group(3)))
    return out

def sects(o):
    s = {}
    for l in run(BIN + 'psp-objdump', '-h', o).splitlines():
        m = re.match(r'\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S+)', l)
        if m: s[m.group(1)] = (int(m.group(2), 16), m.group(3))
    return s

def globs(o, ren):
    g = {}
    for l in run(BIN + 'psp-nm', '-g', o).splitlines():
        p = l.split()
        if len(p) == 3: g[rename(p[2]) if ren else p[2]] = (int(p[0], 16), p[1])
    return g

with tempfile.TemporaryDirectory() as td:
    ro = os.path.join(td, 'me_rest_ref.o')
    subprocess.run([BIN + 'psp-gcc', '-c', REF, '-o', ro], check=True)
    a = insns(OBJ, False); b = insns(ro, True)
    sa = sects(OBJ); sb = sects(ro); ga = globs(OBJ, False); gb = globs(ro, True)

if len(a) != len(b):
    bad.append('%d instructions, the reference %d' % (len(a), len(b)))
for nm, ins, se in (('the object', a, sa), ('the reference', b, sb)):   # an empty or short disassembly never passes
    if not ins or len(ins) * 4 != se.get('.text', (0,))[0]:
        bad.append('%s: %d instructions read for .text of %d bytes (want one per 4 bytes, at least one)' % (nm, len(ins), se.get('.text', (0,))[0]))
if not ga:
    bad.append('no global symbols read')
words_diff = 0
for x, y in zip(a, b):
    if (x[0], x[2], x[3], x[4]) != (y[0], y[2], y[3], y[4]):
        bad.append('at +%#x: %s %s %s, the reference %s %s %s' % (x[0], x[2], x[3], x[4], y[2], y[3], y[4]))
    if x[1] != y[1]: words_diff += 1
for n in ('.text', '.data', '.bss'):
    if sa.get(n, (0,))[0] != sb.get(n, (0,))[0]:
        bad.append('%s %s bytes, the reference %s' % (n, sa.get(n, (0,))[0], sb.get(n, (0,))[0]))
if sa.get('.data', (0,))[0] or sa.get('.bss', (0,))[0]:
    bad.append('data or bss bytes in the object')
al = run(BIN + 'psp-objdump', '-h', OBJ)
m = re.search(r'\.text\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)', al)
if not m or m.group(1) != '2**2':
    bad.append('.text alignment %s (2**2 expected, as v30me5)' % (m.group(1) if m else '?'))
if ga != gb:
    bad.append('global symbols differ: %s, the reference renamed %s' % (sorted(ga.items()), sorted(gb.items())))
und = run(BIN + 'psp-nm', '-u', OBJ).split()
if [u for u in und if u != 'U']:
    bad.append('undefined symbols: %s' % ' '.join(u for u in und if u != 'U'))
sp = {}
for x in a:
    mn = x[2]
    if mn in ('mtc0', 'mfc0'): k = '%s %s' % (mn, x[3].split(',')[-1].strip())
    elif mn in ('eret', 'sw', 'syscall', 'break', 'jal', 'jalr'): k = mn
    elif mn == '.word': k = '.word ' + x[1]
    else: continue
    sp[k] = sp.get(k, 0) + 1
for k in sp:
    if k.startswith('mtc0') and k.split()[1] not in ('$12', '$14', '$25'): bad.append('a CP0 write other than $12, $14, $25: ' + k)
    if k in ('syscall', 'break', 'jal', 'jalr'): bad.append('forbidden: ' + k)
    if k.startswith('.word') and k != '.word 70000000': bad.append('a data word in the code: ' + k)
print('xme_rest_chk: %s: %d instructions (.text %d bytes), the reference %s renamed: %d; words differing %d; globals %s; special: %s'
      % (OBJ, len(a), sa.get('.text', (0,))[0], REF, len(b), words_diff, ' '.join(sorted(ga)), ', '.join('%s %d' % (k, sp[k]) for k in sorted(sp))))
for x in bad:
    print('PROBLEM ' + x)
print('xme_rest_chk: ' + ('PASS' if not bad else 'FAIL (%d)' % len(bad)))
sys.exit(1 if bad else 0)
