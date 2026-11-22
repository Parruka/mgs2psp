# jobs.py: f_dg and f_gs (task #160) summed by job, function lists by name
import subprocess, re, collections, sys
import os
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fnsplit.py')
def fns(tag):
    out = subprocess.run(['python3', '-I', S, '/home/claude/sb17km/model/' + tag, '/home/claude/sb17km/d', '80', 'w00a', '100000'], capture_output=True, text=True).stdout
    d = {}; tot = float(re.match(r'total ([\d.]+)', out).group(1))
    for ln in out.split('\n')[1:]:
        m = re.match(r'(\S+)\s+([\d.]+) ms', ln)
        if m: d[m.group(1)] = float(m.group(2))
        if ln.startswith('by group'): break
    return tot, d
DG = [('VU0 micro programs (the per-object cull and transform)', r'^(vupv_|vup_|vuc_|vu_run)'),
      ("the effects' depth sort (sort pass)", r'^(P12dc90|nat_dbc0|P12dbc0|P12d910|P12d810|P12d7d0|P12d688|P12dc50)$'),
      ('per-object matrices and screen tests (libdg/screen)', r'^(P11fa58|P120860|P120538|P11fde8|P11f968|P11fa10|P120f00|P121060|P11f8c8)$'),
      ("the ship's meshes (chain, patch, P1214a0)", r'^(P121c30|P13bf78|P1214a0.*|P1226c8|P13bd00|P121a90|P122608|P122138)$'),
      ('the people (chain2)', r'^(P12e8b8|P12f130)$'),
      ('DMA kicks, copies, stores (runtime)', r'^(io_dma_start|rt_qcopy_nov|rt_store|io_flush|memcpy|rt_st32|rt_mm|memset)$'),
      ('interrupts and polls', r'^(k_take_interrupts|k_poll.*|rt_poll.*)$')]
GS = [('the DMA chain walk (io_dma_start)', r'^io_dma_start$'), ('GS packets, quadword by quadword (gif_qw)', r'^gif_qw$'),
      ('unpacking the VIF data (ud_*, vm_*, vif_cmd, xl_vif1_span, kick_vm)', r'^(ud_.*|vm_.*|vif_cmd|xl_vif1_span|kick_vm)$'),
      ("the translator's calls (call, on_call)", r'^(call|on_call)$'),
      ('texture and palette uploads (gsup2)', r'^(gu_.*|rec_ident|rows|gc_.*|commit|ld_sig|put|clut_new)$'),
      ('GS register front end (gf_*, drew_ctx, kick)', r'^(gf_.*|drew_ctx|kick)$'),
      ('lane 1, the ship (xl_lane_p04)', r'^xl_lane_p04$'), ('lane 2, the people (xl_lane_p06)', r'^xl_lane_p06$'),
      ('lane 3, the effects (xl_lane_fx)', r'^xl_lane_fx$'), ('timing hooks (host_enter / leave, prof_mark)', r'^(host_enter|host_leave|prof_mark.*)$')]
for tag, jobs in (('f_dg', DG), ('f_gs', GS)):
    tot, d = fns(tag); used = set(); print('%s total %.2f' % (tag, tot))
    for name, rx in jobs:
        s = sum(v for k, v in d.items() if re.match(rx, k) and k not in used); used |= {k for k in d if re.match(rx, k)}
        print('  %-70s %6.2f' % (name, s))
    rest = sum(v for k, v in d.items() if k not in used); print('  %-70s %6.2f' % ('the rest', rest))
    print('  top of the rest:', ', '.join('%s %.2f' % (k, v) for k, v in sorted(((k, v) for k, v in d.items() if k not in used), key=lambda kv: -kv[1])[:8]))
