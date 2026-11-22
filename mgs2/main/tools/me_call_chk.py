# me_call_chk.py <build dir> [more roots] (gl21): the ME may never reach a system call.  gl20's ME died at its start on a call through a pointer
# to sceKernelGetSystemTimeLow (the main CPU's clock): on the ME an import stub's syscall is an exception, and the emulator, which runs
# the loop as a thread of the main CPU, cannot see it.  This check reads MGS2T.elf:
#  1. the roots: every function of the ME-safe objects (mesec_chk.py's list, by the ELF's FILE symbols) and the functions
#     psp/xl_psp.c gives the ME through the context's pointers that only the ME calls (clk_me, w_me, and the cache steps);
#  2. follows every jal, j and branch whose target is another function's start (calls and tail calls) from the roots;
#  3. FAIL if a reached function lies in .sceStub.text (an import stub), holds a syscall, or has a name starting "sce";
#     the functions reached outside the ME-safe objects are listed (libc's memcpy and the like are expected).
# Calls through other pointers are not followed (the ME's other pointers are set to ME-safe functions at the set-up and ran on the ME
# from gl18me to gl19); that is the check's known gap.  Prints "me_call_chk: PASS" or "me_call_chk: FAIL (n)"; exit 0 on PASS.
# gl28: psp/xme_rest.S (the ME's rest, MGS2_ME_REST) is ME code too.  An assembled object carries no FILE symbol, so its functions
# are also roots by name when the ELF has them (xme_rest, xme_rest_init, xme_rest_vec and the labels inside xme_rest); an ELF without
# them (gl27 and before) gets the same roots as before.
import sys, os, re, subprocess
B = sys.argv[1]
BIN = '/home/claude/pspsdk/pspdev/bin/'
ME = ['xlive', 'xlate', 'gsfront', 'texconv', 'texcache', 'gelive', 'gebatch', 'xconv', 'vu1conv', 'gelist', 'gsup2', 'xlme', 'xme', 'me_rt',
      'xme_me', 'xme_rest']   # gl28: xme_rest (by name below as well)
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'psp', 'xl_psp.c')
elf = os.path.join(B, 'MGS2T.elf')
dis = subprocess.run([BIN + 'psp-objdump', '-d', elf], capture_output=True, text=True, check=True).stdout
fn = {}; start = {}; sec = None; cur = None
for l in dis.splitlines():
    m = re.match(r'Disassembly of section (\S+):', l)
    if m: sec = m.group(1); continue
    m = re.match(r'([0-9a-f]{8}) <([^>]+)>:', l)
    if m:
        cur = m.group(2); a = int(m.group(1), 16); start[a] = cur
        fn.setdefault(cur, {'sec': sec, 'addr': a, 'to': set(), 'sys': 0}); continue
    if cur is None: continue
    m = re.match(r'\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(\S+)\s*(.*)', l)
    if not m: continue
    op, args = m.group(2), m.group(3)
    if op == 'syscall': fn[cur]['sys'] += 1
    t = re.search(r'\b([0-9a-f]{7,8}) <', args)
    if t and (op in ('jal', 'j') or op.startswith('b')): fn[cur]['to'].add(int(t.group(1), 16))
# the FILE of each function symbol
files = {}; curf = ''
for l in subprocess.run([BIN + 'psp-readelf', '-sW', elf], capture_output=True, text=True, check=True).stdout.splitlines():
    p = l.split()
    if len(p) < 8 or not p[0].endswith(':'): continue
    if p[3] == 'FILE': curf = p[7]; continue
    if p[3] == 'FUNC': files.setdefault(p[7], curf)
def me_obj(name):
    f = os.path.splitext(os.path.basename(files.get(name, '')))[0]
    return f in ME or f.startswith('ge_') and f[3:] in ME
roots = {n for n in fn if me_obj(n)} | set(sys.argv[2:])   # (more roots by name: the check's own test)
roots |= {n for n in fn if re.match(r'xme_rest(_init|_vec|_begin|_halt|_end|_win_end|_win_last)?$', n)}   # gl28
src = open(SRC).read()
for field in ('clk_me', 'w_me'):
    for m in re.finditer(r'->' + field + r'\s*=\s*(\w+)', src): roots.add(m.group(1))
for m in re.finditer(r'->cache\s*=\s*[^;]*?\b(xme_me_\w+)', src): roots.add(m.group(1))
missing = sorted(r for r in roots if r not in fn)
bad = ['root %s not in the ELF' % r for r in missing if r in ('xme_me_clk', 'xme_me_wait', 'xme_me_cache', 'xme_me_main')]
seen = set(); todo = [r for r in roots if r in fn]
while todo:
    n = todo.pop()
    if n in seen: continue
    seen.add(n)
    for a in fn[n]['to']:
        t = start.get(a)
        if t and t != n and t not in seen: todo.append(t)
outside = []
for n in sorted(seen):
    f = fn[n]
    if f['sec'] == '.sceStub.text' or n.startswith('sce') or f['sys']:
        bad.append('%s (%s, %08x, syscalls %d) is reached' % (n, f['sec'], f['addr'], f['sys']))
    elif n not in roots:
        outside.append(n)
print('me_call_chk: roots %d (ME-safe objects %d), reached %d, reached outside the ME-safe objects %d: %s'
      % (len(roots), sum(1 for r in roots if me_obj(r)), len(seen), len(outside), ' '.join(outside[:40]) + (' ...' if len(outside) > 40 else '')))
xr = sorted(n for n in seen if n.startswith('xme_rest'))   # gl28
if xr: print('me_call_chk: the rest (psp/xme_rest.S) reached: %s' % ' '.join(xr))
for b in bad: print('me_call_chk: ' + b)
print('me_call_chk: PASS' if not bad else 'me_call_chk: FAIL (%d)' % len(bad))
sys.exit(0 if not bad else 1)
