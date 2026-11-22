"""qsr.py: shared save / restore helpers for the guest stack frames (after regcache).  The PSP's 16 KB I-cache misses cost ~125 cycles each
(PSP-3000 hardware counters, TTGame v10) and most recompiled functions run as straight-line code once per fetch, so their prologue saves
(sq / sd of the callee-saved registers from R[] / RH[] to the guest stack) and epilogue restores (RCLD + RH[] from the guest stack) are mostly
I-cache misses: ~9 host instructions per register, inline in every function.  Here a run of them becomes one call of qsr_save / qsr_rest
(demo_src/guest.h) with a table of (register, kind, offset): the same stores and loads, in the same order, from one hot copy of the code.

Saves: WR64(r29 + K, R[n]) [+ WR64(r29 + K + 8, RH[n])]; a run may skip over register-only lines that neither write R[n] / RH[n] of a later
save nor r29 (the saves move up to the first one: they read R[] values those lines do not change, and nothing in between touches memory).
Restores: { qa_ = r29 + K; RCLD(rn, n, RD64(qa_)); RH[n] = RD64(qa_ + 8); } and RCLD(r31, 31, RD64(r29 + K)): consecutive lines only; the
locals are set again from R[] after the call (RCLD's own definition).  Functions that loop in the I-cache (model/hot_fns.txt) keep the
inline form.  QSR=0 disables the pass."""
import re, os
SAVE_Q = re.compile(r'^WR64\(\(\(uint32_t\)\(r29 \+ (\d+)\)\), R\[(\d+)\]\); WR64\(\(\(\(uint32_t\)\(r29 \+ (\d+)\)\)\) \+ 8, RH\[(\d+)\]\);$')
SAVE_D = re.compile(r'^WR64\(\(uint32_t\)\(r29 \+ (\d+)\), R\[(\d+)\]\);$')
REST_Q = re.compile(r'^\{ uint32_t qa_ = \(\(uint32_t\)\(r29 \+ (\d+)\)\); if \((\d+)\) \{ RCLD\(r(\d+), (\d+), \(int64_t\)RD64\(qa_\)\); RH\[(\d+)\] = \(int64_t\)RD64\(qa_ \+ 8\); \} \}$')
REST_D = re.compile(r'^RCLD\(r(\d+), (\d+), \(int64_t\)RD64\(\(uint32_t\)\(r29 \+ (\d+)\)\)\);$')
LINE = re.compile(r'^(/\*[0-9a-f]{6}\*/) ?(.*)$')
NEUTRAL = re.compile(r'^(?:(?:r\d+ = [^;]*?;|RCMV\(r\d+, \d+, \d+\);)\s*)*$')

def _save(body):
    m = SAVE_Q.match(body)
    if m and m.group(1) == m.group(3) and m.group(2) == m.group(4): return int(m.group(2)), 1, int(m.group(1))
    m = SAVE_D.match(body)
    if m: return int(m.group(2)), 0, int(m.group(1))
    return None

def _rest(body):
    m = REST_Q.match(body)
    if m and len({m.group(2), m.group(3), m.group(4), m.group(5)}) == 1: return int(m.group(2)), 1, int(m.group(1))
    m = REST_D.match(body)
    if m and m.group(1) == m.group(2): return int(m.group(1)), 0, int(m.group(3))
    return None

def _neutral(body):
    """a register-only line a save may move over: only local assignments and RCMV, no memory / call / branch / r29 write"""
    if not NEUTRAL.match(body): return False
    if re.search(r'\b(RD\w*|WR\w*|goto|JALR|X_\w+|[PO][0-9a-f]{6}\w*|VU\w*|VF|VN|VACC|vu_\w+|RH|F|HW\w*|return)\b', body.replace('RCMV(', '')): return False
    if re.search(r'(?<![\w])R\[', body.replace('RCMV(', '')): return False
    if re.search(r'\br29 = ', body): return False
    return True

