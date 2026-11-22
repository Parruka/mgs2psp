Stage B work tree (renderer_notes 11.20-11.22, tasks #100 and #102), 2026-10-30.  Not in the shared code yet.
sb1_stageb.diff: all the tree's changes against the shared sources (lane 1 and the direct reads; apply in a copy of runtime/, ge/,
psp/, boot/ to rebuild the container tree /home/claude/sb1).  sb1_lane1.diff: lane 1 alone (17:45), kept for reference.
mk.sh: the x86 twin from that tree; codec_x86.sh: the codec route on it; mkpsp.sh <new dir>: gl7's PSP build with the tree's
ge/, xl_psp.c and boot_rt (g_opt OPT_XL_LANE 0x100000, on by default; off = gl7's path; OPT_XL_NEED 0x200000 is read only in
builds made with -DSB_NEED: direct reads are exact but slower in the model, renderer_notes 11.22).
x86 switches: MGS2_SB_LANE=1 (lane 1), MGS2_SB_CHK=1 (each lane df0 call checked against vc_df0), MGS2_SB_NEED=1 (direct reads:
decode only the VU memory a converter reads), MGS2_SB_NCHK=1 (each converter call run on both memories and compared).
pgo_gl.sh <step> [<work dir> [<seed or trace>]]: the picture code's first PGO profile and a code layout with it (task #103,
renderer_notes 11.23): gen, run, use, trace 0 / 1, order <seed>, d <seed> (work dir /home/claude/sbg; then psp/gl3.sh with
GL3=<work dir>: emu hw, model).  tools/layout_order.py lists the picture's objects for it.
