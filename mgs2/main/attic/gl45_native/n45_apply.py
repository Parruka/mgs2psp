#!/usr/bin/env python3
"""gl45 study: hook the native forms of boot/native45.h into a generated header.
usage: python3 -I n45_apply.py <gen_elf.h> [<out>]     (in place without <out>; refuses a header already hooked)
Each hooked translation `static void P<addr>(void)` becomes P<addr>_tr and the wrapper P<addr> (N45_WRAP) follows its body; native45.h is
included before the first generated function, native45.c after the last."""
import re, sys
FORMS = [  # address, bit name, native form
    ("11cd68", "N45_QNORM", "n45_qnorm"),
    ("165d78", "N45_MULM", "n45_mulm"),
    ("11c828", "N45_Q2M", "n45_q2m"),
    ("166090", "N45_ECS", "n45_ecs"),
    ("166108", "N45_ROTZ", "n45_rotz"),
    ("1661b0", "N45_ROTX", "n45_rotx"),
    ("166258", "N45_ROTY", "n45_roty"),
]
CLONES = [  # address, bit name, native wrapper name, label line of the translated text, text put after it in the second copy
    ("14b008", "N45_HZX", "n45_hzx", "L14b2f4:;", "N45_HZX_FF;   /* tools/n45_apply.py: boot/native45.h */"),
]
def main():
    src = sys.argv[1]; dst = sys.argv[2] if len(sys.argv) > 2 else src
    L = open(src).read().split("\n")
    if any("n45_apply.py" in l for l in L): sys.exit("n45_apply: already hooked: " + src)
    first = next(i for i, l in enumerate(L) if re.fullmatch(r"static void P[0-9a-f]+\(void\)", l) and L[i + 1].startswith("{"))
    ins = {}   # line index -> lines to put after it
    for addr, bit, nat in FORMS:
        at = [i for i, l in enumerate(L) if l == "static void P%s(void)" % addr and L[i + 1].startswith("{")]
        if len(at) != 1: sys.exit("n45_apply: P%s: %d definitions" % (addr, len(at)))
        i = at[0]; L[i] = "static void P%s_tr(void)" % addr
        e = next(j for j in range(i + 1, len(L)) if L[j] == "}")
        ins[e] = "void P%s(void) { N45_WRAP(%s, 0x%s, %s, P%s_tr); }   /* tools/n45_apply.py */" % (addr, bit, addr, nat, addr)
    for addr, bit, nat, lab, text in CLONES:
        at = [i for i, l in enumerate(L) if l == "static void P%s(void)" % addr and L[i + 1].startswith("{")]
        if len(at) != 1: sys.exit("n45_apply: P%s: %d definitions" % (addr, len(at)))
        i = at[0]; e = next(j for j in range(i + 1, len(L)) if L[j] == "}")
        body = L[i + 1:e + 1]
        if body.count(lab) != 1: sys.exit("n45_apply: P%s: label line %s %d times" % (addr, lab, body.count(lab)))
        k = body.index(lab); clone = ["static void P%s_n45(void)   /* tools/n45_apply.py: the translation's text with the hand-written part */" % addr] + body[:k + 1] + [text] + body[k + 1:]
        L[i] = "static void P%s_tr(void)" % addr
        ins[e] = "\n".join(clone + ["static int %s(void) { P%s_n45(); return 1; }" % (nat, addr),
                                    "void P%s(void) { N45_WRAP(%s, 0x%s, %s, P%s_tr); }   /* tools/n45_apply.py */" % (addr, bit, addr, nat, addr)])
    out = []
    for i, l in enumerate(L):
        if i == first: out.append('#include "native45.h"   /* tools/n45_apply.py */')
        out.append(l)
        if i in ins: out.append(ins[i])
    while out and out[-1] == "": out.pop()
    out.append('#include "native45.c"   /* tools/n45_apply.py: after every generated function */'); out.append("")
    open(dst, "w").write("\n".join(out))
    print("n45_apply: %s: %d forms hooked, %d with a second copy" % (dst, len(FORMS), len(CLONES)))
main()
