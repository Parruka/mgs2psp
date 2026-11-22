# Zeroed handler entry (lean_plan.md 1b, dev, 2026-10-24)

Dev tools of the 1b measurement; the results are in recomp/lean_plan.md (1b, "Measured again 2026-10-24 evening").

- `regcache.py`: recomp/regcache.py (2026-10-24 afternoon) + `RC_RETSCR` (1c's return poison test) + `RC_SPECIAL=1` (SYSCALL / COP0OP /
  COP0W / JSIF sets).
- `rc_jsif.py`: RCPY wrapper; renames the SIF dispatcher's two table calls (0x175d98 / 0x175dd4) to JSIF, then runs this folder's regcache.py.
- `native_apply_dev.py`: tools/native_apply.py accepting any flush list in the poll at 0x33377c; `mk_headers_dev.sh` uses it.
- `jsif.h`: the x86 JSIF (exit 7 on a target outside the ten); `xbuild_kz.sh <headers> <out> <boot_rt.o>`: tools/dev/lean/xbuild.sh with it.
- `kernel_hzero.patch`: `-DK_HZERO` (a handler / alarm starts from a zeroed KCtx) and `-DK_HPOISON` (every register but zero
  poisoned) in runtime/kernel.h; built into a copy of /home/claude/play/rt/boot.c (the v26 x86 runtime object: without the
  flags the copy gives v26's boot_rt.o byte for byte).
- `cmpab.py A B`: compares two route runs' hash lines field by field; `ramdiff.py`: word ranges that differ between two EE RAM dumps;
  `mask_istack.txt`: the MGS2_GS_MASK file (0x70000-0x80000).
- Headers: hX = h7 (`RC_SOUND=1 RC_XUNIT=1`); hY = + `RC_SPECIAL=1 RC_POLL_R= RC_POLL_F=` with RCPY=rc_jsif.py; hZ = + `RC_SPECIAL=1`
  with RC_POLL_R=1,2,7-28,30 and RC_POLL_F=0-31.
- Since 2026-10-24 evening the project has 1b itself: runtime/kernel.h clears the registers at a handler's start by default (K_HKEEP
  for v26's behaviour, K_HPOISON for the check), recomp/regcache.py's RC_SPECIAL renames the two SIF table calls to JSIF, and
  boot/gen_pre.h defines JSIF (so `xbuild_kz.sh` no longer includes `jsif.h`, which would define it twice; `kernel_hzero.patch` and
  `rc_jsif.py` are history).
