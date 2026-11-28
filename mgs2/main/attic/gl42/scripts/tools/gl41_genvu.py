#!/usr/bin/env python3
"""gl41_genvu.py (gl41 levers P3 and P1, attic/gl41s, 2026-11-23): a gl41 gen_vu.h from a vupipe.py gen_vu.h (gl39: gen/vp22/gen_vu.h,
md5 feb948ce8beb6f19c1b764b20ab8da0e).  Usage: python3 -I gl41_genvu.py SRC DST [--all]   (python3 -I: no import from the folder)

P3: the box cull's VFPU forms vupv_96936c0f_0, vupv_8baafbc3_0 and vupv_660a267e_0 (three texts that GCC folds into one function in
gl39's ELF, 0x08a45bec, 0xd84 bytes; the walk calls it through 0x96936c0f and 0x8baafbc3, the rain wait through 0x8baafbc3 only [C model
m_off]).  The three texts are checked to be one program: equal after copy propagation of their single-assignment locals (`pA = pB;`)
and renaming of locals and labels (unused labels dropped).  vupv_96936c0f_0 gets a copy vupv_96936c0f_0_p3 without the per-corner
`vcmp.q ES` checks (the first check of each corner register; --all: every single-register check the rule below proves, for a report
only, not used by gl41); the rule is checked on each of the three texts; each of the three starts with
VU_L41_P3_HOOK(vupv_96936c0f_0_p3) (runtime/vu0.h: the switch MGS2_OPT2 bit 20 and probe (c)): the same hook in all three, so GCC can
still fold the three hooked forms into one, as gl39's.  vuc_p3_pairs (gl39 form, P3 form) lets probe (c) find the gl39 form.  Without
the macro VU_L41_P3_HOOK the file compiles to gl39's code.
The rule, checked here on the generated text, not assumed: a check of VFPU register r (vcmp.q ES r, r; bvtl; goto vfail_) may go when
  1. a later check of r (one of its own, or the multi-register check before out_) comes before out_;
  2. between the two, r is written only by `vmul.q r, r, t` (no vpfxs; vpfxd only masks lanes; any vpfxt) on the PSP side (VPA) and only
     by `vq_[r].f[c] = vu_rnmul(vq_[r].f[c], ...)` on the x86 side (VPC): a lane that was NaN or Inf stays NaN or Inf (IEEE; the VFPU
     too: NaN * x = NaN, Inf * 0 = NaN 7f800001, Inf * finite = Inf [C PSP, harness v24 probe]; a denormal reads as 0);
  3. before out_ the function writes nothing but its locals and VFPU registers (C statements on a whitelist of locals, no store in an
     asm, no other goto than vfail_ and the last goto out_), and vfail_ is `g_vgrp_fb++; return 0;`.
So a corner that would have failed its check still fails at the later check: vfail_ (g_vgrp_fb++, return 0, nothing else written) and the
C form runs from the same state, as in gl39; a corner that passes runs the same instructions as gl39 minus the check.

P1: vuc_fns(hash, pc, &v, &f) (GEN_VU_FNS): for each (hash, entry) of vuc_dispatch, the VFPU form (or 0) and the C form its case calls,
made from vuc_dispatch's own text; checked: the same keys as vuc_entry and vuc_dispatch, and every C form returns only 1 (so
`v() || VPF(f)` and `f()` are 1, as vuc_dispatch's value is in gl39).
"""
import hashlib, re, sys

GL39_MD5 = 'feb948ce8beb6f19c1b764b20ab8da0e'
FNS = ('vupv_96936c0f_0', 'vupv_8baafbc3_0', 'vupv_660a267e_0')   # the box cull's three texts
FN = FNS[0]
GL39_FIRST = (104069, 104109, 104146, 104183, 104283, 104320, 104357, 104396)   # vupv_96936c0f_0's 8 per-corner checks in gl39 (1-based; top3 study)
LOCALS = re.compile(r'^(p\d+|vi\d+|mac_|clip_|gvm_|tpc_|i_|q_|st_|m[0-3]_|ua_|ub_|cg_|cl_|vck_)$')


def die(msg):
    sys.exit('gl41_genvu: ' + msg)


