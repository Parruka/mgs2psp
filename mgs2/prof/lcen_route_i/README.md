# Route i reader census for the logic thread (2026-11-07, thread "MGS2 port")

Numbers and names only. x86 twin: the trace build's objects (every unit with the load and store hooks) plus
/home/claude/lcen/boot_lc.c (counting only, no effect on the game), route i (play/pad_w01af.txt) to vb 37,800, with the hash-reference
settings (FPU_RZ, SKIP_MOVIES). The run's state hashes equal run hr2's (../hr2_route_i/timeline.txt) at all 378 points from vb 100
to 37,800. Scripts (container only): /home/claude/lcen/mk_lc.sh and run_lc.sh.

`report.txt` groups every counted read by guest function, with reads, first and last vb, and the addresses read. Names come from
our function-name lists (version 2) where known.

- (a) Guest loads of 0x18db60-0x18db77, with their widths.
- (b) Timer and counter reads:
  - Loads of the EE timer registers 0x10000000-0x10001fff. These take the register path (rt_mm), not the RAM path, so their width
    is not known.
  - COP0 Count reads: none on route i.
  - Performance counter reads: sel 1 is PCR0, which the game's profiler sets to count CPU cycles. The twin returns its virtual
    time there (boot.c COP0R).
- (b) Alarm sets: SetAlarm (0xfc in the game's libkernel numbering) per call site, keyed by the guest return address. The twin's
  alarm times come from its virtual time (K_READ). The 4 calls at vb 1,037-1,040 come from 0x200728 in n_title, so their name is
  S_n_title_P2006e0+0x48. Stage image 0caa7966 is n_title from vb 214 to 3,026. The report lists every stage image that has code
  at that address.
- (c) Loads of the pad area: the one open port's padArea at 0x1e6d00 (two 128-byte records). Padman's status buffer was never set
  up on route i, so it has no loads.

## What stands out

- T1 COUNT (0x10000800) is the timer the game polls: 71.1 M reads.
  - 65.4 M of them are P1159d0, a delay helper in the sound code (g_sound). It waits until T1 has moved more than 0x106 ticks,
    several waits in a row. Its callers are KariLoad (0x115b50) and GM_LoadPack (0x115dd0). Its reads run from vb 44 to 31,658.
  - DG_StartFrame: 4.17 M reads. That is about 152 a call, if it reads 0x18db70 once a call [I].
  - DG_SortChanlSystem: 0.89 M. P1238e8 (renderer frame code): 0.38 M. Act1: 55 k. A few stage functions: 1-5 each.
- There is one T3 read (MODE, vb 0, P174490). There are no T0 or T2 reads.
- How many polls a wait takes in the twin depends on how its virtual time moves per poll. On the PS2 that number is set by real
  cycles, so these poll counts are the twin's, not the console's.

## Stage names for the "stage image" hashes in ../hr2_route_i/timeline.txt

The hash is FNV-1a of the 64 bytes at 0x1f1c00, matched to the head of each stage file. The vb is when the image is complete. The
other hashes, 2-3 vb earlier, are a half-loaded image.

| hash | stage | vb |
|---|---|---|
| 86912629 | init | 4 |
| 106690bf | r_title (the same image as r_plt0-5) | 39 |
| 0caa7966 | n_title | 214 |
| 35c9d5cd | r_tnk0 | 3,027 |
| e4c654c6 | d00t | 3,317 |
| 1e9b348a | d01t | 19,950 |
| b8ce4ff4 | w00a | 23,004 |
| 1e9b348a | d01t | 23,477 |
| b8ce4ff4 | w00a | 28,812 |
| 68829306 | w01a | 31,324 |

Rerun the reader: `python3 -I lcen_report.py <run log> <boot binary>` (container: /home/claude/lcen/run_lc.log, /home/claude/lcen/boot).
