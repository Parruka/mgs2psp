# stepcmp44.py <steps.csv A> <steps.csv B> (gl44 block A): the timed steps (vb 24,001 on) of two emulator runs of the same package side
# by side, by turn of 100 vblanks and in all: mean step, main parts; B - A.  On the emulator the step times are its model of the PSP's
# time [C emu], not the PSP's.
import sys, csv
def load(p):
    r = {}
    for d in csv.DictReader(open(p)):
        r[int(d['vb'])] = d
    return r
A, B = load(sys.argv[1]), load(sys.argv[2])
vb = sorted(set(A) & set(B))
def mean(R, k, vs): return sum(int(R[v][k]) for v in vs) / max(1, len(vs)) / 1000.0
print('stepcmp44: %d common vblanks (%d to %d); ms a step, B - A' % (len(vb), vb[0], vb[-1]))
for k in ('step_us', 'iop_us', 'dg_us', 'gl_us', 'gw_us', 'disc_us'):
    print('  %-8s A %8.3f  B %8.3f  B-A %+7.3f' % (k, mean(A, k, vb), mean(B, k, vb), mean(B, k, vb) - mean(A, k, vb)))
d = [(int(B[v]['step_us']) - int(A[v]['step_us'])) for v in vb]
print('  per step B-A (us): mean %+.1f, max %+d, min %+d; steps where B is over A by more than 1 ms: %d' % (sum(d) / len(d), max(d), min(d), sum(1 for x in d if x > 1000)))
same = sum(1 for v in vb if A[v]['step_us'] == B[v]['step_us']); print('  steps with the same time to the microsecond: %d of %d' % (same, len(vb)))
