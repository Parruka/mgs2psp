# ARK-4 options for speed and memory (research, 2026-10-21)

Asked by Ffm: "what options does ark4 have to increase performance? Like extra memory, memory speed, etc?"
Research only: nothing was changed on the PSP or the PC. Sources: the ARK-4 wiki pages, its CHANGELOG, and ARK-4's own source
(raw.githubusercontent.com, branch main, read 2026-10-21). "(inferred)" marks my own reasoning.

## Short answer

ARK-4 has no setting that makes a homebrew EBOOT's CPU, RAM or bus faster than what the harness already gets.
The harness already runs at the top stock speed (333 MHz CPU, 166 MHz bus) and already gets the largest user memory ARK
hands out (the "extra memory" mode, because the EBOOT asks for large memory). What ARK settings *can* do is make things
worse: a forced clock setting or game plugins. So the useful actions are checks, not new features.

## What the harness already uses (from psp_harness.md and prof/hw_v19/log.txt)

- Clock: `scePowerSetClockFrequency(333, 333, 166)`; v19 log: "CPU 333 MHz, bus 166 MHz".
- Large memory: `PSP_LARGE_MEMORY = 1` (psp/Makefile), loaded at 0x08804000; host stack pool at 0x0b8a0000, so the upper 32 MB is in use.
- Volatile 4 MB block at 0x08400000 (SPU2 RAM), locked at start, unlocked at exit.
- Machine state in the 16 KB scratchpad (v18).
- Memory left in the v19 run: heap headroom 4096 KB, outside the heap 764 KB (largest block 512 KB).

## ARK-4 settings, one by one

| Setting (SETTINGS.TXT key) | What it does | Gain for the harness | Risk | Fits Ffm's rules? |
|---|---|---|---|---|
| Use Extra Memory (`highmem`, Auto / Forced) | Grows the user partition on 2000/3000/Go | None: already on (see "Extra memory" below) | Forced on retail games can break cheats/plugins | Yes, no change needed |
| Overclock (`overclock`) | Sets CPU/bus 333/166 at boot and turns the app's own clock calls into no-ops | None: same 333/166 the harness sets | None for us | Yes (this is stock 333, not real overclock) |
| Balanced (`defaultclock`) / Power Save (`powersave`) | Force 222/111 or 133/66 and block the app's clock calls | **Negative**: the harness would silently run at 222 or 133 | Wrong timings | Keep **off** for homebrew |
| Memory Stick Speedup (`mscache`) | 4 KB read cache in the Memory Stick driver | Load times only (overlays, disc pack reads); no effect on step time. Not measured. | Old stability issues fixed in 4.20.68 r10 | Yes; optional |
| Inferno Cache (`infernocache`) | Read cache for ISO games | None: only for ISOs, not EBOOTs | Was crashing with highmem in 4.19.x (fixed) | Not needed |
| Skip Sony Logos, Hide PIC0/PIC1 | Faster boot / XMB | Boot convenience only | None | Optional |
| LEDs, UMD drive, analog off | Power saving | None for speed | None | Optional |
| Real overclock above 333 (psp-beyond-444mhz plugin, ARK-5 `cpuclock`) | 433-471 MHz | +30-40% clock | Stability per unit | **Out**: Ffm's no-overclock rule. Listed only for completeness. |

Wiki: https://github.com/PSP-Archive/ARK-4/wiki/Custom-Firmware-Settings and https://github.com/PSP-Archive/ARK-4/wiki/settings

### Clock settings in the source
`core/systemctrl/src/cpuclock.c` `SetSpeed(cpu, bus)` calls the kernel `scePowerSetClockFrequency(cpu, cpu, bus)` and then
replaces `scePowerSetClockFrequency` and the CPU/bus clock calls with "return 0" stubs. So with any clock setting active the
EBOOT's own 333 request is ignored but still returns 0 (the harness logs "clock 333 MHz requested: 0" either way).
The harness also logs the *read-back* clock, which is the real check: v19 said 333/166, so no forcing setting is active today.
There is **no separate RAM or bus speed option**: the bus follows the CPU (half of it), and 166 MHz is the most the stock power
call allows (web_research.md "Clocks").

