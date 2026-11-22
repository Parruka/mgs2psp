"""recomp.py with automatic jump tables: on 'jr non-ra at X' the switch before X (sltiu n / lui hi / addiu lo) becomes a jt=X:table:n argument.
usage: recomp_jt.py <out.h> NAME=0xa-0xb ... (same as recomp.py); env as recomp.py.  Prints the jt arguments it added."""
import re, subprocess, sys, os
dis = open('/tmp/all.dis').read().split('\n'); idx = {}
for i, ln in enumerate(dis):
    m = re.match(r'^\s+([0-9a-f]+):', ln)
    if m: idx[int(m.group(1), 16)] = i
def find_jt(addr):
    i = idx[addr]; n = hi = lo = None; reg = None
    for j in range(i - 1, i - 24, -1):
        ln = dis[j]
        m = re.search(r'sltiu\s+\$\d+, \$\d+, (0x[0-9a-f]+)', ln)
        if m and n is None: n = int(m.group(1), 16)
        m = re.search(r'addiu\s+\$(\d+), \$(\d+), (-?0x[0-9a-f]+)', ln)
        if m and lo is None and m.group(1) == m.group(2): lo = int(m.group(3), 16); reg = m.group(1)
        m = re.search(r'lui\s+\$(\d+), (0x[0-9a-f]+)', ln)
        if m and hi is None and lo is not None and m.group(1) == reg: hi = int(m.group(2), 16)
    if None in (n, hi, lo): raise Exception('no jump table pattern before %x' % addr)
    return 'jt=0x%x:0x%x:%d' % (addr, ((hi << 16) + lo) & 0xffffffff, n)
args = sys.argv[1:]; added = []
while True:
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'recomp.py')] + args, capture_output=True, text=True)
    m = re.search(r'jr non-ra at ([0-9a-f]+)', r.stderr)
    if not m: break
    jt = find_jt(int(m.group(1), 16)); added.append(jt); args.append(jt)
sys.stdout.write(r.stdout); sys.stderr.write(r.stderr[-2000:])
if added: print('auto jump tables:', ' '.join(added))
sys.exit(r.returncode)
