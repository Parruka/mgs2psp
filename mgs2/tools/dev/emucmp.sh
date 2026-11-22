#!/bin/sh
# emucmp.sh <log> <reference>: a harness log's hash lines (vb ee iop [vfb]) against an x86 hash file (prof/hashes_*: vb ee iop vfb)
# or a hashref file (a name with "hashref": vb ee vfb [vuh]); the VFPU fallback count (vfb) only where the log prints it
# (MGS2_VFPU_STAT=1); prints equal / compared and the first difference; against a hashref file also the "vuh:" lines (VU0 state,
# the hashref's fourth column), then the log's first "split:" lines (v28, 2026-10-25)
case $(basename $2) in *hashref*) M=r ;; *) M=x ;; esac
grep -a '^hash: vb' $1 | awk '{print $3, $5, $7, $9}' | awk -v M=$M 'NR == FNR {r[$1] = $0; next}
     ($1 in r) {n++; split(r[$1], f, " ")
       if (M == "x") {k = (NF >= 4) ? f[1] " " f[2] " " f[3] " " f[4] : f[1] " " f[2] " " f[3]; v = (NF >= 4) ? $1 " " $2 " " $3 " " $4 : $1 " " $2 " " $3}
       else {k = (NF >= 4) ? f[1] " " f[2] " " f[3] : f[1] " " f[2]; v = (NF >= 4) ? $1 " " $2 " " $4 : $1 " " $2}
       if (k == v) e++; else if (!d) d = v " (want " k ")"}
     END {printf "%d / %d hash lines equal; first difference: %s\n", e, n, d ? d : "none"}' $2 -
[ $M = r ] && grep -a '^vuh: vb' $1 | awk 'NR == FNR {if (NF >= 4) r[$1] = $4; next} ($3 in r) {n++; if (r[$3] == $4) e++; else if (!d) d = $0 " (want " r[$3] ")"}
  END {printf "%d / %d vuh lines equal; first difference: %s\n", e, n, d ? d : "none"}' $2 -
grep -a '^split:' $1 | head -3
