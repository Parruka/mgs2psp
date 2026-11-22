# listdiff.py <log a> <log b>: the frames whose GE list digest differs, with both lines
import re, sys
def pics(p):
    d = {}
    for l in open(p, errors='replace'):
        if l.startswith('picture: vb ') and ' list ' in l:
            vb = int(l.split()[2]); d[vb] = l.split(': ', 1)[1].strip()
    return d
a, b = pics(sys.argv[1]), pics(sys.argv[2])
n = 0
for v in sorted(set(a) & set(b)):
    if a[v].split(' frame ')[0] != b[v].split(' frame ')[0]:
        n += 1
        if n <= int(sys.argv[3]) if len(sys.argv) > 3 else 40: print(v, '|', a[v][:150], '|', b[v][:150])
print('total', n)
