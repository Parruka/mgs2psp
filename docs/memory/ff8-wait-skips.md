---
name: ff8-wait-skips
description: FF8 exact skips of polling wait loops (pad routine's four loops, s6 steps 10-11): how a skip is proven plain and how the x86 PADCHECK compares every skip with the real iterations
metadata:
  type: project
---

Part of [[ff8-s6]] (rt.c pad_wait, sio.c, hw.c; notes stage5_notes.md "Step 10" on). The older VSync wait skip (rt_idle_vwait) follows the same idea with two observed iterations.

- METHOD: a hook at the loop head flushes pending cycles and compares counters with the last arrival: blocks run, slow block ends (rt_nbes), full ends, I-cache epoch, lean reads (rt_npure; the general path counts rt_npure_g). One plain iteration of length d -> k more at once while c + k*d < min(c + fast_rel, next event, timer deadline) and the device guard holds (sio_skip_room; timers_skip_ok + timeout). x86 CHECK: env PADCHECK=n makes no skip, keeps the state each skip would leave and compares it when the real iterations arrive (every n-th with RAM + the saved-state section); last run 325,894 compared, 0 differ. IOCHECK=1 still checks the lean reads (0 differ). LEANHIST=1 prints lean reads per block for the two profile windows (870 -> 332 a quiet frame after 10b).
- Loops (main exe): A 0x8003A258, C 0x8003A0FC, D 0x8003A50C spin on JOY_STAT bit 1 (one block, one lean read); B 0x8003A304 spins on I_STAT bit 7 with timer 2's timeout (four blocks, three lean reads; v1 = the count the last skipped iteration reads, 6 + dma_steal cycles after the head). gen.py IDLE names the heads; such functions stay C on the PSP.
- Pad port (step 11): a bit shows outside the port only as the last bit of a byte (out, in, or a device's) or while an /ACK wait is pending; other bits are clocked by whoever brings the port up to date (sio_run), so the event is set for the next bit that shows. --sioeverybit = old events, --nopadskip = no skips (x86, for comparing).
- **How to apply:** a new wait-loop skip gets a hook in IDLE, its plain test from the counters, a device guard, and must pass PADCHECK (0 differ, 0 never reached) plus out.txt aae53db0 before any PSP build.
