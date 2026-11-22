# gl25 (2026-11-19): the ME's walk runs on through the next drawing step

Delivered as MGS2T_gl25 = build_gl25d: gl24b + gl25_apply.py + the xl_psp.c part of gl25b_apply.py (the two-hook bound) + gl25c_apply.py
(the review's counter fixes). gl25b's xc_keep change in ge/xlme.c was dropped (it changed the picture against the x86 twin from vb 10,516).
gl25d.diff = t25 (gl25) -> t25d (delivered). Built with l24/relink.sh (gl23's stage objects; ge_xme, ge_xme_me, xl_psp compiled again).
relink25.sh also rebuilds ge_xlme.o (used only for gl25b). Review context for the workflow: context.md. Late-read checks: ../lchk_gl24d/README.md.

## Review of gl25 (workflow wf_bf60e660-a50, completed 2026-11-19 ~17:45; 15 agents, each finding voted on by 2)
- High, real 2/2: the walk at vb 20,692 reads 011a09f0-011a53c0, which the step 20,693->20,694 rewrites; gl25 (no wait at a hook without records) exposes it. Handled in gl25d by the two-hook bound plus DEVIATIONS C7 (lchk4: 20,692 and 26,008 are the only two-area walks still exposed; the copy fix changed the picture and was dropped).
- Medium (argument gap), real 0/2: lchk3/lchk4 hash only at hooks, so a rewrite undone within a step is not seen. Stays an open [I] limit of the check.
- Low, real 2/2: stale two_pend in two_post after mid-step walks, and the two_poll race (walk_due cleared while a half stays held). Fixed in gl25c (two_poll at the start of two_post; one tail read in two_poll).
- Nit, real 2/2: 'hooks without a wait' also counts the hand-over hook (which syncs). Counter only, at most 1 a turn; read the gl25 log with that in mind.
- Checked and found sound: the halves and the in-order queue, every plan path, the full waits that stay (mq_late(3,1), the hand-over sync, mq_drain, mq_thin).
