# mk_rd42in.py <rd42pc.py> <emulator log.txt of the gl42in run> <out rd42in.py> <unused> [extra EXPNOP vbs, comma separated]
# Makes the indoor reader from gl42's reader: the schedule extended by the cycles' wrap (t74 = t0 ...) to vb 37,800, the stretches
# 'to the door' and 'indoor' added, the expected hash / digest / content fingerprints taken from the emulator run of the same package.
import sys, re, hashlib
src, emu, out, door = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
s = open(src, encoding='utf-8').read()
la = open(emu, encoding='latin-1').read().splitlines()
def md(x): return hashlib.md5('\n'.join(x).encode()).hexdigest()[:12]
H, D, C = [], [], []
for l in la:
    m = re.match(r'hash: vb (\d+) ee ([0-9a-f]+) iop ([0-9a-f]+)', l)
    if m: H.append('%s %s %s' % m.groups()); continue
    m = re.match(r'picture: vb (\d+) buffer \d+ list ([0-9a-f]+) frame ([0-9a-f]+):.*?; dz (.*)$', l)
    if m: D.append(('%s %s %s' % m.groups()[:3], m.group(4).strip())); continue
    m = re.match(r'picture: vb (\d+) content ([0-9a-f]+)$', l)
    if m: C.append('%s %s' % m.groups())
def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a, s.count(a)); s = s.replace(a, b)
rep("EH, NH = '70686ac12d16', 62", "EH, NH = '%s', %d" % (md(H), len(H)))
rep("ED, EDZ, ND = '7042ca2d2d2b', '88c7ad1e6583', 61", "ED, EDZ, ND = '%s', '%s', %d" % (md([a for a, _ in D]), md([a + ' ' + z for a, z in D]), len(D)))
rep("EC, NC = 'a1db01cc2e5d', 61", "EC, NC = '%s', %d" % (md(C), len(C)))
import subprocess, os
sched = subprocess.run(['python3', '-I', os.path.join(os.path.dirname(os.path.abspath(src)), '..', 'tools', 'sched42in.py'), 'py'], capture_output=True, text=True, check=True).stdout.strip()
i = s.index('SCHED = ['); j = s.index('\n', i)
s = s[:i] + 'SCHED = ' + sched + '   # (turn, vb, kind, NGE digit, bound kind, digest off, XP, scene, stretch, F1): tools/sched42in.py py' + s[j:]
rep("S = {vb: (t, k, ng, bk, d, xp, sc, st) for (t, vb, k, ng, bk, d, xp, sc, st) in SCHED}",
    "F1X = {vb: f1 for (t, vb, k, ng, bk, d, xp, sc, st, f1) in SCHED}\nS = {vb: (t, k, ng, bk, d, xp, sc, st) for (t, vb, k, ng, bk, d, xp, sc, st, f1) in SCHED}")
rep("KN = {'A': 'K-A all drawn',", "KN = {'D': 'past the cycle', 'A': 'K-A all drawn',")
rep("BK = {'A': 0,", "BK = {'D': 0, 'A': 0,")
rep("x.get('f1') != 1): bad.append((vb, 'kind/F1'))", "x.get('f1') != F1X[vb]): bad.append((vb, 'kind/F1'))")
rep("def dg(vb): return 'none' if vb + 99 < 29490 else 'off' if S[vb][4] else 'on'",
    "def dg(vb): return 'none' if vb + 99 < 29490 else ('on' if vb in (32900, 35900) else 'none') if vb >= 31400 else 'off' if S[vb][4] else 'on'   # gl42in: indoors digests only in t89, t119")
if len(sys.argv) > 5 and sys.argv[5]: rep("EXPNOP = [25100, 25300, 25700]", "EXPNOP = [25100, 25300, 25700, %s]" % sys.argv[5])
rep("        for k in 'AN2R7':\n            rs = rows(stretch, k, d)", "        for k in 'AN2R7D':\n            rs = rows(stretch, k, d)")
rep("(the pictures\\' lists in the digest-on turns t55, t73)", "(the pictures\\' lists in the digest-on turns t55, t73, t89, t119)")
rep("for stretch in ('deck', 'rain wait', 'walk'):", "for stretch in ('deck', 'rain wait', 'walk', 'to the door', 'indoor', 'indoor, past the cycle'):", 2)
rep("# rd42pc.py: read an MGS2 harness gl42 PSP run on the PC.   python -I rd42pc.py <LOGS\\gl42 folder>",
    "# rd42in.py: read an MGS2 harness gl42in PSP run (gl42 with the indoor settings file, run to vb 37,800) on the PC.   python -I rd42in.py <LOGS\\gl42in folder>\n"
    "# = rd42pc.py with the indoor schedule to vb 37,800 (tools/sched42in.py), the stretches 'to the door', 'indoor' and 'indoor, past the cycle', and this run's expected fingerprints.")
s += ("print('== INDOOR BY TURN (vb, kind, who | step main ME P | logic pipe gew):')\n"
      "for vb in sorted(S):\n"
      "    x = T.get(vb, {})\n"
      "    if vb >= 31400 and x.get('mean') is not None: print('  %d t%-3d %-17s %-4s | %s %s %s %s | %s %s %s' % (vb, S[vb][0], KN[S[vb][1]], x.get('who', '?'), f(x.get('mean')), f(x.get('main')), f(x.get('me_busy')), f(x.get('P')), f(x.get('logic')), f(x.get('pipe')), f(x.get('gew'))))\n"
      "for s in la:\n"
      "    if s.startswith(('stage ', 'load ', 'overlay')) or 'w01a' in s[:60]: print('  ', s[:200])\n")
open(out, 'w', encoding='utf-8').write(s)
print('wrote', out, 'hash lines', len(H), md(H), 'digests', len(D), 'content', len(C))
