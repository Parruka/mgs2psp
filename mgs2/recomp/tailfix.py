"""Post-process of a recomp.py output: `goto L<addr>;` to a label outside the current function = a tail jump (j) into another function ->
call it and return.  The callee is the recompiled function with that start address (NAME=start-end args of the spec) or the host extern X_<addr>.
usage: tailfix.py <gen.h> NAME=0xstart-0xend ..."""
import re, sys
p = sys.argv[1]; h = open(p).read()
names = {}
for a in sys.argv[2:]:
    m = re.match(r'(\w+)=0x([0-9a-f]+)-0x[0-9a-f]+$', a)
    if m: names[int(m.group(2), 16)] = m.group(1)
parts = re.split(r'(?=^static void \w+\(void\)\n\{)', h, flags=re.M)
ext = set(); out = []
for part in parts:
    labels = set(re.findall(r'\bL([0-9a-f]+):', part))
    def rep(m):
        t = m.group(1)
        if t in labels: return m.group(0)
        ta = int(t, 16); nm = names.get(ta)
        if nm is None: nm = 'X_%x' % ta; ext.add(ta)
        return '{ %s(); return; }' % nm
    out.append(re.sub(r'goto L([0-9a-f]+);', rep, part))
h = ''.join(out)
for t in sorted(ext):
    if 'static void X_%x(void);' % t not in h: h = 'static void X_%x(void);\n' % t + h
open(p, 'w').write(h); print('tailfix:', len(ext), 'externs', [hex(x) for x in sorted(ext)])
