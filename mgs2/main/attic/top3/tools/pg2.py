#!/usr/bin/env python3
"""pg2.py DUMP STEPS ELF OVL ELF_G OVL_G PS2ELF STAGEBIN OUTPREFIX  (python3 -I)
prof_guest.py (speed_notes s49) redone for gl39: cost (1.65 i + 68 im + 86 dm)/333000 a step; per GUEST FUNCTION figures (host instr,
guest instr executed [estimate], ratio, misses) and per line class.  A header line's guest instructions = its /*hex*/ markers outside an
inlined body; a line's executions = max count of its PCs, lines without PCs take the next PC line's count in the same block (labels end
a block).  Lines holding a '/* inlined P..' body are counted as class 'inlined leaf' and their guest instructions are not estimated."""
import sys, os, re, struct, subprocess, collections, bisect
dump, steps, elf_p, ovl_p, elf_g, ovl_g, ps2elf, stagebin, outp = sys.argv[1:10]
steps = int(steps); BIN = '/home/claude/pspsdk/pspdev/bin/'
D = open(ps2elf, 'rb').read(); SD = open(stagebin, 'rb').read(); SBASE = struct.unpack_from('<I', SD, 0)[0]
def W(a):
    if SBASE <= a < SBASE + len(SD): return struct.unpack_from('<I', SD, a - SBASE)[0]
    o = a - 0x100000 + 0x80
    return struct.unpack_from('<I', D, o)[0] if 0 <= o < len(D) - 3 else None
SPECIAL = {0:'sll',2:'srl',3:'sra',4:'sllv',6:'srlv',7:'srav',8:'jr',9:'jalr',10:'movz',11:'movn',12:'syscall',13:'break',15:'sync',16:'mfhi',17:'mthi',18:'mflo',19:'mtlo',20:'dsllv',22:'dsrlv',23:'dsrav',24:'mult',25:'multu',26:'div',27:'divu',32:'add',33:'addu',34:'sub',35:'subu',36:'and',37:'or',38:'xor',39:'nor',42:'slt',43:'sltu',44:'dadd',45:'daddu',46:'dsub',47:'dsubu',56:'dsll',58:'dsrl',59:'dsra',60:'dsll32',62:'dsrl32',63:'dsra32'}
OPS = {2:'j',3:'jal',4:'beq',5:'bne',6:'blez',7:'bgtz',8:'addi',9:'addiu',10:'slti',11:'sltiu',12:'andi',13:'ori',14:'xori',15:'lui',20:'beql',21:'bnel',22:'blezl',23:'bgtzl',24:'daddi',25:'daddiu',26:'ldl',27:'ldr',30:'lq',31:'sq',32:'lb',33:'lh',34:'lwl',35:'lw',36:'lbu',37:'lhu',38:'lwr',39:'lwu',40:'sb',41:'sh',42:'swl',43:'sw',44:'sdl',45:'sdr',46:'swr',47:'cache',49:'lwc1',51:'pref',54:'lqc2',55:'ld',57:'swc1',62:'sqc2',63:'sd'}
def mnem(w):
    op = w >> 26
    if op == 0: return 'nop' if w == 0 else SPECIAL.get(w & 63, 'special')
    if op == 1: return 'regimm'
    if op == 16: return 'cop0'
    if op == 17:
        fmt = (w >> 21) & 31
        if fmt == 8: return 'bc1'
        if fmt in (0, 2, 4, 6): return {0:'mfc1',2:'cfc1',4:'mtc1',6:'ctc1'}[fmt]
        return 'fpu'
    if op == 18:
        if (w >> 25) & 1: return 'vu0op'
        return {1:'qmfc2',2:'cfc2',5:'qmtc2',6:'ctc2',8:'bc2'}.get((w >> 21) & 31, 'cop2')
    if op == 28: return 'mmi'
    return OPS.get(op, 'op%d' % op)
