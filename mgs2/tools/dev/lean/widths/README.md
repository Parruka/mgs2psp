# Register widths checker (lean_plan.md step 2; notes: recomp/widths_step2.md)

Dev tools, run in the container. Inputs there (ephemeral, rebuilt from the project's scripts): the post-narrow sources
(postnarrow.py -> /home/claude/lean/w2/pn), v27's headers with their interprocedural and return sets (tools/mk_headers.sh with the
v27 options -> /home/claude/lean/hP), the survey outputs (SV, /home/claude/lean/w3/tmp).

- wpo2.py: parser of regcache.py's output (events per block) and phase 1, the forward W analysis. wpd.py: the demand fixpoint, all
  units (`WPD_STATE=<file> python3 wpd.py <out.json>`; WPD_EXM=1 exact memory is the default; WPO_NOIND=1 is a dev bound, not
  sound; WPD_V32=<widths json> runs the narrow-in variant for the hybrid: functions without wide registers in that json get no W at
  entry or from callees, only from their own frame loads). rcdrv.py: regcache's process() for one function with chosen widths (RC_DIR picks the regcache copy). wstat.py: time
  by width class from the v26 profile. wpstate.py: load a saved fixpoint. taken.py: address-taken functions. wpd_why.py: labels.
- dev/: hybrid.py (proof widths in v27's 64-bit functions, v27's in the rest), mkh_wpw.sh (headers with RC_WPW; pass RCPY and
  RC_CONDCLEAN=1 for the fixed regcache), setcmp.py (sets against hP), why_st.py / jalr_st.py (why a local is wide), emit1.py
  (regcache output of one function), wdiff.py, fncmp.py (per-function model deltas), quick.sh, mixw.py, nchk_strip.py,
  x86_ref_x.sh (x86 build with extra flags, e.g. XCF=-DGEN_DG_TRACE), model1.sh, emucmp.sh.  2026-10-25: hybrid2.py (proof widths in
  v27's 64-bit functions, narrow-in widths in its 32-bit ones: hyb7), sqmiss.py / ldst.py (the functions v27's WIDE list made 32-bit
  with an sq to a base other than $sp / a frame ld-lq to sd-sq copy: regcache's RC_SQWIDE), dumpdiff.py (two EE RAM dumps word by
  word), ww_map.py (names the writers in 'wwatch:' lines).
- runs/: xrun27.sh + xcmp27.sh (three routes against the v27 references, per-frame window with all fields), rune.sh + ecmp.sh
  (a hash at every vblank, against v27's), runh_n.sh / xrun27n.sh (keep NCHK lines), dump2.sh / dumpf.sh (EE RAM dumps, container
  only, delete after), ww.sh (write watch on a traced build, no address randomisation), chain6.sh (checker -> headers -> routes).  2026-10-25: rf.sh (the free route
  from a build dir with chosen variables, e.g. MGS2_WWATCH / MGS2_READ_LOG on a traced build), cmpb.sh (two builds' route outputs
  side by side), pspb.sh (plain PSP build + model + emulator hashes), chain10.sh (hyb7), chain12.sh (the all-64-bit reference:
  RC_WIDE=1 with recomp/widths/cov_a64.txt.gz, the coverage list plus the 326 ELF functions v27 inlines), chain13.sh.
- regcache_condclean.diff: the RC_CONDCLEAN fix (an RCLD / RCMV inside an if-block cleans only the paths through that block),
  against recomp/regcache.py md5 d1dcf91d5eef435cbaf9866aa8749adc.  Both fixes are in the project's regcache.py since 2026-10-25
  (RC_CONDCLEAN=1 and RC_SQWIDE=1, md5 c79518699e4c41d1510498afb4e963ba; with neither option it makes v27's headers byte for byte).
