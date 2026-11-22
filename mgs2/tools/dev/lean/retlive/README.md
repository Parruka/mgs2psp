# Step 1c dev tools (recomp/lean_plan.md 1c, 2026-10-24)

The project's own form is `RC_RETLIVE=1` in recomp/regcache.py with tools/ret_open.py (run by tools/mk_headers.sh). These are the
container experiments around it (paths under /home/claude/lean are ephemeral):

- `cg_wrap.c`, `mk_cg.py`, `xbuild_rl.sh`: the x86 guard build: `--wrap=gen_jalr,--wrap=gen_guest_call` stops the run (exit 8) when a
  call through a register or from the kernel reaches a closed function (lists from the headers' `ret_*.json`; the stage found by its
  signature words as in boot/dispatch.h). It never tripped on the three routes. The project's regcache.py drops the closed functions'
  dispatch cases instead, which gives every build (x86, emulator, PSP) the same stop at no cost.
- `retclass.py`: the first measure of return-store traffic by open / closed function (before 1c).
- `retsplit.py`: the dynamic return stores of built headers, split by open / closed function and by register.
- `regcache_openset.py` (rc6) + `openret_iter.sh`: OPENSET, open functions getting a return set too (what can be read after any call
  through a register or into another unit, plus their direct callers' needs). Round 0 from empty sets: those sites read everything
  (R 0-15, 24-27, 31; F 0-19), because a JALR's own read set is "everything"; it needs 1d (indirect-call sets) first. [I] 0.4-0.7 ms.
