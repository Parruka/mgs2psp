/* psp/bbox.c - gl44 block A (2026-11-27; attic/gl44/state.md; freeze_study.md section 3, items 1 and 2): the harness keeps the runs'
   files (H1) and writes a black box a second (H2).  User mode; nothing but files is written (no hardware register), no power or clock
   call but the four battery getters the battery thread already makes (psp/v26me_bat.c bat_line), nothing new on the Media Engine (its
   words are read as the battery thread reads them, psp/xl_psp.c xl_psp_rec).
   H1 (always, MGS2_BBOX or not): bb_rotate, from main() before any file is written: for log.txt, battery.txt, crash.txt, steps.csv and
     bbox.txt next to the EBOOT, the _prev2 file is removed, the _prev1 file becomes _prev2 and the file itself _prev1 (log_prev1.txt,
     steps_prev1.csv, ...), whether or not the run before reached its end; the shots stay.  By sceIoRename (gl44's one new import); a
     rename that fails is tried with the bare name, then the file is copied and the original removed.  What was done goes into the new
     log.txt's first lines (bb_rotate_report), with whether the run before's log had its end line.
   H2 (MGS2_BBOX=1; off when the key is missing or anything but 1): bb_start makes bbox.txt at its full size at the start (BB_N records
     of BB_RL bytes, blank lines) and writes record 0; from the first window (vb 100: by then the picture took its blocks of the system's
     memory, so the thread's stack does not move them) a thread at priority 0x10 (above the game thread's 0x20, the battery, HOME and
     power threads' 0x11; beside the crash writer's 0x10) writes one record a second into slot seq % BB_N with open, seek, write and close:
     the file's size never changes.  The thread makes the record from plain memory reads first (nothing that can wait), writes it, and
     only then calls the system (the battery getters, the GE list's state) for the next record: a call that hangs leaves this second's
     record on the stick.  Its own semaphore is held while it writes; the exit (bb_exit) takes it and writes the last record (EX).
   The record (BB_RL bytes: 127 characters and a new line; tools: build_gl44/rd/rdbbox.py):
     s<seq 7> t<us since the start 10> c<the PSP's display vblanks 6> v<the game's vblank 5> n<the game thread's phase changes, hex 5>
     p<phase tag 2><detail, hex 2> d<the last disc pack read: sector, hex 6>+<sectors, hex 3> f<the picture: turn S/C/M, where e/h/x/t>
     h<the loop's heartbeat, hex 6> a<the loop's place word, hex 4> m<the ME's own heartbeat, hex 6> q<entries waiting, hex 2>
     g<the last GE list id, hex 8>:<its state, sceGeListSync peek> b<battery mV 4>,<% 3>,<c charging><x external power>
     l<this wake's lateness, ms 3> w<the write before's time, ms 3>
   Numbers wrap at their width (the decimal ones modulo 10^width).  The battery fields and the GE state come from just after the record
   before was written. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "bbox.h"

volatile uint32_t g_bb_ph = BB_ST, g_bb_n;   /* the game thread's (psp/bbox.h) */
extern volatile uint32_t g_disc_lsn, g_disc_k;   /* rt/iop_host.h (gl44): the pack read under way or the last one */
extern volatile long me_mix_last_vb;   /* psp/gl1_glue.c: the latest vblank (-1 before the first) */
void xl_psp_rec(uint32_t v[16]);      /* psp/xl_psp.c (gl28): the picture loop's and the ME's words, read only */
extern int ge_list_executed[];        /* libpspgu (sceGuStart): the id of the last list given to the GE (all of ours are GU_DIRECT) */
char *mgs2_getenv(const char *k);     /* psp/main_psp.c: mgs2.cfg's settings */
static uint64_t bb_now(void) { return (uint64_t)sceKernelGetSystemTimeWide(); }

