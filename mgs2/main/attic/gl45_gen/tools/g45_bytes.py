#!/usr/bin/env python3
"""gl45 batch 3: code bytes of PSP objects.  usage: g45_bytes.py <object> ... -> for each: all .text* bytes, the bytes of the
untouched copies (.text.unlikely.*: tools/gen_levers.py's NAME_s, cold), functions (sections) changed against the first object."""
import subprocess, sys, re
def secs(o):
    out = subprocess.run(["psp-objdump", "-h", o], capture_output=True, text=True).stdout
    return {m.group(1): int(m.group(2), 16) for m in re.finditer(r"^\s*\d+ (\.text\S*)\s+([0-9a-f]{8})", out, re.M)}
base = None
for o in sys.argv[1:]:
    s = secs(o); tot = sum(s.values()); cold = sum(v for k, v in s.items() if k.startswith(".text.unlikely"))
    if base is None: base = s
    grew = sum(1 for k, v in s.items() if not k.startswith(".text.unlikely") and base.get(k) is not None and v > base[k])
    shr = sum(1 for k, v in s.items() if not k.startswith(".text.unlikely") and base.get(k) is not None and v < base[k])
    print("%-22s code %8d bytes (%+7d, %+5.2f%%): hot/normal sections %8d (%+7d), untouched copies %7d; functions larger %d, smaller %d" % (
        o.split("/")[-1], tot, tot - sum(base.values()), 100.0 * (tot - sum(base.values())) / sum(base.values()), tot - cold, tot - cold - sum(base.values()), cold, grew, shr))
