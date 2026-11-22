#!/usr/bin/env python3
"""pollset.py <handler list> <ipo_*.json ...>: the registers a poll must store (recomp/lean_plan.md step 1b), as regcache.py's RC_POLL_R and
RC_POLL_F.  A poll can run an interrupt handler or an alarm function; runtime/kernel.h copies the register file to a host KCtx, sets a0-a2,
sp and ra, runs the function and restores the copy (a thread switch only saves the file in the thread's host KCtx).  So the registers whose
value at a poll can reach the guest's memory are those a handler may read before writing them: the union of the handlers' IN sets (the
sets regcache.py saves with RC_IPO_SAVE, through their direct calls), less a0-a2, sp and ra.  The FPU registers: the union of their FIN.
<handler list>: lines "khandler: <kind> <hex address> vb <n>" (x86 builds with -DK_HANDLER_LOG) or bare hex addresses.  An address below
the stage image (0x1f1c00) is P<addr> of any unit; one above it S_<stage>_P<addr> of every stage table given (the union: conservative).
A handler with no sets in the tables is an error (its IN would be every register).  Prints the two lists and the handlers' sets."""
import json, re, sys
KSET = {4, 5, 6, 29, 31}
hs = set()
for ln in open(sys.argv[1]):
    m = re.match(r'^khandler: \w+ ([0-9a-f]+)', ln) or re.match(r'^([0-9a-fx]+)\s*$', ln.strip())
    if m: hs.add(int(m.group(1), 16))
T = {}
for f in sys.argv[2:]:
    for k, v in json.load(open(f)).items(): T.setdefault(k, []).append((f, v))
R, F = set(), set()
for a in sorted(hs):
    names = ['P%x' % a] if a < 0x1f1c00 else [k for k in T if re.fullmatch(r'S_\w+_P%x' % a, k)]
    got = [(n, f, v) for n in names for f, v in T.get(n, [])]
    if not got: sys.exit('pollset: handler %x has no sets in the tables given' % a)
    for n, f, v in got:
        R |= set(v[0]); F |= set(v[2])
        print('# %08x %s (%s): IN %s FIN %s' % (a, n, f.rsplit('/', 1)[-1], v[0], v[2]))
R -= KSET
print('RC_POLL_R=%s' % ','.join(str(x) for x in sorted(R)))
print('RC_POLL_F=%s' % ','.join(str(x) for x in sorted(F)))
