# xme_me_chk.py <psp-objdump -d of ge_xme_me.o> (task #180 step 4d-4, me_step4_notes.md 4d): the ME's side of the picture loop
# (psp/xme_me.c) holds the instructions tools/dev/me14/mecheck.py rule 4 keeps out of the picture code, and only these: Count reads
# (mfc0 $9), cache 0x14 (index write back and invalidate) and 0x19 (hit invalidate), sync, HALT (.word 0x70000000), ctc1 to $31 (the
# FPU rule) and one $gp write (or gp,gp,at: ME_CALLP's top bit).  No other COP0 access, no other cache op, no cfc1, no COP2 or VFPU,
# no syscall, break or trap, no ll/sc, no eret, no jalr and no jr but jr ra (the one call it makes, xme_run, is a jal).  Prints what
# it finds and "xme_me_chk: PASS" (exit 0) or "xme_me_chk: FAIL (n)".  Run it with python3 -I.
import sys, re
bad = []; n = 0; gpw = 0; seen = {}; cw = 0   # gl19: one Count start (mtc0 zero,$9) allowed, in xme_me_main
STORES = {'sb', 'sh', 'sw', 'swl', 'swr', 'swc1', 'sdc1'}
NOWRITE = STORES | {'beq', 'bne', 'beql', 'bnel', 'blez', 'bgtz', 'bltz', 'bgez', 'blezl', 'bgtzl', 'bltzl', 'bgezl', 'beqz', 'bnez',
                    'beqzl', 'bnezl', 'b', 'jr', 'j', 'jal', 'mtc1', 'ctc1', 'cache', 'sync', 'mult', 'multu', 'div', 'divu', 'madd',
                    'maddu', 'msub', 'msubu', 'mthi', 'mtlo', 'nop'}
TRAP = re.compile(r'^(break|sdbbp|syscall|teqi?|tnei?|tgei?u?|tlti?u?|tgeu|tltu|eret|deret|ll|sc|jalr|mtc0|cfc1|mfic|mtic|halt|'
                  r'mtc2|mfc2|ctc2|cfc2|lwc2|swc2|lv\.|sv\.|lvl|lvr|svl|svr|mtv|mfv|mtvc|mfvc|bvf|bvt|v)')
for line in open(sys.argv[1]):
    m = re.match(r'^\s+([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)\s*(.*)$', line.rstrip('\n'))
    if not m:
        continue
    n += 1
    word, mn, ops = m.group(2), m.group(3), m.group(4).split('#')[0].strip()
    o = [x.strip() for x in ops.split(',')] if ops else []
    seen[mn] = seen.get(mn, 0) + 1
    if mn == '.word':
        if word != '70000000':
            bad.append('a data word in the code: %s' % line.strip())
        continue
    if mn == 'mfc0':
        if o[1:2] != ['$9']:
            bad.append('a COP0 read other than Count: %s' % line.strip())
        continue
    if mn == 'mtc0':
        cw += 1
        if o[:2] != ['zero', '$9']:
            bad.append('a COP0 write other than the Count start: %s' % line.strip())
        continue
    if mn == 'ctc1':
        if o[1:2] != ['$31']:
            bad.append('ctc1 to other than $31: %s' % line.strip())
        continue
    if mn == 'cache':
        if o[0] not in ('0x14', '0x19'):
            bad.append('a cache op other than 0x14 / 0x19: %s' % line.strip())
        continue
    if mn == 'jr':
        if o != ['ra']:
            bad.append('jr through a register other than ra: %s' % line.strip())
        continue
    if TRAP.match(mn) and mn not in ('vsync',):
        bad.append('forbidden: %s' % line.strip())
        continue
    if o and o[0] == 'gp' and mn not in NOWRITE:
        gpw += 1
        if not (mn == 'or' and o == ['gp', 'gp', 'at']):
            bad.append('a $gp write other than ME_CALLP\'s: %s' % line.strip())
if cw != 1:
    bad.append('%d Count starts (one expected: mtc0 zero,$9)' % cw)
if gpw != 1:
    bad.append('%d $gp writes (one expected: or gp,gp,at)' % gpw)
print('xme_me_chk: %d instructions; %s' % (n, ', '.join('%s %d' % (k, seen[k]) for k in sorted(seen))))
for b in bad:
    print('PROBLEM ' + b)
print('xme_me_chk: ' + ('PASS' if not bad else 'FAIL (%d)' % len(bad)))
sys.exit(1 if bad else 0)
