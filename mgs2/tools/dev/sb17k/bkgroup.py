# bkgroup.py <tag>...: bkread.py's logic and picture rows summed in groups by file name (task #160)
import sys, re, importlib.util, collections, io, contextlib
import os
spec = importlib.util.spec_from_file_location('bk', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bkread.py'))
src = open(spec.origin).read().replace('show(sys.argv[1:])', '')
bk = {}; exec(compile(src, 'bkread', 'exec'), bk)
WEATHER = r'^(rain.*|d_splash_.*|ft_splsh|line_sph|body_sph|charaspread|ripple|wave2?|wind_man|windnois|puddle|win_rain|thund_flash)\.c$'
GROUPS = [('weather: rain, drops, splashes, sea, wind', WEATHER), ('guards', r'^(watcher|attacker|z_man)\.c$'),
          ('Snake', r'^(raiden|pl_arm|pl_global)\.c$'), ('ropes', r'^ropemain2?\.c$'),
          ('lights', r'^(fade_lgt|c_light|bonbori_.*|brk_spotlgt|lit_man|george_bonbori)\.c$'), ('doors', r'^wt_door\.c$'),
          ('trail and blur', r'^(afterimage|blur)\.c$')]
def grp(name):
    for g, rx in GROUPS:
        if re.match(rx, name): return g
    return 'the rest'
tags = sys.argv[1:]; out = collections.OrderedDict()
for t in tags:
    part, R = bk['rows'](t); acc = collections.defaultdict(float)
    for (kind, label), v in R.items():
        if kind == 'logic':
            nm = label.split(' ', 1)[1]; acc[('logic', grp(nm))] += v
        else:
            if label.startswith('people'): g = 'people (P06, all passes)'
            elif label.startswith('rooms'): g = 'rooms and ship (df0)'
            elif label.startswith('frame'): g = 'frame work'
            else:
                ent, own = label.split(' / '); kind2 = ent.split()[0]
                g = ('effects drawn since Stage A' if kind2 in ('fans', 'strips') else 'effects C4 (new)') + ': ' + grp(own if own.endswith('.c') else 'x')
            acc[('picture', g)] += v
    out[t] = acc
keys = []
for t in tags:
    for k in out[t]:
        if k not in keys: keys.append(k)
keys.sort(key=lambda k: (k[0], -max(out[t].get(k, 0) for t in tags)))
print('%-58s' % '' + ''.join('%10s' % t for t in tags))
for k in keys: print('%-58s' % (k[0] + ': ' + k[1]) + ''.join('%10.2f' % out[t].get(k, 0) for t in tags))
