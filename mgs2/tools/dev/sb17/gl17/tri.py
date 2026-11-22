# tri.py <A> <B> <C> <vb...>: pixel sets dAB, dAC, dBC for three shot folders (A = gl17, B = gl16, C = effects-on master)
import sys, struct
def rd(p):
    d = open(p, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]; w, h = struct.unpack_from('<ii', d, 18); bp = struct.unpack_from('<H', d, 28)[0] // 8
    row = (w * bp + 3) & ~3
    return [d[off + y * row + x * bp: off + y * row + x * bp + 3] for y in range(abs(h)) for x in range(w)]
A, B, C = sys.argv[1:4]
for vb in sys.argv[4:]:
    a, b, c = (rd('%s/shot_%s.bmp' % (D, vb)) for D in (A, B, C))
    ab = {i for i in range(len(a)) if a[i] != b[i]}; ac = {i for i in range(len(a)) if a[i] != c[i]}; bc = {i for i in range(len(a)) if b[i] != c[i]}
    print('%s: gl17-gl16 %d, gl17-on %d, gl16-on %d; (gl17-gl16) inside (gl16-on): %d; (gl17-on) inside (gl16-on): %d; both: %d; gl17 equal to one of them: %d of %d'
          % (vb, len(ab), len(ac), len(bc), len(ab & bc), len(ac & bc), len(ab & ac), sum(1 for i in range(len(a)) if a[i] == b[i] or a[i] == c[i]), len(a)))