def gclass(m):
    if m in ('lb','lbu','lh','lhu','lw','lwu'): return 'load 8-32'
    if m in ('ld','lq'): return 'load 64/128'
    if m in ('lwl','lwr','ldl','ldr','swl','swr','sdl','sdr'): return 'unaligned load/store'
    if m in ('sb','sh','sw'): return 'store 8-32'
    if m in ('sd','sq'): return 'store 64/128'
    if m in ('lwc1','swc1'): return 'fpu load/store'
    if m in ('lqc2','sqc2'): return 'vu0 load/store'
    if m in ('jal','jalr'): return 'call'
    if m == 'jr': return 'jr (return/switch)'
    if m == 'j': return 'jump'
    if m in ('beq','bne','blez','bgtz','beql','bnel','blezl','bgtzl','regimm','bc1','bc2'): return 'branch'
    if m.startswith('d') and m not in ('div','divu'): return 'alu 64'
    if m in ('mult','multu','div','divu','mfhi','mflo','mthi','mtlo'): return 'mul/div hi/lo'
    if m == 'mmi': return 'mmi'
    if m == 'vu0op': return 'vu0 macro op'
    if m in ('qmfc2','qmtc2','cfc2','ctc2','cop2'): return 'vu0 move/ctrl'
    if m in ('fpu','mfc1','mtc1','cfc1','ctc1'): return 'fpu op'
    if m in ('syscall','break','sync','cop0','cache','pref'): return 'system'
    if m == 'nop': return 'nop'
    return 'alu 32'
rows = []; tot = ''
for l in open(dump):
    if l.startswith('#'): tot = l.strip(); continue
    p = l.split()
    if len(p) >= 4: rows.append((int(p[0], 16), int(p[1]), int(p[2]), int(p[3])))
