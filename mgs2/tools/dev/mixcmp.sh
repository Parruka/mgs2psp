#!/bin/sh
# mixcmp.sh <log> [<PC checkpoints>]: the log's "mix: ck" lines against the PC's checkpoints (default prof/mix_ck_x86_free_v26me.txt; v30's
# indoor route: prof/mix_ck_x86_in_v30.txt) (index, frames, commands,
# CRC, B's state hash), and how many carry the log's own "eq" (the Media Engine's A = B)
grep -a '^mix: ck' $1 | awk -v R=${2:-/mnt/project-files/mgs2/prof/mix_ck_x86_free_v26me.txt} '
  BEGIN {while ((getline l < R) > 0) {split(l, f, " "); r[f[1]] = f[2] " " f[3] " " f[4] " " f[5]}}
  {n++; v = $5 " " $7 " " $9 " " $11; if (($3 in r) && r[$3] == v) e++; else if (!d) d = $0; if ($14 == "eq" && $11 == $13) q++}
  END {printf "%d / %d checkpoints equal to the PC; %d eq (A = B); first difference: %s\n", e, n, q, d ? d : "none"}'