/* ---- H1 ---- */
#define BB_NF 5
static const char *const bb_nm[BB_NF][2] = { { "log", ".txt" }, { "battery", ".txt" }, { "crash", ".txt" }, { "steps", ".csv" }, { "bbox", ".txt" } };
static char bb_rot[BB_NF][96]; static int bb_rot_end = -2;   /* -2 no log.txt, -1 not read, 0 no end line, 1 the end line */
static int bb_there(const char *p) { SceIoStat st; memset(&st, 0, sizeof st); return sceIoGetstat(p, &st) >= 0; }
static int bb_copy(const char *a, const char *b, char *buf, int n)   /* a's bytes to a new b: 0, or < 0 (b removed) */
{
  SceUID f = sceIoOpen(a, PSP_O_RDONLY, 0), g; int k, ok = 1;
  if (f < 0) return -1;
  if ((g = sceIoOpen(b, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) < 0) { sceIoClose(f); return -2; }
  while ((k = sceIoRead(f, buf, n)) > 0) if (sceIoWrite(g, buf, k) != k) { ok = 0; break; }
  if (k < 0) ok = 0;
  sceIoClose(g); sceIoClose(f);
  if (!ok) { sceIoRemove(b); return -3; }
  return 0;
}
static int bb_move(const char *a, const char *b, char *buf, int n)   /* a to b (no b there): 0 renamed, 1 renamed by the bare name,
                                                                        2 copied, else the rename's error (a kept) */
{
  int r = sceIoRename(a, b); const char *s = strrchr(b, '/');
  if (r >= 0 && !bb_there(a) && bb_there(b)) return 0;
  if (s && sceIoRename(a, s + 1) >= 0 && !bb_there(a) && bb_there(b)) return 1;
  if (bb_there(a) && !bb_there(b) && bb_copy(a, b, buf, n) == 0) { sceIoRemove(a); return 2; }
  return r < 0 ? r : -1;
}
static int bb_endline(const char *p, char *buf, int n)   /* "end: " at a line's start in the last 64 KB: 1, 0, -1 not read */
{
  SceUID f = sceIoOpen(p, PSP_O_RDONLY, 0); SceOff sz, at; int k, i, found = 0;
  if (f < 0) return -1;
  sz = sceIoLseek(f, 0, PSP_SEEK_END); at = sz > 65536 ? sz - 65536 : 0;
  if (sz < 0 || sceIoLseek(f, at, PSP_SEEK_SET) != at) { sceIoClose(f); return -1; }
  while (!found && (k = sceIoRead(f, buf, n)) > 0) {
    for (i = 0; i + 5 <= k; i++) if (!memcmp(buf + i, "end: ", 5) && ((i == 0 && at == 0) || (i > 0 && buf[i - 1] == '\n'))) { found = 1; break; }
    if (k < n) break;
    at += k - 6; if (sceIoLseek(f, at, PSP_SEEK_SET) != at) break;   /* 6 bytes again, so a line start across two reads is seen */
  }
  sceIoClose(f); return found;
}
int bb_rotate(const char *dir)
{
  char b[8192], c[256], p1[256], p2[256]; int i, kept = 0;
  for (i = 0; i < BB_NF; i++) {
    const char *nm = bb_nm[i][0], *ex = bb_nm[i][1]; int r1 = -100, r0 = -100, had1;
    snprintf(c, sizeof c, "%s%s%s", dir, nm, ex); snprintf(p1, sizeof p1, "%s%s_prev1%s", dir, nm, ex); snprintf(p2, sizeof p2, "%s%s_prev2%s", dir, nm, ex);
    if (bb_there(p2)) sceIoRemove(p2);
    if ((had1 = bb_there(p1))) r1 = bb_move(p1, p2, b, sizeof b);
    if (bb_there(c)) {
      if (i == 0) bb_rot_end = bb_endline(c, b, sizeof b);
      if (bb_there(p1)) sceIoRemove(p1);   /* (its move failed: the run before's file matters more than the one before it) */
      r0 = bb_move(c, p1, b, sizeof b); if (r0 >= 0 && r0 <= 2) kept++;
    }
    snprintf(bb_rot[i], sizeof bb_rot[i], "%s%s %s; %s", nm, ex,
             r0 == -100 ? "none" : r0 == 0 ? "-> _prev1" : r0 == 1 ? "-> _prev1 (bare name)" : r0 == 2 ? "-> _prev1 (copied)" : "NOT KEPT",
             !had1 ? "no _prev1" : r1 == 0 ? "_prev1 -> _prev2" : r1 == 1 ? "_prev1 -> _prev2 (bare name)" : r1 == 2 ? "_prev1 -> _prev2 (copied)"
             : "_prev1 removed (its move failed)");
    if (r0 < -1 && r0 != -100) { int k = (int)strlen(bb_rot[i]); snprintf(bb_rot[i] + k, sizeof bb_rot[i] - k, " (rename %08x)", (unsigned)r0); }
  }
  return kept;
}
void bb_rotate_report(void (*o)(const char *, ...))
{
  o("gl44 H1 (the files of the runs before kept, at every start): %s; %s; %s; %s; %s\n", bb_rot[0], bb_rot[1], bb_rot[2], bb_rot[3], bb_rot[4]);
  o("gl44 H1: the run before's log.txt (now log_prev1.txt) %s\n", bb_rot_end == 1 ? "has its end line (that run reached its end)" :
    bb_rot_end == 0 ? "has NO end line (that run did not reach its end: a stop, a freeze or a power-off)" :
    bb_rot_end == -1 ? "could not be read for its end line" : "was not there");
}

/* ---- H2 ---- */
#define BB_N 3600
#define BB_RL 128
static char bb_path[256], bb_r[BB_RL]; static int bb_on, bb_made, bb_why; static SceUID bb_sema = -1, bb_th = -1;
static uint64_t bb_t0; static uint32_t bb_seq; static volatile int bb_stop;
static int bb_mv = -1, bb_pc = -1, bb_chg, bb_ext, bb_gst = -1000; static uint32_t bb_gid;   /* the system's values for the next record */
static uint32_t bb_nrec, bb_nfail, bb_wus, bb_wmax, bb_lmax, bb_prep_us, bb_size; static uint64_t bb_wsum; static long bb_go_vb = -1;
/* (out of line, 32-bit: the record's code stays small, so newlib's heap keeps its room: the image takes it from the heap) */
static __attribute__((noinline)) char *bdec(char *p, uint32_t v, int w) { char *e = p + w; while (e > p) { *--e = (char)('0' + v % 10); v /= 10; } return p + w; }
static __attribute__((noinline)) char *bhex(char *p, uint32_t v, int w) { char *e = p + w; while (e > p) { *--e = "0123456789abcdef"[v & 15u]; v >>= 4; } return p + w; }
static __attribute__((noinline)) char *bkey(char *p, char k) { *p++ = ' '; *p++ = k; return p; }
static char bb_pc1(uint32_t c) { return c >= 0x21 && c < 0x7f ? (char)c : '?'; }
static void bb_slow(void)   /* after a record's write: the system's values for the next one */
{
  int id;
  bb_mv = scePowerGetBatteryVolt(); bb_pc = scePowerGetBatteryLifePercent();
  bb_chg = scePowerIsBatteryCharging() > 0; bb_ext = scePowerIsPowerOnline() > 0;
  id = ge_list_executed[0]; bb_gid = (uint32_t)id; bb_gst = id ? sceGeListSync(id, 1) : -1000;
}
static void bb_make(uint32_t seq, uint64_t t, uint32_t ph, uint32_t late)   /* the record in bb_r (plain reads, no call that can wait) */
{
  char *p = bb_r; uint32_t v[16], n = g_bb_n, lsn = g_disc_lsn, k = g_disc_k; long vb = me_mix_last_vb; uint32_t lm = late / 1000, wm = bb_wus / 1000;
  *p++ = 's'; p = bdec(p, seq, 7);
  p = bkey(p, 't'); p = bdec(p, (uint32_t)(t / 1000000u % 10000u), 4); p = bdec(p, (uint32_t)(t % 1000000u), 6);   /* us modulo 10^10 */
  p = bkey(p, 'c'); p = bdec(p, (uint32_t)sceDisplayGetVcount(), 6);
  p = bkey(p, 'v'); p = bdec(p, vb < 0 ? 0u : (uint32_t)vb, 5);
  p = bkey(p, 'n'); p = bhex(p, n, 5);
  p = bkey(p, 'p'); *p++ = bb_pc1(ph & 0xffu); *p++ = bb_pc1(ph >> 8 & 0xffu); p = bhex(p, ph >> 16 & 0xffu, 2);
  p = bkey(p, 'd'); p = bhex(p, lsn, 6); *p++ = '+'; p = bhex(p, k > 0xfffu ? 0xfffu : k, 3);
  xl_psp_rec(v);
  p = bkey(p, 'f');
  if (!(v[0] & 1u)) { *p++ = '-'; *p++ = '-'; }
  else { *p++ = v[0] & 16u ? 'S' : v[0] & 8u ? 'C' : 'M'; *p++ = v[0] & 2u ? 'e' : v[0] & 4u ? 'h' : v[0] & 64u ? 'x' : 't'; }
  p = bkey(p, 'h'); p = bhex(p, v[3], 6);
  p = bkey(p, 'a'); p = bhex(p, v[4], 4);
  p = bkey(p, 'm'); p = bhex(p, v[5], 6);
  p = bkey(p, 'q'); p = bhex(p, v[1] - v[2], 2);
  p = bkey(p, 'g'); p = bhex(p, bb_gid, 8); *p++ = ':';
  *p++ = bb_gst == -1000 ? '-' : bb_gst < 0 ? 'E' : bb_gst <= 9 ? (char)('0' + bb_gst) : '?';
  p = bkey(p, 'b');
  if (bb_mv < 0) { memcpy(p, "----", 4); p += 4; } else p = bdec(p, (uint32_t)bb_mv, 4);
  *p++ = ',';
  if (bb_pc < 0) { memcpy(p, "---", 3); p += 3; } else p = bdec(p, (uint32_t)bb_pc, 3);
  *p++ = ','; *p++ = bb_chg ? 'c' : '-'; *p++ = bb_ext ? 'x' : '-';
  p = bkey(p, 'l'); p = bdec(p, lm > 999 ? 999 : lm, 3);
  p = bkey(p, 'w'); p = bdec(p, wm > 999 ? 999 : wm, 3);
  while (p < bb_r + BB_RL - 1) *p++ = ' ';
  bb_r[BB_RL - 1] = '\n';
}
static void bb_put(uint32_t seq)   /* bb_r into its slot: open, seek, write, close (the file keeps its size) */
{
  uint64_t t0 = bb_now(); SceUID f = sceIoOpen(bb_path, PSP_O_WRONLY, 0777); int ok = 0;
  if (f >= 0) { SceOff at = (SceOff)(seq % BB_N) * BB_RL;
    ok = sceIoLseek(f, at, PSP_SEEK_SET) == at && sceIoWrite(f, bb_r, BB_RL) == BB_RL; sceIoClose(f); }
  { uint64_t d = bb_now() - t0; bb_wus = d > 0xffffffffu ? 0xffffffffu : (uint32_t)d; }
  bb_nrec++; bb_wsum += bb_wus; if (bb_wus > bb_wmax) bb_wmax = bb_wus; if (!ok) bb_nfail++;
}
static int bb_thread(SceSize n, void *a)
{
  uint64_t next = bb_now(); (void)n; (void)a;
  for (;;) {
    uint64_t t = bb_now(); uint32_t late;
    next += 1000000u;
    if (next > t) sceKernelDelayThread((SceUInt)(next - t));
    t = bb_now(); late = t > next ? (t - next > 0xffffffffu ? 0xffffffffu : (uint32_t)(t - next)) : 0;
    if (late > bb_lmax) bb_lmax = late;
    if (t > next + 1000000u) next = t;   /* a stall of more than a second: the next record a second from now */
    if (sceKernelWaitSema(bb_sema, 1, 0) < 0 || bb_stop) { sceKernelSleepThread(); continue; }
    bb_seq++; bb_make(bb_seq, t - bb_t0, g_bb_ph, late); bb_put(bb_seq);
    sceKernelSignalSema(bb_sema, 1);
    bb_slow();
  }
  return 0;
}
void bb_start(const char *dir)
{
  const char *e = mgs2_getenv("MGS2_BBOX"); char b[4096]; SceUID f; int i, ok = 1; uint64_t t0;
  bb_t0 = bb_now();
  bb_on = e && e[0] == '1' && !e[1];
  if (!bb_on) { fprintf(stderr, "gl44 black box (MGS2_BBOX %s): off\n", e ? e : "not set"); return; }
  snprintf(bb_path, sizeof bb_path, "%sbbox.txt", dir);
  t0 = bb_now();
  for (i = 0; i < (int)sizeof b; i++) b[i] = (i % BB_RL) == BB_RL - 1 ? '\n' : ' ';
  if ((f = sceIoOpen(bb_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777)) < 0) { bb_on = 0; bb_why = f; fprintf(stderr, "gl44 black box: %s NOT made (%08x): off\n", bb_path, (unsigned)f); return; }
  for (i = 0; i < BB_N * BB_RL && ok; i += (int)sizeof b) { int k = BB_N * BB_RL - i < (int)sizeof b ? BB_N * BB_RL - i : (int)sizeof b; ok = sceIoWrite(f, b, k) == k; }
  sceIoClose(f);
  { SceIoStat st; memset(&st, 0, sizeof st); bb_size = sceIoGetstat(bb_path, &st) >= 0 ? (uint32_t)st.st_size : 0; }
  bb_prep_us = (uint32_t)(bb_now() - t0);
  if (!ok || bb_size != BB_N * BB_RL) { bb_on = 0; bb_why = -2; fprintf(stderr, "gl44 black box: %s written short (%u bytes): off\n", bb_path, (unsigned)bb_size); return; }
  bb_made = 1;
  bb_slow(); bb_make(0, bb_now() - bb_t0, g_bb_ph, 0); bb_put(0);   /* record 0: the start (main thread) */
  fprintf(stderr, "gl44 black box (MGS2_BBOX 1): %s made at %u bytes (%d records of %d) in %u ms, record 0 written in %.2f ms; the thread from the first window\n", bb_path, (unsigned)bb_size, BB_N, BB_RL, (unsigned)(bb_prep_us / 1000), bb_wus / 1000.0);
}
void bb_go(void)
{
  if (!bb_on || bb_th >= 0 || bb_stop) return;
  bb_go_vb = me_mix_last_vb;
  if ((bb_sema = sceKernelCreateSema("bbox", 0, 1, 1, 0)) < 0) { bb_on = 0; bb_why = bb_sema; return; }
  if ((bb_th = sceKernelCreateThread("bbox", bb_thread, 0x10, 0x1000, 0, 0)) < 0 || sceKernelStartThread(bb_th, 0, 0) < 0) {
    bb_why = bb_th; bb_on = 0; fprintf(stderr, "gl44 black box: thread NOT started (%08x): record 0 only\n", (unsigned)bb_th); return; }
  fprintf(stderr, "gl44 black box: thread bbox %08x at priority 0x10 (4 KB stack) from vb %ld: a record a second into %s\n", (unsigned)bb_th, bb_go_vb, bb_path);
}
void bb_exit(void)
{
  SceUInt to = 500000;
  if (!bb_made || bb_stop) return;
  if (bb_sema >= 0 && sceKernelWaitSema(bb_sema, 1, &to) < 0) return;   /* (a write that does not end in 0.5 s: no last record) */
  bb_stop = 1; bb_seq++; bb_make(bb_seq, bb_now() - bb_t0, BB_EX, 0); bb_put(bb_seq);   /* the semaphore stays taken: no record after it */
}
void bb_report(void (*o)(const char *, ...))
{
  const char *e = mgs2_getenv("MGS2_BBOX");
  if (!bb_made) { if (e && e[0] == '1') o("gl44 black box (MGS2_BBOX %s): OFF: bbox.txt could not be made (%08x)\n", e, (unsigned)bb_why);
                  else o("gl44 black box (MGS2_BBOX %s): off\n", e ? e : "not set"); return; }
  o("gl44 black box (MGS2_BBOX 1): bbox.txt made at %u bytes (%d records of %d) in %u ms at the start; the thread from vb %ld (%s): %u records "
    "(%u writes failed), write time mean %.2f ms, max %.2f ms; the thread's wake lateness max %.2f ms\n", (unsigned)bb_size, BB_N, BB_RL,
    (unsigned)(bb_prep_us / 1000), bb_go_vb, bb_th >= 0 ? "started" : "NOT started", (unsigned)bb_nrec, (unsigned)bb_nfail,
    bb_nrec ? (double)bb_wsum / bb_nrec / 1000.0 : 0.0, bb_wmax / 1000.0, bb_lmax / 1000.0);
}
/* rt/iop_host.h's clock of the pack reads, called at a read's start and at its end, in pairs (main_psp.c sets it): the read's tag */
static uint32_t bb_dold; static int bb_din;
uint64_t bb_disc_clock(void)
{
  uint64_t t = bb_now();
  if (!bb_din) { bb_dold = bb_set(BB_DR); bb_din = 1; } else { bb_set(bb_dold); bb_din = 0; }
  return t;
}
