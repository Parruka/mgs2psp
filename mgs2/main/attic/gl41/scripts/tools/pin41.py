"""pin41.py layout_elf.ld PIN REF OFF: gl41s's placement rule for P3's copy (attic/gl41s/results.md section 2) written into a layout file
made by layout_order.py: '_l41_f = .;' just before REF's line, and at the end of the ordered list (before the closing brace of the
section that holds REF's line) '. = . + ((_l41_f + OFF - .) & 8191);' and PIN's line.  PIN must not be in the ordered list already."""
import sys, re
p, pin, ref, off = sys.argv[1:5]
L = open(p).read().split('\n')
def line_of(name):
    rx = re.compile(r'[( ]\.text(\.hot|\.unlikely|\.startup)?\.' + re.escape(name) + r'[ )]')
    return [i for i, l in enumerate(L) if rx.search(l)]
assert not line_of(pin), 'the pinned function is in the order already'
r = line_of(ref); assert len(r) == 1, ('ref lines', r)
obj = L[r[0]].strip().split('(')[0]
i = r[0]; L.insert(i, '    _l41_f = .;')
j = i + 1
while L[j].strip() != '}': j += 1
L[j:j] = ['    . = . + ((_l41_f + %s - .) & 8191);' % off,
          '    %s(.text.%s .text.hot.%s .text.unlikely.%s .text.startup.%s)' % (obj, pin, pin, pin, pin)]
open(p, 'w').write('\n'.join(L))
print(p, ': _l41_f before', ref, '(line %d);' % (i + 1), pin, 'at the list\'s end (line %d), object' % (j + 2), obj)
