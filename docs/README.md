# docs: notes kept outside the project folder's `mgs2/`

`sync_from_project.py` refreshes this folder in the same pass as `mgs2/`. Everything here is our own writing; the same rules
apply as for the rest of the repository (nothing taken from the game, the private filter applied).

- `method/psp-recomp-port-method.md`: how these ports are built and how the owner wants the work done: the stages, what
  "exact" means, how speed is measured, builds and deliveries, the hardware lines never crossed, what needs the owner's OK.
- `memory/`: a backup of the project's memory notes: one file per subject, `MEMORY.md` is the index. This is what a new
  session reads to get up to speed: the rules, the status, each build's results, the working facts. It is a FILTERED copy
  (a few lines and one file are left out by the private rules), so it is a safety copy of what we know, not a byte-exact
  restore. The newest status is `memory/mgs2-status-now.md`; the stop of the other port is recorded in `memory/MEMORY.md`.
- `notes/upstream_check_2026-11-16.md`: the last check of upstream tools and public skills.
- `tt_port/`: the hand-over notes of the earlier Two Towers port, whose method this port follows (notes only, no code).

Not here: the briefing page for the owner (it lives on claude.ai and is rebuilt from `mgs2/psp_harness.md`), the PC-side
PSP control tools (on the owner's PC under `utils\psplink`; to be added when that folder is next in reach), and the other
port's work folder.
