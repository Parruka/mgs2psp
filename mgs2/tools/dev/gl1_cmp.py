#!/usr/bin/env python3
"""gl1_cmp.py <psp log.txt> <x86 out.log>: the "picture: vb" lines of a psp/gl1.sh run against tools/dev/gl1_x86.sh's (renderer_notes
11.13 "3e", task #56): for each frame the draw buffer, the list digest and the frame digest must be equal; the batch, PRIM, texture and
dropped counts too.  The word counts may differ by BASE words (the memory lies elsewhere) and are only reported.  Also the first frame's
vblank and the "all" digest of the totals.  Exit 0 when everything that must be equal is."""
import re, sys

LINE = re.compile(r"^picture: vb (\d+) buffer (\d) list ([0-9a-f]{16}) frame ([0-9a-f]{16}): (\d+) words, (\d+) aux bytes, (\d+) batches, "
                  r"(\d+) PRIMs, (\d+) textures, (\d+) dropped")
FIRST = re.compile(r"^picture: the first frame begins at vb (\d+)")
ALL = re.compile(r"digests (\d+) \(vb (-?\d+)\.\.(-?\d+)\), all ([0-9a-f]{16})")


def read(path):
    frames, first, alls = {}, None, None
    with open(path, errors="replace") as f:
        for l in f:
            m = LINE.match(l)
            if m:
                g = m.groups()
                frames[int(g[0])] = dict(buf=g[1], list=g[2], frame=g[3], words=int(g[4]), aux=int(g[5]), batches=int(g[6]),
                                         prims=int(g[7]), tex=int(g[8]), dropped=int(g[9]))
                continue
            m = FIRST.match(l)
            if m and first is None:
                first = int(m.group(1))
            m = ALL.search(l)
            if m and l.startswith("picture:"):
                alls = (int(m.group(1)), m.group(4))
    return frames, first, alls


def main():
    a, af, aa = read(sys.argv[1])
    b, bf, ba = read(sys.argv[2])
    bad = 0
    print(f"first frame: PSP vb {af}, x86 vb {bf}" + ("" if af == bf else "  DIFFERENT"))
    bad += af != bf
    only_a, only_b = sorted(set(a) - set(b)), sorted(set(b) - set(a))
    if only_a or only_b:
        print(f"frames only in the PSP log: {only_a[:10]}{' ...' if len(only_a) > 10 else ''}; only in the x86 log: {only_b[:10]}"
              f"{' ...' if len(only_b) > 10 else ''}")
        bad += 1
    both = sorted(set(a) & set(b))
    wdiff = 0
    for vb in both:
        p, x = a[vb], b[vb]
        keys = [k for k in ("buf", "list", "frame", "aux", "batches", "prims", "tex", "dropped") if p[k] != x[k]]
        if keys:
            bad += 1
            if bad <= 20:
                print(f"vb {vb}: differs in {', '.join(keys)}: PSP {p} / x86 {x}")
        if p["words"] != x["words"]:
            wdiff += 1
    print(f"{len(both)} frames in both; {bad} differences; word counts differ in {wdiff} (BASE words)")
    print(f"totals: PSP {aa}, x86 {ba}" + ("" if aa == ba else "  DIFFERENT"))
    bad += aa != ba
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
