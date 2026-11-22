#!/usr/bin/env python3
"""split34.py ELF FRAMES GROUPS VERTS LABEL A B   (gl34: gl32's split32.py with the parts named after the split counters' stages of
ge/natsplit.h, so that a PSP log's 'nat split' line can be put next to the model part by part; run with python3 -I)
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
CAT = [   # (name, the split counters' stages it holds, functions by base name: GCC's suffixes .isra.N .constprop.N .part.N are cut)
 ('1 waits (the harness waiting for the ME thread or the GE; not work)', '-', ['hw']),
 ('2 main CPU: the stub (records)', '-', ['rt_nat_p04', 'P1214a0']),
 ('3 hand-off: the copy plan (extents) and the copy into the arena', "the line's hand-off (the ME's records ms)", ['xm_plan', 'xc_merge', 'xc_run', 'xc_add', 'xc_walk', 'me_memcpy', 'xm_fill', 'xm_lay', 'xm_nat_room', 'mq_rec', 'mp_plan', 'xme_entry', 'xc_tag', 'xc_nat']),
 ('4 the walk: the merge by key, the record into the front end\'s form, its pointers', 'walk and pointers + walk\'s end', ['xm_walk', 'xl_vif1_span']),
 ('5 front end, plain: the block, material and header into the VU model, the streams\' descriptors, the call, its A+D packet', 'register + A+D packet + call\'s rest + way back (plain turns)', ['xl_p04_core', 'ud_ok', 'ud_new', 'ud_eager', 'ud_kind', 'own_fill', 'lo_touch', 'lo_src', 'call', 'gif_qw', 'gs_write', 'gf_reg', 'gf_unknown', 'kick_vm', 'ent_index', 'vmq', 'vm_one', 'ud_vec', 'ud_k', 'gf_setup', 'ec_slot', 'me_memset', 'me_memcmp', 'xl_vm_need']),
 ('6 front end, fast: the same work in one function', 'register + A+D packet + call\'s rest + way back (fast turns)', ['xl_p04_fast']),
 ('7 GS model tracking: the CLUT loads, the drawn areas, the texture pages', 'inside A+D packet (the CLUT load) + drawn areas', ['gu_clut_load', 'gu_clut_id', 'gu_pages_sig', 'rows', 'touch', 'drew_ctx', 'gu_drawn', 'gf_drew', 'gu_tex', 'gu_reg', 'gu_write', 'texture', 'settle', 'ld_now', 'clut_new', 'st3']),
 ('8 converter, per group: the call\'s set-up, GS state, texture look-up, batch state', 'host call and GS state + texture + GE transform and batch data + into the frame', ['gl_call', 'xm_call', 'xc_gs', 'gl_tex', 'tk_get_um', 'tk_get', 'tk_pal', 'add_tex', 'xc_xform', 'xc_state', 'xc_twopass', 'gb_xform_in', 'gb_room', 'gb_left', 'tk_find', 'tk_key', 'gb_tex', 'gl_conv_entry', 'xc_call']),
 ('9 converter: the transform (matrix products)', 'transform', ['vc_xform']),
 ('10 converter, off-screen cut (the box\'s 8 corners)', 'cut test', ['xc_off', 'xc_df0_off']),
 ('11 converter, depth range', 'depth range', ['xc_zr_df0d', 'xc_zr_df0', 'xc_df0_zin', 'xc_zr_end']),
 ('12 converter, per vertex: positions and box, colours / UVs / flags, strips', 'positions + attributes + strips', ['vc_df0_pos', 'vc_df0_pos_k', 'vc_df0_att', 'vc_strips', 'df0_pos']),
 ('13 list build at the frame\'s end: the batches into the GE list, the shadow pairing (outside the walk)', '- (the ME\'s frame ends and lists)', ['gx_batch', 'gx_frame', 'xf_setup', 'xf_draw', 'xf_rest', 'dz_pair', 'dz_batch', 'dz_tris', 'dz_sortk', 'dz_sortt', 'dz_tin', 'dz_vin', 'dz_mv', 'dz_near', 'dz_key', 'dz_uniq', 'dz_tuniq', 'dz_tmp', 'set_tex', 'set_blend', 'xm_list', 'gl_end', 'gl_list', 'blend', 'state', 'put', 'put_sym']),
]
base = lambda n: n.split('.')[0]
D2 = {}
for n in D:
    b = base(n); x = D2.setdefault(b, [0, 0, 0])
    for j in range(3): x[j] += D[n][j]
D = D2
used = set(); tot = [0, 0, 0]
for n in D:
    for j in range(3): tot[j] += D[n][j]
print('%s: B %s - A %s over %d pictures; %.0f groups, %.0f vertices a picture' % (label, pb, pa, fr, ng, nvx))
print('all: instructions %+.0f, I-misses %+.0f, D-misses %+.0f = %+.2f ms a picture [C model]; per group %.0f instructions, %.1f I-misses, %.1f D-misses = %.1f us' % (
    tot[0], tot[1], tot[2], ms(tot), tot[0] / ng, tot[1] / ng, tot[2] / ng, 1000 * ms(tot) / ng))
rows = []
for name, stg, fs in CAT:
    name = '%s  [counters: %s]' % (name, stg)
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
rows.append(('14 everything else (each under its name when 0.10 ms or more)', rest, rp))
for name, t, parts in rows:
    print('\n%s\n      = %+7.2f ms a picture (%4.1f%% of the work without the waits)  per group: %5.0f instr %5.1f I-miss %5.1f D-miss = %5.1f us' % (
        name, ms(t), 100 * ms(t) / (ms(tot) - ms(D.get('hw', [0, 0, 0]))), t[0] / ng, t[1] / ng, t[2] / ng, 1000 * ms(t) / ng))
    for m, f, d in sorted(parts, key=lambda x: -abs(x[0])):
        print('      %-28s %+7.3f ms  (instr %+9.0f, I-miss %+7.0f, D-miss %+7.0f a picture; a group %6.0f / %5.1f / %5.1f; a vertex %5.1f instr)' % (
            f[:28], m, d[0], d[1], d[2], d[0] / ng, d[1] / ng, d[2] / ng, d[0] / nvx))
