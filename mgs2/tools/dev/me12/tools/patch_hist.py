# me12: histogram of a step's non-upload copy (all kinds; tags + data after tags) for the copy buffer's size
p='/home/claude/me12/boot/xlive_chk.c'
s=open(p).read()
old='''static void cs_step(void)   /* at each hook: the step's copied bytes, the most of a step */
{
'''
assert s.count(old)==1
new='''static long cs_hist[2][129];   /* me12: steps with transfers by the bytes their copy holds without the upload granules, in 16 KB buckets
                                  (the last: 2 MB and more); [0] all kinds, [1] tags and the data after them */
static void cs_hist_line(void)
{
  static const char *wn[2] = { "all kinds", "tags and the data after them" };
  for (int w = 0; w < 2; w++) { long n = 0, c = 0; int pct[5] = { 50, 90, 99, 999, 1000 }; for (int i = 0; i <= 128; i++) n += cs_hist[w][i];
    fprintf(stderr, "me12 copy sizes (%s, without upload sources): %ld steps;", wn[w], n);
    for (int j = 0, i = 0; j < 5 && n; j++) { long need = pct[j] == 1000 ? n : pct[j] == 999 ? (n * 999 + 999) / 1000 : (n * pct[j] + 99) / 100;
      while (i <= 128 && c + cs_hist[w][i] < need) c += cs_hist[w][i++];
      { static const char *pn[5] = { "half", "90%", "99%", "99.9%", "all" };
        fprintf(stderr, " %s %s%d KB;", pn[j], i >= 128 ? ">= " : "<= ", i >= 128 ? 2048 : (i + 1) * 16); } }
    fputc('\\n', stderr); }
}
static void cs_step(void)   /* at each hook: the step's copied bytes, the most of a step */
{
  { long a = cs_w.stepb[0][0] + cs_w.stepb[1][0] + cs_w.stepb[2][0] + cs_w.stepb[3][0], t = cs_w.stepb[0][0] + cs_w.stepb[1][0];
    if (a) { cs_hist[0][a >> 14 > 128 ? 128 : a >> 14]++; cs_hist[1][t >> 14 > 128 ? 128 : t >> 14]++; } }
'''
s=s.replace(old,new)
old='cs_regions(); if (wt_on) wt_report(); }'
assert s.count(old)==1
s=s.replace(old,'cs_regions(); cs_hist_line(); if (wt_on) wt_report(); }')
open(p,'w').write(s)
print("hist patched")
