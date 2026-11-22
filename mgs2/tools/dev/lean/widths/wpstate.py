"""wpstate.py (dev): load wpd.py's saved fixpoint (WPD_STATE) so a tool can look at single functions without the whole run: units are
read on demand and a function is parsed again with its fixpoint widths (rcdrv's emit, wpo2's parse)"""
import os, sys, pickle
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wpo2 as W
import wpd as P
D = W.D
_U = {}
class LazyFuncs(dict):
    """W.FUNCS for a tool: every translated name is in it (callsum / csum test membership); a function is parsed when first looked up"""
    def __init__(self, st): super().__init__(); self.st = st
    def __contains__(self, k): return k in self.st['UNIT']
    def __missing__(self, k):
        if k not in self.st['UNIT']: raise KeyError(k)
        return func(self.st, k)
    def get(self, k, d=None): return self[k] if k in self else d
def load(path):
    st = pickle.load(open(path, 'rb'))
    for k, v in st['W'].items():
        cur = getattr(W, k)
        if isinstance(cur, dict): cur.clear(); cur.update(v)
        elif isinstance(cur, set): cur.clear(); cur.update(v)
        elif isinstance(cur, list): cur[:] = v
        else: setattr(W, k, v)
    P.WIDE.clear(); P.WIDE.update(st['WIDE']); P.SUM.clear(); P.SUM.update(st['SUM']); P.SUMIND[0] = st['SUMIND']
    P.DRET.clear(); P.DRET.update(st['DRET']); P.DIND[0] = st['DIND']; P.DSIF[0] = st['DSIF']
    W.CG.clear(); W.CG.update(st['CG']); W.CG['US'] = _U
    W.FUNCS = LazyFuncs(st); P.W.FUNCS = W.FUNCS
    return st
def unit(u):
    if u not in _U: _U[u] = D.load(u)
    return _U[u]
def func(st, name, wide=None):
    """function name parsed with its fixpoint widths (or the given set), entered in W.FUNCS"""
    u = st['UNIT'][name]; U = unit(u); D.ctx(U)
    wr = set(P.WIDE[name] if wide is None else wide)
    F = W.parse(name, U, D.emit(U, name, wr), wr)
    if wide is None: dict.__setitem__(W.FUNCS, name, F)
    return F
