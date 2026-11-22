# prims6.py <probe log> (python3 -I): the effect prims (first tag STCYCL cl 3/6/12 wl 1 + V4-32 to T+0x100) in an MGS2_SB_PROBE listing
# (probe_x86.sh; sb6: x1/out_p30850.log, the indoor route at vb 30,850 on the aft deck: 489 prims), counted by the pattern of their six
# tags (cl/wl:STCYCL word:UNPACK word without its count) and the entry of the call after them; renderer_notes 11.33 results
import re, sys, collections
L = [l for l in open(sys.argv[1], errors='replace') if l.startswith('tag ') or l.startswith('call ')]
T = []
for l in L:
    if l.startswith('tag '):
        p = l.split()
        T.append(('tag', int(p[1], 16), int(p[3]), int(p[5]), int(p[7], 16), int(p[9], 16), int(p[10], 16)))
    else:
        m = re.search(r'entry (\w+) top (\w+).* src (\w+)', l)
        T.append(('call', m.group(1), m.group(2), m.group(3)))
pat = collections.Counter(); ex = {}
i = 0; nprim = 0
while i < len(T):
    t = T[i]
    if t[0] == 'tag' and t[5] in (0x01000103, 0x01000106, 0x0100010c) and (t[6] & 0xff00ffff) == 0x6c008100:
        seq = []
        j = i
        while j < len(T) and T[j][0] == 'tag' and len(seq) < 6:
            u = T[j]; seq.append('%d/%d:%08x:%08x' % (u[2], u[3] if u[2] != 3 or True else 0, u[5], u[6] & 0xff00ffff)); j += 1
        call = T[j] if j < len(T) and T[j][0] == 'call' else None
        key = ' | '.join(seq) + ' -> ' + (call[1] if call else 'nocall')
        pat[key] += 1; ex.setdefault(key, (i, t, call)); nprim += 1
        i = j + (1 if call else 0); continue
    i += 1
print('prims', nprim)
for k, v in pat.most_common(): print(v, k, '  first', hex(ex[k][1][1]))