def sect(elf, name):
    for l in subprocess.run([BIN + 'psp-objdump', '-h', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) > 4 and p[1] == name: return int(p[3], 16), int(p[2], 16)
ob, osz = sect(ovl_p, '.ovl')
def syms(elf):
    o = subprocess.run([BIN + 'psp-nm', '-S', '--defined-only', '-n', elf], capture_output=True, text=True).stdout.splitlines()
    seen = collections.Counter(); res = []
    for l in o:
        p = l.split()
        if len(p) == 4 and p[2] in ('t', 'T', 'w', 'W'):
            a, sz, n = int(p[0], 16), int(p[1], 16), p[3]; res.append((a, sz, (n, seen[n]))); seen[n] += 1
    return res
def mover(pe, ge):
    sp = syms(pe); sg = {k: a for a, sz, k in syms(ge)}; st = [a for a, sz, k in sp]
    def mv(pc):
        i = bisect.bisect_right(st, pc) - 1
        if i < 0: return None, None
        a, sz, k = sp[i]
        if pc >= a + sz: return None, None
        return (sg[k] + (pc - a)) if k in sg else None, k[0]
    return mv
mv_m = mover(elf_p, elf_g); mv_o = mover(ovl_p, ovl_g)
def a2l(elf, pcs):
    res = {}
    if not pcs: return res
    o = subprocess.run([BIN + 'psp-addr2line', '-e', elf, '-a', '-f', '-i'], input='\n'.join('%x' % p for p in pcs) + '\n', capture_output=True, text=True).stdout.splitlines()
    cur = None; fr = []
    for_i = 0
    while for_i < len(o):
        l = o[for_i]
        if l.startswith('0x'):
            if cur is not None: res[cur] = fr[-1] if fr else ('??', '??:0')
            cur = int(l, 16); fr = []; for_i += 1; continue
        fr.append((l, o[for_i + 1] if for_i + 1 < len(o) else '??:0')); for_i += 2
    if cur is not None: res[cur] = fr[-1] if fr else ('??', '??:0')
    return res
G = {}; hostfn = {}
for pc, *_ in rows:
    if pc in G: continue
    g, n = (mv_o if ob <= pc < ob + osz else mv_m)(pc)
    G[pc] = (g, ob <= pc < ob + osz); hostfn[pc] = n
lm = a2l(elf_g, sorted({g for g, o in G.values() if g is not None and not o}))
lo = a2l(ovl_g, sorted({g for g, o in G.values() if g is not None and o}))
cache = {}
VIRT = {}   # #line-mapped headers: virtual path -> list of lines indexed by virtual line number
for real in os.environ.get('PG2_HDRS', '').split(':'):
    if not real: continue
    vp, vl = real, 1
    for t in open(real, errors='replace').read().split('\n'):
        m = re.match(r'^#line (\d+) "([^"]+)"', t)
        if m: vp, vl = m.group(2), int(m.group(1)); continue
        L = VIRT.setdefault(vp, {}); L[vl] = t; vl += 1
for vp, L in VIRT.items():
    n = max(L) if L else 0; cache[vp] = [L.get(i, '') for i in range(1, n + 1)]
def text(path):
    if path not in cache:
        try: cache[path] = open(path, errors='replace').read().split('\n')
        except OSError: cache[path] = []
    return cache[path]
GA = re.compile(r'^\s*/\*([0-9a-f]+)\*/'); MK = re.compile(r'/\*([0-9a-f]{5,7})\*/')
DEF = re.compile(r'^static (?:void|int) ((?:S_\w+?_)?P[0-9a-f]{6}\w*)\(void\)$')
def classify(t):
    if '/* inlined P' in t: return 'inlined leaf body', 'inl'
    m = GA.match(t)
    if m:
        w = W(int(m.group(1), 16)); mm = mnem(w) if w is not None else '?'; k = gclass(mm)
        if 'IRQ_POLL' in t and k in ('branch', 'jump'): k += ' + loop poll'
        return k, mm
    s = t.strip()
    if t.startswith('FN_ENTER') or s.startswith('float') or s.startswith('int64_t') or s.startswith('int32_t') or t.startswith('static') or s == '{': return 'entry: register loads', 'entry'
    if s == '}': return 'exit: register stores', 'exit'
    return 'generated, other', 'other'
agg = collections.defaultdict(lambda: [0, 0, 0]); F = collections.defaultdict(lambda: [0, 0, 0])
FC = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0]))
OTH = collections.defaultdict(lambda: [0, 0, 0]); lexec = collections.defaultdict(int); lcls = {}; fnfile = {}
for pc, c, im, dm in rows:
    g, o = G[pc]
    fn, lc = ((lo if o else lm).get(g, ('??', '??:0')) if g is not None else (hostfn.get(pc) or '(none)', '??:0'))
    path, _, ln = lc.rpartition(':'); ln = ln.split()[0] if ln else '0'
    try: ln = int(ln)
    except ValueError: ln = 0
    b = os.path.basename(path)
    if b.startswith('gen_') and b.endswith('.h') and g is not None:
        t = text(path)[ln - 1] if 0 < ln <= len(text(path)) else ''
        k, mm = classify(t); key = (path, ln); lexec[key] = max(lexec[key], c); lcls[key] = k
        if k == 'generated, other': o_ = OTH[(b, t.strip()[:70])]; o_[0] += c; o_[1] += im; o_[2] += dm
        fname = hostfn.get(pc) or fn; fnfile.setdefault(fname.split('.')[0], path)
    else:
        k = 'runtime'; fname = hostfn.get(pc) or fn
    a = agg[k]; a[0] += c; a[1] += im; a[2] += dm
    f = F[fname]; f[0] += c; f[1] += im; f[2] += dm
    x = FC[fname][k]; x[0] += c; x[1] += im; x[2] += dm
# guest executions: walk each header's lines
gexec_cls = collections.defaultdict(float); gexec_fn = collections.defaultdict(float)
for path in {p for p, l in lexec}:
    T = text(path); cur = None; pend = []
    def flush(cnt):
        for (fnm, k, n) in pend:
            gexec_cls[k] += n * cnt; gexec_fn[fnm] += n * cnt
        pend.clear()
    for i, t in enumerate(T, 1):
        m = DEF.match(t)
        if m: flush(0); cur = m.group(1); continue
        if cur is None: continue
        if re.match(r'^L[0-9a-f]+\w*:;', t) or t == '}': flush(0)
        if '/* inlined P' in t: continue
        n = len(MK.findall(t)) if GA.match(t) else 0
        key = (path, i)
        if n: pend.append((cur, lcls.get(key) or classify(t)[0], n))
        if key in lexec: flush(lexec[key])
