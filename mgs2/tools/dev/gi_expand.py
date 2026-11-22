"""gi_expand.py <gi counts> <PspProf per-pc file> <main elf> <stage ovl elf> [top] [logic|draw|all]: PSP instructions per guest instruction, by function.
gi counts: boot.c GEN_GI_COUNT (MGS2_GI) output "pc count dg_count" for a window; the PspProf file must cover the same window (both per
100 steps).  Host addresses are mapped to symbols (psp-nm); a generated function P<addr> / S_<stage>_P<addr> (with .cold parts) is set
against the guest instructions counted between its address and the next function's.  Prints the totals, the ratio for the game code,
the host cost outside the game code (runtime, VU0, libraries), and the top functions by PSP cost.  logic (default): the guest count
outside the draw pipeline, for a "busy" profile (prof A B: logic only); all: both, for a full-step profile (prof A B all)."""
import sys, re, bisect, subprocess, collections
gi, prof, elf, ovl = sys.argv[1:5]; top = int(sys.argv[5]) if len(sys.argv) > 5 else 40; part = sys.argv[6] if len(sys.argv) > 6 else 'logic'
g = {}
for l in open(gi):
    p, a, b = l.split(); g[int(p, 16)] = {'logic': int(a), 'draw': int(b)}.get(part, int(a) + int(b))
syms = []
for f in (elf, ovl):
    for l in subprocess.run(['psp-nm', '-n', f], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tT': syms.append((int(p[0], 16), p[2]))
syms.sort(); sa = [s[0] for s in syms]
def sym(a):
    i = bisect.bisect_right(sa, a) - 1
    return syms[i][1] if i >= 0 else '?'
FN = re.compile(r'^(?:S_\w+?_)?P([0-9a-f]{5,6})(?:\.\w+)*$')
host = collections.defaultdict(lambda: [0, 0, 0]); tot = [0, 0, 0]
for l in open(prof):
    if l[0] == '#': continue
    p = l.split(); a = int(p[0], 16); c = (int(p[1]), int(p[2]), int(p[3]))
    s = sym(a); m = FN.match(s); k = int(m.group(1), 16) if m else s
    for i in range(3): host[k][i] += c[i]; tot[i] += c[i]
starts = sorted(set(k for k in host if isinstance(k, int)) | set())
# guest function starts: every P<addr> symbol in either file (also those the profile never hit)
allst = sorted(set(int(FN.match(s).group(1), 16) for _, s in syms if FN.match(s)))
gf = collections.Counter()
for pc, n in g.items():
    i = bisect.bisect_right(allst, pc) - 1
    gf[allst[i] if i >= 0 else -1] += n
cost = lambda c: (c[0] + 125 * (c[1] + c[2])) / 333000 / 100
gsum = sum(gf.values()); gh = [0, 0, 0]
for k, c in host.items():
    if isinstance(k, int):
        for i in range(3): gh[i] += c[i]
print('window: %.3f M guest instr per step; PSP %.2f M instr per step, %.1f ms model' % (gsum / 100e6, tot[0] / 100e6, cost(tot)))
print('game code: %.2f M PSP instr (%.2f per guest instr), %.1f k I-miss, %.1f k D-miss, %.1f ms model' % (gh[0] / 100e6, gh[0] / max(gsum, 1), gh[1] / 100e3, gh[2] / 100e3, cost(gh)))
other = sorted(((cost(c), k, c) for k, c in host.items() if not isinstance(k, int)), reverse=True)
print('outside the game code: %.1f ms model; largest:' % sum(o[0] for o in other))
for ms, k, c in other[:15]: print('   %-28s %6.2f ms  %7.0f k instr' % (k, ms, c[0] / 100e3))
rows = sorted(((cost(host[k]), k) for k in host if isinstance(k, int)), reverse=True)
print('top game functions: ms model, PSP instr per step, guest instr per step, ratio, I-miss, D-miss per step')
for ms, k in rows[:top]:
    c = host[k]; n = gf.get(k, 0)
    print('  P%06x %6.2f ms %8.0f %7.0f %6.1f %6.0f %6.0f' % (k, ms, c[0] / 100, n / 100, c[0] / n if n else float('nan'), c[1] / 100, c[2] / 100))
