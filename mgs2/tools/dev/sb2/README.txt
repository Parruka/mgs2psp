Stage B lane 2 work tree (renderer_notes 11.24, task #101), 2026-10-30.  Not in the shared code yet.
sb2_stageb.diff: all the tree's changes against the shared sources (lanes 1 and 2 and the parked direct reads; apply in a copy of
runtime/, ge/, psp/, boot/ to rebuild the container tree /home/claude/sb2).  sb2_lane2.diff: lane 2 alone, against sb1's tree
(tools/dev/sb1/sb1_stageb.diff applied).
mk.sh: the x86 twin from that tree; codec_x86.sh: the codec route on it; dig_x86.sh: the per-call digest file (ge/xldig.c) of a
vblank range; mkpsp.sh <new dir>: gl7's PSP build with the tree's ge/, xl_psp.c and boot_rt (no PGO for those; the model build).
x86 switches: MGS2_SB_LANE (bit 0 lane 1, bit 1 lane 2: 3 = both), MGS2_SB_CHK=1 (each lane df0 and P06 call checked against
vc_df0 / vc_p06 on the decoded VU memory), MGS2_SB_NEED=1 and MGS2_SB_NCHK=1 as sb1's.
PSP g_opt: OPT_XL_LANE 0x100000 (lane 1), OPT_XL_LANE2 0x400000 (lane 2), both on by default; off = gl7's path
(MGS2_OPT=ffbfffff: lane 1 only; ffafffff: neither).  OPT_XL_NEED 0x200000 only in builds made with -DSB_NEED.
Step c sizing (renderer_notes 11.25, task #106): stepc_cfg.py <build dir> > cfg (entry points, parts, ranges, return addresses);
stepc_sim.c (gcc -O2) replays a PspProf instruction trace (PSPPROF_ITRACE, a FIFO works) through layout_sim's I-cache as traced and with
df0 and P06 calls held back to their transfer's end (variants B, C, D; E, added for gl9's build: D with only the P06 calls held back);
stepc_trace.sh prep|sim|emu <build dir> <out dir> runs it on the
indoor route, 8 windows of 10 steps (the harness takes at most 8 prof windows; more leaves PPSSPP idling and battery.txt growing).
pgo_gl.sh: sb1's PGO + layout script with T = this tree (gl9's build, task #107); its order step frees each trace before writing the
reordered copy (the disk allowance).
