#!/usr/bin/env python3
"""split32.py ELF FRAMES GROUPS VERTS LABEL A B [--all]   (gl32 plan step 1; run with python3 -I)
The cost of drawing the ship's groups as the difference of two PspProf windows of the same vblanks with the same game state [C model]:
B (arm 3: stubbed, recorded, drawn) - A (arm 4: stubbed, not drawn), per function of the main ELF, grouped by what the function does
for a ship-mesh group; per picture (FRAMES pictures in the window), per group (GROUPS a picture) and, for the per-vertex parts, per
vertex (VERTS a picture).  ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000: a model, not a PSP time."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, fr, ng, nvx, label, pa, pb = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), sys.argv[5], sys.argv[6], sys.argv[7]
S = []
for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
def load(p):
    F = collections.defaultdict(lambda: [0, 0, 0])
    for ln in open(p):
        if ln.startswith('#'): continue
        f = ln.split()
        if len(f) < 4: continue
        pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
        n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else "(not in the ELF's functions)"
        x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
    return F
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
A, B = load(pa), load(pb)
D = {n: [(B.get(n, [0, 0, 0])[j] - A.get(n, [0, 0, 0])[j]) / fr for j in range(3)] for n in set(A) | set(B)}
CAT = [
 ('1 waits (the harness waiting for the ME thread or the GE; not work)', ['hw']),
 ('2 main CPU: the stub (records)', ['rt_nat_p04', 'P1214a0']),
 ('3 hand-off: the copy plan (extents) and the copy into the arena', ['xm_plan', 'xc_merge', 'xc_run', 'xc_add', 'xc_walk', 'me_memcpy', 'xm_fill', 'xm_lay', 'xm_nat_room', 'mq_rec', 'mp_plan', 'xme_entry', 'xc_tag', 'xc_nat']),
 ('4 front end: record -> translator (the block, material and header into the VU model, the streams\' descriptors)', ['xm_walk', 'xl_p04_core', 'ud_ok', 'ud_new', 'ud_eager', 'ud_kind', 'own_fill', 'lo_touch', 'lo_src', 'xl_vif1_span']),
 ('5 front end: the call (MSCAL), its A+D packet into the GS model, the GS state and texture tracking', ['call', 'me_memset', 'gif_qw', 'gs_write', 'gf_reg', 'gf_unknown', 'gf_drew', 'put', 'put_sym', 'texture', 'rows', 'touch', 'drew_ctx', 'gu_drawn', 'gu_clut_load', 'gu_clut_id', 'gu_pages_sig', 'kick_vm', 'ent_index', 'vmq', 'vm_one', 'ud_vec', 'ud_k', 'gu_tex', 'gu_reg', 'gu_write', 'me_memcmp', 'gf_setup', 'ec_slot']),
 ('6 converter, per group: the call\'s set-up, matrix, GS state, texture look-up, batch state', ['gl_call', 'xm_call.part.0', 'xm_call', 'xc_gs', 'vc_xform', 'gl_tex', 'tk_get_um', 'tk_get', 'tk_pal', 'add_tex', 'xc_xform', 'xc_state', 'xc_twopass', 'gb_xform_in', 'gb_room', 'gb_left', 'tk_find', 'tk_key', 'gb_tex', 'gl_conv_entry']),
 ('7 converter, off-screen cut (the box\'s 8 corners)', ['xc_off.constprop.0', 'xc_off', 'xc_df0_off']),
 ('8 converter, depth range (the box test; a scan of every vertex when the box cannot tell)', ['xc_zr_df0d.isra.0', 'xc_zr_df0d', 'xc_zr_df0', 'xc_df0_zin', 'xc_zr_end']),
 ('9 converter, per vertex: positions and box (vc_df0_pos), colours / UVs / flags (vc_df0_att), strips (vc_strips)', ['vc_df0_pos', 'vc_df0_att', 'vc_strips']),
 ('10 converter: xc_call itself (the call\'s body: index list, the blob, the calls above)', ['xc_call']),
 ('11 list build: the batches into the GE list (gelist.c), the shadow pairing', ['gx_batch', 'gx_frame', 'xf_setup', 'xf_draw', 'xf_rest', 'dz_pair', 'dz_batch', 'dz_tris', 'dz_sortk', 'dz_sortt', 'dz_tin', 'dz_vin', 'dz_mv', 'dz_near', 'dz_key', 'dz_uniq', 'dz_tuniq', 'dz_tmp', 'set_tex', 'set_blend', 'xm_list', 'gl_end', 'gl_list', 'blend', 'state']),
]
used = set(); tot = [0, 0, 0]
for n in D:
    for j in range(3): tot[j] += D[n][j]
print('%s: B %s - A %s over %d pictures; %.0f groups, %.0f vertices a picture' % (label, pb, pa, fr, ng, nvx))
print('all: instructions %+.0f, I-misses %+.0f, D-misses %+.0f = %+.2f ms a picture [C model]; per group %.0f instructions, %.1f I-misses, %.1f D-misses = %.1f us' % (
    tot[0], tot[1], tot[2], ms(tot), tot[0] / ng, tot[1] / ng, tot[2] / ng, 1000 * ms(tot) / ng))
rows = []
for name, fs in CAT:
    t = [0, 0, 0]; parts = []
    for f in fs:
        if f in D and f not in used:
            used.add(f); d = D[f]
            for j in range(3): t[j] += d[j]
            if abs(ms(d)) >= 0.02: parts.append((ms(d), f, d))
    rows.append((name, t, parts))
rest = [0, 0, 0]; rp = []
for n in D:
    if n in used: continue
    for j in range(3): rest[j] += D[n][j]
    if abs(ms(D[n])) >= 0.10: rp.append((ms(D[n]), n, D[n]))
rows.append(('12 everything else (each under its name when 0.10 ms or more)', rest, rp))
for name, t, parts in rows:
    print('\n%-100s %+7.2f ms a picture (%4.1f%% of the work without the waits)  per group: %5.0f instr %5.1f I-miss %5.1f D-miss = %5.1f us' % (
        name, ms(t), 100 * ms(t) / (ms(tot) - ms(D.get('hw', [0, 0, 0]))), t[0] / ng, t[1] / ng, t[2] / ng, 1000 * ms(t) / ng))
    for m, f, d in sorted(parts, key=lambda x: -abs(x[0])):
        print('      %-28s %+7.3f ms  (instr %+9.0f, I-miss %+7.0f, D-miss %+7.0f a picture; a group %6.0f / %5.1f / %5.1f; a vertex %5.1f instr)' % (
            f[:28], m, d[0], d[1], d[2], d[0] / ng, d[1] / ng, d[2] / ng, d[0] / nvx))