ms = lambda v: (1.65 * v[0] + 68 * v[1] + 86 * v[2]) / 333000 / steps
out = open(outp + '_class.txt', 'w')
Tt = [sum(v[i] for v in agg.values()) for i in range(3)]
print(tot, file=out); print('per step: %.3f M instr, %.1f k I-miss, %.1f k D-miss, %.2f ms [C model]' % (Tt[0]/steps/1e6, Tt[1]/steps/1e3, Tt[2]/steps/1e3, ms(Tt)), file=out)
gen = [sum(v[i] for k, v in agg.items() if k != 'runtime') for i in range(3)]; gg = sum(v for k, v in gexec_cls.items())
print('generated code: %.3f M instr, %.1f k I, %.1f k D = %.2f ms; guest instr ~%.0f k a step; %.2f PSP instr per guest instr' % (gen[0]/steps/1e6, gen[1]/steps/1e3, gen[2]/steps/1e3, ms(gen), gg/steps/1e3, gen[0]/gg if gg else 0), file=out)
print('\n%-32s %9s %8s %6s %7s %7s %6s %6s %6s' % ('class', 'instr/st', 'guest/st', 'psp/g', 'I/st', 'D/st', 'ms', 'ms_i', 'ms_m'), file=out)
for k, v in sorted(agg.items(), key=lambda kv: -ms(kv[1])):
    g = gexec_cls.get(k, 0)
    print('%-32s %9.0f %8.0f %6s %7.0f %7.0f %6.2f %6.2f %6.2f' % (k[:32], v[0]/steps, g/steps, '%.1f' % (v[0]/g) if g else '-', v[1]/steps, v[2]/steps, ms(v), 1.65*v[0]/333000/steps, (68*v[1]+86*v[2])/333000/steps), file=out)
out.close()
import json
json.dump({fn: {k: v for k, v in c.items()} for fn, c in FC.items()}, open(outp + '_fc.json', 'w'))
json.dump(dict(gexec_fn), open(outp + '_gx.json', 'w'))
out = open(outp + '_other.txt', 'w')
for k, v in sorted(OTH.items(), key=lambda kv: -ms(kv[1]))[:80]: print('%7.3f %9.0f %s | %s' % (ms(v), v[0]/steps, k[0], k[1]), file=out)
out.close()
out = open(outp + '_fn.txt', 'w')
print('%-28s %7s %8s %7s %6s %6s %6s %6s %6s  %s' % ('function', 'ms', 'instr/st', 'guest/st', 'psp/g', 'ms_i', 'ms_I', 'ms_D', 'call%', 'top classes (ms)'), file=out)
for fn, v in sorted(F.items(), key=lambda kv: -ms(kv[1])):
    if os.environ.get('PG2_GENONLY') and fn.split('.')[0] not in fnfile: continue
    g = gexec_fn.get(fn.split('.')[0], 0)
    cls = FC[fn]; cl = sorted(cls.items(), key=lambda kv: -ms(kv[1]))[:5]
    callp = sum(ms(cls[k]) for k in cls if k in ('call', 'entry: register loads', 'exit: register stores', 'jr (return/switch)'))
    print('%-28s %7.3f %8.0f %7.0f %6s %6.3f %6.3f %6.3f %5.0f%%  %s' % (fn[:28], ms(v), v[0]/steps, g/steps, '%.1f' % (v[0]/g) if g else '-', 1.65*v[0]/333000/steps, 68*v[1]/333000/steps, 86*v[2]/333000/steps, 100*callp/ms(v) if ms(v) else 0, '; '.join('%s %.3f' % (k, ms(x)) for k, x in cl)), file=out)
out.close()
