import sys
p = sys.argv[1] + '/psp/xl_psp.c'; s = open(p).read()
def rep(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, (c, old[:90])
    s = s.replace(old, new)
# hold time charged to the bucket its count went into (flip_w = 1 + bucket)
rep("""static int flip_cfg, flip_on, flip_w, flip_force; static long flip_from = -1; static uint32_t flip_t0;""",
"""static int flip_cfg, flip_on, flip_w, flip_force; static long flip_from = -1; static uint32_t flip_t0;   /* flip_w: 1 + the bucket of the
                                                                                                           hold now on (gl27b), 0: none */
static int flip_sel(long vb);""")
rep("""      if (flip_on) { if (!flip_w) { flip_w = 1; flip_t0 = sceKernelGetSystemTimeLow(); flip_hold[fb]++; } return; }""",
"""      if (flip_on) { if (!flip_w) { flip_w = 1 + fb; flip_t0 = sceKernelGetSystemTimeLow(); flip_hold[fb]++; } return; }""")
rep("""    if (flip_w) { flip_us[fb] += sceKernelGetSystemTimeLow() - flip_t0; flip_w = 0; }""",
"""    if (flip_w) { flip_us[flip_w - 1] += sceKernelGetSystemTimeLow() - flip_t0; flip_w = 0; }   /* gl27b: to the hold's own bucket */""")
rep("""static void flip_line(const char *who, long vb0, long vb, int b)   /* gl27: a turn's flip counts */
{
""",
"""static void flip_line(const char *who, long vb0, long vb, int b)   /* gl27: a turn's flip counts */
{
  if (flip_w == 1 + b) { uint32_t n = sceKernelGetSystemTimeLow(); flip_us[b] += n - flip_t0; flip_t0 = n; }   /* gl27b: a hold still on:
                                                                                                               its time so far */
""")
# the first turn's flip wait by flip_sel
rep("""      if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;""",
"""      if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;
      flip_on = flip_sel(me_vb);   /* gl27b: the first turn's */""")
rep("""\n  if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;""",
"""\n  if (mq_turn) { mq_turn_k = (int)((me_vb / mq_turn) & 1); mq_self = mq_turn_k; } else mq_self = 0;
  flip_on = flip_sel(me_vb);   /* gl27b: the first turn's */""")
# the cold take-back drops the held result
rep("""  mq_cons = XS(mq)->listed;""", """  mq_cons = XS(mq)->listed; flip_w = 0;   /* gl27b: a held result dropped too */""")
# the run's end: the last list drawn, as gl26
rep("""  if (mq_sem >= 0) { sceKernelDeleteSema(mq_sem); mq_sem = -1; }
  mq_service(); mq_ge_finish();
}""",
"""  if (mq_sem >= 0) { sceKernelDeleteSema(mq_sem); mq_sem = -1; }
  flip_force = 1; mq_service(); mq_ge_finish(); flip_force = 0;   /* gl27b: the run's last list, no flip wait (the text goes up next) */
}""")
# the no-loop count where the no-loop path reports
rep(""""into the shown buffer %ld; main CPU turns: held %ld (%.2f s), drawn into the shown buffer %ld; lists of the main CPU's own picture "
          "(no loop) drawn into the shown buffer %ld\\n", flip_cfg, flip_from, flip_hold[0], flip_us[0] / 1e6, flip_race[0], flip_hold[1],
          flip_us[1] / 1e6, flip_race[1], flip_race_dir);""",
""""into the shown buffer %ld; main CPU turns: held %ld (%.2f s), drawn into the shown buffer %ld\\n", flip_cfg, flip_from, flip_hold[0],
          flip_us[0] / 1e6, flip_race[0], flip_hold[1], flip_us[1] / 1e6, flip_race[1]);""")
rep("""          (unsigned long long)dig_all);
  gl_line(f, &glf.all,""",
"""          (unsigned long long)dig_all);
  if (!mq) fprintf(f, "picture display (gl27): lists drawn into the shown buffer (the main CPU's own picture, no loop; counted only) %ld\\n",
                   flip_race_dir);
  gl_line(f, &glf.all,""")
open(p, 'w').write(s); print('ok')
