import sys
p = sys.argv[1] + '/psp/xl_psp.c'; s = open(p).read()
def rep(old, new):
    global s
    c = s.count(old); assert c == 1, (c, old[:90]); s = s.replace(old, new)
rep("""    if (k != mq_turn_k) { mq_drain(); if (mq_th >= 0) { mq_turn_k = k; mq_self = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0; } } }""",
"""    if (k != mq_turn_k) { mq_drain(); if (mq_th >= 0) { mq_turn_k = k; mq_self = k; mq_turns++; mq_thin_due = 0; mq_walk_due = 0;
        flip_on = flip_sel(vb); } } }   /* gl27c: mode 3's turns choose the flip wait too */""")
rep("""  flip_on = flip_cfg != 0;""", """  flip_on = flip_cfg == 1 || flip_cfg == 2;   /* gl27c: as flip_sel before the turns (other values: off, as printed) */""")
open(p, 'w').write(s); print('ok')
