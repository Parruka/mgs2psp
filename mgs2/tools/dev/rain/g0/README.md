# G0 for line_sph (task #165, 2026-11-03)

The exact skip of the rain's unseen drop work (`rain_study.md` §3, §5; Ffm chose "Exact skip" on the card, 22:03), line_sph first.
Results and proof: `rain_study.md` §6; the other three actors' prims hidden (H3): §7; body_sph's parts done natively (NBS): §8; charaspread's particle sets skipped natively (G0C): §9; all three in the sections below. The working trees are private and ephemeral (`/home/claude/sbg0`, `/home/claude/sbg0m`);
this folder keeps what is needed to rebuild them.

## Sources

| File | What it is |
|---|---|
| `native_w00a.c`, `native_w00a.h` | line_sph's native form with G0 / G0H (the seed's jump-ahead table, the last vertex and copy registers, the scratchpad image at the flush, the hide bits; `-DNAT_CHECK` runs the translation after each G0 stretch and compares) |
| `g0_nat.diff` | the same against the shared `mgs2/boot/native_w00a.c` (md5 9ca96905) and `.h` (8f94ecc8) |
| `host_time.h`, `boot.c` | the runtime of the G0 tree: `OPT2_G0` 2048, `OPT2_G0H` 4096 (off by default; gl16's and gl17's cycle words have both bits set, so a build with G0 must choose its words), the read-hash marks of the words G0H sets (`MGS2_READ_G0`) |
| `g0_rt.diff` | the same against sb17's `runtime/host_time.h` (8807446a) and `boot/boot.c` (d903d06d) |
| `boot_vu.diff` | boot.c + `MGS2_VU_LOG=<from>:<to>`: VU0's clip, status and MAC flags and a hash of VF and ACC before and after each draw-pipeline call (x86 only) |

## Builds

