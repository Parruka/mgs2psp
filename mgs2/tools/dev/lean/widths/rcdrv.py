"""rcdrv.py (dev, lean_plan.md step 2): regcache.py's process() for any function of any unit with a chosen set of int64_t locals.
The units' inputs are the post-narrow sources (postnarrow.py: what process() sees in a v27 run) and the v27 run's interprocedural and
return sets (mk_headers.sh with the v27 options: <hdr>/ipo_<unit>.json, ret_<unit>.json, open/open_<unit>.txt)."""
import os, sys, json, re
for k, v in (('RC_SOUND', '1'), ('RC_XUNIT', '1'), ('RC_SPECIAL', '1'), ('RC_POLL_R', ''), ('RC_POLL_F', ''), ('RC_RETLIVE', '1'),
             ('RC_KEEPMASK', '1'), ('RC_PERREG', '1')):
    os.environ[k] = v
sys.path.insert(0, os.environ.get('RC_DIR', '/mnt/project-files/mgs2/recomp'))
import regcache as rc
UNITS = ['elf', 'init', 'r_title', 'r_tnk0', 'n_title', 'd00t', 'd01t', 'w00a', 'w01a']
PN = os.environ.get('PN', '/home/claude/lean/w2/pn')
HDR = os.environ.get('HDR', '/home/claude/lean/hP')
def hname(u): return 'gen_elf.h' if u == 'elf' else 'gen_s_%s.h' % u
class Unit:
    pass
def _ipo(path):
    return {k: tuple(set(x) for x in v) for k, v in json.load(open(path)).items()}
def load(u):
    U = Unit(); U.name = u
    src = open(os.path.join(PN, hname(u))).read()
    U.bodies = {m.group(1): m.group(2) for m in rc.FN.finditer(src)}
    U.order = list(U.bodies)
    U.ipo = _ipo(os.path.join(HDR, 'ipo_%s.json' % u))
    r = json.load(open(os.path.join(HDR, 'ret_%s.json' % u)))
    U.ret = {k: set(v[0]) for k, v in r.items()}; U.fret = {k: set(v[1]) for k, v in r.items()}
    U.open = set(open(os.path.join(HDR, 'open', 'open_%s.txt' % u)).read().split())
    return U
ELF_IPO = [None]
_WR = [set()]
IS_WIDE = rc.is_wide   # the original width decision (v27: a 64-bit construct anywhere -> every local int64_t)
rc.is_wide = lambda body: True
rc.wide_regs = lambda body: set(_WR[0])
def ctx(U):
    """regcache's globals for unit U (its sets; a stage's calls into the ELF use the ELF's, as RC_IPO_LOAD gives them)"""
    rc.IPO = U.ipo
    if U.name == 'elf': rc.XIPO = {}
    else:
        if ELF_IPO[0] is None: ELF_IPO[0] = _ipo(os.path.join(HDR, 'ipo_elf.json'))
        rc.XIPO = ELF_IPO[0]
    rc.RET = U.ret; rc.FRET = U.fret
def emit(U, name, wr):
    """process()'s output for function name of unit U (ctx(U) first) with int64_t locals for the registers in wr"""
    _WR[0] = set(wr); rc.CUR[0] = name; rc.PROBE[0] = None
    return rc.process(U.bodies[name])
