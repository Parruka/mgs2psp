# mesec_chk.py <build dir> [log] (task #180 step 4d-3, me_step4_notes.md 4d: the ME's memory on 64-byte lines of its own).  Checks the
# build mkpsp18e.sh (4d-4: mkpsp18f.sh) made (MGS2T.elf and the objects in <build dir>) and, given a run's log, its set-up line check and report:
#  1. .me_data and .me_bss (psp/me_sec.ld) start and end on 64-byte lines, their symbols are their bounds, and no other allocated
#     section has a byte on their lines;
#  2. every ME-safe object (ge/*.c built with -DMGS2_ME_SAFE) has no writable section but .me_data and .me_bss, and no COMMON symbol;
#     in the ELF each of its data symbols (local ones found by the object's FILE symbol) lies in .me_data, .me_bss or read-only data;
#  3. psp/xl_psp.c's .me_bss holds exactly its ME_LN objects (M glf gxl gxh xmp mp_view), and in the ELF they lie inside .me_bss;
#  4. the data the ME-safe objects name outside themselves lies in .me_data, .me_bss or read-only data, and none of them names a
#     symbol of psp/spr_state.ld (the main CPU's scratchpad state: the ME has a scratchpad of its own);
#  5. (with the log) the set-up line "me picture lines" says problems 0, and the report's line check problems 0 and regions left off
#     a line 0.
# Prints what it finds and "mesec_chk: PASS" or "mesec_chk: FAIL (n)"; exit status 0 on PASS.  Run it with python3 -I.
import sys, os, re, subprocess
B = sys.argv[1]; LOG = sys.argv[2] if len(sys.argv) > 2 else None
BIN = '/home/claude/pspsdk/pspdev/bin/'
ME = ['xlive', 'xlate', 'gsfront', 'texconv', 'texcache', 'gelive', 'gebatch', 'xconv', 'vu1conv', 'gelist', 'gsup2', 'xlme', 'xme', 'me_rt',
      'xme_me']   # 4d-4: the ME's side of the loop (psp/xme_me.c, mkpsp18f.sh)
LN = {'M', 'glf', 'gxl', 'gxh', 'xmp', 'mp_view'}
SPR = '/mnt/project-files/mgs2/psp/spr_state.ld'
bad = []

def run(*a):
    return subprocess.run(list(a), capture_output=True, text=True, check=True).stdout

def sections(f):   # name -> (addr, size, flags), and index -> name
    out, idx = {}, {}
    for l in run(BIN + 'psp-readelf', '-SW', f).splitlines():
        m = re.match(r'\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([A-Za-z]*)\s', l)
        if m:
            out[m.group(2)] = (int(m.group(3), 16), int(m.group(4), 16), m.group(5)); idx[m.group(1)] = m.group(2)
    return out, idx

def symbols(f):    # [(file, name, value, size, type, bind, ndx)] in the table's order; file: the last FILE symbol's name
    out, cur = [], ''
    for l in run(BIN + 'psp-readelf', '-sW', f).splitlines():
        p = l.split()
        if len(p) < 7 or not p[0].endswith(':') or not p[0][:-1].isdigit():
            continue
        name = p[7] if len(p) > 7 else ''
        if p[3] == 'FILE':
            cur = name; continue
        sz = int(p[2], 16) if p[2].startswith('0x') else int(p[2])
        out.append((cur, name, int(p[1], 16), sz, p[3], p[4], p[6]))
    return out

def mine(sec):   # a section the ME may write, or read-only
    return sec in ('.me_data', '.me_bss') or sec.startswith('.rodata')

elf = os.path.join(B, 'MGS2T.elf')
S, SI = sections(elf)
ES = symbols(elf)
G = {}
for fi, n, v, sz, ty, bd, nd in ES:
    if bd in ('GLOBAL', 'WEAK') and nd != 'UND':
        G.setdefault(n, (v, sz, ty, nd))

# 1
for sec, a, b in (('.me_data', '__me_data_start', '__me_data_end'), ('.me_bss', '__me_bss_start', '__me_bss_end')):
    if sec not in S:
        bad.append('1: no %s section in the ELF' % sec); continue
    ad, sz, fl = S[sec]
    if ad & 63 or sz & 63:
        bad.append('1: %s at %08x, %x bytes: not on whole lines' % (sec, ad, sz))
    lo, hi = ad & ~63, (ad + sz + 63) & ~63
    for n, (a2, s2, f2) in S.items():
        if n != sec and 'A' in f2 and s2 and a2 < hi and a2 + s2 > lo:
            bad.append('1: %s (%08x-%08x) has bytes on the lines of %s (%08x-%08x)' % (n, a2, a2 + s2, sec, lo, hi))
    if G.get(a, (None,))[0] != ad or G.get(b, (None,))[0] != ad + sz:
        bad.append('1: %s / %s (%s, %s) are not the bounds of %s' % (a, b, G.get(a), G.get(b), sec))
    print('1: %s %08x-%08x (%u bytes)' % (sec, ad, ad + sz, sz))

