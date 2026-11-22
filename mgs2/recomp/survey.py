"""Coverage survey: discover every function of the ELF (and optionally a stage image), run recomp.gen_func on each, and report what the
TT recompiler cannot translate yet.  Jump tables are found from the code itself (sltiu n / lui / addiu before 'jr reg').
Usage: SLUS=SLES_503.83 [STAGE=w00a.bin [SPLIT=w00a]] python3 survey.py [out.h]
SPLIT: write only the stage's functions (and ELF functions only the stage's pointers reveal), named S_<split>_P<addr>, with
S_<split>_init / S_<split>_call; it is compiled in the same unit as the ELF translation, whose P<addr> functions it calls."""
import os, re, sys, struct, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recomp, funcs
from funcs import find_jt
from recomp import W

ELF_TEXT = (0x100000, 0x17cf78)
ELF_DATA = [(0x17d180, 0x193e34), (0x1ada00, 0x1b08a0)]

def s16(x): return x - 0x10000 if x & 0x8000 else x
def main():
    regions = [ELF_TEXT]; data = list(ELF_DATA); extra = [0x100010, 0x1001a0, 0x1000d0]
    if recomp.SD:
        base = recomp.SBASE; end, bss, tab = struct.unpack_from('<3I', recomp.SD, 4)
        last = max(a for a in range(tab, end, 4) if W(a) == funcs.JR_RA)      # data after the last return is not code
        a = tab
        while W(a + 4): extra.append(W(a + 4)); a += 8
        for x in extra:                     # an exported function past the last return (r_tnk0 0x1f2320 ends in a tail `j`)
            if tab <= x < end and x > last:
                y = x
                while y < end and W(y) != funcs.JR_RA and W(y) >> 26 != 2: y += 4
                last = max(last, y)
        regions.append((tab, last + 8)); data.append((tab, end))
    fns = funcs.discover(regions, data, extra)
    split = os.environ.get('SPLIT'); pfx = 'S_%s_' % split if split else ''
    elf_names = set(s_ for s_, e_ in funcs.discover([ELF_TEXT], list(ELF_DATA), [0x100010, 0x1001a0, 0x1000d0])) if split else set()
    known = {s: ('P%x' % s if s in elf_names else pfx + 'P%x' % s) for s, e in fns}
    if split: fns = [(s, e) for s, e in fns if s not in elf_names]
    errs = collections.Counter(); bad = []; ok = 0; out = []; jts = 0; tcs = 0; code_bytes = 0
    for s, e in fns:
        code_bytes += e - s
        for _ in range(64):
            try:
                out.append(recomp.gen_func(known[s], s, e, known)); ok += 1; break
            except RuntimeError as ex:
                m = re.search(r'jr non-ra at ([0-9a-f]+)', str(ex))
                if m:
                    a = int(m.group(1), 16); jt = find_jt(a, s)
                    if jt and not all(s <= W(jt[0] + 4 * k) < e for k in range(jt[1])): errs['jump table target outside function'] += 1; bad.append((s, 'jt %x' % a)); break
                    if jt and a not in recomp.JT: recomp.JT[a] = jt; jts += 1; continue
                    if not jt and a not in recomp.TC: recomp.TC.add(a); tcs += 1; continue    # jr through a register, no table: tail call via JALR
                k = re.sub(r' at [0-9a-f]+', '', str(ex)); errs[k] += 1; bad.append((s, str(ex))); break
            except Exception as ex:
                errs[type(ex).__name__ + ': ' + str(ex)[:60]] += 1; bad.append((s, repr(ex))); break
    print('functions', len(fns), 'code bytes', code_bytes, 'translated', ok, 'failed', len(bad), 'jump tables', jts, 'register tail jumps', tcs)
    for k, v in errs.most_common(40): print('%6d  %s' % (v, k))
    for s_, why in bad[:10]: print('  failed P%x: %s' % (s_, why[:120]))
    if len(sys.argv) > 1:
        h = ''.join(out); ext = set(); res = []
        for part in re.split(r'(?=^static void \w+\(void\)\n\{)', h, flags=re.M):     # tailfix.py: goto outside the function = tail jump
            labels = set(re.findall(r'\bL([0-9a-f]+):', part))
            def rep(m):
                t = m.group(1)
                if t in labels: return m.group(0)
                ta = int(t, 16); nm = known.get(ta)
                if nm is None: nm = 'X_%x' % ta; ext.add(ta)
                return '{ %s(); return; }' % nm
            res.append(re.sub(r'goto L([0-9a-f]+);', rep, part))
        body = ''.join(res)
        # a branch back to an earlier address (a loop) polls for interrupts: IRQ_POLL() (guest.h; empty unless the host defines it)
        def back(line):
            m = re.match(r'/\*([0-9a-f]+)\*/', line)
            if not m: return line
            here = int(m.group(1), 16)
            return re.sub(r'goto L([0-9a-f]+);', lambda g: '{ IRQ_POLL(); goto L%s; }' % g.group(1) if int(g.group(1), 16) <= here else g.group(0), line)
        body = '\n'.join(back(l) for l in body.split('\n'))
        protos = ''.join('static void %s(void);\n' % known[s_] for s_ in sorted(set(s_ for s_, e_ in fns)))
        # a jump outside every known function (data decoded as code in a falsely seeded function): a stop if it is ever reached
        protos += ''.join('__attribute__((weak)) static void X_%x(void) { BREAK(0x%x); }\n' % (t, t) for t in sorted(ext | recomp.externs - set(known)))
        if not split: protos += 'static void JALR(uint32_t);\n'
        init = 'static void ' + pfx + ('init' if split else 'gen_init') + '(void)   /* jump tables of the original switch statements */\n{\n' + ''.join('  WR32(0x%x, 0x%x);\n' % (b_ + 4 * k, W(b_ + 4 * k)) for (b_, n_) in sorted(set(recomp.JT.values())) for k in range(n_)) + '}\n'
        own = set(s_ for s_, e_ in fns)
        init += 'static int ' + pfx + ('call' if split else 'gen_call') + '(uint32_t a)   /* calls through a register (jalr): 1 if a is a translated function */\n{\n  switch (a) {\n' + ''.join('  case 0x%x: %s(); return 1;\n' % (a_, n_) for a_, n_ in sorted(known.items()) if a_ in own) + '  }\n  return 0;\n}\n'
        open(sys.argv[1], 'w').write(protos + body + init); print('tail jumps to unknown code:', [hex(x) for x in sorted(ext)][:20], len(ext))
main()