def regs_elems(name, size):
    """the elements (matrix, column, row) a VFPU register operand names; None: unknown (treated as every element of its matrix)"""
    m = re.match(r'^([SCRME])(\d)(\d)(\d)$', name)
    if not m:
        return None
    k, a, b, c = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    n = {'s': 1, 'p': 2, 't': 3, 'q': 4}.get(size, 4)
    if k == 'S':
        return {(a, b, c)}
    if k == 'C':
        return {(a, b, (c + i) & 3) for i in range(n)}
    return {(a, x, y) for x in range(4) for y in range(4)}   # R / M / E: the whole matrix (safe side)


def asm_text(line):
    m = re.search(r'__asm__(?: volatile)?\("((?:[^"\\]|\\.)*)"', line)
    return m.group(1).replace('\\n', '\n').replace('\\t', ' ') if m else None


def asm_ops(line):
    """the instructions of a VPA line: (mnemonic, size, operands)"""
    t = asm_text(line)
    if t is None:
        die('no asm in ' + line[:80])
    out = []
    for ins in t.split('\n'):
        ins = ins.strip()
        if not ins or ins.startswith('.') or re.match(r'^\d+:$', ins):
            continue
        p = ins.split(None, 1)
        mn, ops = p[0], [o.strip() for o in (p[1].split(',') if len(p) > 1 else [])]
        size = mn.split('.')[1] if '.' in mn else ''
        out.append((mn.split('.')[0], size, ops))
    return out


NOWRITE = {'sv', 'svl', 'svr', 'usv', 'mfv', 'mfvc', 'vcmp', 'vsync', 'vnop', 'vflush', 'bvt', 'bvf', 'bvtl', 'bvfl', 'vpfxs',
           'vpfxt', 'vpfxd', 'mtvc'}


