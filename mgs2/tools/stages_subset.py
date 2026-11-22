#!/usr/bin/env python3
"""stages_subset.py [--ovl] <stages.h> <out.h> <stage> [<stage> ...]: the stage table (recomp output stages.h) cut down to the named stage
images, for builds that carry only the boot path (the PSP timing harness).  A stage the game loads that is not in the table is reported by
boot/dispatch.h as at any untranslated image.  --ovl: the stages' code is not linked in (boot/dispatch.h STAGE_OVL: each stage is an
overlay file loaded on demand, psp/Makefile), so the table names no stage functions."""
import re, sys
ovl = sys.argv[1] == '--ovl'
if ovl: del sys.argv[1]
src, out, keep = sys.argv[1], sys.argv[2], set(sys.argv[3:])
s = open(src).read()
blk = re.compile(r'#ifndef STAGES_EXTERN\n#include "gen_s_([a-z0-9_]+)\.h"\n#else\n[^\n]*\n#endif\n')
s = blk.sub(lambda m: m.group(0) if m.group(1) in keep and not ovl else '', s)
if ovl: s = re.sub(r'S_[a-z0-9_]+_init, S_[a-z0-9_]+_call \}', '0, 0 }', s)
ent = re.compile(r'^  \{ "([a-z0-9_]+)", .*\n', re.M)
s = ent.sub(lambda m: m.group(0) if m.group(1) in keep else '', s)
found = set(re.findall(r'^  \{ "([a-z0-9_]+)"', s, re.M))
if found != keep: raise SystemExit('stages_subset: not in %s: %s' % (src, ' '.join(sorted(keep - found))))
open(out, 'w').write(s)
