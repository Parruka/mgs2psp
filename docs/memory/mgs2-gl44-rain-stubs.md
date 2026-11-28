---
name: mgs2-gl44-rain-stubs
description: 2026-11-27 gl44 design from the Vita study thread: line_sph and rain.c as every-step exact stubs (MGS2_OPT2 bit 22), twin proof, files in mgs2/main/attic/gl44_vita/
metadata:
  type: project
---

Asked by the MGS2 port lead 2026-11-27 (after Ffm's "just make it happen"): the Vita's rain list for our PAL deck as a twin-proven design. Delivered 20:05 UTC in mgs2/main/attic/gl44_vita/ (gl44_vita_design.md, native_w00a.c/.h diffs, v44_apply.py, boot_x86_actlog.diff, cmp.py, dcmp.py, run2.sh); told to the port thread and the coordinator. Building, model, read hash, emulator and PSP run are the port thread's.

Facts [C twin unless marked]:
- In our build the rain actors of the Vita's list are not drawn, so every-step stubs beat the Vita's "shown steps only" rule and need no parity signal.
- Switch NAT_V44 = MGS2_OPT2 bit 22 (4194304); bit 21 is gl43's B. Today's cycle words have bit 22 SET: an off turn clears it (ffbff9df on the gl18me twin).
- line_sph (act 0x333380): stub in nat_ls_prim (seed jump, polls x 64, hide bits, loop registers) plus the first 32 vertices of the act's last unit's scratchpad image, because putmodel.c works on scratchpad vectors 16..21 it never wrote (22 = measured minimum).
- rain.c (act 0x31af28): hook at 0x31b47c skips the vertex and fill loops of in-place entries; prologue, tail and last entry stay translated. A plain return before the loop is not safe (entry vectors, 0x41e080, 0x41e090, actor+84). VU0's random instructions exist only in rain.c's loops [C code].
- off vs on, route i to 37,800: every act's seeds and k_time at entry identical (27,686 passes), IOP and vfb 685/685, 37,831 picture lines equal; EE RAM differs only in rain.c's hidden vertex buffers (8,448 words). x86 ticks: line_sph -41%, rain.c -77%; PSP about -1.4 ms a rain step [I], not measured.
- Other acts: ripple 0x332180 needs a state-only form (<= 0.33 ms wait); body_sph cannot keep the dice; charaspread done (G0C); rain_cm 0x32a7d0 too small. d00t/d01t run their own copies of line_sph / rain.c / rain_slow.c with no natives.
- Shown step: rt_dg_call's rule is decided after the scheduler pass; wait word >= 2 means every step shown.

Tools and traps: this thread's container had an x86 twin at /home/claude/main (rebuilt from mgs2/main with hdr.sh, x86.sh, vu.sh; inputs staged from the PC; 685/685 vs hashref_x86_in_gl18me) and an act log (MGS2_ACTLOG, x86 only). MGS2_HALF is HEX: "16" = 0x16 switches lamps and sea to half rate; use 10 for a bit of no table entry. Containers are temporary: rebuild if gone.

Related: [[vita-port-study]], [[mgs2-rain-study]], [[mgs2-status-now]], [[mgs2-visible-choices]].
