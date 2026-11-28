---
name: mgs2-survey-leads
description: Leads from mgs2/psp_ports_survey.md (2026-11-10) to look at after STOP ALL lifts: BullyPSP's draw-block replay (H1 overlap), Memory Stick vs PSPLink memory (our headroom figures), PS2RC facts
metadata:
  type: project
---
- On hold under STOP ALL (Ffm 2026-11-08 19:31); the coordinator relayed these 2026-11-10 15:18 for notes only, not to act on.
- BullyPSP (built on PS2RC) replays identical repeated DMA draw blocks instead of converting them again: its profile shows ~1,100 batches a frame with batch setup 15% of the time; their estimate 2-4 ms saved (not measured by us). Overlaps H1 ([[mgs2-h1]]) and the translator's batching. First step when work resumes: one measurement on w00a, how many of our draw blocks repeat word for word frame to frame and what their conversion costs, before any design.
- sa1-psp: largest free memory block 7,680 KB when started from the Memory Stick against 11,776 KB under PSPLink (~4 MB less); its ME branch also hangs the next PSPLink reboot. For us: each README's step 4 starts the build from the XMB Game menu on the Memory Stick [C]; that Ffm never used PSPLink is [I]. The emulator headroom rule (gl3.sh's hwmem calibration) is fitted to those PSP runs. To confirm when work resumes: one PSP log's memory lines against the system's free-block report at start.
- PS2RC itself: uses the ME, needs ~50 MB of memory, publishes no fps figures from a real PSP.
