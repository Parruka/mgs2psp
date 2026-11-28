---
name: upstream-check
description: GitHub upstream check of the ports' tools (pspdev, PPSSPP, ARK-4, ff8-decomp, psp-developer) and the skill search of 2026-11-16, with verdicts
metadata:
  type: project
---

2026-11-16, thread "Check GitHub for tool updates"; report /mnt/project-files/upstream_check_2026-11-16.md.

**Upstream:** our pspdev tarball (2026-10-18) = v20261001 [I]; v20261101 same pspdev commit, gcc unchanged: no update needed. PPSSPP 1.20.4 still newest release; master is 819 commits past our reference 83e616ea and turns Media Engine emulation ON by default (2026-11-12); reference stays 83e616ea, which ALREADY has the ME emulation behind `--me-core` (web_research_2.md:191, never tried) and a headless WebSocket debugger (`--debugger`, `--debugger-run`). ARK-4 no code since May. ff8-decomp +25 commits incl. DrawMovieMask (for [[ff8-m3]]). psp-media-engine-libs and ajitmohapatr/ps2-recomp-Agent-SKILL no longer public (hkmodd/ps2-recomp-Agent-SKILL is; verdict "do not install" stands).

**Done on Ffm's "go" (20:39):** mgs2/third_party/psp-developer refreshed 49326ad -> d27e89a (new references/recompiled-port-lessons.md: a CFW can override the clock while the getters say 333; eDRAM hot code +6-8%, their figures). Clock proof line (timed fixed loop in the log) handed to the FF8 thread for its next build; same line owed to the MGS2 gl harness when MGS2 resumes.


**Own skill (Ffm 20:46 "go ahead"):** drafted `psp-recomp-port-method` (method by stage, exactness, PSP-only speed figures, build cadence and delivery, hardware lines, consents, notes, game data, housekeeping, tools; says memory and Ffm's latest word win over it). Project copy for every thread: /mnt/project-files/skills/psp-recomp-port-method/SKILL.md (copy the folder to ~/.claude/skills/ in a new container, as with psp-developer). SAVED by Ffm to their account skills 2026-11-16 20:50 (shows in sessions as `anthropic-skills:psp-recomp-port-method`; load it with the Skill tool at a thread's start). When a rule changes, update the project copy and propose the same edit to the account skill.
