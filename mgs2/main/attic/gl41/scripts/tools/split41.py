"""split41.py order.txt deck_out w01a_out: a layout_sim order (region 0 main ELF, 1 w00a, 2 w01a) into the two routes' order files
(layout_order.py's form: region 0 and 1), as build_gl40/rt40b.sh did inline."""
import sys
L = [l.split() for l in open(sys.argv[1]) if l.strip()]
open(sys.argv[2], 'w').write(''.join('%s %s\n' % (r, n) for r, n in L if r in '01'))
open(sys.argv[3], 'w').write(''.join('%s %s\n' % ('1' if r == '2' else r, n) for r, n in L if r in '02'))
print(sys.argv[1], 'main', sum(1 for r, n in L if r == '0'), 'w00a', sum(1 for r, n in L if r == '1'), 'w01a', sum(1 for r, n in L if r == '2'))
