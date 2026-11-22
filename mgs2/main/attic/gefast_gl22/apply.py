# gl22: the GE's finish seen at once (sceGu's finish callback) + GE timestamps; MGS2_ME_PIC_GEFAST 0 off, 1 on, 2 every other ME turn
import sys
p = sys.argv[1]; s = open(p).read()
def rep(a, b, n=1):
    global s
    c = s.count(a); assert c == n, (a[:70], c); s = s.replace(a, b)
rep("static uint32_t mq_cons, mq_subm; static uint64_t mq_ge_t0;   /* results taken from the ME, lists given to the GE; the last one's time */",
"""static uint32_t mq_cons, mq_subm; static uint64_t mq_ge_t0;   /* results taken from the ME, lists given to the GE; the last one's time */
/* gl22: the GE's finish seen at once.  sceGu's finish callback (the GE's interrupt) stores the time and counts; with MGS2_ME_PIC_GEFAST on
   it also sets ge_done, so the ME may begin its next frame without waiting for the main CPU to notice (the main CPU still shows the frame
   and gives the GE the next list in mq_service).  Timestamps per list: submit -> finish (the GE's drawing; the list is whole when
   submitted) and finish -> the main CPU's notice; by maker: 0 ME turns fast off, 1 ME turns fast on, 2 main CPU turns */
static volatile uint32_t ge_fin_n, ge_fin_t; static uint32_t ge_sub_t, ge_fin_n0; static int ge_fast_cfg, ge_fast;
static long ge_n[3], ge_nofin; static uint64_t ge_draw[3], ge_note[3]; static uint32_t ge_draw_max[3];
static long ge_tn0; static uint64_t ge_td0, ge_tq0;   /* the ME turn's start (mq_tstart) */
static void ge_fin_cb(int id)
{
  (void)id; ge_fin_t = sceKernelGetSystemTimeLow(); ge_fin_n++;
  if (ge_fast && mq && gl_ge && !mq_self) XS(mq)->ge_done = mq_subm;
}
static void ge_noted(void)   /* the main CPU has seen the GE's list finished */
{
  uint32_t now = sceKernelGetSystemTimeLow(); int b = mq_self ? 2 : ge_fast ? 1 : 0;
  if (ge_fin_n != ge_fin_n0) { uint32_t d = ge_fin_t - ge_sub_t; ge_n[b]++; ge_draw[b] += d; ge_note[b] += now - ge_fin_t;
    if (d > ge_draw_max[b]) ge_draw_max[b] = d; }
  else ge_nofin++;
}""")
# the callback registered with the GE's set-up
rep("  sceGuInit();                           /* as psp/ge_replay, without sceGuDisplay: the harness shows the frames itself */\n",
    "  sceGuInit();                           /* as psp/ge_replay, without sceGuDisplay: the harness shows the frames itself */\n"
    "  sceGuSetCallback(GU_CALLBACK_FINISH, ge_fin_cb);   /* gl22: the GE's finish, timed (and with MGS2_ME_PIC_GEFAST told to the ME) */\n")
# submit: the time and the finish count before the GE starts
rep("  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */\n  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(r->list); sceGuFinish();",
    "  sceKernelDcacheWritebackAll();         /* the GE reads the list, the frame and the textures from RAM */\n  ge_fin_n0 = ge_fin_n; ge_sub_t = sceKernelGetSystemTimeLow();   /* gl22 */\n  sceGuStart(GU_DIRECT, gl_dl); sceGuCallList(r->list); sceGuFinish();")
# the two places the main CPU notices
rep("    if (!r) { gl_show(); XS(mq)->ge_done = mq_subm; }", "    if (!r) { ge_noted(); gl_show(); XS(mq)->ge_done = mq_subm; }")
rep("  if (r) { mq_released_all(); return; }   /* (gl_sync's stop: gl_ok 3, the text on the screen) */\n  gl_show(); XS(mq)->ge_done = mq_subm;",
    "  if (r) { mq_released_all(); return; }   /* (gl_sync's stop: gl_ok 3, the text on the screen) */\n  ge_noted(); gl_show(); XS(mq)->ge_done = mq_subm;")
