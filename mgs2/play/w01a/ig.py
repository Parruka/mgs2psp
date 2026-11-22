import sys
sys.path.insert(0, '/home/claude/play2')
from scen2 import *
F = 31800
def route(shift=0, wshift=0):
    P = []
    def at(vb, n, k): P.append((vb - F, n, k))
    w = wshift
    # walk 32300-34110 (port hallway laps, the aft corridor's west half), ending at z ~4900 standing
    at(32300, 52, R); at(32352, 150, D); at(32502, 150, U); at(32652, 150, D); at(32802, 150, U)
    at(32952 + w, 179, D); at(33133 + w, 80, R); at(33215 + w, 80, L); at(33297 + w, 150, U); at(33449 + w, 150, D)
    at(33601 + w, 150, U); at(33753 + w, 135, D)
    # crawl 34110-34510, punches and a shot 34530-34840, first person 34900-35500
    at(34110, 6, X); at(34150, 360, U); at(34510, 6, X)
    for i in range(6): at(34530 + 30 * i, 6, O)
    at(34720, 120, SQ)
    at(34900, 600, R1); at(34950, 100, L); at(35100, 100, R); at(35250, 100, U); at(35350, 100, D)
    # sneak 35520-36440: down to the aft corridor, east to x ~3000, wait round the corner from the guard's hallway
    at(35520, 110, D); at(35632, 130, R)
    # 36440: to the corner and into the starboard hallway (the guard turns and calls it in), then away and laps
    t = 36440 + shift
    at(t, 60, R); at(t + 62, 16, U)
    at(t + 382, 20, D); at(t + 404, 260, L); u = t + 666
    for i in range(10): at(u, 150, U if i % 2 == 0 else D); u += 152
    return P
out = []
for tag, sh, ws in (('ig1', 0, 0), ('ig2', -2, 0), ('ig3', 2, 0), ('ig4', 0, -2), ('ig5', 0, 2)):
    l = line(tag, F, 6100, route(sh, ws)); p = l.split(' '); p[2] = '37900:0'; out.append(' '.join(p))
open('/home/claude/play2/run/batch45.txt', 'w').write('\n'.join(out) + '\n')