# 2
for o in ME:
    f = os.path.join(B, 'ge_%s.o' % o)
    so, si = sections(f); w = []
    for n, (a2, s2, f2) in so.items():
        if 'W' in f2 and 'A' in f2 and s2:
            if n in ('.me_data', '.me_bss'):
                w.append('%s %u' % (n, s2))
            else:
                bad.append('2: ge_%s.o has the writable section %s (%u bytes)' % (o, n, s2))
    loc = set(); glo = set()
    for fi, n, v, sz, ty, bd, nd in symbols(f):
        if nd == 'COM':
            bad.append('2: ge_%s.o has the COMMON symbol %s' % (o, n))
        if ty == 'OBJECT' and nd not in ('UND', 'ABS'):
            (glo if bd in ('GLOBAL', 'WEAK') else loc).add(n)
    nin = 0
    for fi, n, v, sz, ty, bd, nd in ES:   # the object's local data in the ELF, by its FILE symbol
        if fi == o + '.c' and n in loc and ty == 'OBJECT':
            sec = SI.get(nd, nd); nin += 1
            if not mine(sec):
                bad.append('2: %s (ge/%s.c) at %08x in %s' % (n, o, v, sec))
    for n in glo:
        g = G.get(n)
        if not g:
            bad.append('2: %s (ge/%s.c) not in the ELF' % (n, o)); continue
        sec = SI.get(g[3], g[3])
        if not mine(sec):
            bad.append('2: %s (ge/%s.c) at %08x in %s' % (n, o, g[0], sec))
    print('2: ge_%s.o: %s; its data symbols in the ELF: %d local, %d global' % (o, ', '.join(w) if w else 'no writable data', nin, len(glo)))

# 3
xo = os.path.join(B, 'xl_psp.o')
xs, xsi = sections(xo); inb = set()
for fi, n, v, sz, ty, bd, nd in symbols(xo):
    sec = xsi.get(nd, nd)
    if sec == '.bss.me':
        bad.append('3: xl_psp.o still has .bss.me (%s)' % n)
    if sec == '.me_bss' and ty == 'OBJECT':
        inb.add(n)
if inb != LN:
    bad.append('3: the .me_bss of xl_psp.o holds %s, not %s' % (sorted(inb), sorted(LN)))
found = {}
for fi, n, v, sz, ty, bd, nd in ES:
    if fi == 'xl_psp.c' and n in LN and ty == 'OBJECT':
        found[n] = (v, sz)
if '.me_bss' in S:
    lo, hi = S['.me_bss'][0], S['.me_bss'][0] + S['.me_bss'][1]
    for n in sorted(LN):
        if n not in found:
            bad.append('3: %s not among the ELF\'s xl_psp.c symbols' % n); continue
        v, sz = found[n]
        if not (lo <= v and v + sz <= hi):
            bad.append('3: %s at %08x (%u bytes) outside .me_bss' % (n, v, sz))
print('3: xl_psp.c in .me_bss: ' + ', '.join('%s %08x %u' % (n, found[n][0], found[n][1]) for n in sorted(found)))

# 4
spr = set(re.findall(r'^\s*([A-Za-z_]\w*)\s*=', open(SPR).read(), re.M))
refs = {}
for o in ME:
    for t in run(BIN + 'psp-nm', '-u', os.path.join(B, 'ge_%s.o' % o)).split():
        if t != 'U':
            refs.setdefault(t, []).append(o)
for n in sorted(refs):
    if n in spr:
        bad.append('4: %s (psp/spr_state.ld) named by %s' % (n, ','.join(refs[n]))); continue
    g = G.get(n)
    if not g or g[2] == 'FUNC':
        continue
    sec = SI.get(g[3], g[3])
    if g[2] == 'NOTYPE' and sec == '.text':
        continue
    ok = mine(sec) or g[3] == 'ABS'
    print('4: %s (%s, %s) named by %s%s' % (n, g[2], sec, ','.join(refs[n]), '' if ok else ': NOT the ME\'s'))
    if not ok:
        bad.append('4: %s in %s named by %s' % (n, sec, ','.join(refs[n])))

# 5
if LOG:
    t = open(LOG, 'rb').read().decode('latin-1')
    m = re.search(r'me picture lines \(task #180 step 4d-3\).*?problems (\d+)\n', t)
    if not m:
        bad.append('5: no set-up line check in the log')
    else:
        print('5: the set-up line check: problems %s' % m.group(1))
        if m.group(1) != '0':
            bad.append('5: the set-up line check found %s problems' % m.group(1))
    m = re.search(r'the lines \(step 4d-3\): problems at set-up (\d+), the heap\'s regions moved by realloc and copied to a line start (\d+), '
                  r'left off one (\d+)', t)
    if not m:
        bad.append('5: no line check in the report')
    else:
        print('5: the report: problems at set-up %s, regions copied to a line start %s, left off one %s' % m.groups())
        if m.group(1) != '0' or m.group(3) != '0':
            bad.append('5: the report: %s' % (m.groups(),))
for x in bad:
    print('PROBLEM ' + x)
print('mesec_chk: ' + ('PASS' if not bad else 'FAIL (%d)' % len(bad)))
sys.exit(1 if bad else 0)
