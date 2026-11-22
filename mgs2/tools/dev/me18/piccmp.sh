#!/bin/sh
# piccmp.sh <log> <reference pic lines>: the log's frame digest lines ("picture: vb N buffer ...") against a reference's, by vb
grep -a '^picture: vb [0-9]* buffer' "$1" | awk 'NR == FNR {r[$3] = $0; next} {n++; if (!($3 in r)) {m++; next} if (r[$3] == $0) e++; else {d++; if (d <= 3) print "differs: " $0 "\n   want: " r[$3]}}
  END {printf "%d frames: %d equal, %d differ, %d not in the reference (reference %d)\n", n, e, d, m, length(r)}' "$2" -
