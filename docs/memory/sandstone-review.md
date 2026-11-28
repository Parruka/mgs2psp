---
name: sandstone-review
description: What the SandstoneDev repos (PS2RC, universal-ps2-recompiler, psp-developer) offer the MGS2 port, and the licence status
metadata:
  type: project
  modified: 2026-10-21T15:30:00.000Z
---

- 2026-10-17 review: /mnt/project-files/mgs2/sandstone_review.md (repos at PS2RC fac73d4, kit 1a7b196, psp-developer c4354ed).
- 2026-10-21 recheck: the 33 copied code files + NOTICE.md are unchanged upstream; LICENSE is now 1.2 (drops the Listed Disc clause; splash may be removed if the README keeps the credit). No action needed.
- Licence PS2RC 1.1 allows modifying and using their code for a personal port of an owned disc; only SLES-52541 (GTA SA) is a Listed Disc, MGS2 is not. **Ffm decided 2026-10-17: reuse PS2RC's sound and movie code** (IOP recompiler + rt, SPU2 model, ME audio split, WSOLA stretch, PSMF muxer) in a separate folder with their LICENSE/NOTICE.md; everything else findings-only, TT recompiler stays the core.
- Top items for MGS2: IOP recompiler + lockstep sound + SPU2/ME/WSOLA; overlay-table model for stage images; 2.3x code-size levers; PCSX2 PINE oracle and PPSSPP PR 22414 exact GE renderer for testing without a PSP; 25 fps judders on the 60 Hz LCD (30 shown with 50 Hz logic is an option); PS2RC has no N-logic-steps-per-frame mechanism and drops framebuffer effects like motion blur.

**Why:** avoids re-reading 84k lines; the licence answer decides whether code can be reused.
**How to apply:** read the review before planning sound, stage loading or pacing; only the sound/movie modules may be copied; keep them in their own folder under the PS2RC licence.
