# codec_time.awk: awk [-v C=1] -f codec_time.awk battery.txt (2026-10-28, me_switchoff_plan.md "gl4c run 1"); C=1 for route c (gl4c)
# time inside codec calls from battery.txt lines (t s, vb N): story 1 vb 15,643-19,901, story 2 26,070-28,762, and (route c) the
# codec opened at 30,510 to the end; the rest of the run's time is outside; the last line's t is the run's length (to a stop)
match($0, /t +[0-9]+ s, vb +-?[0-9]+/) { s = substr($0, RSTART, RLENGTH); gsub(/[^0-9 -]/, " ", s); split(s, a, " +"); t = a[2] + 0; vb = a[3] + 0
  if (pt != "" && vb >= 0) { dt = t - pt; mid = (vb + pvb) / 2
    inc = (mid > 15643 && mid <= 19901) || (mid > 26070 && mid <= 28762) || (C && mid > 30510)
    if (inc) cin += dt; else cout += dt }
  pt = t; pvb = vb }
END { printf "%-26s codec %5.1f min, outside %5.1f min, length %5.1f min, last vb %d\n", FILENAME, cin / 60, cout / 60, pt / 60, pvb }