## Extra memory (the main question)

How ARK does it (`core/compat/psp/high_mem.c`, `core/systemctrl/src/plugin.c`, `core/systemctrl/src/syspatch.c`):
- For an EBOOT (apitype 0x141) whose PARAM.SFO has `MEMSIZE = 1`, ARK sets `force_high_memory` itself (plugin.c
  `loadSettings`), which is what "Auto" means. `PSP_LARGE_MEMORY = 1` in our Makefile writes that SFO field, so the harness
  already gets it.
- It then calls `sctrlHENApplyMemory(MAX_HIGH_MEMSIZE)` (55 MB), and `patch_partitions` cuts that to
  `56 - 4 (partition 11) - 1 (partition 8)` = **51 MB** of user memory on a 2000/3000 (55 MB only if partition 11 does not
  exist). That matches the "about 51 MB" in psp_harness.md.
- The 56 MB user area of a 64 MB PSP is therefore: 51 MB user partition (ours), 1 MB partition 8 at 51-52 MB, 4 MB
  partition 11 at 52-56 MB. Plus the 4 MB volatile block at 0x08400000 that we already lock.

What is left that we do not use (inferred, untested):
- **Partitions 8 and 11, about 5 MB.** ARK moves them but leaves them to the system. A kernel-mode allocation from them
  might work, but I found no source saying what the firmware keeps there on 6.60 (partition 11 is created by ARK's own
  `prepatch_partitions`). Worth a test only when the GE renderer's memory budget is laid out and becomes tight; it
  needs kernel-mode code, which ARK allows (see below).
- **Media Engine's own 2 MB eDRAM** (Wikipedia, PSP hardware: 4 MiB eDRAM in all models, 2 MiB for the GPU, 2 MB for the ME).
  Only reachable through ME code; past decision "ME offload: don't pursue" for logic. Only relevant if audio moves to the ME.
- No source says the upper 32 MB is slower than the lower (web_research.md).

## Kernel mode

ARK runs unsigned kernel-mode modules and offers `kubridge` (`kuKernelCall`) to user programs (systemctrl Makefile
`src/kubridge.o`). What kernel mode could give us:
- Allocations from partitions 8 / 11 (above).
- Nothing faster for normal code: cache instructions (`cache 0x18`, `0x1e`) already work in user mode (v18 uses them).
- Hardware registers (memory controller timing, bus, power) are reachable from kernel mode, but writing power / clock
  registers is against Ffm's rules, and the harness's "no hardware counters" rule (A10) stays. Not recommended.

## Plugins (possible hidden cost)

Plugins listed in `SEPLUGINS/PLUGINS.TXT` for `always` or `homebrew` load with the EBOOT: they take memory from the same
partitions and many run a polling thread (cheat devices, button remappers, screenshot plugins). (inferred) A plugin thread at
a higher priority than the game thread steals CPU time from every step and evicts cache lines; the logs would show it only
as noise. ARK's changelog shows such clashes (cwcheat with Force Extra RAM, 4.20.69 r13).

## Recommendation

1. No ARK setting will speed up the step; keep the current setup.
2. Check, once, that for the homebrew runlevel there is **no** `powersave` / `defaultclock` line in `SEPLUGINS/SETTINGS.TXT`
   (today's log proves 333/166, so this is only a guard) and that `PLUGINS.TXT` loads **no** plugin for `always` /
   `homebrew` during timing runs (or that the list is empty).
3. Optional, harmless: turn on Memory Stick Speedup for faster loading (not step time).
4. Later, only if the renderer runs out of memory: a hardware test of a 5 MB allocation from partitions 8 / 11 via a small
   kernel-mode helper. Not now.