def asm_writes(line, r):
    """(writes of r's elements that the rule allows, writes it does not, stores); r: a C register name"""
    re_ = regs_elems(r, 'q'); ok, bad, store = 0, [], False
    pend = {}
    for mn, size, ops in asm_ops(line):
        if mn in ('sv', 'svl', 'svr', 'usv', 'sw', 'sh', 'sb', 'swc1', 'sd', 'swl', 'swr'):
            store = True
        if mn in ('vpfxs', 'vpfxt', 'vpfxd'):
            pend[mn] = ops
            continue
        if mn in NOWRITE or not (mn.startswith('v') or mn in ('lv', 'lvl', 'lvr', 'ulv', 'mtv')):
            if mn.startswith('v'):
                pend = {}
            continue
        dst = ops[1] if mn == 'mtv' else (ops[0] if ops else '')
        el = regs_elems(dst, size)
        if el is None or (el & re_):
            if (mn == 'vmul' and size == 'q' and len(ops) == 3 and ops[0] == r and ops[1] == r and 'vpfxs' not in pend
                    and all(f in ('', 'm') for f in pend.get('vpfxd', []))):
                ok += 1
            else:
                bad.append(f'{mn}.{size} {",".join(ops)} (prefixes {pend})')
        pend = {}
    return ok, bad, store


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        die('usage: gl41_genvu.py SRC DST [--all]')
    src, dst, allc = a[0], a[1], '--all' in a[2:]
    raw = open(src, 'rb').read(); md5 = hashlib.md5(raw).hexdigest()
    L = raw.decode().split('\n')
    print(f'source {src} md5 {md5}' + (' (gl39)' if md5 == GL39_MD5 else ' (not gl39: the rule is checked all the same)'))
    # the function
    def p3_for(FN):
        i0 = [i for i, l in enumerate(L) if l == f'static int {FN}(void)']
        if len(i0) != 1 or L[i0[0] + 1] != '{':
            die(f'{FN} not found once')
        i0 = i0[0]; i1 = i0 + 1
        while L[i1] != '}':
            i1 += 1
        body = L[i0:i1 + 1]
        out_ = [k for k, l in enumerate(body) if l == ' out_:']; vf = [k for k, l in enumerate(body) if l == ' vfail_:']
        if len(out_) != 1 or len(vf) != 1 or body[vf[0] + 1] != '  g_vgrp_fb++; return 0;' or vf[0] + 2 != len(body) - 1:
            die('out_ / vfail_ not as expected')
        out_ = out_[0]
        if body[out_ - 1].strip() != f'tpc_ = {body[out_ - 1].split("=")[1].strip()}' or 'goto out_;' not in body[out_ - 1]:
            die('the line before out_ is not "tpc_ = N; goto out_;"')
        gotos = [l for l in body if 'goto' in l and 'goto vfail_;' not in l]
        if gotos != [body[out_ - 1]]:
            die('other gotos: ' + repr(gotos[:3]))
        # rule 3: before out_, nothing but locals and VFPU registers
        decl_end = next(k for k, l in enumerate(body) if l.startswith('  VPA(__asm__ volatile("lv.q'))
        for k in range(decl_end + 2, out_):
            s = body[k].strip()
            if not s or s.startswith('/*') or re.match(r'^L\d+:$', s) or s == 'if (__builtin_expect(vck_, 0)) goto vfail_;':
                continue
            if s.startswith('VPA('):
                if asm_writes(body[k], 'C000')[2]:
                    die(f'a store before out_: line {i0 + k + 1}')
                continue
            if s.startswith('VPC('):
                for t in re.findall(r'([A-Za-z_][\w\[\]\.]*)\s*=[^=]', s[4:]):
                    base = t.split('[')[0].split('.')[0]
                    if base not in ('vq_', 'vck_', 'cg_', 'cl_') and not LOCALS.match(base) and not re.match(r'^r\d_$', base):
                        die(f'VPC writes {t} before out_: line {i0 + k + 1}')
                if 'memcpy' in s or 'VF[' in s.split('=')[0]:
                    die(f'VPC memcpy / VF before out_: line {i0 + k + 1}')
                continue
            for st in [x.strip() for x in s.split(';') if x.strip()]:
                if re.match(r'^VP_ST0\(p\d+\)$', st):
                    continue
                if st == 'goto out_' and k == out_ - 1:
                    continue
                m = re.match(r'^(\w+) = ', st)
                if not m or not LOCALS.match(m.group(1)):
                    die(f'a statement before out_ that is not on the locals list: "{st}" line {i0 + k + 1}')
        # the checks
        chk = []   # (k, regs, vpc indexes, multi)
        for k in range(decl_end, out_ + 2):
            l = body[k]
            if 'vcmp.q ES' in l and l.strip().startswith('VPA('):
                rs = re.findall(r'vcmp\.q ES, (C\d\d0), \1', asm_text(l).replace('\n', ' '))
                n_es = asm_text(l).count('vcmp.q ES')
                ixs = [int(x) for x in re.findall(r'vu_fin\(vq_\[(\d+)\]\.u\[0\]\)', body[k + 1])]
                if len(rs) != n_es or not body[k + 1].strip().startswith('VPC(vck_ = !(') or 'goto vfail_;' not in body[k + 2]:
                    die(f'a check not in the known form: line {i0 + k + 1}')
                if [int(r[1]) * 4 + int(r[2]) for r in rs] != ixs:
                    die(f'VPA and VPC check other registers: line {i0 + k + 1}')
                multi = len(rs) > 1
                if not multi and body[k + 2].strip() != 'if (__builtin_expect(vck_, 0)) goto vfail_;':
                    die(f'check line 3: line {i0 + k + 1}')
                if multi and k != out_ + 1:
                    die('a multi-register check elsewhere than at out_')
                chk.append((k, rs, multi))
        single = [c for c in chk if not c[2]]
        print(f'{FN}: lines {i0 + 1}-{i1 + 1}, {len(single)} single-register checks, multi check at out_ of {len(chk[-1][1]) if chk and chk[-1][2] else 0} registers')

        def proves(c):
            k, (r,), _ = c
            nxt = next((d for d in chk if d[0] > k and r in d[1]), None)
            if nxt is None:
                return None, 'no later check'
            ix = int(r[1]) * 4 + int(r[2]); okn = 0
            for j in range(k + 3, nxt[0]):
                l = body[j]; s = l.strip()
                if s.startswith('VPA('):
                    ok, bad, _ = asm_writes(l, r)
                    if bad:
                        return None, f'line {i0 + j + 1}: {bad}'
                    okn += ok
                elif s.startswith('VPC('):
                    defs = dict(re.findall(r'float (r\d_) = (vu_rn\w+\(vq_\[\d+\]\.f\[\d\], )', s))
                    for (n, fu, c, v) in re.findall(r'vq_\[(\d+)\]\.([fu])\[(\d)\] = (\w+);', s):
                        if int(n) != ix:
                            continue
                        if fu != 'f' or defs.get(v) != f'vu_rnmul(vq_[{ix}].f[{c}], ':
                            return None, f'line {i0 + j + 1}: VPC writes vq_[{ix}] otherwise than by vu_rnmul of itself'
                    if f'memcpy(vq_[{ix}]' in s:
                        return None, f'line {i0 + j + 1}: VPC memcpy into vq_[{ix}]'
            return nxt, f'{okn} vmul.q {r}, {r}, t between'

        first = {}
        for c in single:
            first.setdefault(c[1][0], c)
        drop = []
        for c in single:
            nxt, why = proves(c)
            isfirst = first[c[1][0]] is c
            tag = f'check of {c[1][0]} at line {i0 + c[0] + 1} ({"first" if isfirst else "later"})'
            if nxt is None:
                print(f'  {tag}: kept: {why}')
                continue
            where = 'out_' if nxt[2] else f'line {i0 + nxt[0] + 1}'
            print(f'  {tag}: redundant (next check of it at {where}; {why})')
            if isfirst or allc:
                drop.append(c[0])
        if FN == FNS[0] and md5 == GL39_MD5 and not allc and sorted(i0 + k + 1 for k in drop) != list(GL39_FIRST):
            die(f'the checks left out are not gl39\'s 8 per-corner checks: {sorted(i0 + k + 1 for k in drop)}')
        if FN != FNS[0] and not allc and len(drop) != 8:
            die(f'{FN}: {len(drop)} per-corner checks, not 8')
        print(f'P3: {len(drop)} checks left out: lines {sorted(i0 + k + 1 for k in drop)}')
        p3 = []
        for k, l in enumerate(body):
            if any(k in (d, d + 1, d + 2) for d in drop):
                continue
            p3.append(l)
        p3[0] = f'static int {FN}_p3(void)   /* gl41 P3 (tools/gl41_genvu.py): {FN} without {len(drop)} checks */'
        return i0, i1, p3, drop
    P3 = {FN: p3_for(FN) for FN in FNS}

    def norm(b):   # copy propagation of `pA = pB;` (each p assigned once: checked), locals and labels renamed, unused labels dropped
        asg = {}
        for l in b:
            for v in re.findall(r'\b(p\d+) = ', l):
                asg[v] = asg.get(v, 0) + 1
        if any(n > 1 for n in asg.values()):
            die('a local assigned twice: no copy propagation')
        alias, canon, lab, res = {}, {}, {}, []
        for l in b[1:]:   # (without the name line)
            m = re.match(r'^  (p\d+) = (p\d+);$', l)
            if m:
                alias[m.group(1)] = alias.get(m.group(2), m.group(2)); continue
            if l.startswith('  uint32_t p1,'):
                continue
            l = re.sub(r'\bp\d+\b', lambda m: canon.setdefault(alias.get(m.group(0), m.group(0)), 'q%d' % len(canon)), l)
            res.append(re.sub(r'\bL\d+\b', lambda m: lab.setdefault(m.group(0), 'M%d' % len(lab)), l))
        used = {t for l in res if not re.match(r'^M\d+:$', l.strip()) for t in re.findall(r'\bM\d+\b', l)}
        return [l for l in res if not (re.match(r'^M\d+:$', l.strip()) and l.strip()[:-1] not in used)]
    n0 = norm(L[P3[FN][0]:P3[FN][1] + 1])
    for f in FNS[1:]:
        if norm(L[P3[f][0]:P3[f][1] + 1]) != n0:
            die(f'{f} is not {FN} after renaming: one P3 copy cannot serve both')
    print(f'P3: {", ".join(FNS[1:])} = {FN} after copy propagation and renaming: one copy {FN}_p3 for the three')

    # P1: vuc_fns from vuc_dispatch
    def cases(name):
        s = L.index(f'static int {name}(uint32_t hash, int pc)'); e = s
        while L[e] != '}':
            e += 1
        res = {}
        for l in L[s:e]:
            m = re.match(r'^  case (0x[0-9a-f]{8})u: switch \(pc\) \{ (.*) \} return 0;$', l)
            if m:
                for pc, ex in re.findall(r'case (\d+): return ([^;]*);', m.group(2)):
                    res[(m.group(1), int(pc))] = ex
        return res, s, e
    ent, _, _ = cases('vuc_entry'); dis, ds, de = cases('vuc_dispatch')
    if set(ent) != set(dis) or any(v != '1' for v in ent.values()):
        die('vuc_entry and vuc_dispatch keys differ')
    nfun = {}
    for i, l in enumerate(L):
        m = re.match(r'^static int (vupv?_[0-9a-f]{8}_\d+)\(void\)$', l)
        if m:
            j = i
            while L[j] != '}':
                j += 1
            nfun[m.group(1)] = set(re.findall(r'return[^;]*;', '\n'.join(L[i:j + 1])))
    rows = {}
    for key, ex in dis.items():
        m1 = re.match(r'^(vup_[0-9a-f]{8}_\d+)\(\)$', ex); m2 = re.match(r'^VPV\((vupv_[0-9a-f]{8}_\d+)\) VPF\((vup_[0-9a-f]{8}_\d+)\)$', ex)
        if m1:
            v, f = None, m1.group(1)
        elif m2:
            v, f = m2.group(1), m2.group(2)
        else:
            die(f'a dispatch case not in a known form: {key} {ex}')
        if nfun.get(f) != {'return 1;'}:
            die(f'C form {f} returns {nfun.get(f)}')
        if v and nfun.get(v) != {'return 1;', 'return 0;'}:
            die(f'VFPU form {v} returns {nfun.get(v)}')
        rows.setdefault(key[0], []).append((key[1], v, f))
    print(f'P1: vuc_fns: {len(dis)} (hash, entry) of {len(rows)} images; {sum(1 for x in dis.values() if x.startswith("VPV"))} with a VFPU form; every C form returns 1')
    fns = ['#define GEN_VU_FNS 1   /* gl41 P1 (tools/gl41_genvu.py): the functions vuc_dispatch calls, per (hash, entry), made from its cases */',
           '#if defined(VP_VFPU) && !defined(VPV_STATS)', '#define VUC_V(f) f', '#else', '#define VUC_V(f) 0', '#endif',
           'static __attribute__((unused)) int vuc_fns(uint32_t hash, int pc, int (**v)(void), int (**f)(void))', '{', '  switch (hash) {']
    for h in rows:   # vuc_dispatch's order
        cs = ' '.join(f'case {pc}: *v = {"VUC_V(" + v + ")" if v else "0"}; *f = {f}; return 1;' for pc, v, f in rows[h])
        fns.append(f'  case {h}u: switch (pc) {{ {cs} }} return 0;')
    fns += ['  }', '  return 0;', '}']
    # the output
    endif = len(L) - 1
    while L[endif] != '#endif':
        endif -= 1
    if not L[0].startswith('/*') and 'GEN_VU_H' not in L[1]:
        die('no GEN_VU_H guard at the top')
    hook = ['#ifdef VU_L41_P3_HOOK   /* gl41 P3 (tools/gl41_genvu.py; runtime/vu0.h): the switch and probe (c); without it gl39\'s form */',
            f'static int {FN}_p3(void);', '#endif']
    tail = ['/* ---- gl41 (attic/gl41s, tools/gl41_genvu.py) ---- */', '#ifdef VU_L41_P3_HOOK', '#define GEN_VU_P3 1']
    tail += P3[FN][2]
    tail += ['#define VUC_P3_N 1',
             f'static int (*const vuc_p3_pairs[VUC_P3_N][2])(void) = {{ {{ {FN}, {FN}_p3 }} }};   /* (gl39 form, P3 form) */',
             '#endif'] + fns
    O = L[:endif]
    for f in sorted(FNS, key=lambda f: -P3[f][0]):   # from the last function up: the line numbers above stay
        i0 = P3[f][0]
        hin = ['#ifdef VU_L41_P3_HOOK', f'  VU_L41_P3_HOOK({FN}_p3);', '#endif']
        O = O[:i0 + 2] + hin + O[i0 + 2:]
    i0 = min(P3[f][0] for f in FNS)
    O = O[:i0] + hook + O[i0:] + tail + L[endif:]
    open(dst, 'w').write('\n'.join(O))
    print(f'wrote {dst}: {len(O)} lines ({len(O) - len(L):+d}), md5 {hashlib.md5(chr(10).join(O).encode()).hexdigest()}')


main()