# config
rep('  mq_turn = (e = mgs2_getenv("MGS2_ME_PIC_TURN")) && atoi(e) > 0 ? atoi(e) : 0;\n',
    '  mq_turn = (e = mgs2_getenv("MGS2_ME_PIC_TURN")) && atoi(e) > 0 ? atoi(e) : 0;\n'
    '  ge_fast_cfg = (e = mgs2_getenv("MGS2_ME_PIC_GEFAST")) ? atoi(e) : 0; ge_fast = ge_fast_cfg == 1;   /* gl22 */\n'
    '  fprintf(stderr, "me picture GE (gl22): the GE\'s finish timed by sceGu\'s finish callback; the ME told of it at once (MGS2_ME_PIC_GEFAST): %s\\n",\n'
    '          ge_fast_cfg == 2 ? "in every other ME turn (off, on, ...)" : ge_fast_cfg == 1 ? "always" : "never (the main CPU tells it, as gl21)");\n')
# turns: fast in every other ME turn
rep("    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); }\n  mq_turn_k = k;",
    "    if (ge_fast_cfg == 2) ge_fast = (int)((vb / (2 * mq_turn)) & 1);   /* gl22: off, on, off, ... by ME turn */\n"
    "    xme_heap_ask(me_rs_on); mq_self = 0; mq_tstart(vb); }\n  mq_turn_k = k;")
# per-turn snapshot and line
rep("  mq_tovu0 = mq_ov_us; mq_tovn0 = mq_ov_n; mq_ov_f = 0;\n",
    "  mq_tovu0 = mq_ov_us; mq_tovn0 = mq_ov_n; mq_ov_f = 0;\n  { int b = ge_fast ? 1 : 0; ge_tn0 = ge_n[b]; ge_td0 = ge_draw[b]; ge_tq0 = ge_note[b]; }   /* gl22 */\n")
rep('''"the main CPU waited (ms): copies as they ended %.1f, fills %.1f, the arena %.1f, the rest %.1f; from a hook to its first copy plan "
          "%ld times, mean %.2f ms\\n", mq_tvb0''',
'''"the main CPU waited (ms): copies as they ended %.1f, fills %.1f, the arena %.1f, the rest %.1f; from a hook to its first copy plan "
          "%ld times, mean %.2f ms; gl22: idle in this turn %.1f; GE fast %d: lists %ld, drawing %.1f ms, seen %.1f ms after its finish\\n", mq_tvb0''')
rep('''          mq_ov_n > mq_tovn0 ? (double)(mq_ov_us - mq_tovu0) / 1000.0 / (double)(mq_ov_n - mq_tovn0) : 0.0);
  mq_tvb0 = -1;''',
'''          mq_ov_n > mq_tovn0 ? (double)(mq_ov_us - mq_tovu0) / 1000.0 / (double)(mq_ov_n - mq_tovn0) : 0.0,
          tt - (all - ms[XME_T_IDLE]), ge_fast, ge_n[ge_fast ? 1 : 0] - ge_tn0, (double)(ge_draw[ge_fast ? 1 : 0] - ge_td0) / 1000.0,
          (double)(ge_note[ge_fast ? 1 : 0] - ge_tq0) / 1000.0);
  mq_tvb0 = -1;''')
# report
rep('''static void mq_report(FILE *f)
{
''', '''static void mq_report(FILE *f)
{
  { static const char *nm[3] = { "ME turns, the main CPU tells the ME", "ME turns, the finish tells the ME (GEFAST)", "main CPU turns" };   /* gl22 */
    for (int b = 0; b < 3; b++) if (ge_n[b])
      fprintf(f, "me picture GE (gl22): %s: %ld lists, the GE drawing a list %.2f ms on average (at most %.2f), the main CPU seeing its finish "
              "%.2f ms after it on average\\n", nm[b], ge_n[b], (double)ge_draw[b] / 1000.0 / ge_n[b], ge_draw_max[b] / 1000.0,
              (double)ge_note[b] / 1000.0 / ge_n[b]);
    fprintf(f, "me picture GE (gl22): finishes counted by the callback %u, lists seen finished with no callback %ld\\n", (unsigned)ge_fin_n, ge_nofin); }
''')
open(p, 'w').write(s)
print('ok')