| Script | Builds |
|---|---|
| `cc_w00a.sh <out> <nat dir> [flags]` | the x86 w00a stage unit with the private native_w00a.c (as `tools/dev/x86_ref.sh`) |
| `mk.sh <out> <gen_s_w00a.o> [ge flags]` | the x86 twin (sb17's) with this runtime and that w00a object |
| `mkt.sh` | the read-hash proof build `/home/claude/sbg0/xt` (every unit with `GEN_DG_TRACE`, w00a's native code untraced) |
| `mkpsprg0.sh <dir>` | the PSP bucket-model build (`mkpsprk2.sh` with the G0 w00a unit) |

## Runs

| Script | Runs |
|---|---|
| `runr.sh <build> <tag> <OPT2 words> [env]` | the indoor route with gl17's settings: game-state hash (renderer mask), virtual time, sound, picture digests |
| `rundump.sh ...` | the same to a vblank, EE RAM dumped (stays in the container; delete after use) |
| `runt.sh`, `runtl.sh`, `runtlz.sh`, `runtsz.sh` | the read-hash route on xt: one `rh:` line a vblank (every load outside the draw pipeline); with `MGS2_READ_LOG` the loads one by one (`rl:` lines, about 20 MB a vblank raw); `z` = log gzipped; `runtsz.sh` adds `MGS2_READ_SKIP` ranges |
| `runvu.sh` | `runtl.sh` on the VU-log build xt2 |
| `modelg0.sh` | the model: G0 off (`MGS2_OPT2=ffffe7df`) against G0H on (`ffffffdf`), the aft deck in the rain (vb 30,810-30,890) |

The proof runs of §6 (in `/home/claude/sbg0`; G0 off = `ffffe1df,ffffe7df`, G0H = `fffff9df,ffffffdf`):
- whole route: `sh runt.sh <tag> <words>` for both, `python3 -I /mnt/project-files/mgs2/tools/dev/rh_cmp.py` on the two logs;
- without the two clip reloads: `sh runtsz.sh <tag> <words> 37800 f523bf:f523c0,12fffcd:12fffce MGS2_READ_LOG=<vblanks>`, then
  `rh_cmp.py` on the `rh:` lines and, in xt, `python3 -I /mnt/project-files/mgs2/tools/dev/rlz_cmp.py out_<a>.log.gz out_<b>.log.gz ./boot`
  on the logged vblanks.

## Readers

- `../../rh_cmp.py`, `../../rlz_cmp.py`: the vblanks whose loads differ; the differing loads grouped by reader, writer, block and lane.
- `../bkread_r.py` (`BKD=/home/claude/sbg0m/model/`): the model by part, actor and picture owner.
- `fittop.py <off> <on> 80 <build d> none` (`OVL=w00a`): the model by function (one overlay's symbols: overlays share addresses).

## H3: body_sph's, charaspread's and rain_gas_pers's prims hidden (`rain_study.md` §7)

| File | What it is |
|---|---|
| `h3_w00a.diff` | the w00a unit (`/home/claude/v31/WPL/gen_s_w00a.h`, the translation) with the six show-path stores of +164 behind `H3_WR` (hide bits set when `g_opt2` has 8192) and the 14 read-modify-write loads behind `H3_RD` (trace builds only); apply to a copy in `/home/claude/sbg0/wpl` |
| `h3_boot.diff`, `h3_host_time.diff` | `boot.c` (`g_h3_allow`: a marked word loaded at those 14 sites is hashed with bits 12-13 clear) and `host_time.h` (`OPT2_H3` 8192, off by default) against this folder's `boot.c` and `host_time.h` |
| `cc_w00a.sh`, `runtsz.sh` | updated: `W=/home/claude/sbg0/wpl` compiles the H3 copy of the unit; `X=/home/claude/sbg0/xt3` runs the H3 read-hash build |
| `mkt_h3.sh` | the read-hash build `/home/claude/sbg0/xt3`: xt's objects with boot.c and the H3 w00a unit rebuilt (`-O1 -g1`, `GEN_DG_TRACE`) |
| `mkpsph3.sh` | the model build: `/home/claude/sbg0m/d`'s w00a object rebuilt from the H3 unit (as `mkpsprg0.sh`) |
| `modelh3.sh` | the model runs: all off `ffffc7df`, G0H `ffffdfdf`, G0H with H3 `ffffffdf` (vb 30,810-30,890, 80 steps) |
| `h3log.sh <batch> <end vb> <vblanks>` | one logging batch on xt3: all off against G0H with H3, the given vblanks logged load by load, then `rlz_cmp.py` (about 27 MB of gzipped log a run for 10 deck frames: delete after reading) |
| `h3skip.py` | the host offsets of the two clip-flag reloads in a build (`MGS2_READ_SKIP`); xt3: `f51bb5:f51bb6,13004ee:13004ef`. Its line-range search did not find the w00a reload: the offsets were taken from addr2line on each `rt_dgt_rd` call of S_w00a_P331160 (the return address is the call + 5) |
| `rhz_cmp.py <a.log.gz> <b.log.gz> [<c> <d>]` | the `rh:` lines of two gzipped trace runs compared; with a second pair, which differing vblanks the two pairs share |

The proof runs of §7 (in `/home/claude/sbg0`; all off = `ffffc1df,ffffc7df`, G0H with H3 = `fffff9df,ffffffdf`): the twin `/home/claude/sbg0/x3`
with `runr.sh` and `g0cmp.py`-style comparison of hash, time and picture lines; `X=/home/claude/sbg0/xt3 sh runtsz.sh <tag> <words> 37800 <skip>`
for the read hash (skip `1:2` = none extra, or the two reloads above), `rhz_cmp.py` on the pair, then `h3log.sh` on the differing vblanks.

## NBS: body_sph's parts done natively (`rain_study.md` §8)

| File | What it is |
|---|---|
| `nbs_nat.diff` | this folder's `native_w00a.c` (md5 7b2ca19f) and `.h` (615160ed) with body_sph's native part (`nat_bs_part`, PSP VFPU asm and x86 forms, `NAT_BS_MAT` 0x1b99f0) and its `-DNAT_CHECK` code (the state before and after each part, the translation's result compared field by field, all 32 MB every `MGS2_NATCHK_FULL`-th part); gives the private tree's `nat/native_w00a.c` 1d2db06d and `.h` 9a9a62ec |
| `nbs_w00a.diff` | the H3 unit (`h3_w00a.diff` applied, md5 15140a0f) with the hook after `L3275b8:` (the act's locals to `nat_bs_part`, and on success back to `L327858`) and, in `NAT_CHECK` builds, the comparison after `L327858:`; gives ed441b44 |
| `nbs_host_time.diff` | `OPT2_H3` 8192 and `OPT2_NBS` 16384 (both off by default) against this folder's `host_time.h` (gives 1b6fb0a9) |
| `mkpsnbs.sh` | the model build: `/home/claude/sbg0m/d`'s w00a overlay rebuilt with the NBS unit (EBOOT e987ffbf, ovl_w00a.bin 62239a82: only the overlay changes) |
| `modelnbs.sh`, `modelnbs2.sh` | the model pair (NBS off `ffffbfdf` against on `ffffffdf`, G0H and H3 on in both; vb 30,810-30,890) and the repeat of the "on" run that sized the picture side's run-to-run spread |
| `bscmp.py <a.log> <b.log>` | two twin logs' hash, time and picture lines compared (g0cmp.py with body_sph's counters) |
| `fncost.py <profile> <calls> <steps> <elf>... -- <functions>` | the model's cost by function (self) and each function's callers a step, from the per-address profile and the call counts |

The proof runs of §8 (in `/home/claude/sbg0`): the check build `W=/home/claude/sbg0/wpl sh cc_w00a.sh wnbc nat -DNAT_CHECK`, `sh mk.sh xnbc2 wnbc/gen_s_w00a.o`,
`sh runr.sh xnbc2 nbc2 fffff9df,ffffffdf` (the `natchk body_sph:` line at the end of the log); the twin from the plain object (`wnb`, `xnb`) with
`runr.sh` for `ffffb9df,ffffbfdf` and `fffff9df,ffffffdf` and `bscmp.py` on the two logs.

## G0C: charaspread's particle sets skipped natively (`rain_study.md` §9)

| File | What it is |
|---|---|
| `cs_nat.diff` | the private tree's `nat/native_w00a.c` (NBS: md5 1d2db06d) and `.h` (9a9a62ec) with charaspread's native sets (`nat_cs_sets`: the lives, the births with the stage seed's draws and calls to the game's own random routine P16d358, the emitters, the stack below the act's frame, the registers, VU0's flags and the polls; the lerp `nat_cs_lerp` in VFPU asm on the PSP, `nat_vm` / `nat_va` on x86), the seven drawing buffers added to the x86 hash mask in both modes, and the `-DNAT_CHECK` code (every register, VU0's, the time and all 32 MB kept before each native call, the translation run from the same state, the two compared at 0x2d304c); gives d8dc9a23 and 2ac3bab5 |
| `cs_w00a.diff` | the NBS unit (ed441b44) with the hook before `L2d2980:` (the act's locals to `nat_cs_sets`, and on success on at `L2d304c`), `CS_RH` around the set loop (read-hash builds leave its loads out), in `NAT_CHECK` builds the comparison after `L2d304c:`, message 3's two show-path stores of +164 behind `H3_WR`, and three `#line` resyncs after the NBS blocks; gives 600b4378 |
| `cs_host_time.diff` | `OPT2_G0C` 32768 (off by default) against the NBS `host_time.h` (1b6fb0a9); gives 546b8eaf |
| `cs_boot.diff` | `boot.c` (H3 and NBS: 4d59dd16) with `g_rh_off`: `dgt_rh_add` counts no load while it is set; gives 4435c7b9 |
| `mkt_cs.sh` | the read-hash build `/home/claude/sbg0/xt4` (as `mkt_h3.sh`, with the G0C unit and `-DNAT_IN_TRACE`) |
| `mkpscs.sh` | the model build: `/home/claude/sbg0m/d`'s w00a overlay rebuilt with the G0C unit (EBOOT 31a98166, ovl_w00a.bin cfac3dd4: only the overlay changes) |
| `modelcs.sh`, `modelcs2.sh` | the model pair (G0C off `ffff7fdf` against on `ffffffdf`, G0H, H3 and NBS on in both; vb 30,810-30,890) and its repeat, which sized the run-to-run spread |

The proof runs of §9 (in `/home/claude/sbg0`): the check build `W=/home/claude/sbg0/wpl sh cc_w00a.sh wcsc nat -DNAT_CHECK`, `sh mk.sh xcsc wcsc/gen_s_w00a.o`,
`sh runr.sh xcsc csc fffff9df,ffffffdf` (the `natchk charaspread:` line at the end of the log); the twin from the plain object (`wcs`, `xcs`) with
`runr.sh` for `ffff79df,ffff7fdf` and `fffff9df,ffffffdf`; the read hash `X=/home/claude/sbg0/xt4 sh runtsz.sh <tag> <words> 37800 1:2` for both
words and `rh_cmp.py` on the `rh:` lines, then the same to vb 23,700 with `MGS2_READ_LOG=23669:23669` and, in xt4, `rlz_cmp.py` on the two logs for the one vblank that differs.

## H3R and G0R: rain.c's streaks hidden, and their loop done in place (`rain_study.md` §10)

| File | What it is |
|---|---|
| `h3r_w00a.diff` | the G0C unit (`wpl/gen_s_w00a.h`, 600b4378) with H3R: the block before the unit's first line (`h3r_second`, `H3R_WR`, the counters), `h3r_second` before the act's load at 0x31af68, the first set's show-path store at 0x31afd8 behind `H3R_WR`, the act's two loads of +164 marked (`H3_RD`), the init's store of +164 masked (`H3_MASK`); gives 2e1047d2 |
| `h3r_host_time.diff` | `OPT2_H3R` 65536 (off by default) against the G0C `host_time.h` (546b8eaf); gives 68c694dd |
| `g0r_w00a.diff` | the H3R unit (2e1047d2) with the G0R hooks: `G0R_START` at 0x31b3c4 (before the loop), `G0R_ENTRY` at each entry's start (L31b3c8), the four copies behind `G0R_D`, the two buffer registers at 0x31b488 / 0x31b48c, the jump past the offsets and the second copy-out at 0x31b65c, `G0R_END` at the loop's exit (L31b6fc: in `NAT_CHECK` builds the second pass and the comparison); gives 923514f4 |
| `g0r_nat.diff` | the private `nat/native_w00a.c` (G0C: d8dc9a23) and `.h` (2ac3bab5) with `g0r_call` (the whole-loop decision: sizes, plain RAM, the 3n buffers against each other and the game data, the second set hidden, the polls), `g0r_entry`, the second set's buffers added to the x86 hash mask in both modes, and the `-DNAT_CHECK` code (every register, VU0's, the time, all 32 MB and the act's locals kept before the loop, the translation run from the same state, both compared at 0x31b6fc); gives 536fb887 and fe4aba81 |
| `g0r_host_time.diff` | `OPT2_G0R` 131072 (off by default) against the H3R `host_time.h` (68c694dd); gives 4d23aae5 |
| `mkt_h3r.sh`, `mkt_g0r.sh` | the read-hash builds `/home/claude/sbg0/xt5` (H3R) and `xt6` (H3R and G0R; `G0R_RH` leaves the loop's loads out in both modes) |
| `mkpsg0r.sh` | the model build: `/home/claude/sbg0m/d`'s w00a overlay rebuilt with the G0R unit (EBOOT 5e156d54, ovl_w00a.bin 3a9faffc) |
| `modelg0r.sh`, `modelg0r2.sh` | the model runs: H3R and G0R off `fffcffdf`, H3R `fffdffdf`, H3R and G0R `ffffffdf` (G0H, H3, NBS and G0C on in all; vb 30,810-30,890), and their repeat, which sized the run-to-run spread |
| `twin_h3r.sh`, `twin_g0r.sh` | the twins: H3R off `fffef9df,fffeffdf` against on `fffff9df,ffffffdf` (xh3r); G0R off `fffdf9df,fffdffdf` against on `fffff9df,ffffffdf` (xg0r) |
| `dumps_h3r.sh`, `dumps_g0r.sh <vb>` | EE dumps of both modes at a vblank, then `dmask.py` (the dumps stay in the container; delete after use) |
| `dmask.py <a> <b> <dyn a> <dyn b>` | two EE dumps word by word: differing words in the static mask, in the added ranges, outside both |
| `twincmp.py <a.log> <b.log>` | two twin logs: hash fields, time lines, picture and xlive digests |
| `h3rlog.sh <batch> <end vb> <vblanks>` | one H3R logging batch on xt5 (`MGS2_READ_LOG`), then `rlz_cmp.py` |
| `wwatch.sh`, `wwatch2.sh`, `wwmap.py` | the writers of watched words (`MGS2_WWATCH`, trace builds, ASLR off) named by `nm` |

The proof runs of §10 (in `/home/claude/sbg0`): H3R's twin from `wh3r` / `xh3r`; G0R's check build `W=/home/claude/sbg0/wpl sh cc_w00a.sh /home/claude/sbg0/wg0rc /home/claude/sbg0/nat -DNAT_CHECK`, `sh mk.sh /home/claude/sbg0/xg0rc /home/claude/sbg0/wg0rc/gen_s_w00a.o`, `sh runr.sh /home/claude/sbg0/xg0rc g0rc fffff9df,ffffffdf` (the `natchk rain:` line at the end of the log); the twin from the plain object (`wg0r`, `xg0r`); the read hash `X=/home/claude/sbg0/xt6 sh runtsz.sh <tag> <words> 37800 1:2` for both words and `rh_cmp.py` on the `rh:` lines. `cc_w00a.sh` and `mk.sh` change directory first: give them absolute paths (the G0C note above, `cc_w00a.sh wcsc nat`, means `/home/claude/sbg0/wcsc` and `/home/claude/sbg0/nat`).
