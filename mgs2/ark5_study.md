# ARK-5 vs ARK-4 for the port (research, 2026-11-02)

Asked by Ffm (00:34 UTC): "would instaling ark5 be better?" Research only: nothing installed or changed.
Sources: ARK-5 README and source (raw.githubusercontent.com/PSP-Arkfive/ARK-5/main, read 2026-11-02),
FasterARK README (PSP-Arkfive/FasterARK), ARK-4 README and source (PSP-Archive/ARK-4/main). "[I]" = my inference.

## What ARK-5 is
- Real: the successor of ARK-4 by the same team. ARK-4's README now opens with "Archived: We moved over to ARK-5".
  ARK-4 gets no more fixes.
- README features: built with the latest PSPSDK, easier install, better plugin management, "true overclocking for all PSP
  models except PS Vita", "improved memory management for consoles with extra ram", under-the-hood fixes.
- Install from ARK-4: OTA update, or the ARK_UPDATE.zip updater. From OFW 6.60/6.61: FasterARK, then optional CustomIPL.
  The package carries a FLASH0.ARK; the updater writes the CFW's files to the PSP (flash0) [I from the build files].

## Compared on what matters to the port

| | ARK-4 (today) | ARK-5 | Gain for us |
|---|---|---|---|
| CPU / bus at stock | 333 / 166 via scePowerSetClockFrequency | Same. `sctrlHENSetSpeed` hook only acts above 333 (`Compat/PSP/src/overclock.c` `overclockHandler`); at or below 333 it calls the original | None |
| Above 333 | plugin only | Built in: `cpuclock:NNN` in SETTINGS.TXT writes the PLL registers 0xBC1000FC / 0xBC100068 and the domain ratios 0xBC200000/04 directly | Out: Ffm's rules (no overclock, no clock-register writes) |
| User RAM for our EBOOT (MEMSIZE=1) | 51 MB (56 - 4 p11 - 1 p8) | Same code in `Compat/PSP/src/high_mem.c` (only a struct rewrite); 51 MB | None |
| Partitions 8-11 (~5 MB) | `uprotectExtraMemory` opens the extra RAM to user code; partitions stay kernel addresses (kernel helper needed) | Same, plus at boot it sets partitions 8-11 to user addresses (`SystemControl/src/sysmem.c`) so user code may allocate there [untested] | Maybe later: ~5 MB without a kernel helper, if the renderer budget gets tight |
| Plugins in extra RAM | no | With highmem Auto, plugins load into p9; not for our EBOOT, because MEMSIZE=1 sets "force max" (`settings.c`) | None (we run with no plugins) |
| Kernel modules / kubridge | yes | yes (`kubridge.o` in SystemControl) | None |
| ME, VRAM, GE | no CFW involvement | no change found | None |
| Maintenance | archived | active | Future fixes only |

## Risks of switching now
- Every hardware number and exact pass (gl5..gl12, ME audio v30me6c) was measured on ARK-4. A new CFW means one re-baseline
  run (gl12 again: hashes, digests, shots, timing) before any Stage C step can be compared.
- New code line: the NID resolver, module loader and sysmem patches were rewritten for the latest SDK; our kernel-mode ME
  helpers and kernel-alias calls would need that re-check [I].
- The install writes the PSP's flash0 (the CFW's own files). Low risk with the official updater, but not zero.

## Recommendation
Stay on ARK-4 now. ARK-5 gives the port nothing measurable at 333 MHz: same clocks, same 51 MB, same kernel access. Its
headline feature, true overclock, is excluded by Ffm's rules. Revisit when (a) the renderer needs the ~5 MB of
partitions 8/11, or (b) ARK-4 shows a bug that ARK-5 fixed. If Ffm ever switches: do it between builds, with no
`cpuclock`/`overclock`/`powersave` lines in SETTINGS.TXT, and run gl12 once as the new baseline.
