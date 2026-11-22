me12: the writer tracker of the ME hand-off design (task #142, mgs2/me_handoff_design.md section 2).  Saved 2026-11-03 from the container
tree /home/claude/me12 (ephemeral).  boot/xlive_chk.c = sb9's (md5 fe1f7de9) + me12.diff (result 6098915d); runtime/ee_io.h = sb9's +
ee_io.diff (result 4f0722bc); both checked 2026-11-03 by patching sb9's files.  mk.sh builds the tracker in xd/: the game code with
GEN_DG_TRACE (every store reports itself), the runtime with WT_TRACK (its scratchpad DMA copies report too), MGS2_XLIVE_WT=1 the tracker.
wruns.sh the three routes (hash lines against sb9/x1's out_<r>2on.log), chkw.sh the checks; tools/: wt_tables.py, wt_union.py, wt_names.py
(writers by function), ws_cost.py, ws_cost2.py (guard costs), patch_hist*.py, patch_ws.py (copy sizes).  Logs stay in the container.
