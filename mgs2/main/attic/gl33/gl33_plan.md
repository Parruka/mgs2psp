# gl33 plan: gl31 + in-place static streams (in turns) + a fresh PGO and code order, 2026-11-21 02:35 UTC

Lead plan for the MGS2 port thread. gl32 (vertex cache) was built and gated but not shipped: the model said it loses to gl31 in 3 of 4 windows (attic/gl32/report.txt). gl33 takes the one part that paid and fixes the code-placement cost.

## What gl33 is
1. Tree /home/claude/t33 = gl31's tree (t31) + ONLY the in-place rule from gl32 (static ship streams outside packet memory read from RAM on the ME instead of copied in the hand-off), behind its own cfg switch, default off in code. No vertex cache code. (build_gl32/alt is this in substance: reuse it, but gate it fully.)
2. Native ship meshes every turn (MGS2_NAT_CYCLE=3,3,3,3). The in-place rule in turns: plain native / native + in place (e.g. 0,0,2,2 on the existing key, or its own key), so the PSP times it inside one run.
3. A FRESH profile-guided build and code order trained on t33 (gl20's method: psp_harness.md "Build gl20" and gl19/gl20 entries: training sets elf, rt3, w00a, w01a; psp.sh's one shadow path; the lost-profile check; layout_sim rounds; FF8's lopt placement if it beats it). Train with both in-place arms so both paths get profiles. Report functions without a profile after the retrain (gl20: 4 functions, 0.06%).
4. Everything else as gl31's cfg (FLIPWAIT_FROM=0, DZ X whole run, ME_PIC 4, live captures).

## Scoring before delivery [C model]
- Four windows as gl32's (waiting ME 29,810-29,890, waiting main 29,910-29,990, rain ME 31,010-31,090, rain main 31,110-31,190), and the steady deck 26,300-28,700 (d01t) and one title/opening window as a control.
- gl33 plain native vs gl31 arm 3 = the retrain's gain alone; gl33 in place vs gl33 plain = the rule's gain. Delivered only if gl33 is ahead of gl31 in the deck windows.
- Also the model's lost-profile figure and I-misses a picture vs gl31.

## Gates
As gl31_plan.md G1-G7 (twin, read hash, emulator 62/62 + digests = twin + pictures vs gl31's emulator, ME gates, memory, reader rd33.py = rd31 + the in-place counters, reproducible). The hashref may equal gl31's or gl32's: say which and why.

## Limits
As gl32_plan.md: no PSP, nothing on the owner's PC, game data stays in the container; read-only outside t33, main/build_gl33 and attic/gl33; delete only there by literal path after looking (build_gl32's emulator game folders may go if disk is short: say so); stop processes by PID; python3 -I. Disk 4.5 GB free: a PGO build peaks ~1.1 GB; keep new use under 2.5 GB. Reports as .txt (build_gl33/report.txt, copy attic/gl33/report.txt). Every figure [C], [C model], [I] or UNKNOWN.