def run(path, hot_path=None):
    if os.environ.get('QSR') == '0': return
    hot = set()
    if hot_path and os.path.exists(hot_path):
        hot = {l.strip() for l in open(hot_path) if l.strip() and not l.startswith('#')}
    s = open(path).read()
    tabs = []; tab_ix = {}
    def table(entries):
        key = tuple(entries)
        if key not in tab_ix:
            tab_ix[key] = sum(len(t) + 1 for t in tabs); tabs.append(key)
        return tab_ix[key]
    out = []; pos = 0; nsave = nrest = nfn = 0
    for fm in re.finditer(r'\nstatic void ([PO][0-9a-f]{6}\w*)\(void\)\n\{\n(.*?)\n\}\n', s, re.S):
        name = fm.group(1)
        if name.split('_')[0] in hot or name in hot: continue
        lines = fm.group(2).split('\n'); changed = False
        i = 0
        while i < len(lines):
            m = LINE.match(lines[i])
            if not m: i += 1; continue
            body = m.group(2).strip()
            sv = _save(body)
            if sv:
                run_ix = [(i, sv)]; j = i + 1; skipped = []
                while j < len(lines):
                    mj = LINE.match(lines[j])
                    if not mj: break
                    bj = mj.group(2).strip(); s2 = _save(bj)
                    if s2: run_ix.append((j, s2)); j += 1; continue
                    if bj == '' or _neutral(bj):
                        skipped.append(j); j += 1; continue
                    break
                # saves after a skipped line must not move over a write of their register (RCMV) / of r29
                ok = []
                for (k, e) in run_ix:
                    if any(sk < k and re.search(r'RCMV\(r\d+, %d, ' % e[0], LINE.match(lines[sk]).group(2)) for sk in skipped): break
                    ok.append((k, e))
                if len(ok) >= 2 and all(0 <= e[2] < 8192 and e[2] % 8 == 0 for _, e in ok):
                    t = table([e[0] | e[1] << 5 | (e[2] >> 3) << 6 for _, e in ok])
                    first = ok[0][0]
                    lines[first] = LINE.match(lines[first]).group(1) + ' qsr_save((uint32_t)r29, g_qsr_tab + %d);   /* %d saves (model/qsr.py) */' % (t, len(ok))
                    for k, _ in ok[1:]: lines[k] = LINE.match(lines[k]).group(1)
                    nsave += len(ok); changed = True
                    i = ok[-1][0] + 1; continue
                i += 1; continue
            rs = _rest(body)
            if rs:
                run_ix = [(i, rs)]; j = i + 1
                while j < len(lines):
                    mj = LINE.match(lines[j])
                    if not mj: break
                    bj = mj.group(2).strip(); r2 = _rest(bj)
                    if r2: run_ix.append((j, r2)); j += 1; continue
                    if bj == '': j += 1; continue
                    break
                if len(run_ix) >= 2 and all(0 <= e[2] < 8192 and e[2] % 8 == 0 for _, e in run_ix):
                    t = table([e[0] | e[1] << 5 | (e[2] >> 3) << 6 for _, e in run_ix])
                    first = run_ix[0][0]
                    sets = ' '.join('r%d = (int32_t)R[%d];' % (e[0], e[0]) for _, e in run_ix)
                    lines[first] = LINE.match(lines[first]).group(1) + ' qsr_rest((uint32_t)r29, g_qsr_tab + %d); %s   /* %d restores (model/qsr.py) */' % (t, sets, len(run_ix))
                    for k, _ in run_ix[1:]: lines[k] = LINE.match(lines[k]).group(1)
                    nrest += len(run_ix); changed = True
                    i = run_ix[-1][0] + 1; continue
            i += 1
        if changed:
            nfn += 1
            out.append(s[pos:fm.start(2)]); out.append('\n'.join(lines)); pos = fm.end(2)
    out.append(s[pos:])
    s = ''.join(out)
    flat = []
    for t in tabs: flat.append(len(t)); flat.extend(t)
    hdr = 'static const uint16_t g_qsr_tab[%d] = { %s };   /* model/qsr.py: count, then register | sq << 5 | offset / 8 << 6 */\n' % (max(1, len(flat)), ', '.join(map(str, flat)) or '0')
    s = hdr + s
    open(path, 'w').write(s)
    print('qsr: %d functions, %d saves and %d restores through the helpers, %d tables' % (nfn, nsave, nrest, len(tabs)))

if __name__ == '__main__':
    import sys
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
