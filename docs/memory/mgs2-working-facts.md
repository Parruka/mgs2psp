---
name: mgs2-working-facts
description: MGS2 working facts: Ffm's PC folder layout and the 15-file delivery, PC copy quirks, backup rule, cost model and its units trap, PGO staleness, PPSSPP reference setup, disk budget
metadata:
  type: reference
  modified: 2026-11-22T17:30:34.003Z
---
- PC folder Desktop\mg2 port (reorganized by Ffm 2026-11-08): Builds\MGS2T_<version>, LOGS\<version>, Sources\ (both ISOs, SLES_503.83, system.cnf, modules\, mgs2\, pspdev + ppsspp-1.20.4 tarballs), _claude_tmp, PROJECT (spare space), SHOTS, utils, "Metal Gear Solid 2" (reference), pcsx2, FF8\CDs.
- DELIVERY = 15 files: EBOOT, 8 overlays, mgs2.cfg, README from the container; SLES_503.83 from Sources; mgs2disc.pak (only in Builds\MGS2T_gl20 and older, md5 189ca53f) copied on the PC (or the pak made with _claude_tmp/make_pak.py, disc root "$HOME/mnt/mg2 port/Sources", foreground); memcard0 empty. md5 every file on the PC.
- PC copies: device_commit_files takes stagedPath under /mnt/project-files (delete staged game files after); a PNG copied to the PC gains a C2PA chunk, so compare image chunks; the PC shell sees the folder as "$HOME/mnt/mg2 port".
- BACKUP: PROJECT\backup_2026-11-16 (md5-checked). Back up again ONLY when cloud space is nearly full, then delete the backed-up files (Ffm 2026-11-16 21:12): [[project-backup]].
- COST MODEL: PSP ms ~ (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333k. UNITS TRAP [C, found 2026-11-22]: the gl3x model scripts' "ms a step" is ms a PICTURE (2 steps); PSP logs' "ms a step" is a vblank. Halve model figures (ME and main) before comparing; the old "PSP = 0.5x model" was mostly this. BUT score39.py / sides.py / prof39.py divide by 80 vblanks = already per PSP step (gl39 K7 model 68.3 vs PSP 67.1): halve only scripts that divide by pictures. Exact levers are timed by turns every 100 vblanks in one PSP run.
- PGO STALENESS (gl32/gl33): code added since the last profile pays I-misses; a fresh PGO + order gave -4.5..-6.2% [C model]. Retrain when a build adds hot code (gen.o needs -fno-partial-inlining). PGO text is ~66 KB over no-PGO: watch the heap low point (gl38).
- PPSSPP: needs PPSSPP_VFPU_RN=1 (container patch); exact picture reference = master 83e616ea + tools/ppsspp_master_vfpu_rn.diff + ppsspp_master_fpu_fs.diff via tools/dev/ppsspp_master_run.sh; judge shots by the master. github.com blocked; raw.githubusercontent.com works.
- DISK: container allowance ~39 GB (df Avail = what is left); an emulator run ~45 MB (delete game folders after checking); a PGO build peaks ~1.1 GB; look before deleting.
- FF8 lessons in the plan (2026-11-18): 16-bit 5650 screens, PGO lost-profile check, FF8 lopt and D-cache placement, machine pointer register, clock proof, deck-start state for fast runs.
- PROFILING EMULATOR (2026-11-23): model/profile runs use build/PPSSPPHeadless_fcr (the patched PspProf interpreter computed mul.s/div.s/sqrt.s/cvt.s.w in a stale host rounding mode after a thread switch: profile windows were not state-exact; patch attic/gl39/tools/ppsspp_pspprof_fcr_2026-11-23.diff). The original PPSSPPHeadless stays for JIT runs. A profiled run whose hashes differ is a finding, not noise.
