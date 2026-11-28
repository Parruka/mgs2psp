---
name: tt-port-files
description: Where the Two Towers port material lives in the shared folder, what is missing, and its role as the base for Ffm's next PS2-to-PSP port
metadata:
  type: project
  modified: 2026-10-17T22:25:27.418Z
---

- 2026-10-17: Ffm said the TT knowledge is the base for a new port of a different PS2 game (not named yet). TT gameplay consents (LOD, elf/follower counts, orc cap, T8 textures) do not carry over to the new game.
- Shared folder: /mnt/project-files/tt_port/ holds TT_PSP_port_handoff.md, src/ (unzipped code), and ORIENTATION.md (reusable vs TT-specific split, missing-file list, handoff-vs-code check). Read ORIENTATION.md first.
- src/ has no demo/ dir: all gen_*.h (gen_pl/co/wc/pr/ow/me/vu/stub/dual/hacts) and game binaries must be regenerated; Python modules `ids` and `relset` are absent; scripts hard-code /home/claude/proj/TwoTowers_PSP_viewer and /home/claude/work.
- recomp.py assumes ELF load base 0x100000, file offset 0x80 (W()); change for a new ELF.
- Game-derived items inside the zip: model/isul1.lgc and the x86 binaries demo_src/test_* (personal use only).
- Toolchain to rebuild: pspdev psp-gcc 15.2.0; PPSSPP hrydgard commit 66dc8972 + src/ppsspp_patches.diff.
